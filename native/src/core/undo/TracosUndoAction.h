#pragma once
#include <memory>
#include <vector>
#include "model/ElementInsertionPosition.h"
#include "undo/UndoAction.h"
class Control;
class Layer;

// One native history entry owns whichever side of the conversion is not on the page.
class TracosUndoAction final: public UndoAction {
public:
    TracosUndoAction(const PageRef& page, Layer* layer, InsertionOrder originals, std::vector<Element*> texts);
    bool undo(Control* control) override;
    bool redo(Control* control) override;
    std::string getText() override;
private:
    Layer* layer;
    InsertionOrder originals;
    InsertionOrderRef originalRefs;
    std::vector<Element*> texts;
    std::vector<ElementPtr> textsOwned;
};
