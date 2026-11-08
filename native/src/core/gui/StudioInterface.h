#pragma once

#include <string>
#include <vector>
#include <gtk/gtk.h>
#include "control/ToolEnums.h"
#include "enums/Action.enum.h"
#include "RadialMenu.h"

class MainWindow;

/** Compact native chrome. Document input and tool state remain owned by Control. */
class StudioInterface {
public:
    StudioInterface(MainWindow* window, GtkOverlay* overlay, RadialMenu* radial);
    ~StudioInterface();
    bool isModern() const { return mode != "classic"; }
    void applyLayout();
    void updatePageNumbers(size_t page, size_t count);
private:
    struct Context { GtkWidget* widget; ToolCapabilities capability; Action action; };
    MainWindow* window;
    RadialMenu* radial;
    GtkOverlay* overlay;
    GtkWidget *header, *dock, *toolRow, *contextRow, *title, *subtitle, *modePicker;
    GtkWidget *colorButton, *colorImage, *sizeButton, *sizeLabel, *drawingButton, *drawingLabel;
    GtkWidget *selectionButton, *eraserButton, *pageLabel, *zoomLabel, *exportLabel, *saveLabel;
    GtkWidget* selectionTool;
    std::vector<Context> context;
    std::vector<GObject*> watched;
    std::string mode = "studio", configPath;
    guint refreshIdle = 0;
    ToolType active = TOOL_NONE;
    bool updatingPicker = false, compact = false;
    bool classicRadial = true;
    bool preFocusSidebar = true;
    int classicIconTheme = 0;
    size_t currentPage = 0, pageCount = 1;
    GtkWidget* iconButton(const char* label, const char* icon, bool toggle = false);
    GtkWidget* actionButton(GtkWidget* box, const char* label, const char* icon, Action action,
                            GVariant* target = nullptr, bool toggle = false);
    GtkWidget* optionButton(const char* label, const char* icon, RadialMenu::Option option);
    void bind(GtkWidget* widget, Action action, GVariant* target = nullptr);
    void setMode(const std::string& next);
    void refresh();
    void scheduleRefresh();
    void save();
    void resize(int width);
    void updateTitle();
};
