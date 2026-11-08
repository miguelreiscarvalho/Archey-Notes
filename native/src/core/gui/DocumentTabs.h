#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <gtk/gtk.h>
#include "filesystem.h"
class Control;
class MainWindow;
class GladeSearchpath;

/** Tabbed workspace. Each document retains its native controller, view and undo stack. */
class DocumentTabs {
public:
    DocumentTabs(GtkApplication* app, GladeSearchpath* resources, Control* first, MainWindow* window, bool disableAudio);
    ~DocumentTabs();
    void open(fs::path path = {}, std::function<void(bool)> callback = [](bool){}, int page = -1, bool attach = false);
    void requestQuit(bool allowCancel = true);
    void shutdown();
private:
    struct Page {
        std::unique_ptr<Control> ownedControl;
        std::unique_ptr<MainWindow> ownedWindow;
        Control* control;
        MainWindow* window;
        GtkWidget *root, *bar, *content;
        bool closed = false, hosted = false;
        size_t number = 0;
        int hostX = -1, hostY = -1, hostWidth = -1, hostHeight = -1;
        unsigned geometryUpdates = 0;
        gint64 lastGeometryUpdate = 0;
    };
    GtkApplication* app;
    GladeSearchpath* resources;
    bool disableAudio, busy = false, quitting = false;
    Page* current = nullptr;
    std::vector<std::unique_ptr<Page>> pages;
    guint refreshIdle = 0, inboxTimer = 0;
    Page* add(Control* control, MainWindow* window);
    void select(Page* page);
    void close(Page* page);
    void refresh();
    void scheduleRefresh();
    void updateHost();
    std::string hostState;
    void confirmQuit(size_t index, bool allowCancel);
};
