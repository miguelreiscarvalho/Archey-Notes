#include "DocumentTabs.h"
#include <algorithm>
#include "MainWindow.h"
#include "XournalView.h"
#include "control/Control.h"
#include "control/CrashHandler.h"
#include "control/jobs/XournalScheduler.h"
#include "control/settings/Settings.h"
#include "model/Document.h"
#include "util/PathUtil.h"
#include "util/Util.h"
#include "util/XojMsgBox.h"
#ifdef _WIN32
#include <gdk/gdkwin32.h>
#include <windows.h>
#undef min
#undef max
#endif

namespace {
GtkWidget* textButton(const char* text, std::function<void()> callback) {
    auto* b=gtk_button_new_with_label(text);
    auto* fn=new std::function<void()>(std::move(callback));
    g_signal_connect_data(b,"clicked",G_CALLBACK((+[](GtkButton*,gpointer p){(*static_cast<std::function<void()>*>(p))();})),fn,
        +[](gpointer p,GClosure*){delete static_cast<std::function<void()>*>(p);},GConnectFlags(0));
    return b;
}
}
DocumentTabs::DocumentTabs(GtkApplication* app,GladeSearchpath* resources,Control* first,MainWindow* window,bool disableAudio):
    app(app),resources(resources),disableAudio(disableAudio) {
    auto* css=gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,R"CSS(
    .document-tabs {padding:4px 8px;background:@theme_bg_color;border-bottom:1px solid alpha(@theme_fg_color,.12);}
    .document-tabs button {padding:6px 10px;min-height:24px;border-radius:7px 7px 0 0;box-shadow:none;}
    .document-tabs .active-tab {background:alpha(@theme_selected_bg_color,.25);border-color:alpha(@theme_selected_bg_color,.4);}
    )CSS",-1,nullptr);
    gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(window->getWindow()),GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION+3);
    g_object_unref(css);
    current=add(first,window);refresh();
    if(g_getenv("XOURNAL_WORKSPACE_INBOX"))inboxTimer=g_timeout_add(120,+[](gpointer data)->gboolean{
        auto* self=static_cast<DocumentTabs*>(data);
        self->updateHost();
        if(self->busy||self->quitting)return G_SOURCE_CONTINUE;
        auto* folder=g_getenv("XOURNAL_WORKSPACE_INBOX");
        auto* dir=g_dir_open(folder,0,nullptr);if(!dir)return G_SOURCE_CONTINUE;
        const char* name=nullptr;
        while((name=g_dir_read_name(dir))){
            if(g_str_has_suffix(name,".close")){
                gchar* full=g_build_filename(folder,name,nullptr);std::error_code ec;fs::remove(fs::path(std::u8string_view(reinterpret_cast<const char8_t*>(full))),ec);g_free(full);
                self->requestQuit();break;
            }
            if(g_getenv("XOURNAL_WORKSPACE_QA")&&g_str_has_suffix(name,".qa")){
                gchar* full=g_build_filename(folder,name,nullptr);gchar* operation=nullptr;
                if(g_file_get_contents(full,&operation,nullptr,nullptr)){
                    std::error_code ec;fs::remove(fs::path(std::u8string_view(reinterpret_cast<const char8_t*>(full))),ec);
                    if(g_strcmp0(operation,"next")==0)g_action_group_activate_action(G_ACTION_GROUP(self->current->window->getWindow()),"document-tab-next",nullptr);
                    if(g_strcmp0(operation,"close")==0)self->close(self->current);
                    g_free(operation);
                }
                g_free(full);break;
            }
            if(!g_str_has_suffix(name,".open"))continue;
            gchar* full=g_build_filename(folder,name,nullptr);gchar* text=nullptr;gsize size=0;
            if(g_file_get_contents(full,&text,&size,nullptr)){
                std::error_code ec;fs::remove(fs::path(std::u8string_view(reinterpret_cast<const char8_t*>(full))),ec);
                if(g_utf8_validate(text,size,nullptr)){
                    auto path=fs::path(std::u8string_view(reinterpret_cast<const char8_t*>(text),size));self->open(std::move(path));
                }
                g_free(text);
            }
            g_free(full);break;
        }
        g_dir_close(dir);return G_SOURCE_CONTINUE;
    },this);

}
DocumentTabs::~DocumentTabs() {
    if(refreshIdle)g_source_remove(refreshIdle);
    if(inboxTimer)g_source_remove(inboxTimer);
    for(auto& p:pages){p->control->setDocumentTabs(nullptr);g_signal_handlers_disconnect_by_data(p->window->getWindow(),this);}
}
DocumentTabs::Page* DocumentTabs::add(Control* ctrl,MainWindow* win) {
    auto p=std::make_unique<Page>();p->control=ctrl;p->window=win;p->number=pages.size()+1;
    p->content=gtk_bin_get_child(GTK_BIN(win->getWindow()));g_object_ref(p->content);
    gtk_container_remove(GTK_CONTAINER(win->getWindow()),p->content);
    p->root=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);gtk_container_add(GTK_CONTAINER(win->getWindow()),p->root);
    auto* scroll=gtk_scrolled_window_new(nullptr,nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),GTK_POLICY_AUTOMATIC,GTK_POLICY_NEVER);
    gtk_widget_set_size_request(scroll,-1,44);
    p->bar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,3);
    gtk_style_context_add_class(gtk_widget_get_style_context(p->bar),"document-tabs");
    gtk_container_add(GTK_CONTAINER(scroll),p->bar);gtk_box_pack_start(GTK_BOX(p->root),scroll,false,false,0);
    gtk_box_pack_start(GTK_BOX(p->root),p->content,true,true,0);g_object_unref(p->content);
    gtk_widget_show_all(scroll);gtk_widget_show(p->content);gtk_widget_show(p->root);
    auto* page=p.get();
    ctrl->setDocumentTabs(this);
    g_signal_connect(win->getWindow(),"notify::title",G_CALLBACK((+[](GObject*,GParamSpec*,gpointer self){static_cast<DocumentTabs*>(self)->scheduleRefresh();})),this);
    auto action=[&](const char* id,const char* key,auto callback){
        auto* a=g_simple_action_new(id,nullptr);auto* fn=new std::function<void()>(callback);
        g_signal_connect_data(a,"activate",G_CALLBACK((+[](GSimpleAction*,GVariant*,gpointer f){(*static_cast<std::function<void()>*>(f))();})),fn,
            +[](gpointer f,GClosure*){delete static_cast<std::function<void()>*>(f);},GConnectFlags(0));
        g_action_map_add_action(G_ACTION_MAP(win->getWindow()),G_ACTION(a));g_object_unref(a);
        auto detailed=std::string("win.")+id;const char* keys[]={key,nullptr};gtk_application_set_accels_for_action(app,detailed.c_str(),keys);
    };
    action("document-tab-close","<Control>w",[this,page]{close(page);});
    action("document-tab-next","<Control>Tab",[this]{
        auto it=std::find_if(pages.begin(),pages.end(),[this](auto& p){return p.get()==current;});
        for(size_t n=0;n<pages.size();++n){if(++it==pages.end())it=pages.begin();if(!(*it)->closed){select(it->get());break;}}
    });
    pages.push_back(std::move(p));return page;
}
void DocumentTabs::scheduleRefresh(){if(!refreshIdle)refreshIdle=g_idle_add(+[](gpointer d)->gboolean{auto* s=static_cast<DocumentTabs*>(d);s->refreshIdle=0;s->refresh();return G_SOURCE_REMOVE;},this);}
void DocumentTabs::refresh() {
    for(auto& host:pages)if(!host->closed||host.get()==current) {
        auto* children=gtk_container_get_children(GTK_CONTAINER(host->bar));for(auto* l=children;l;l=l->next)gtk_widget_destroy(GTK_WIDGET(l->data));g_list_free(children);
        auto put=[&](GtkWidget* b,bool active){gtk_box_pack_start(GTK_BOX(host->bar),b,false,false,0);if(active)gtk_style_context_add_class(gtk_widget_get_style_context(b),"active-tab");};

        for(auto& p:pages)if(!p->closed){
            auto* page=p.get();auto* box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
            std::string title=gtk_window_get_title(GTK_WINDOW(p->window->getWindow()));
            auto suffix=title.rfind(" - Xournal++");if(suffix!=std::string::npos)title.resize(suffix);
            if(p->control->getDocument()->getEvMetadataFilename().empty())title="Sem título "+std::to_string(p->number)+(title.find('*')!=std::string::npos?" *":"");
            auto* b=textButton(title.c_str(),[this,page]{select(page);});
            auto* label=GTK_LABEL(gtk_bin_get_child(GTK_BIN(b)));gtk_label_set_ellipsize(label,PANGO_ELLIPSIZE_MIDDLE);gtk_label_set_max_width_chars(label,24);
            gtk_widget_set_tooltip_text(b,title.c_str());gtk_box_pack_start(GTK_BOX(box),b,false,false,0);
            gtk_box_pack_start(GTK_BOX(box),textButton("×",[this,page]{close(page);}),false,false,0);
            put(box,false);if(host.get()==current&&page==current)gtk_style_context_add_class(gtk_widget_get_style_context(b),"active-tab");
        }
        put(textButton("+",[this]{open();}),false);gtk_widget_show_all(host->bar);
    }
}
void DocumentTabs::select(Page* page) {
    if(busy||quitting)return;
    if(current!=page){
        int x,y,w,h;auto* old=GTK_WINDOW(current->window->getWindow());auto* next=GTK_WINDOW(page->window->getWindow());
        gtk_window_get_position(old,&x,&y);gtk_window_get_size(old,&w,&h);
        bool maximized=gtk_window_is_maximized(old);
        if(!g_getenv("XOURNAL_WORKSPACE_HOST")) {
            gtk_window_move(next,x,y);gtk_window_resize(next,w,h);
            if(maximized)gtk_window_maximize(next);else gtk_window_unmaximize(next);
        }
        gtk_widget_hide(GTK_WIDGET(old));current=page;
    }
    gtk_widget_show(page->content);
    XojMsgBox::setDefaultWindow(GTK_WINDOW(page->window->getWindow()));
    setEmergencyDocument(page->control->getDocument());
    if(g_getenv("XOURNAL_WORKSPACE_HOST")) {
        // A hidden GtkWindow can restore its pre-host placement when remapped.
        // Supply the host rectangle before mapping it, including on tab return.
        gtk_widget_realize(page->window->getWindow());
        page->hostX=page->hostY=-1;
        updateHost();
        if(gtk_widget_get_visible(page->window->getWindow()))gtk_window_present(GTK_WINDOW(page->window->getWindow()));
    } else gtk_window_present(GTK_WINDOW(page->window->getWindow()));
    scheduleRefresh();
}
void DocumentTabs::open(fs::path path,std::function<void(bool)> cb,int page,bool attach) {
    if(busy||quitting){cb(false);return;}
    if(!path.empty())for(auto& p:pages)if(!p->closed){
        auto existing=p->control->getDocument()->getEvMetadataFilename();std::error_code ec;
        if(!existing.empty()&&fs::equivalent(path,existing,ec)&&!ec){select(p.get());cb(true);return;}
    }
    busy=true;
    auto ctrl=std::make_unique<Control>(G_APPLICATION(app),resources,disableAudio);
    g_debug("DocumentTabs: controller created");
    auto win=std::make_unique<MainWindow>(resources,ctrl.get(),app);
    g_debug("DocumentTabs: window created");
    ctrl->initWindow(win.get());win->populate(resources);
    auto* p=add(ctrl.get(),win.get());p->ownedControl=std::move(ctrl);p->ownedWindow=std::move(win);
    p->control->openFileWithoutSavingTheCurrentDocument(std::move(path),attach,page,[this,p,cb=std::move(cb)](bool ok){
        p->control->getScheduler()->start();busy=false;
        if(ok)select(p);else{p->closed=true;gtk_widget_hide(p->window->getWindow());setEmergencyDocument(current->control->getDocument());scheduleRefresh();}
        cb(ok);
    });
}
void DocumentTabs::close(Page* page) {
    if(busy||quitting||page->closed)return;
    select(page);busy=true;
    page->control->close([this,page](bool ok){
        busy=false;if(!ok)return;
        page->control->enableAutosave(false);page->control->getScheduler()->stop();
        page->control->close([](bool){},true,true,true);page->closed=true;
        auto other=std::find_if(pages.begin(),pages.end(),[](auto& p){return !p->closed;});
        if(other!=pages.end())select(other->get());
        else { shutdown();g_application_quit(G_APPLICATION(app)); }
        scheduleRefresh();
    });
}
void DocumentTabs::requestQuit(bool allowCancel){if(busy||quitting)return;quitting=true;confirmQuit(0,allowCancel);}
void DocumentTabs::confirmQuit(size_t index,bool allowCancel){
    while(index<pages.size()&&pages[index]->closed)++index;
    if(index==pages.size()){shutdown();g_application_quit(G_APPLICATION(app));return;}
    auto* p=pages[index].get();quitting=false;select(p);quitting=true;
    p->control->close([this,index,allowCancel](bool ok){if(ok)confirmQuit(index+1,allowCancel);else quitting=false;},false,allowCancel);
}
void DocumentTabs::shutdown(){for(auto& p:pages){p->control->enableAutosave(false);p->control->getScheduler()->stop();}if(current)current->control->saveSettings();}

