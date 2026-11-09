#include "RadialMenu.h"
#include "RadialMenuGeometry.h"
#include "WindowUiActions.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include "MainWindow.h"
#include "IconNameHelper.h"
#include "gui/toolbarMenubar/icon/ColorIcon.h"
#include "control/Control.h"
#include "control/Tool.h"
#include "control/ToolHandler.h"
#include "control/actions/ActionDatabase.h"
#include "gui/toolbarMenubar/model/ColorPalette.h"
#include "util/PathUtil.h"
#include "util/NamedColor.h"
#include "util/i18n.h"

namespace {
constexpr double PI=3.141592653589793;
struct ToolEntry {ToolType type;const char* label;const char* icon;};
const ToolEntry entries[]={
 {TOOL_PEN,N_("Pen"),"tool-pencil"},{TOOL_ERASER,N_("Eraser"),"tool-eraser"},
 {TOOL_HIGHLIGHTER,N_("Highlighter"),"tool-highlighter"},{TOOL_TEXT,N_("Text"),"tool-text"},
 {TOOL_SELECT_RECT,N_("Select Rectangle"),"select-rect"},{TOOL_HAND,N_("Hand"),"hand"},
 {TOOL_IMAGE,N_("Image"),"tool-image"},{TOOL_VERTICAL_SPACE,N_("Vertical Space"),"spacer"}};
const ToolEntry& entryFor(ToolType type) {
 for (const auto& entry:entries) if(entry.type==type) return entry;
 static const ToolEntry other[]={
  {TOOL_SELECT_REGION,N_("Select Region"),"select-lasso"},
  {TOOL_SELECT_MULTILAYER_RECT,N_("Select Multi-Layer Rectangle"),"select-multilayer-rect"},
  {TOOL_SELECT_MULTILAYER_REGION,N_("Select Multi-Layer Region"),"select-multilayer-lasso"},
  {TOOL_SELECT_OBJECT,N_("Select Object"),"object-select"},
  {TOOL_SELECT_PDF_TEXT_LINEAR,N_("Select Linear PDF Text"),"select-pdf-text-ht"},
  {TOOL_SELECT_PDF_TEXT_RECT,N_("Select PDF Text in Rectangle"),"select-pdf-text-area"},
  {TOOL_PLAY_OBJECT,N_("Play Object"),"object-play"},
  {TOOL_LASER_POINTER_PEN,N_("Laser Pointer"),"laser-pointer"},
  {TOOL_LASER_POINTER_HIGHLIGHTER,N_("Laser Pointer"),"laser-pointer"}};
 for(const auto& entry:other)if(entry.type==type)return entry;
 return entries[0];
}
size_t primaryIndex(ToolType type) {
 for(size_t i=0;i<std::size(entries);++i)if(entries[i].type==type)return i;
 return isSelectToolType(type)?4:0;
}
void accessible(GtkWidget* widget,const char* text) {
 gtk_widget_set_has_tooltip(widget,false);atk_object_set_name(gtk_widget_get_accessible(widget),text);
}
void syncSwatch(GObject* action,GParamSpec*,gpointer data) {
 auto* button=GTK_WIDGET(data);
 auto* state=g_action_get_state(G_ACTION(action));
 if(!state)return;
 bool selected=g_variant_get_uint32(state)==GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button),"radial-swatch-color"));
 g_variant_unref(state);
 if(selected)gtk_widget_set_state_flags(button,GTK_STATE_FLAG_CHECKED,false);
 else gtk_widget_unset_state_flags(button,GTK_STATE_FLAG_CHECKED);
}
}

