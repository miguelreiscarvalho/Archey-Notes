#include "StudioInterface.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include "MainWindow.h"
#include "IconNameHelper.h"
#include "WindowUiActions.h"
#include "control/Control.h"
#include "control/Tool.h"
#include "control/ToolHandler.h"
#include "control/actions/ActionDatabase.h"
#include "control/settings/Settings.h"
#include "control/zoom/ZoomControl.h"
#include "gui/toolbarMenubar/icon/ColorIcon.h"
#include "util/PathUtil.h"
#include "util/i18n.h"

namespace {
void style(GtkWidget* w, const char* name) { gtk_style_context_add_class(gtk_widget_get_style_context(w), name); }
void name(GtkWidget* w, const char* text) {
    atk_object_set_name(gtk_widget_get_accessible(w), text);
    gtk_widget_set_tooltip_text(w, text);
}
const char* toolLabel(ToolType tool) {
    switch (tool) {
        case TOOL_PEN: return "Caneta";
        case TOOL_ERASER: return "Borracha";
        case TOOL_HIGHLIGHTER: return "Marca-texto";
        case TOOL_TEXT: return "Texto";
        case TOOL_HAND: return "Mão";
        case TOOL_IMAGE: return "Imagem";
        case TOOL_VERTICAL_SPACE: return "Espaço vertical";
        default: return isSelectToolType(tool) ? "Seleção" : "Ferramenta";
    }
}
}

