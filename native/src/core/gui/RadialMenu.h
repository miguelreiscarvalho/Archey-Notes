#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <gtk/gtk.h>
#include "control/ToolEnums.h"
#include "enums/Action.enum.h"
class MainWindow;

/** Native action-backed menu. All coordinates are viewport pixels, never document coordinates. */
class RadialMenu {
public:
    RadialMenu(MainWindow* window, GtkOverlay* overlay);
    ~RadialMenu();
    enum class Option { COLOR, SIZE, DRAWING, STYLE, ERASER, SELECTION };
    void showToolOptions(GtkWidget* anchor, Option option);
    void dismissOptions();
    void setInterfaceVisible(bool visible);
    bool isEnabled() const { return enabled; }
private:
    struct Item {
        RadialMenu* menu;
        GtkWidget* widget;
        double angle, radius = 0, hover = 0;
        bool hovered = false;
        bool focused = false;
        std::function<void()> click;
    };
    MainWindow* window;
    GtkOverlay* overlay;
    GtkWidget *fixed, *catcher, *hub, *hubImage, *pager;
    GtkWidget* popover = nullptr;
    GSimpleAction* visibility;
    GSimpleAction* summon;
    std::vector<std::unique_ptr<Item>> tools, options;
    std::vector<GObject*> watched;
    guint tick = 0, refreshIdle = 0, popoverCloseIdle = 0;
    gint64 lastFrame = 0;
    bool enabled = true, open = false, expanded = false, settingsOpen = false;
    bool pressed = false, dragged = false, touching = false;
    double progress = 0, settingsProgress = 0;
    double x = -1, y = -1, startX = 0, startY = 0, pressX = 0, pressY = 0;
    double diameter = 500;
    ToolType active = TOOL_NONE;
    ToolType settingsTool = TOOL_NONE;
    int toolPage = 0;
    GdkEventSequence* touchSequence = nullptr;
    std::string configPath;
    GtkWidget* button(std::vector<std::unique_ptr<Item>>& items, const char* label, const char* icon,
                      double angle, std::function<void()> click, bool toggle = true);
    void bind(GtkWidget* widget, Action action, GVariant* target = nullptr);
    void activate(Action action, GVariant* target = nullptr);
    void refresh();
    void updateHubIcon();
    void rebuildOptions();
    void scheduleRefresh();
    void animate();
    bool frame(GdkFrameClock* clock);
    void layout();
    void setOpen(bool value);
    void openAtPointer();
    void setEnabled(bool value);
    void save();
    void closePopover();
    void closePopoverAfterSelection();
    void panel(GtkWidget* anchor, const char* title, std::function<void(GtkWidget*)> populate);
    void colors(GtkWidget* anchor);
    void sizes(GtkWidget* anchor);
    void modes(GtkWidget* anchor);
    void styles(GtkWidget* anchor);
    void erasers(GtkWidget* anchor);
    void selections(GtkWidget* anchor);
    void panelAction(GtkWidget* box, const char* label, Action action, GVariant* target = nullptr,
                     const char* icon = nullptr);
    gboolean hubEvent(GdkEvent* event);
};