RadialMenu::RadialMenu(MainWindow* window,GtkOverlay* overlay):window(window),overlay(overlay) {
 auto path=Util::getConfigFile("radial-menu.ini").u8string();configPath.assign(path.begin(),path.end());
 auto* config=g_key_file_new();
 if(g_key_file_load_from_file(config,configPath.c_str(),G_KEY_FILE_NONE,nullptr)) {
  enabled=g_key_file_get_boolean(config,"menu","enabled",nullptr);
  x=g_key_file_get_double(config,"menu","x",nullptr);y=g_key_file_get_double(config,"menu","y",nullptr);
  if(!std::isfinite(x)||!std::isfinite(y)) {x=-1;y=-1;}
 }
 g_key_file_unref(config);
 auto* css=gtk_css_provider_new();
 gtk_css_provider_load_from_data(css,R"CSS(
 #radial-surface {background:transparent;}
 .radial-item {border-radius:999px;padding:0;min-width:0;min-height:0;background:@theme_bg_color;
   border:1px solid alpha(@theme_fg_color,.16);box-shadow:0 3px 9px alpha(black,.20);
   transition:background-color 140ms ease-out,border-color 140ms ease-out;}
 .radial-item:hover {background:mix(@theme_bg_color,@theme_selected_bg_color,.18);border-color:@theme_selected_bg_color;}
 .radial-item:checked, .radial-item.radial-group-active {background:mix(@theme_bg_color,@theme_selected_bg_color,.25);border:2px solid @theme_selected_bg_color;}
 .radial-item label {font-size:9px;}
 #radial-hub {border-radius:999px;background:@theme_selected_bg_color;border:0;box-shadow:none;}
 #radial-hub image {background:transparent;box-shadow:none;-gtk-icon-shadow:none;}
 .radial-panel {padding:12px;}
 .radial-panel button {min-height:36px;}
 )CSS",-1,nullptr);
 // Scoped classes make this provider harmless to the original toolbar and document.
 gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(window->getWindow()),GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION+1);
 g_object_set_data_full(G_OBJECT(overlay),"radial-css",css,g_object_unref);
 catcher=gtk_event_box_new();gtk_event_box_set_visible_window(GTK_EVENT_BOX(catcher),false);
 gtk_widget_set_hexpand(catcher,true);gtk_widget_set_vexpand(catcher,true);
 gtk_widget_set_no_show_all(catcher,true);
 gtk_widget_add_events(catcher,GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_TOUCH_MASK|GDK_POINTER_MOTION_MASK);
 gtk_overlay_add_overlay(overlay,catcher);
 g_signal_connect(catcher,"event",G_CALLBACK((+[](GtkWidget*,GdkEvent* e,gpointer data)->gboolean {
  auto* self=static_cast<RadialMenu*>(data);
  // Consume the complete closing sequence, including release; never pass the tap into the canvas.
  if(e->type==GDK_BUTTON_RELEASE||e->type==GDK_TOUCH_END) self->setOpen(false);
  return e->type==GDK_BUTTON_PRESS||e->type==GDK_BUTTON_RELEASE||e->type==GDK_TOUCH_BEGIN||
         e->type==GDK_TOUCH_END||e->type==GDK_TOUCH_UPDATE||e->type==GDK_MOTION_NOTIFY;
 })),this);
 fixed=gtk_fixed_new();gtk_widget_set_name(fixed,"radial-surface");gtk_widget_set_no_show_all(fixed,true);
 gtk_widget_set_halign(fixed,GTK_ALIGN_START);gtk_widget_set_valign(fixed,GTK_ALIGN_START);
 gtk_overlay_add_overlay(overlay,fixed);
 hub=gtk_event_box_new();gtk_widget_set_name(hub,"radial-hub");
 // Paint on the parent surface: a visible EventBox owns a rectangular GDK
 // background that can show pale corners behind the circular button on Windows.
 gtk_event_box_set_visible_window(GTK_EVENT_BOX(hub),false);
 g_signal_connect(hub,"draw",G_CALLBACK((+[](GtkWidget* widget,cairo_t* cr,gpointer)->gboolean {
  auto* context=gtk_widget_get_style_context(widget);
  int width=gtk_widget_get_allocated_width(widget),height=gtk_widget_get_allocated_height(widget);
  gtk_render_background(context,cr,0,0,width,height);gtk_render_frame(context,cr,0,0,width,height);
  return false;
 })),nullptr);
 gtk_widget_set_can_focus(hub,true);gtk_widget_set_size_request(hub,60,60);
 gtk_widget_add_events(hub,GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_POINTER_MOTION_MASK|GDK_TOUCH_MASK);
 hubImage=gtk_image_new();gtk_container_add(GTK_CONTAINER(hub),hubImage);gtk_widget_show(hubImage);
 g_signal_connect(hub,"notify::scale-factor",G_CALLBACK((+[](GObject*,GParamSpec*,gpointer data) {
  static_cast<RadialMenu*>(data)->updateHubIcon();
 })),this);
 gtk_fixed_put(GTK_FIXED(fixed),hub,2,2);gtk_widget_show(hub);
 accessible(hub,"Menu radial: toque para abrir; arraste para mover");
 atk_object_set_role(gtk_widget_get_accessible(hub),ATK_ROLE_PUSH_BUTTON);
 g_signal_connect(hub,"event",G_CALLBACK((+[](GtkWidget*,GdkEvent* event,gpointer data)->gboolean {
  return static_cast<RadialMenu*>(data)->hubEvent(event);
 })),this);
 g_signal_connect(hub,"key-press-event",G_CALLBACK((+[](GtkWidget*,GdkEventKey* e,gpointer data)->gboolean {
  auto* self=static_cast<RadialMenu*>(data);
  if(!(e->state&(GDK_CONTROL_MASK|GDK_MOD1_MASK|GDK_SUPER_MASK)) && (e->keyval==GDK_KEY_space||e->keyval==GDK_KEY_Return)) {self->setOpen(!self->open);return true;}return false;
 })),this);
 // Compact windows still need access to the remaining tools; this is a more-tools
 // control, not a back button. Normal menus have no additional navigation button.
 pager=gtk_button_new_from_icon_name("view-more-symbolic",GTK_ICON_SIZE_BUTTON);
 gtk_button_set_always_show_image(GTK_BUTTON(pager),true);
 gtk_style_context_add_class(gtk_widget_get_style_context(pager),"radial-item");
 accessible(pager,"Mais ferramentas");gtk_widget_set_size_request(pager,40,40);gtk_fixed_put(GTK_FIXED(fixed),pager,0,0);
 g_signal_connect(pager,"clicked",G_CALLBACK((+[](GtkButton*,gpointer data) {
  auto* self=static_cast<RadialMenu*>(data);self->closePopover();self->toolPage=(self->toolPage+1)%2;self->animate();self->layout();
 })),this);
 for(size_t i=0;i<std::size(entries);++i) {
  const auto& entry=entries[i];
  auto* widget=button(tools,_(entry.label),entry.icon,-PI/2+2*PI*i/std::size(entries),[this,type=entry.type]() {
   bool collapse=settingsOpen&&settingsTool==type;
   closePopover();settingsOpen=!collapse;settingsTool=type;
   if(settingsOpen)rebuildOptions();
   animate();
  },false);
  bind(widget,Action::SELECT_TOOL,g_variant_new_uint64(entry.type));
 }
 visibility=g_simple_action_new_stateful("radial-menu",xoj::ui::windowActionParameterType("radial-menu").value(),g_variant_new_boolean(enabled));
 g_signal_connect(visibility,"change-state",G_CALLBACK((+[](GSimpleAction*,GVariant* v,gpointer data) {
  static_cast<RadialMenu*>(data)->setEnabled(g_variant_get_boolean(v));
 })),this);
 g_signal_connect(visibility,"activate",G_CALLBACK((+[](GSimpleAction*,GVariant*,gpointer data) {
  auto* self=static_cast<RadialMenu*>(data);self->setEnabled(!self->enabled);
 })),this);
 g_action_map_add_action(G_ACTION_MAP(window->getWindow()),G_ACTION(visibility));
 const char* keys[]={"<Control><Alt>r",nullptr};
 gtk_application_set_accels_for_action(gtk_window_get_application(GTK_WINDOW(window->getWindow())),"win.radial-menu",keys);
 summon=g_simple_action_new("radial-menu-at-pointer",xoj::ui::windowActionParameterType("radial-menu-at-pointer").value());
 g_signal_connect(summon,"activate",G_CALLBACK((+[](GSimpleAction*,GVariant*,gpointer data) {
  static_cast<RadialMenu*>(data)->openAtPointer();
 })),this);
 g_action_map_add_action(G_ACTION_MAP(window->getWindow()),G_ACTION(summon));
 const char* summonKeys[]={"<Control>space",nullptr};
 gtk_application_set_accels_for_action(gtk_window_get_application(GTK_WINDOW(window->getWindow())),"win.radial-menu-at-pointer",summonKeys);
 auto* db=window->getControl()->getActionDatabase();
 for(auto action:{Action::SELECT_TOOL,Action::TOOL_SIZE,Action::TOOL_COLOR,Action::TOOL_PEN_LINE_STYLE,
      Action::TOOL_ERASER_TYPE,Action::TOOL_PEN_FILL,Action::TOOL_HIGHLIGHTER_FILL,Action::FONT,
      Action::TOOL_DRAW_LINE,Action::TOOL_DRAW_SHAPE_RECOGNIZER,Action::TOOL_DRAW_RECTANGLE,
      Action::TOOL_DRAW_ELLIPSE,Action::TOOL_DRAW_ARROW,Action::TOOL_DRAW_DOUBLE_ARROW,
      Action::TOOL_DRAW_COORDINATE_SYSTEM,Action::TOOL_DRAW_SPLINE,Action::TRACOS_TEXT}) {
  auto* object=G_OBJECT(db->getAction(action).get());g_object_ref(object);watched.push_back(object);
  g_signal_connect(object,"notify",G_CALLBACK((+[](GObject*,GParamSpec*,gpointer data) {
   static_cast<RadialMenu*>(data)->scheduleRefresh();
  })),this);
 }
 g_signal_connect(overlay,"get-child-position",G_CALLBACK((+[](GtkOverlay* overlay,GtkWidget* child,GdkRectangle* rect,gpointer data)->gboolean {
  auto* self=static_cast<RadialMenu*>(data);
  if(child!=self->fixed)return false;
  int width=gtk_widget_get_allocated_width(GTK_WIDGET(overlay)),height=gtk_widget_get_allocated_height(GTK_WIDGET(overlay));
  if(width<128||height<128) {rect->x=0;rect->y=0;rect->width=64;rect->height=64;return true;}
  if(self->x<0) {self->x=width-66;self->y=height*.65;}
  self->x=std::clamp(self->x,34.0,std::max(34.0,width-34.0));self->y=std::clamp(self->y,34.0,std::max(34.0,height-34.0));
  self->diameter=std::max(220.0,std::min({440.0,width-16.0,height-16.0}));
  double p=xoj::radial::smooth(self->progress);
  double cx=self->x+(xoj::radial::clampCenter(self->x,width,self->diameter)-self->x)*p;
  double cy=self->y+(xoj::radial::clampCenter(self->y,height,self->diameter)-self->y)*p;
  int size=self->expanded?static_cast<int>(self->diameter):64;
  rect->x=static_cast<int>(cx-size/2);rect->y=static_cast<int>(cy-size/2);rect->width=size;rect->height=size;
  return true;
 })),this);
 g_signal_connect(overlay,"size-allocate",G_CALLBACK((+[](GtkWidget*,GtkAllocation*,gpointer data) {
  static_cast<RadialMenu*>(data)->layout();
 })),this);
 g_signal_connect(window->getWindow(),"key-press-event",G_CALLBACK((+[](GtkWidget*,GdkEventKey* e,gpointer data)->gboolean {
  auto* self=static_cast<RadialMenu*>(data);
  if(e->keyval!=GDK_KEY_Escape||!self->open)return false;
  if(self->popover)self->closePopover();
  else if(self->settingsOpen) {self->settingsOpen=false;self->animate();}
  else self->setOpen(false);
  return true;
 })),this);
 refresh();layout();gtk_widget_set_visible(fixed,enabled);
}

