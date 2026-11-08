#include "TracosUndoAction.h"
#include "control/Control.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/XojPage.h"
#include "util/i18n.h"

TracosUndoAction::TracosUndoAction(const PageRef& page, Layer* layer, InsertionOrder originals,
                                 std::vector<Element*> texts):
        UndoAction("TracosUndoAction"), layer(layer), originals(std::move(originals)), texts(std::move(texts)) {
    this->page = page;
    originalRefs = refInsertionOrder(this->originals);
}
bool TracosUndoAction::undo(Control* control) {
    auto* doc = control->getDocument();
    doc->lock();
    for (auto* text: texts) textsOwned.push_back(std::move(layer->removeElement(text).e));
    for (auto& original: originals) layer->insertElement(std::move(original.e), original.pos);
    doc->unlock();
    for (auto* text: texts) page->fireElementChanged(text);
    for (auto& original: originalRefs) page->fireElementChanged(original.e);
    this->undone = true;
    return true;
}
bool TracosUndoAction::redo(Control* control) {
    auto* doc = control->getDocument();
    doc->lock();
    originals = layer->removeElementsAt(originalRefs);
    for (auto& text: textsOwned) layer->addElement(std::move(text));
    textsOwned.clear();
    doc->unlock();
    for (auto& original: originalRefs) page->fireElementChanged(original.e);
    for (auto* text: texts) page->fireElementChanged(text);
    this->undone = false;
    return true;
}
std::string TracosUndoAction::getText() { return _("Handwriting to Text (Tracos)"); }