void DocumentTabs::updateHost() {
#ifdef _WIN32
    auto* hostText=g_getenv("XOURNAL_WORKSPACE_HOST");auto* folder=g_getenv("XOURNAL_WORKSPACE_INBOX");
    auto* ownerText=g_getenv("XOURNAL_WORKSPACE_OWNER");if(!hostText||!folder||!ownerText)return;
    auto parent=reinterpret_cast<HWND>(static_cast<uintptr_t>(g_ascii_strtoull(hostText,nullptr,10)));
    DWORD owner=0;GetWindowThreadProcessId(parent,&owner);
    if(!IsWindow(parent)||owner!=g_ascii_strtoull(ownerText,nullptr,10))return;
    auto* config=g_key_file_new();auto* file=g_build_filename(folder,"layout.ini",nullptr);
    bool loaded=g_key_file_load_from_file(config,file,G_KEY_FILE_NONE,nullptr);g_free(file);
    if(!loaded){g_key_file_unref(config);return;}
    auto value=[&](const char* key){return g_key_file_get_double(config,"layout",key,nullptr);};
    double vw=value("viewportWidth"),vh=value("viewportHeight");
    if(vw<=0||vh<=0){g_key_file_unref(config);return;}
    RECT rect;GetClientRect(parent,&rect);double sx=rect.right/vw,sy=rect.bottom/vh;
    int x=static_cast<int>(value("x")*sx),y=static_cast<int>(value("y")*sy);
    int width=std::max(1,static_cast<int>(value("width")*sx)),height=std::max(1,static_cast<int>(value("height")*sy));
    bool visible=g_key_file_get_boolean(config,"layout","visible",nullptr);g_key_file_unref(config);
    auto* state=g_key_file_new();int count=0;
    for(auto& p:pages)if(!p->closed&&gtk_widget_get_realized(p->window->getWindow())){
        auto hwnd=static_cast<HWND>(gdk_win32_window_get_handle(gtk_widget_get_window(p->window->getWindow())));
        if(!p->hosted){
            // Keep GTK's top-level drawing/input context. The Electron window
            // owns this borderless editor surface; no cross-process reparenting.
            gtk_window_set_decorated(GTK_WINDOW(p->window->getWindow()),false);
            gtk_window_set_skip_taskbar_hint(GTK_WINDOW(p->window->getWindow()),true);
            gtk_window_set_gravity(GTK_WINDOW(p->window->getWindow()),GDK_GRAVITY_STATIC);
            SetWindowLongPtr(hwnd,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(parent));
            p->hosted=GetWindow(hwnd,GW_OWNER)==parent;
            if(!p->hosted)continue;
        }
        POINT origin{0,0};ClientToScreen(parent,&origin);
        bool show=visible&&p.get()==current&&IsWindowVisible(parent)&&!IsIconic(parent);
        bool mapping=show&&!gtk_widget_get_visible(p->window->getWindow());
        if(mapping)p->hostX=p->hostY=-1;
        int screenX=origin.x+x,screenY=origin.y+y;
        RECT actual;GetWindowRect(hwnd,&actual);
        bool drift=show&&(std::abs(actual.left-screenX)>2||std::abs(actual.top-screenY)>2)&&
                   g_get_monotonic_time()-p->lastGeometryUpdate>250000;
        if(p.get()==current&&(screenX!=p->hostX||screenY!=p->hostY||width!=p->hostWidth||height!=p->hostHeight||drift)){
            int scale=std::max(1,gtk_widget_get_scale_factor(p->window->getWindow()));
            // GTK owns allocation and the backing surface. Raw SetWindowPos
            // races its queued paints and can leave copied strips on Windows.
            gtk_window_move(GTK_WINDOW(p->window->getWindow()),screenX/scale,screenY/scale);
            gtk_window_resize(GTK_WINDOW(p->window->getWindow()),std::max(1,width/scale),std::max(1,height/scale));
            gtk_widget_queue_draw(p->window->getWindow());
            p->hostX=screenX;p->hostY=screenY;p->hostWidth=width;p->hostHeight=height;++p->geometryUpdates;p->lastGeometryUpdate=g_get_monotonic_time();
        }
        gtk_widget_set_visible(p->window->getWindow(),show);
        auto group=std::string("window")+std::to_string(count++);
        g_key_file_set_boolean(state,group.c_str(),"visible",IsWindowVisible(hwnd));
        g_key_file_set_boolean(state,group.c_str(),"attached",GetWindow(hwnd,GW_OWNER)==parent);
        g_key_file_set_integer(state,group.c_str(),"requestedWidth",width);
        g_key_file_set_integer(state,group.c_str(),"requestedHeight",height);
        g_key_file_set_integer(state,group.c_str(),"geometryUpdates",p->geometryUpdates);
        g_key_file_set_boolean(state,group.c_str(),"layered",(GetWindowLongPtr(hwnd,GWL_EXSTYLE)&WS_EX_LAYERED)!=0);
        RECT frame;GetWindowRect(hwnd,&frame);
        g_key_file_set_integer(state,group.c_str(),"x",frame.left);
        g_key_file_set_integer(state,group.c_str(),"y",frame.top);
        g_key_file_set_integer(state,group.c_str(),"requestedX",screenX);
        g_key_file_set_integer(state,group.c_str(),"requestedY",screenY);
        g_key_file_set_integer(state,group.c_str(),"tab",static_cast<int>(p->number));
        RECT childRect;GetClientRect(hwnd,&childRect);
        g_key_file_set_integer(state,group.c_str(),"width",childRect.right);g_key_file_set_integer(state,group.c_str(),"height",childRect.bottom);
    }
    g_key_file_set_integer(state,"workspace","count",count);
    auto* text=g_key_file_to_data(state,nullptr,nullptr);g_key_file_unref(state);
    if(hostState!=text){hostState=text;file=g_build_filename(folder,"state.ini",nullptr);g_file_set_contents(file,text,-1,nullptr);g_free(file);}g_free(text);
#endif
}