RadialMenu::~RadialMenu() {
 save();if(refreshIdle)g_source_remove(refreshIdle);if(tick)gtk_widget_remove_tick_callback(fixed,tick);
 closePopover();
 for(auto* object:watched){g_signal_handlers_disconnect_by_data(object,this);g_object_unref(object);}
 g_signal_handlers_disconnect_by_data(overlay,this);g_signal_handlers_disconnect_by_data(window->getWindow(),this);
 g_action_map_remove_action(G_ACTION_MAP(window->getWindow()),"radial-menu");g_object_unref(visibility);
 g_action_map_remove_action(G_ACTION_MAP(window->getWindow()),"radial-menu-at-pointer");g_object_unref(summon);
 gtk_widget_destroy(fixed);gtk_widget_destroy(catcher);
}

GtkWidget* RadialMenu::button(std::vector<std::unique_ptr<Item>>& items,const char* label,const char* icon,double angle,std::function<void()> click,bool toggle) {
 auto item=std::make_unique<Item>();item->menu=this;item->widget=toggle?gtk_toggle_button_new():gtk_button_new();item->angle=angle;item->click=std::move(click);
 auto* widget=item->widget;gtk_style_context_add_class(gtk_widget_get_style_context(widget),"radial-item");
 auto* box=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
 auto name=IconNameHelper(window->getControl()->getSettings()).iconName(icon);
 if(!gtk_icon_theme_has_icon(gtk_icon_theme_get_default(),name.c_str())) name=icon;
 auto* image=std::string(icon)=="color-select-symbolic" ?
  ColorIcon::newGtkImage(window->getControl()->getToolHandler()->getActiveTool()->getColor(),22) :
  gtk_image_new_from_icon_name(name.c_str(),GTK_ICON_SIZE_LARGE_TOOLBAR);
 if(std::string(icon)=="color-select-symbolic")g_object_set_data(G_OBJECT(widget),"radial-color-image",image);
 gtk_box_pack_start(GTK_BOX(box),image,true,false,0);
 gtk_container_add(GTK_CONTAINER(widget),box);gtk_widget_show_all(box);accessible(widget,label);
 gtk_widget_set_size_request(widget,58,58);gtk_fixed_put(GTK_FIXED(fixed),widget,0,0);
 g_signal_connect(widget,"clicked",G_CALLBACK((+[](GtkButton*,gpointer data) {auto* item=static_cast<Item*>(data);item->click();})),item.get());
 g_signal_connect(widget,"enter-notify-event",G_CALLBACK((+[](GtkWidget*,GdkEventCrossing*,gpointer data)->gboolean {
  auto* item=static_cast<Item*>(data);item->hovered=true;
  item->menu->animate();return false;
 })),item.get());
 g_signal_connect(widget,"leave-notify-event",G_CALLBACK((+[](GtkWidget*,GdkEventCrossing*,gpointer data)->gboolean {
  auto* item=static_cast<Item*>(data);item->hovered=false;item->menu->refresh();item->menu->animate();return false;
 })),item.get());
 g_signal_connect(widget,"focus-in-event",G_CALLBACK((+[](GtkWidget*,GdkEventFocus*,gpointer data)->gboolean {
  auto* item=static_cast<Item*>(data);item->focused=true;item->menu->animate();return false;
 })),item.get());
 g_signal_connect(widget,"focus-out-event",G_CALLBACK((+[](GtkWidget*,GdkEventFocus*,gpointer data)->gboolean {
  auto* item=static_cast<Item*>(data);item->focused=false;item->menu->animate();return false;
 })),item.get());
 items.push_back(std::move(item));return widget;
}
void RadialMenu::bind(GtkWidget* widget,Action action,GVariant* target) {
 std::string name=std::string("win.")+Action_toString(action);
 if(target)gtk_actionable_set_action_target_value(GTK_ACTIONABLE(widget),target);
 gtk_actionable_set_action_name(GTK_ACTIONABLE(widget),name.c_str());
}
void RadialMenu::activate(Action action,GVariant* target) {
 auto* a=G_ACTION(window->getControl()->getActionDatabase()->getAction(action).get());
 if(!g_action_get_enabled(a))return;
 if(target) {g_variant_ref_sink(target);g_action_activate(a,target);g_variant_unref(target);}
 else g_action_activate(a,nullptr);
 scheduleRefresh();
}
void RadialMenu::scheduleRefresh() {
 if(refreshIdle)return;
 refreshIdle=g_idle_add(+[](gpointer data)->gboolean {auto* self=static_cast<RadialMenu*>(data);self->refreshIdle=0;self->refresh();return G_SOURCE_REMOVE;},this);
}
void RadialMenu::updateHubIcon() {
 const auto& entry=entryFor(active);
 auto icon=IconNameHelper(window->getControl()->getSettings()).iconName(entry.icon);
 auto* theme=gtk_icon_theme_get_default();
 if(!gtk_icon_theme_has_icon(theme,icon.c_str()))icon=entry.icon;
 if(expanded) {
  gtk_image_set_from_icon_name(GTK_IMAGE(hubImage),icon.c_str(),GTK_ICON_SIZE_LARGE_TOOLBAR);
  return;
 }
 // Keep the selected tool's silhouette and alpha; recolor only the closed hub.
 // A surface with device scale preserves a 24px logical icon on HiDPI screens.
 const int scale=std::max(1,gtk_widget_get_scale_factor(hub));
 auto* source=gtk_icon_theme_load_icon_for_scale(theme,icon.c_str(),24,scale,GTK_ICON_LOOKUP_FORCE_SIZE,nullptr);
 if(!source) {
  gtk_image_set_from_icon_name(GTK_IMAGE(hubImage),icon.c_str(),GTK_ICON_SIZE_LARGE_TOOLBAR);
  return;
 }
 auto* white=gdk_pixbuf_copy(source);g_object_unref(source);
 if(!white)return;
 auto* pixels=gdk_pixbuf_get_pixels(white);
 const int stride=gdk_pixbuf_get_rowstride(white),channels=gdk_pixbuf_get_n_channels(white);
 for(int y=0;y<gdk_pixbuf_get_height(white);++y)for(int x=0;x<gdk_pixbuf_get_width(white);++x) {
  auto* pixel=pixels+y*stride+x*channels;pixel[0]=255;pixel[1]=255;pixel[2]=255;
 }
 auto* surface=gdk_cairo_surface_create_from_pixbuf(white,scale,nullptr);
 gtk_image_set_from_surface(GTK_IMAGE(hubImage),surface);
 cairo_surface_destroy(surface);g_object_unref(white);
}
void RadialMenu::refresh() {
 auto* tool=window->getControl()->getToolHandler()->getActiveTool();
 if(active!=tool->getToolType()) {active=tool->getToolType();settingsTool=entries[primaryIndex(active)].type;closePopover();if(settingsOpen)rebuildOptions();}
 for(auto& item:options)if(auto* image=static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(item->widget),"radial-color-image"))) {
  auto pixbuf=ColorIcon::newGdkPixbuf(tool->getColor(),22);gtk_image_set_from_pixbuf(GTK_IMAGE(image),pixbuf.get());
 }
 const auto& entry=entryFor(active);
 for(size_t i=0;i<tools.size();++i) {
  bool selected=i==primaryIndex(active);
  if(selected)gtk_widget_set_state_flags(tools[i]->widget,GTK_STATE_FLAG_CHECKED,false);
  else gtk_widget_unset_state_flags(tools[i]->widget,GTK_STATE_FLAG_CHECKED);
 }
 updateHubIcon();
 std::ostringstream text;text<<_(entry.label);
 const char* sizeNames[]={N_("Very Fine"),N_("Fine"),N_("Medium"),N_("Thick"),N_("Very Thick")};
 if(tool->hasCapability(TOOL_CAP_SIZE))text<<"  ·  "<<_(sizeNames[std::clamp(int(tool->getSize()),0,4)]);
 if(tool->hasCapability(TOOL_CAP_COLOR)) {
  auto c=tool->getColor();char hex[12];g_snprintf(hex,sizeof(hex),"#%02X%02X%02X",c.red,c.green,c.blue);text<<"\n"<<hex;
 }
 if(active==TOOL_PEN||active==TOOL_HIGHLIGHTER) {
  const char* modes[]={"","Desenho livre",N_("Draw Line"),N_("Draw Rectangle"),N_("Draw Ellipse"),
    N_("Draw Arrow"),N_("Draw Double Arrow"),N_("Draw Coordinate System"),N_("Shape Recognizer"),N_("Draw Spline")};
  text<<" · "<<_(modes[std::clamp(int(tool->getDrawingType()),0,9)]);
 }
 if(popover)if(auto* free=static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(popover),"radial-freehand"))) {
  if(tool->getDrawingType()==DRAWING_TYPE_DEFAULT)gtk_widget_set_state_flags(free,GTK_STATE_FLAG_CHECKED,false);
  else gtk_widget_unset_state_flags(free,GTK_STATE_FLAG_CHECKED);
 }
 accessible(hub,(std::string("Menu radial · ")+text.str()+" · toque para abrir/recolher; arraste para mover").c_str());
}
void RadialMenu::rebuildOptions() {
 closePopover();for(auto& item:options)gtk_widget_destroy(item->widget);options.clear();
 auto* tool=window->getControl()->getToolHandler()->getActiveTool();
 auto add=[&](const char* label,const char* icon,auto callback,bool toggle=false) {
  auto* widget=button(options,label,icon,0,[]{},toggle);
  options.back()->click=[widget,callback]() {callback(widget);};
  return widget;
 };
 if(tool->hasCapability(TOOL_CAP_SIZE))add("Espessura","thickness-medium",[this](auto* w){sizes(w);});
 if(tool->hasCapability(TOOL_CAP_COLOR))add("Cor","color-select-symbolic",[this](auto* w){colors(w);});
 if(tool->hasCapability(TOOL_CAP_LINE_STYLE))add("Traço","line-style-dash",[this](auto* w){styles(w);});
 if(tool->hasCapability(TOOL_CAP_RULER)||tool->hasCapability(TOOL_CAP_RECOGNIZER))add("Desenho","draw-line",[this](auto* w){modes(w);});
 if(active==TOOL_ERASER)add("Modo","tool-eraser",[this](auto* w){erasers(w);});
 if(tool->hasCapability(TOOL_CAP_FILL)) {
  auto a=active==TOOL_HIGHLIGHTER?Action::TOOL_HIGHLIGHTER_FILL:Action::TOOL_PEN_FILL;
  auto* widget=add("Preencher","fill",[](auto*){},true);options.back()->click=[]{};bind(widget,a);
  add("Opacidade","fill-opacity",[this](auto*) {closePopover();activate(active==TOOL_HIGHLIGHTER?Action::TOOL_HIGHLIGHTER_FILL_OPACITY:Action::TOOL_PEN_FILL_OPACITY);});
 }
 if(active==TOOL_TEXT)add("Fonte","tool-text",[this](auto*) {activate(Action::SELECT_FONT);});
 if(isSelectToolType(active))add("Seleção","select-lasso",[this](auto* w){selections(w);});
 for(size_t i=0;i<options.size();++i) {options[i]->angle=-PI/2+2*PI*i/options.size();gtk_widget_set_visible(options[i]->widget,open);}
 settingsProgress=0;animate();
}
void RadialMenu::setOpen(bool value) {
 closePopover();open=value;
 if(value) {
  expanded=true;settingsOpen=false;settingsProgress=0;
  for(auto& item:tools)gtk_widget_show(item->widget);
  for(auto& item:options)gtk_widget_hide(item->widget);
  gtk_widget_show(catcher);
  gtk_widget_grab_focus(hub);
 }
 updateHubIcon();animate();layout();
}
void RadialMenu::openAtPointer() {
 auto* top=window->getWindow();
 auto* display=gtk_widget_get_display(top);
 auto* seat=gdk_display_get_default_seat(display);
 auto* pointer=seat?gdk_seat_get_pointer(seat):nullptr;
 if(!pointer||!gtk_widget_get_realized(top)||pressed)return;
 double px=0,py=0;
 gdk_window_get_device_position_double(gtk_widget_get_window(top),pointer,&px,&py,nullptr);
 int ox=0,oy=0;
 if(!gtk_widget_translate_coordinates(GTK_WIDGET(overlay),top,0,0,&ox,&oy))return;
 px-=ox;py-=oy;
 int width=gtk_widget_get_allocated_width(GTK_WIDGET(overlay));
 int height=gtk_widget_get_allocated_height(GTK_WIDGET(overlay));
 if(px<0||py<0||px>=width||py>=height)return;
 if(!enabled)setEnabled(true);
 closePopover();progress=0;settingsProgress=0;
 x=std::clamp(px,34.0,std::max(34.0,width-34.0));
 y=std::clamp(py,34.0,std::max(34.0,height-34.0));
 setOpen(true);save();gtk_widget_queue_resize(GTK_WIDGET(overlay));
}
void RadialMenu::setEnabled(bool value) {
 enabled=value;closePopover();open=false;expanded=false;settingsOpen=false;progress=0;settingsProgress=0;
 updateHubIcon();
 if(tick) {gtk_widget_remove_tick_callback(fixed,tick);tick=0;}
 for(auto* group:{&tools,&options})for(auto& item:*group)gtk_widget_hide(item->widget);
 gtk_widget_hide(catcher);gtk_widget_hide(pager);
 layout();gtk_widget_set_visible(fixed,enabled);gtk_widget_queue_resize(GTK_WIDGET(overlay));
 g_simple_action_set_state(visibility,g_variant_new_boolean(value));save();
}
void RadialMenu::save() {
 auto* key=g_key_file_new();g_key_file_set_boolean(key,"menu","enabled",enabled);
 g_key_file_set_double(key,"menu","x",x);g_key_file_set_double(key,"menu","y",y);
 gsize size;auto* data=g_key_file_to_data(key,&size,nullptr);g_file_set_contents(configPath.c_str(),data,size,nullptr);
 g_free(data);g_key_file_unref(key);
}
void RadialMenu::animate() {
 if(tick)return;lastFrame=0;
 tick=gtk_widget_add_tick_callback(fixed,+[](GtkWidget*,GdkFrameClock* clock,gpointer data)->gboolean {
  auto* self=static_cast<RadialMenu*>(data);bool moving=self->frame(clock);if(!moving)self->tick=0;return moving;
 },this,nullptr);
}
bool RadialMenu::frame(GdkFrameClock* clock) {
 auto now=gdk_frame_clock_get_frame_time(clock);double dt=lastFrame?std::min(40.0,(now-lastFrame)/1000.0):16;lastFrame=now;
 gboolean animations=true;g_object_get(gtk_widget_get_settings(fixed),"gtk-enable-animations",&animations,nullptr);
 auto advance=[&](double& value,bool target,double duration) {
  if(!animations){value=target?1:0;return;}
  value=std::clamp(value+(target?1:-1)*dt/duration,0.0,1.0);
 };
 advance(progress,open,open?200:180);advance(settingsProgress,open&&settingsOpen,180);
 bool moving=progress!=(open?1:0)||settingsProgress!=((open&&settingsOpen)?1:0);
 for(auto* group:{&tools,&options})for(auto& item:*group) {
  bool highlight=item->hovered||item->focused;
  advance(item->hover,highlight,130);moving|=item->hover!=(highlight?1:0);
 }
 if(!open&&progress==0) {
  const bool wasExpanded=expanded;
  expanded=false;settingsOpen=false;gtk_widget_hide(catcher);gtk_widget_hide(pager);
  if(wasExpanded)updateHubIcon();
  for(auto* group:{&tools,&options})for(auto& item:*group)gtk_widget_hide(item->widget);
 }
 layout();gtk_widget_queue_resize(GTK_WIDGET(overlay));return moving;
}
void RadialMenu::layout() {
 double center=expanded?diameter/2:32;
 gtk_widget_set_size_request(fixed,expanded?static_cast<int>(diameter):64,expanded?static_cast<int>(diameter):64);
 gtk_fixed_move(GTK_FIXED(fixed),hub,static_cast<int>(center-30),static_cast<int>(center-30));
 bool compact=diameter<420;
 double opening=xoj::radial::smooth(progress);
 gtk_fixed_move(GTK_FIXED(fixed),pager,static_cast<int>(center-20+42*opening),static_cast<int>(center-20+42*opening));
 gtk_widget_set_visible(pager,expanded&&compact&&!settingsOpen&&settingsProgress==0);
 gtk_widget_set_opacity(pager,opening);gtk_widget_set_sensitive(pager,open&&progress>.8);
 double toolRadius=(diameter/2-40)*(compact?1:.54);
 size_t parent=primaryIndex(active);
 double parentAngle=compact?-PI/2+2*PI*(parent%4)/4:tools[parent]->angle;
 double optionRadius=compact?toolRadius:std::min(diameter/2-28,toolRadius+62);
 auto sensitive=[&](GtkWidget* widget,bool ready) {
  bool nativeEnabled=true;
  if(const char* name=gtk_actionable_get_action_name(GTK_ACTIONABLE(widget))) {
   auto* action=g_action_map_lookup_action(G_ACTION_MAP(window->getWindow()),g_str_has_prefix(name,"win.")?name+4:name);
   nativeEnabled=action&&g_action_get_enabled(action);
  }
  gtk_widget_set_sensitive(widget,ready&&nativeEnabled);
 };
 for(size_t i=0;i<tools.size();++i) {
  auto& item=*tools[i];bool selected=i==parent;
  bool onPage=!compact||int(i/4)==toolPage;
  gtk_widget_set_visible(item.widget,expanded&&(onPage||(selected&&(settingsOpen||settingsProgress>0))));
  double delay=.025*(compact?i%4:i);
  const double phase=std::clamp((progress-delay)/(1-delay),0.0,1.0);
  double p=xoj::radial::smooth(phase);
  const double distance=xoj::radial::spring(phase);
  int size=58+static_cast<int>(6*xoj::radial::smooth(item.hover));
  gtk_widget_set_size_request(item.widget,size,size);
  double angle=compact?-PI/2+2*PI*(i%4)/4:item.angle;
  gtk_fixed_move(GTK_FIXED(fixed),item.widget,static_cast<int>(center+std::cos(angle)*toolRadius*distance-size/2),static_cast<int>(center+std::sin(angle)*toolRadius*distance-size/2));
  double visible=compact&&!selected?1-xoj::radial::smooth(settingsProgress):1;
  gtk_widget_set_opacity(item.widget,p*visible);
  sensitive(item.widget,open&&progress>.8&&visible>.8);
 }
 for(size_t i=0;i<options.size();++i) {
  auto& item=*options[i];
  gtk_widget_set_visible(item.widget,expanded&&(settingsOpen||settingsProgress>0));
  double delay=.025*i;
  const double phase=std::clamp((settingsProgress-delay)/(1-delay),0.0,1.0);
  double p=xoj::radial::smooth(phase);
  const double distance=xoj::radial::spring(phase);
  int size=48+static_cast<int>(4*xoj::radial::smooth(item.hover));
  gtk_widget_set_size_request(item.widget,size,size);
  double target=xoj::radial::optionAngle(parentAngle+(compact?PI:0),i,options.size(),optionRadius);
  double angle=parentAngle+(target-parentAngle)*distance;
  double radius=std::min(diameter/2-size/2-4,toolRadius+(optionRadius-toolRadius)*distance);
  gtk_fixed_move(GTK_FIXED(fixed),item.widget,static_cast<int>(center+std::cos(angle)*radius-size/2),static_cast<int>(center+std::sin(angle)*radius-size/2));
  gtk_widget_set_opacity(item.widget,p);sensitive(item.widget,open&&settingsOpen&&settingsProgress>.8);
 }
}