StudioInterface::StudioInterface(MainWindow* window, GtkOverlay* overlay, RadialMenu* radial):
        window(window), radial(radial), overlay(overlay) {
    auto path = Util::getConfigFile("studio-interface.ini").u8string();
    configPath.assign(path.begin(), path.end());
    auto* settings = window->getControl()->getSettings();
    classicIconTheme = static_cast<int>(settings->getIconTheme());
    classicRadial = radial->isEnabled();
    preFocusSidebar = settings->isSidebarVisible();
    auto* key = g_key_file_new();
    if (g_key_file_load_from_file(key, configPath.c_str(), G_KEY_FILE_NONE, nullptr)) {
        auto* stored = g_key_file_get_string(key, "interface", "mode", nullptr);
        if (stored) { mode = stored; g_free(stored); }
        if (g_key_file_has_key(key, "interface", "classic-icons", nullptr))
            classicIconTheme = g_key_file_get_integer(key, "interface", "classic-icons", nullptr);
        if (g_key_file_has_key(key, "interface", "classic-radial", nullptr))
            classicRadial = g_key_file_get_boolean(key, "interface", "classic-radial", nullptr);
        if (g_key_file_has_key(key, "interface", "focus-sidebar", nullptr))
            preFocusSidebar = g_key_file_get_boolean(key, "interface", "focus-sidebar", nullptr);
    }
    g_key_file_unref(key);
    if (mode != "focus" && mode != "classic") mode = "studio";
    if (classicIconTheme != int(ICON_THEME_COLOR) && classicIconTheme != int(ICON_THEME_LUCIDE))
        classicIconTheme = int(ICON_THEME_COLOR);

    auto* css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, R"CSS(
    .studio-window {font-family:"Segoe UI",sans-serif;}
    .studio-window #sidebar {background:@theme_bg_color;border-right:1px solid alpha(@theme_fg_color,.08);}
    .studio-window #sidebar button {border-radius:9px;}
    .studio-header {background:@theme_bg_color;padding:9px 16px;border-bottom:1px solid alpha(@theme_fg_color,.09);}
    .studio-header button, .studio-header combobox button {border-radius:10px;min-height:30px;box-shadow:none;}
    .studio-header .studio-title {font-weight:600;}
    .studio-subtitle {font-size:11px;opacity:.65;}
    .studio-dock {background:@theme_bg_color;border:1px solid alpha(@theme_fg_color,.12);border-radius:18px;
        padding:7px;box-shadow:0 5px 18px alpha(black,.17);}
    .studio-tool {padding:9px;min-width:24px;min-height:24px;border-radius:11px;border:1px solid transparent;
        background:transparent;box-shadow:none;}
    .studio-tool:hover {background:alpha(@theme_selected_bg_color,.10);}
    .studio-tool:checked, .studio-tool.studio-group-active {background:alpha(@theme_selected_bg_color,.17);
        border-color:alpha(@theme_selected_bg_color,.4);color:@theme_selected_bg_color;}
    .studio-tool:focus {outline:2px solid @theme_selected_bg_color;outline-offset:-3px;}
    .studio-context {padding-top:5px;border-top:1px solid alpha(@theme_fg_color,.08);margin-top:5px;}
    .studio-context button {border-radius:9px;padding:5px 8px;min-height:26px;box-shadow:none;
        background:transparent;border:1px solid transparent;}
    .studio-context button:hover {background:alpha(@theme_selected_bg_color,.10);}
    .studio-context button:checked {background:alpha(@theme_selected_bg_color,.17);}
    .studio-context label {font-size:11px;}
    .studio-primary {background:alpha(@theme_selected_bg_color,.12);border-color:transparent;}
    )CSS", -1, nullptr);
    gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(window->getWindow()), GTK_STYLE_PROVIDER(css),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 2);
    g_object_set_data_full(G_OBJECT(window->getWindow()), "studio-css", css, g_object_unref);

    header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    style(header, "studio-header");
    auto* menu = gtk_menu_button_new();
    gtk_container_add(GTK_CONTAINER(menu), gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu), window->getMenuModel());
    name(menu, "Todos os menus do Xournal");
    gtk_box_pack_start(GTK_BOX(header), menu, false, false, 0);
    actionButton(header, "Páginas e camadas", "sidebar-page-preview", Action::SHOW_SIDEBAR, nullptr, true);
    auto* titles = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    title = gtk_label_new("Xournal · Estúdio");
    style(title, "studio-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_max_width_chars(GTK_LABEL(title), 36);
    subtitle = gtk_label_new("Seu espaço para escrever e desenhar");
    style(subtitle, "studio-subtitle");
    gtk_label_set_xalign(GTK_LABEL(subtitle), 0);
    gtk_box_pack_start(GTK_BOX(titles), title, false, false, 0);
    gtk_box_pack_start(GTK_BOX(titles), subtitle, false, false, 0);
    gtk_widget_set_margin_start(titles, 10);
    gtk_box_pack_start(GTK_BOX(header), titles, true, true, 0);
    actionButton(header, "Desfazer", "edit-undo", Action::UNDO);
    actionButton(header, "Refazer", "edit-redo", Action::REDO);
    auto* saveButton = actionButton(header, "Salvar", "document-save", Action::SAVE);
    saveLabel = gtk_label_new("Salvar");
    gtk_box_pack_start(GTK_BOX(gtk_bin_get_child(GTK_BIN(saveButton))), saveLabel, false, false, 0);
    auto* exportButton = actionButton(header, "Exportar PDF", "document-export-pdf", Action::EXPORT_AS_PDF);
    style(exportButton, "studio-primary");
    exportLabel = gtk_label_new("Exportar");
    gtk_box_pack_start(GTK_BOX(gtk_bin_get_child(GTK_BIN(exportButton))), exportLabel, false, false, 0);
    modePicker = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(modePicker), "studio", "Estúdio");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(modePicker), "focus", "Foco");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(modePicker), "classic", "Clássico");
    name(modePicker, "Modo da interface");
    gtk_box_pack_end(GTK_BOX(header), modePicker, false, false, 4);
    gtk_box_pack_start(GTK_BOX(window->get("mainBox")), header, false, false, 0);
    gtk_box_reorder_child(GTK_BOX(window->get("mainBox")), header, 0);
    gtk_widget_show_all(header);
    gtk_widget_set_no_show_all(header, true);

    dock = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    style(dock, "studio-dock");
    name(dock, "Paleta de ferramentas");
    gtk_widget_set_has_tooltip(dock, false);
    gtk_widget_set_halign(dock, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(dock, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(dock, 22);
    gtk_widget_set_margin_start(dock, 10);
    gtk_widget_set_margin_end(dock, 10);
    toolRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_box_set_homogeneous(GTK_BOX(toolRow), true);
    gtk_box_pack_start(GTK_BOX(dock), toolRow, false, false, 0);
    struct Entry { const char* label; const char* icon; ToolType tool; };
    for (auto e : {Entry{"Caneta", "tool-pencil", TOOL_PEN}, {"Marca-texto", "tool-highlighter", TOOL_HIGHLIGHTER},
                   {"Borracha", "tool-eraser", TOOL_ERASER}, {"Seleção", "select-lasso", TOOL_SELECT_REGION},
                   {"Texto", "tool-text", TOOL_TEXT}, {"Mão", "hand", TOOL_HAND}, {"Imagem", "tool-image", TOOL_IMAGE}}) {
        auto* button = actionButton(toolRow, e.label, e.icon, Action::SELECT_TOOL,
                                     g_variant_new_uint64(e.tool), true);
        style(button, "studio-tool");
        if (e.tool == TOOL_SELECT_REGION) selectionTool = button;
    }
    auto* tracos = actionButton(toolRow, "Traços · caneta texto", "tracos-text", Action::TRACOS_TEXT, nullptr, true);
    style(tracos, "studio-tool");
    contextRow = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(contextRow), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(contextRow), 8);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(contextRow), 1);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(contextRow), 2);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(contextRow), 2);
    style(contextRow, "studio-context");
    gtk_box_pack_start(GTK_BOX(dock), contextRow, false, false, 0);
    colorButton = optionButton("Cor", "color-select", RadialMenu::Option::COLOR);
    colorImage = gtk_bin_get_child(GTK_BIN(colorButton));
    gtk_container_remove(GTK_CONTAINER(colorButton), colorImage);
    colorImage = ColorIcon::newGtkImage(window->getControl()->getToolHandler()->getActiveTool()->getColor(), 20);
    gtk_container_add(GTK_CONTAINER(colorButton), colorImage);
    context.push_back({gtk_widget_get_parent(colorButton), TOOL_CAP_COLOR, Action::TOOL_COLOR});
    sizeButton = optionButton("Espessura", "thickness-medium", RadialMenu::Option::SIZE);
    sizeLabel = gtk_label_new("Média");
    auto* sizeBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_remove(GTK_CONTAINER(sizeButton), gtk_bin_get_child(GTK_BIN(sizeButton)));
    gtk_box_pack_start(GTK_BOX(sizeBox), sizeLabel, false, false, 0);
    gtk_container_add(GTK_CONTAINER(sizeButton), sizeBox);
    context.push_back({gtk_widget_get_parent(sizeButton), TOOL_CAP_SIZE, Action::TOOL_SIZE});
    drawingButton = optionButton("Formas e desenho", "combo-drawing-type", RadialMenu::Option::DRAWING);
    drawingLabel = gtk_label_new("Formas");
    gtk_box_pack_start(GTK_BOX(gtk_bin_get_child(GTK_BIN(drawingButton))), drawingLabel, false, false, 0);
    // Drawing capabilities are checked together in refresh().
    auto* line = optionButton("Estilo do traço", "line-style-dash", RadialMenu::Option::STYLE);
    context.push_back({gtk_widget_get_parent(line), TOOL_CAP_LINE_STYLE, Action::TOOL_PEN_LINE_STYLE});
    eraserButton = optionButton("Modo da borracha", "tool-eraser", RadialMenu::Option::ERASER);
    selectionButton = optionButton("Tipos de seleção", "combo-selection", RadialMenu::Option::SELECTION);
    auto* font = actionButton(contextRow, "Fonte do texto", "tool-text", Action::SELECT_FONT);
    context.push_back({gtk_widget_get_parent(font), TOOL_CAP_NONE, Action::SELECT_FONT});
    auto* fill = actionButton(contextRow, "Preenchimento", "fill", Action::TOOL_FILL, nullptr, true);
    context.push_back({gtk_widget_get_parent(fill), TOOL_CAP_FILL, Action::TOOL_FILL});
    auto* opacity = actionButton(contextRow, "Opacidade do preenchimento", "fill-opacity", Action::TOOL_FILL_OPACITY);
    context.push_back({gtk_widget_get_parent(opacity), TOOL_CAP_FILL, Action::TOOL_FILL_OPACITY});
    gtk_overlay_add_overlay(overlay, dock);
    gtk_widget_show_all(dock);
    gtk_widget_set_no_show_all(dock, true);

    auto* footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    style(footer, "studio-footer");
    gtk_widget_set_margin_start(footer, 12);
    gtk_widget_set_margin_end(footer, 12);
    gtk_widget_set_margin_top(footer, 3);
    gtk_widget_set_margin_bottom(footer, 3);
    actionButton(footer, "Página anterior", "go-previous", Action::GOTO_PREVIOUS);
    pageLabel = gtk_label_new("1 / 1");
    gtk_box_pack_start(GTK_BOX(footer), pageLabel, false, false, 4);
    actionButton(footer, "Próxima página", "go-next", Action::GOTO_NEXT);
    actionButton(footer, "Nova página", "page-add", Action::NEW_PAGE_AFTER);
    auto* space = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(footer), space, true, true, 0);
    actionButton(footer, "Reduzir zoom", "zoom-out", Action::ZOOM_OUT);
    zoomLabel = gtk_label_new("100%");
    gtk_box_pack_start(GTK_BOX(footer), zoomLabel, false, false, 2);
    actionButton(footer, "Ampliar zoom", "zoom-in", Action::ZOOM_IN);
    actionButton(footer, "Ajustar à página", "zoom-fit-best", Action::ZOOM_FIT, nullptr, true);
    gtk_box_pack_end(GTK_BOX(window->get("mainBox")), footer, false, false, 0);
    g_object_set_data(G_OBJECT(header), "studio-footer", footer);
    gtk_widget_show_all(footer);
    gtk_widget_set_no_show_all(footer, true);

    auto* action = g_simple_action_new_stateful("interface-mode", xoj::ui::windowActionParameterType("interface-mode").value(), g_variant_new_string(mode.c_str()));
    g_signal_connect(action, "activate", G_CALLBACK((+[](GSimpleAction*, GVariant* value, gpointer data) {
        if (value) static_cast<StudioInterface*>(data)->setMode(g_variant_get_string(value, nullptr));
    })), this);
    g_signal_connect(action, "change-state", G_CALLBACK((+[](GSimpleAction*, GVariant* value, gpointer data) {
        static_cast<StudioInterface*>(data)->setMode(g_variant_get_string(value, nullptr));
    })), this);
    g_action_map_add_action(G_ACTION_MAP(window->getWindow()), G_ACTION(action));
    g_object_unref(action);
    g_signal_connect(modePicker, "changed", G_CALLBACK((+[](GtkComboBox* picker, gpointer data) {
        auto* self = static_cast<StudioInterface*>(data);
        if (!self->updatingPicker) if (auto* id = gtk_combo_box_get_active_id(picker)) self->setMode(id);
    })), this);
    auto* db = window->getControl()->getActionDatabase();
    for (auto a : {Action::SELECT_TOOL, Action::TOOL_SIZE, Action::TOOL_COLOR, Action::TOOL_PEN_LINE_STYLE,
                   Action::TOOL_ERASER_TYPE, Action::TOOL_FILL, Action::TOOL_FILL_OPACITY, Action::FONT,
                   Action::TOOL_DRAW_LINE, Action::TOOL_DRAW_SHAPE_RECOGNIZER, Action::TOOL_DRAW_RECTANGLE,
                   Action::TOOL_DRAW_ELLIPSE, Action::TOOL_DRAW_ARROW, Action::TOOL_DRAW_DOUBLE_ARROW,
                   Action::TOOL_DRAW_COORDINATE_SYSTEM, Action::TOOL_DRAW_SPLINE, Action::TRACOS_TEXT,
                   Action::ZOOM, Action::SELECT_FONT}) {
        auto* object = G_OBJECT(db->getAction(a).get());
        g_object_ref(object); watched.push_back(object);
        g_signal_connect(object, "notify", G_CALLBACK((+[](GObject*, GParamSpec*, gpointer data) {
            static_cast<StudioInterface*>(data)->scheduleRefresh();
        })), this);
    }
    g_signal_connect(window->getWindow(), "notify::title", G_CALLBACK((+[](GObject*, GParamSpec*, gpointer data) {
        static_cast<StudioInterface*>(data)->updateTitle();
    })), this);
    g_signal_connect(overlay, "size-allocate", G_CALLBACK((+[](GtkWidget*, GtkAllocation* allocation, gpointer data) {
        static_cast<StudioInterface*>(data)->resize(allocation->width);
    })), this);
    setMode(mode);
    refresh(); updateTitle();
}