gboolean RadialMenu::hubEvent(GdkEvent* event) {
 bool touch=event->type==GDK_TOUCH_BEGIN||event->type==GDK_TOUCH_UPDATE||event->type==GDK_TOUCH_END||event->type==GDK_TOUCH_CANCEL;
 bool down=event->type==GDK_BUTTON_PRESS||event->type==GDK_TOUCH_BEGIN;
 bool up=event->type==GDK_BUTTON_RELEASE||event->type==GDK_TOUCH_END;
 bool motion=event->type==GDK_MOTION_NOTIFY||event->type==GDK_TOUCH_UPDATE;
 if(!down&&!up&&!motion&&event->type!=GDK_TOUCH_CANCEL&&event->type!=GDK_GRAB_BROKEN)return false;
 if(touching&&!touch)return true;
 if(touch&&touching&&gdk_event_get_event_sequence(event)!=touchSequence)return true;
 if(event->type==GDK_BUTTON_PRESS&&event->button.button!=1)return true;
 double rx=0,ry=0;gdk_event_get_root_coords(event,&rx,&ry);
 if(down) {
  pressed=true;dragged=false;touching=touch;touchSequence=touch?gdk_event_get_event_sequence(event):nullptr;pressX=rx;pressY=ry;startX=x;startY=y;
  if(expanded) {
   double p=xoj::radial::smooth(progress);
   startX=x+(xoj::radial::clampCenter(x,gtk_widget_get_allocated_width(GTK_WIDGET(overlay)),diameter)-x)*p;
   startY=y+(xoj::radial::clampCenter(y,gtk_widget_get_allocated_height(GTK_WIDGET(overlay)),diameter)-y)*p;
  }
  gtk_grab_add(hub);gtk_widget_grab_focus(hub);return true;
 }
 if(event->type==GDK_GRAB_BROKEN||event->type==GDK_TOUCH_CANCEL) {
  pressed=false;touching=false;if(gtk_widget_has_grab(hub))gtk_grab_remove(hub);return true;
 }
 if(motion&&pressed) {
  double dx=rx-pressX,dy=ry-pressY;
  if(xoj::radial::isDrag(dx,dy)) {
   if(!dragged) {dragged=true;if(open)setOpen(false);}
   x=startX+dx;y=startY+dy;layout();gtk_widget_queue_resize(GTK_WIDGET(overlay));
  }return true;
 }
 if(up&&pressed) {
  pressed=false;touching=false;if(gtk_widget_has_grab(hub))gtk_grab_remove(hub);
  if(dragged)save();else setOpen(!open);return true;
 }
 return true;
}

void RadialMenu::closePopover() {
 if(popoverCloseIdle) {g_source_remove(popoverCloseIdle);popoverCloseIdle=0;}
 if(popover){auto* p=popover;popover=nullptr;gtk_widget_destroy(p);}
}
void RadialMenu::closePopoverAfterSelection() {
 if(!popover||popoverCloseIdle)return;
 // Let GtkActionable apply the chosen color before destroying the clicked widget.
 // Closing/replacing the panel cancels this idle, so it can never dismiss a newer panel.
 popoverCloseIdle=g_idle_add(+[](gpointer data)->gboolean {
  auto* self=static_cast<RadialMenu*>(data);self->popoverCloseIdle=0;self->closePopover();return G_SOURCE_REMOVE;
 },this);
}
void RadialMenu::dismissOptions() { closePopover(); setOpen(false); }
void RadialMenu::setInterfaceVisible(bool visible) { setEnabled(visible); }
void RadialMenu::showToolOptions(GtkWidget* anchor, Option option) {
 switch(option) {
  case Option::COLOR: colors(anchor); break;
  case Option::SIZE: sizes(anchor); break;
  case Option::DRAWING: modes(anchor); break;
  case Option::STYLE: styles(anchor); break;
  case Option::ERASER: erasers(anchor); break;
  case Option::SELECTION: selections(anchor); break;
 }
}
void RadialMenu::panel(GtkWidget* anchor,const char* title,std::function<void(GtkWidget*)> populate) {
 if(popover&&gtk_popover_get_relative_to(GTK_POPOVER(popover))==anchor) {closePopover();return;}
 closePopover();popover=gtk_popover_new(anchor);gtk_popover_set_modal(GTK_POPOVER(popover),false);
 auto* box=gtk_box_new(GTK_ORIENTATION_VERTICAL,5);gtk_style_context_add_class(gtk_widget_get_style_context(box),"radial-panel");
 gtk_box_pack_start(GTK_BOX(box),gtk_label_new(title),false,false,2);populate(box);
 auto* scroll=gtk_scrolled_window_new(nullptr,nullptr);
 gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
 gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(scroll),std::max(160,std::min(440,gtk_widget_get_allocated_height(GTK_WIDGET(overlay))-32)));
 gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll),true);
 gtk_container_add(GTK_CONTAINER(scroll),box);gtk_container_add(GTK_CONTAINER(popover),scroll);
 gtk_widget_show_all(scroll);gtk_popover_popup(GTK_POPOVER(popover));
}
void RadialMenu::panelAction(GtkWidget* box,const char* label,Action action,GVariant* target,const char* icon) {
 auto* button=gtk_toggle_button_new_with_label(label);bind(button,action,target);
 if(icon) {auto name=IconNameHelper(window->getControl()->getSettings()).iconName(icon);gtk_button_set_image(GTK_BUTTON(button),gtk_image_new_from_icon_name(name.c_str(),GTK_ICON_SIZE_BUTTON));gtk_button_set_always_show_image(GTK_BUTTON(button),true);}
 gtk_box_pack_start(GTK_BOX(box),button,false,false,0);
}
void RadialMenu::sizes(GtkWidget* anchor) {
 panel(anchor,"Espessura",[this](auto* box) {
  const char* names[]={_("Very Fine"),_("Fine"),_("Medium"),_("Thick"),_("Very Thick")};
  for(int i=0;i<5;++i)panelAction(box,names[i],Action::TOOL_SIZE,g_variant_new_uint64(i));
 });
}
void RadialMenu::colors(GtkWidget* anchor) {
 panel(anchor,"Cor",[this](auto* box) {
  auto* grid=gtk_grid_new();gtk_grid_set_row_spacing(GTK_GRID(grid),5);gtk_grid_set_column_spacing(GTK_GRID(grid),5);
  const auto& palette=window->getControl()->getPalette();
  for(size_t i=0;i<std::min(size_t(24),palette.size());++i) {
   const auto& color=palette.getColorAt(i);auto c=color.getColor();
   // GtkToggleButton emits clicked when action synchronization changes its active state.
   // A swatch must dismiss only for activation, not while the palette is being initialized.
   auto* button=gtk_button_new();gtk_widget_set_size_request(button,40,40);accessible(button,color.getName().c_str());
   g_object_set_data(G_OBJECT(button),"radial-swatch-color",GUINT_TO_POINTER(uint32_t(c)));
   auto* dot=gtk_drawing_area_new();gtk_widget_set_size_request(dot,24,24);
   g_object_set_data(G_OBJECT(dot),"color",GUINT_TO_POINTER(uint32_t(c)));
   g_signal_connect(dot,"draw",G_CALLBACK((+[](GtkWidget* widget,cairo_t* cr,gpointer)->gboolean {
    auto c=Color(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(widget),"color")));
    cairo_set_source_rgb(cr,c.red/255.0,c.green/255.0,c.blue/255.0);cairo_arc(cr,12,12,10,0,2*PI);cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr,.4,.4,.4,.7);cairo_set_line_width(cr,1);cairo_stroke(cr);return true;
   })),nullptr);
   gtk_container_add(GTK_CONTAINER(button),dot);bind(button,Action::TOOL_COLOR,g_variant_new_uint32(uint32_t(c)));
   auto* action=G_OBJECT(window->getControl()->getActionDatabase()->getAction(Action::TOOL_COLOR).get());
   g_signal_connect_object(action,"notify::state",G_CALLBACK(syncSwatch),button,GConnectFlags(0));
   syncSwatch(action,nullptr,button);
   g_signal_connect_after(button,"clicked",G_CALLBACK((+[](GtkButton*,gpointer data) {
    static_cast<RadialMenu*>(data)->closePopoverAfterSelection();
   })),this);
   gtk_grid_attach(GTK_GRID(grid),button,i%6,i/6,1,1);
  }
  gtk_box_pack_start(GTK_BOX(box),grid,false,false,0);
  auto* more=gtk_button_new_with_label("Outras cores…");gtk_box_pack_start(GTK_BOX(box),more,false,false,0);
  g_signal_connect(more,"clicked",G_CALLBACK((+[](GtkButton*,gpointer data) {auto* self=static_cast<RadialMenu*>(data);self->closePopover();self->activate(Action::SELECT_COLOR);})),this);
 });
}
void RadialMenu::styles(GtkWidget* anchor) {
 panel(anchor,"Traço",[this](auto* box) {
  const char* labels[]={_("standard"),_("dashed"),_("dash-/ dotted"),_("dotted")};
  const char* targets[]={"plain","dash","dashdot","dot"};
  const char* icons[]={"line-style-plain","line-style-dash","line-style-dash-dot","line-style-dot"};
  for(int i=0;i<4;++i)panelAction(box,labels[i],Action::TOOL_PEN_LINE_STYLE,g_variant_new_string(targets[i]),icons[i]);
 });
}
void RadialMenu::erasers(GtkWidget* anchor) {
 panel(anchor,_("Eraser"),[this](auto* box) {
  panelAction(box,_("standard"),Action::TOOL_ERASER_TYPE,g_variant_new_uint64(ERASER_TYPE_DEFAULT));
  panelAction(box,_("whiteout"),Action::TOOL_ERASER_TYPE,g_variant_new_uint64(ERASER_TYPE_WHITEOUT));
  panelAction(box,_("delete stroke"),Action::TOOL_ERASER_TYPE,g_variant_new_uint64(ERASER_TYPE_DELETE_STROKE));
 });
}
void RadialMenu::modes(GtkWidget* anchor) {
 panel(anchor,"Modo de desenho",[this](auto* box) {
  auto* free=gtk_button_new_with_label("Desenho livre");gtk_box_pack_start(GTK_BOX(box),free,false,false,0);
  g_object_set_data(G_OBJECT(popover),"radial-freehand",free);
  if(window->getControl()->getToolHandler()->getActiveTool()->getDrawingType()==DRAWING_TYPE_DEFAULT)
   gtk_widget_set_state_flags(free,GTK_STATE_FLAG_CHECKED,false);
  g_object_set_data(G_OBJECT(popover),"radial-freehand",free);
  if(window->getControl()->getToolHandler()->getActiveTool()->getDrawingType()==DRAWING_TYPE_DEFAULT)gtk_widget_set_state_flags(free,GTK_STATE_FLAG_CHECKED,false);
  g_signal_connect(free,"clicked",G_CALLBACK((+[](GtkButton*,gpointer data) {
   auto* self=static_cast<RadialMenu*>(data);auto* tool=self->window->getControl()->getToolHandler()->getActiveTool();
   switch(tool->getDrawingType()) {
    case DRAWING_TYPE_LINE:self->activate(Action::TOOL_DRAW_LINE);break;
    case DRAWING_TYPE_SHAPE_RECOGNIZER:self->activate(Action::TOOL_DRAW_SHAPE_RECOGNIZER);break;
    case DRAWING_TYPE_RECTANGLE:self->activate(Action::TOOL_DRAW_RECTANGLE);break;
    case DRAWING_TYPE_ELLIPSE:self->activate(Action::TOOL_DRAW_ELLIPSE);break;
    case DRAWING_TYPE_ARROW:self->activate(Action::TOOL_DRAW_ARROW);break;
    case DRAWING_TYPE_DOUBLE_ARROW:self->activate(Action::TOOL_DRAW_DOUBLE_ARROW);break;
    case DRAWING_TYPE_COORDINATE_SYSTEM:self->activate(Action::TOOL_DRAW_COORDINATE_SYSTEM);break;
    case DRAWING_TYPE_SPLINE:self->activate(Action::TOOL_DRAW_SPLINE);break;
    default:break;
   }
  })),this);
  auto* tool=window->getControl()->getToolHandler()->getActiveTool();
  if(tool->hasCapability(TOOL_CAP_RULER))panelAction(box,_("Draw Line"),Action::TOOL_DRAW_LINE,nullptr,"draw-line");
  if(tool->hasCapability(TOOL_CAP_RECOGNIZER))panelAction(box,_("Shape Recognizer"),Action::TOOL_DRAW_SHAPE_RECOGNIZER,nullptr,"shape-recognizer");
  struct Mode {ToolCapabilities cap;Action action;const char* label;const char* icon;};
  for(const auto& mode:std::vector<Mode>{{TOOL_CAP_RECTANGLE,Action::TOOL_DRAW_RECTANGLE,N_("Draw Rectangle"),"draw-rect"},
   {TOOL_CAP_ELLIPSE,Action::TOOL_DRAW_ELLIPSE,N_("Draw Ellipse"),"draw-ellipse"},
   {TOOL_CAP_ARROW,Action::TOOL_DRAW_ARROW,N_("Draw Arrow"),"draw-arrow"},
   {TOOL_CAP_DOUBLE_ARROW,Action::TOOL_DRAW_DOUBLE_ARROW,N_("Draw Double Arrow"),"draw-double-arrow"},
   {TOOL_CAP_COORDINATE_SYSTEM,Action::TOOL_DRAW_COORDINATE_SYSTEM,N_("Draw Coordinate System"),"draw-coordinate-system"},
   {TOOL_CAP_SPLINE,Action::TOOL_DRAW_SPLINE,N_("Draw Spline"),"draw-spline"}})
   if(tool->hasCapability(mode.cap))panelAction(box,_(mode.label),mode.action,nullptr,mode.icon);
  if(active==TOOL_PEN)panelAction(box,"Tracos: caneta texto (pt-BR)",Action::TRACOS_TEXT,nullptr,"tracos-text");
 });
}
void RadialMenu::selections(GtkWidget* anchor) {
 panel(anchor,"Seleção",[this](auto* box) {
  for(const auto& e:std::vector<ToolEntry>{{TOOL_SELECT_RECT,N_("Select Rectangle"),"select-rect"},
   {TOOL_SELECT_REGION,N_("Select Region"),"select-lasso"},
   {TOOL_SELECT_MULTILAYER_RECT,N_("Select Multi-Layer Rectangle"),"select-multilayer-rect"},
   {TOOL_SELECT_MULTILAYER_REGION,N_("Select Multi-Layer Region"),"select-multilayer-lasso"},
   {TOOL_SELECT_OBJECT,N_("Select Object"),"object-select"}})
   panelAction(box,_(e.label),Action::SELECT_TOOL,g_variant_new_uint64(e.type),e.icon);
 });
}