StudioInterface::~StudioInterface() {
    if (refreshIdle) g_source_remove(refreshIdle);
    radial->dismissOptions();
    for (auto* object : watched) { g_signal_handlers_disconnect_by_data(object, this); g_object_unref(object); }
    g_signal_handlers_disconnect_by_data(overlay, this);
    g_signal_handlers_disconnect_by_data(window->getWindow(), this);
    g_action_map_remove_action(G_ACTION_MAP(window->getWindow()), "interface-mode");
    gtk_widget_destroy(GTK_WIDGET(g_object_get_data(G_OBJECT(header), "studio-footer")));
    gtk_widget_destroy(dock); gtk_widget_destroy(header);
}

GtkWidget* StudioInterface::iconButton(const char* label, const char* icon, bool toggle) {
    auto* button = toggle ? gtk_toggle_button_new() : gtk_button_new();
    auto* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    auto iconName = IconNameHelper(window->getControl()->getSettings()).iconName(icon);
    if (!gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), iconName.c_str())) iconName = icon;
    gtk_box_pack_start(GTK_BOX(box), gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR), false, false, 0);
    gtk_container_add(GTK_CONTAINER(button), box);
    name(button, label);
    return button;
}
void StudioInterface::bind(GtkWidget* widget, Action action, GVariant* target) {
    if (target) gtk_actionable_set_action_target_value(GTK_ACTIONABLE(widget), target);
    auto actionName = std::string("win.") + Action_toString(action);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(widget), actionName.c_str());
}
GtkWidget* StudioInterface::actionButton(GtkWidget* box, const char* label, const char* icon, Action action,
                                       GVariant* target, bool toggle) {
    auto* button = iconButton(label, icon, toggle);
    bind(button, action, target);
    if (GTK_IS_BOX(box)) gtk_box_pack_start(GTK_BOX(box), button, false, false, 0);
    else gtk_container_add(GTK_CONTAINER(box), button);
    return button;
}
GtkWidget* StudioInterface::optionButton(const char* label, const char* icon, RadialMenu::Option option) {
    auto* button = iconButton(label, icon);
    g_object_set_data(G_OBJECT(button), "studio-option", GINT_TO_POINTER(static_cast<int>(option)));
    g_signal_connect(button, "clicked", G_CALLBACK((+[](GtkButton* button, gpointer data) {
        auto* self = static_cast<StudioInterface*>(data);
        self->radial->showToolOptions(GTK_WIDGET(button), static_cast<RadialMenu::Option>(GPOINTER_TO_INT(
            g_object_get_data(G_OBJECT(button), "studio-option"))));
    })), this);
    gtk_container_add(GTK_CONTAINER(contextRow), button);
    return button;
}
void StudioInterface::setMode(const std::string& next) {
    if (next != "studio" && next != "focus" && next != "classic") return;
    bool leavingFocus = mode == "focus" && next != "focus";
    bool enteringFocus = mode != "focus" && next == "focus";
    if (enteringFocus) preFocusSidebar = window->getControl()->getSettings()->isSidebarVisible();
    mode = next;
    radial->dismissOptions();
    radial->setInterfaceVisible(mode == "focus" || (mode == "classic" && classicRadial));
    auto* action = g_action_map_lookup_action(G_ACTION_MAP(window->getWindow()), "interface-mode");
    g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_string(mode.c_str()));
    updatingPicker = true;
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(modePicker), mode.c_str());
    updatingPicker = false;
    auto* settings = window->getControl()->getSettings();
    settings->setIconTheme(isModern() ? ICON_THEME_LUCIDE : static_cast<IconTheme>(classicIconTheme));
    window->updateColorscheme();
    if (mode == "focus") window->getControl()->getActionDatabase()->fireChangeActionState(Action::SHOW_SIDEBAR, false);
    else if (leavingFocus) window->getControl()->getActionDatabase()->fireChangeActionState(Action::SHOW_SIDEBAR, preFocusSidebar);
    applyLayout();
    save();
}
void StudioInterface::applyLayout() {
    auto* settings = window->getControl()->getSettings();
    auto* styleContext = gtk_widget_get_style_context(window->getWindow());
    if (isModern()) gtk_style_context_add_class(styleContext, "studio-window");
    else gtk_style_context_remove_class(styleContext, "studio-window");
    for (const auto& w : window->getToolbarWidgets()) {
        gtk_widget_set_visible(w.get(), !isModern() && settings->isToolbarVisible() && gtk_toolbar_get_n_items(GTK_TOOLBAR(w.get())) > 0);
    }
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(window->getWindow()), !isModern() && settings->isMenubarVisible());
    gtk_widget_set_visible(header, isModern());
    gtk_widget_set_visible(dock, mode == "studio");
    auto* footer = GTK_WIDGET(g_object_get_data(G_OBJECT(header), "studio-footer"));
    gtk_widget_set_visible(footer, isModern());
    gtk_widget_set_visible(window->get("statusbar"), !isModern());
    gtk_widget_queue_draw(window->getWindow());
}
void StudioInterface::save() {
    auto* key = g_key_file_new();
    g_key_file_set_string(key, "interface", "mode", mode.c_str());
    g_key_file_set_integer(key, "interface", "classic-icons", classicIconTheme);
    g_key_file_set_boolean(key, "interface", "classic-radial", classicRadial);
    g_key_file_set_boolean(key, "interface", "focus-sidebar", preFocusSidebar);
    g_key_file_save_to_file(key, configPath.c_str(), nullptr);
    g_key_file_unref(key);
}
void StudioInterface::scheduleRefresh() {
    if (refreshIdle) return;
    refreshIdle = g_idle_add(+[](gpointer data) -> gboolean {
        auto* self = static_cast<StudioInterface*>(data); self->refreshIdle = 0; self->refresh(); return G_SOURCE_REMOVE;
    }, this);
}
void StudioInterface::refresh() {
    auto* tool = window->getControl()->getToolHandler()->getActiveTool();
    // RadialMenu owns its navigation and refreshes its options on tool changes.
    // Synchronizing the Studio dock must not dismiss a selection made in that menu.
    active = tool->getToolType();
    auto* db = window->getControl()->getActionDatabase();
    for (const auto& entry : context) {
        gtk_widget_set_visible(entry.widget, entry.action == Action::SELECT_FONT ? active == TOOL_TEXT : tool->hasCapability(entry.capability));
        gtk_widget_set_sensitive(entry.widget, db->isActionEnabled(entry.action));
    }
    gtk_widget_set_visible(gtk_widget_get_parent(drawingButton), tool->hasCapability(TOOL_CAP_RULER) || tool->hasCapability(TOOL_CAP_RECOGNIZER));
    gtk_widget_set_visible(gtk_widget_get_parent(eraserButton), active == TOOL_ERASER);
    gtk_widget_set_visible(gtk_widget_get_parent(selectionButton), isSelectToolType(active));
    auto* selectionStyle = gtk_widget_get_style_context(selectionTool);
    if (isSelectToolType(active)) gtk_style_context_add_class(selectionStyle, "studio-group-active");
    else gtk_style_context_remove_class(selectionStyle, "studio-group-active");
    auto pixbuf = ColorIcon::newGdkPixbuf(tool->getColor(), 20);
    gtk_image_set_from_pixbuf(GTK_IMAGE(colorImage), pixbuf.get());
    const char* names[] = {"Muito fina", "Fina", "Média", "Grossa", "Muito grossa"};
    gtk_label_set_text(GTK_LABEL(sizeLabel), names[std::clamp(int(tool->getSize()), 0, 4)]);
    const char* modes[] = {"", "Livre", "Linha", "Retângulo", "Elipse", "Seta", "Seta dupla", "Eixos", "Reconhecer", "Spline"};
    auto drawing = int(tool->getDrawingType());
    gtk_label_set_text(GTK_LABEL(drawingLabel), drawing > 0 && drawing < 10 ? modes[drawing] : "Formas");
    auto zoom = std::to_string(static_cast<int>(std::round(window->getControl()->getZoomControl()->getZoomReal() * 100))) + "%";
    gtk_label_set_text(GTK_LABEL(zoomLabel), zoom.c_str());
    std::string info = toolLabel(active);
    gtk_label_set_text(GTK_LABEL(subtitle), info.c_str());
}
void StudioInterface::updateTitle() {
    const auto* text = gtk_window_get_title(GTK_WINDOW(window->getWindow()));
    std::string display = text ? text : "Xournal · Estúdio";
    const std::string suffix = " - Xournal++";
    if (display.size() >= suffix.size() && display.compare(display.size()-suffix.size(), suffix.size(), suffix) == 0)
        display.resize(display.size()-suffix.size());
    gtk_label_set_text(GTK_LABEL(title), display.c_str());
}
void StudioInterface::updatePageNumbers(size_t page, size_t count) {
    currentPage = page; pageCount = count;
    auto text = std::to_string(page + 1) + " / " + std::to_string(count);
    gtk_label_set_text(GTK_LABEL(pageLabel), text.c_str());
}
void StudioInterface::resize(int width) {
    // Eight 44px targets fit comfortably until the canvas is narrower than 440px.
    // Keep the palette below the page content for normal tablet/laptop windows.
    bool nextCompact = width < 440;
    if (nextCompact != compact) {
        compact = nextCompact;
        gtk_orientable_set_orientation(GTK_ORIENTABLE(toolRow), compact ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_halign(dock, compact ? GTK_ALIGN_START : GTK_ALIGN_CENTER);
        gtk_widget_set_valign(dock, compact ? GTK_ALIGN_CENTER : GTK_ALIGN_END);
        gtk_orientable_set_orientation(GTK_ORIENTABLE(dock), compact ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(contextRow), compact ? 1 : 8);
    }
    gtk_widget_set_visible(exportLabel, width >= 680);
    gtk_widget_set_visible(saveLabel, width >= 680);
    gtk_widget_set_visible(subtitle, width >= 480);
}
