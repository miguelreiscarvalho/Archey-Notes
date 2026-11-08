#include "TracosController.h"
#include "TracosLayout.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include "control/Control.h"
#include "control/ToolHandler.h"
#include "control/settings/Settings.h"
#include "gui/MainWindow.h"
#include "gui/XournalView.h"
#include "model/Document.h"
#include "model/Font.h"
#include "model/Layer.h"
#include "model/Stroke.h"
#include "model/Text.h"
#include "model/XojPage.h"
#include "undo/TracosUndoAction.h"
#include "util/XojMsgBox.h"

struct TracosController::Region {
    std::vector<Ink> ink; // immutable source coordinates; converted strokes have no live pointer
    Text* text = nullptr;
    std::string content;
    XojFont font;
    Color color;
    double x = 0, y = 0, width = 0;
    tracos::Box source;
};

struct TracosController::Job {
    std::weak_ptr<TracosController> owner;
    PageRef page;
    Layer* layer;
    std::vector<Ink> ink;
    XojFont font;
    Color color;
    uint64_t revision;
    guint timeout = 0;
    std::string input;
    ~Job() { if (timeout && g_main_context_find_source_by_id(nullptr, timeout)) g_source_remove(timeout); }
};

TracosController::TracosController(Control* control): control(control) {}
TracosController::~TracosController() { reset(); }
void TracosController::notice(const char* message) const {
    if (!control->getWindow()) return;
    auto* widget = control->getWindow()->get("tracosStatus");
    if (GTK_IS_LABEL(widget)) {
        gtk_label_set_text(GTK_LABEL(widget), message);
        gtk_widget_show(widget);
    }
}
void TracosController::cancelRequest() {
    if (cancellation) { g_cancellable_cancel(cancellation); g_clear_object(&cancellation); }
    if (process) { g_subprocess_force_exit(process); g_clear_object(&process); }
}
void TracosController::reset() {
    ++revision;
    if (timer) { g_source_remove(timer); timer = 0; }
    cancelRequest(); pending.clear(); region.reset(); page.reset(); layer = nullptr; writing = false;
}
bool TracosController::setEnabled(bool value) {
    reset();
    const char* worker = g_getenv("XOURNAL_TRACOS_WORKER");
    if (value && (!worker || !g_file_test(worker, G_FILE_TEST_IS_REGULAR))) {
        enabled = false;
        XojMsgBox::showErrorToUser(control->getGtkWindow(), "Motor Tracos ausente. Abra pelo atalho do laboratorio atualizado.");
        return false;
    }
    enabled = value;
    notice(enabled ? "Tracos: caneta texto ligada. Escreva e faca uma pausa." : "Tracos: caneta texto desligada.");
    return enabled;
}
void TracosController::penDown() {
    if (!enabled) return;
    ++revision; writing = true;
    if (timer) { g_source_remove(timer); timer = 0; }
    cancelRequest();
    auto now = g_get_monotonic_time() / 1000;
    if (lastUp && now - lastUp >= 0 && now - lastUp < 4000) {
        gaps.push_back(now - lastUp);
        if (gaps.size() > 12) gaps.erase(gaps.begin());
    }
}
void TracosController::penCancelled() { writing = false; reset(); }
void TracosController::historyChanged() {
    // Insertions from the active native pen are expected; undo, deletion and editing invalidate queued work.
    if (!writing && !applying) {
        bool activeRequest = timer || process || !pending.empty();
        reset();
        if (activeRequest) notice("Tracos: reconhecimento cancelado pela edicao; tinta preservada.");
    }
}
bool TracosController::stillPresent(const PageRef& candidate, Layer* target, const std::vector<Ink>& ink) const {
    auto* doc = control->getDocument();
    bool found = false;
    for (size_t i = 0; i < doc->getPageCount(); ++i) if (doc->getPage(i) == candidate) found = true;
    if (!found || control->getCurrentPage() != candidate || candidate->getSelectedLayer() != target) return false;
    const auto& layers = candidate->getLayers();
    if (std::find(layers.begin(), layers.end(), target) == layers.end()) return false;
    const auto& elements = target->getElements();
    for (const auto& item: ink) {
        if (!item.stroke) continue; // historical ink is owned by the region, not the layer
        auto it = std::find_if(elements.begin(), elements.end(), [&](const auto& e) { return e.get() == item.stroke; });
        if (it == elements.end()) return false;
        const auto& points = item.stroke->getPointVector();
        if (points.size() != item.points.size()) return false;
        for (size_t i = 0; i < points.size(); ++i)
            if (points[i].x != item.points[i].x || points[i].y != item.points[i].y) return false;
    }
    return true;
}
void TracosController::strokeFinished(const PageRef& candidate, Layer* target, Stroke* stroke) {
    writing = false; lastUp = g_get_monotonic_time() / 1000;
    auto* tool = control->getToolHandler()->getActiveTool();
    if (!enabled || tool->getToolType() != TOOL_PEN || tool->getDrawingType() != DRAWING_TYPE_DEFAULT) return;
    if (page != candidate || layer != target) { reset(); page = candidate; layer = target; }
    if (pending.size() >= 256) { reset(); page = candidate; layer = target; }
    size_t points = stroke->getPointCount();
    for (const auto& item: pending) points += item.points.size();
    if (points > 200000) { reset(); notice("Tracos: trecho muito longo; tinta preservada."); return; }
    const auto& nativePoints = stroke->getPointVector();
    if (nativePoints.empty()) return;
    tracos::Box box{nativePoints.front().x,nativePoints.front().y,nativePoints.front().x,nativePoints.front().y};
    for (const auto& p: nativePoints) box.include(p.x,p.y);
    bool lowerLine = region && region->text && region->source.startsBelow(
            box,region->y+region->text->getElementHeight(),region->font.getSize());
    if (region && (lowerLine || !region->source.near(box) || region->ink.size()+pending.size() >= 256)) {
        // A queued batch remains one paragraph; start another region only at a conversion boundary.
        if (pending.empty()) region.reset();
    }
    if (!region) {
        region = std::make_shared<Region>();
        region->x = std::max(0.0,box.left); region->y = std::max(0.0,box.top);
        region->font = control->getSettings()->getFont();
        if (region->font.getSize() <= 0) region->font = XojFont("Arial",12);
        region->color = stroke->getColor(); region->source = box;
        region->width = std::max(24.0,candidate->getWidth()-region->x-24);
    }
    pending.push_back({stroke, nativePoints, lastUp});
    auto sorted = gaps;
    std::sort(sorted.begin(), sorted.end());
    // Adapted from Tracos.RecognitionTiming: a natural pause, never during contact.
    int64_t delay = sorted.empty() ? 1200 : std::clamp<int64_t>(sorted[sorted.size()/2]*2+400, 1000, 2200);
    if (timer) g_source_remove(timer);
    timer = g_timeout_add(static_cast<guint>(delay), +[](gpointer data)->gboolean {
        auto* self = static_cast<TracosController*>(data); self->timer = 0; self->request(); return G_SOURCE_REMOVE;
    }, this);
}
void TracosController::convertSelection() {
    reset();
    auto* selection = control->getWindow()->getXournal()->getSelection();
    if (!selection) { notice("Tracos: selecione os tracos manuscritos primeiro."); return; }
    auto selectedPage = control->getCurrentPage();
    auto* selectedLayer = selectedPage->getSelectedLayer();
    // Clear the native selection to commit any pending move/resize before collecting immutable points.
    std::vector<const Stroke*> strokes;
    for (auto* element: selection->getElementsView()) if (auto* s = dynamic_cast<const Stroke*>(element)) strokes.push_back(s);
    control->clearSelection();
    reset(); page = selectedPage; layer = selectedLayer;
    auto now = g_get_monotonic_time()/1000;
    for (auto* s: strokes) pending.push_back({s, s->getPointVector(), now++});
    request();
}
void TracosController::request() {
    if (writing || pending.empty() || !stillPresent(page, layer, pending)) { reset(); return; }
    const char* worker = g_getenv("XOURNAL_TRACOS_WORKER");
    if (!worker || !g_file_test(worker, G_FILE_TEST_IS_REGULAR)) { notice("Tracos: motor indisponivel; tinta preservada."); reset(); return; }
    if (region && region->text) {
        const auto& elements = layer->getElements();
        auto found = std::find_if(elements.begin(),elements.end(),[&](const auto& e) { return e.get()==region->text; });
        if (found==elements.end() || region->text->getText()!=region->content ||
            region->text->getX()!=region->x || region->text->getY()!=region->y ||
            region->text->getFontSize()!=region->font.getSize() ||
            region->text->getFontName()!=region->font.getName()) { region.reset(); }
    }
    if (!region) {
        region = std::make_shared<Region>();
        const auto& first = pending.front();
        if (first.points.empty()) { reset(); return; }
        auto point = first.points.front();
        region->source = {point.x,point.y,point.x,point.y};
        for (const auto& p: first.points) region->source.include(p.x,p.y);
        region->x = std::max(0.0,region->source.left); region->y = std::max(0.0,region->source.top);
        region->width = std::max(24.0,page->getWidth()-region->x-24);
        region->font = control->getSettings()->getFont();
        if (region->font.getSize()<=0) region->font = XojFont("Arial",12);
        region->color = first.stroke->getColor();
    }
    auto ink = region->ink;
    ink.insert(ink.end(),pending.begin(),pending.end());
    // Typed text is usually much shorter than its handwriting. A new dot/comma beside the
    // displayed end belongs there, rather than to a letter at that X in the original ink.
    if (region->text) {
        auto layout=region->text->createPangoLayout();
        pango_layout_set_text(layout.get(),region->content.c_str(),static_cast<int>(region->content.size()));
        PangoRectangle end;
        pango_layout_index_to_pos(layout.get(),static_cast<int>(region->content.size()),&end);
        double endX=region->x+static_cast<double>(end.x)/PANGO_SCALE;
        double baseline=region->y+static_cast<double>(end.y+pango_layout_get_baseline(layout.get()))/PANGO_SCALE;
        double font=region->font.getSize();
        for (size_t i=region->ink.size();i<ink.size();++i) {
            auto& item=ink[i];
            auto p=item.points.front(); tracos::Box mark{p.x,p.y,p.x,p.y};
            for (const auto& point:item.points) mark.include(point.x,point.y);
            if (mark.right-mark.left<=font*.6 && mark.bottom-mark.top<=font*.8 &&
                mark.left>=endX-2 && mark.left<=endX+font*1.6 &&
                std::abs((mark.top+mark.bottom)/2-baseline)<=font*.65) {
                item.recognitionPoints=item.points;
                double dx=region->source.right+font*.35-mark.left;
                double dy=region->source.bottom-baseline;
                for (auto& point:item.recognitionPoints) { point.x+=dx; point.y+=dy; }
            }
        }
    }
    size_t totalPoints = 0;
    for (const auto& item: ink) totalPoints += item.points.size();
    if (ink.size()>256 || totalPoints>200000) { reset(); notice("Tracos: limite do bloco; tinta preservada."); return; }
    std::ostringstream input; input.imbue(std::locale::classic()); input << std::setprecision(17);
    input << "TRACOS1\t" << ink.size() << '\n';
    for (size_t i = 0; i < ink.size(); ++i) {
        input << i << '\t' << ink[i].points.size() << '\n';
        const auto& points=ink[i].recognitionPoints.empty()?ink[i].points:ink[i].recognitionPoints;
        for (const auto& p: points) input << p.x << '\t' << p.y << '\t' << ink[i].time << '\t' << std::max(0.0,p.z) << '\n';
    }
    GError* error = nullptr;
    process = g_subprocess_new(GSubprocessFlags(G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE), &error, worker, nullptr);
    if (!process) { g_clear_error(&error); notice("Tracos: nao foi possivel iniciar o motor; tinta preservada."); reset(); return; }
    cancellation = g_cancellable_new();
    auto* job = new Job{weak_from_this(), page, layer, std::move(ink), region->font, region->color, revision};
    job->timeout = g_timeout_add_full(G_PRIORITY_DEFAULT,10000,+[](gpointer data)->gboolean {
        g_subprocess_force_exit(G_SUBPROCESS(data)); return G_SOURCE_REMOVE;
    },g_object_ref(process),+[](gpointer data) { g_object_unref(data); });
    notice("Tracos: reconhecendo escrita localmente...");
    job->input = input.str();
    g_subprocess_communicate_utf8_async(process,job->input.c_str(),cancellation,+[](GObject* source,GAsyncResult* result,gpointer data) {
        std::unique_ptr<Job> job(static_cast<Job*>(data));
        gchar* out = nullptr; gchar* err = nullptr; GError* error = nullptr;
        bool ok = g_subprocess_communicate_utf8_finish(G_SUBPROCESS(source),result,&out,&err,&error);
        if (auto self = job->owner.lock()) {
            if (self->process == G_SUBPROCESS(source)) { g_clear_object(&self->process); g_clear_object(&self->cancellation); }
            if (job->revision == self->revision && !self->writing) {
                if (ok && out && g_subprocess_get_successful(G_SUBPROCESS(source))) self->accept(*job,out);
                else { self->notice("Tracos: falha no reconhecimento; tinta preservada."); self->pending.clear(); }
            }
        }
        g_free(out); g_free(err); g_clear_error(&error);
    },job);
}
void TracosController::accept(const Job& job, const char* response) {
    if (!stillPresent(job.page,job.layer,job.ink)) { reset(); return; }
    std::vector<tracos::Word> words;
    std::set<size_t> consumed;
    gchar** lines = g_strsplit(response,"\n",-1);
    for (size_t i=0; lines[i]; ++i) g_strchomp(lines[i]);
    bool valid = lines[0] && g_str_equal(lines[0],"TRACOS1");
    for (size_t i=1; valid && lines[i]; ++i) {
        if (!*lines[i]) continue;
        gchar** parts = g_strsplit(lines[i],"\t",-1);
        if (g_strv_length(parts)!=5 || !g_str_equal(parts[0],"TEXT")) { valid=false; g_strfreev(parts); break; }
        gsize length = 0; guchar* decoded = g_base64_decode(parts[1],&length);
        std::string text(reinterpret_cast<char*>(decoded),length); g_free(decoded);
        char* endX = nullptr; char* endY = nullptr;
        double x = g_ascii_strtod(parts[2],&endX), y = g_ascii_strtod(parts[3],&endY);
        if (text.empty() || text.size()>65536 || !g_utf8_validate(text.c_str(),text.size(),nullptr) ||
            !endX || *endX || !endY || *endY || !std::isfinite(x) || !std::isfinite(y)) valid=false;
        tracos::Box box{x,y,x,y};
        bool firstPoint = true;
        std::vector<size_t> sourceIds;
        gchar** ids = g_strsplit(parts[4],",",-1);
        for (size_t n=0; ids[n]; ++n) {
            char* end = nullptr; auto id = g_ascii_strtoull(ids[n],&end,10);
            if (!*ids[n] || *end || id>=job.ink.size() || !consumed.insert(static_cast<size_t>(id)).second) valid=false;
            else sourceIds.push_back(static_cast<size_t>(id));
            if (id<job.ink.size()) for (const auto& p: job.ink[id].recognitionPoints.empty()?job.ink[id].points:job.ink[id].recognitionPoints) {
                if (firstPoint) { box={p.x,p.y,p.x,p.y}; firstPoint=false; }
                box.include(p.x,p.y);
            }
        }
        g_strfreev(ids);
        if (valid) words.push_back({std::move(text),box,std::move(sourceIds)});
        g_strfreev(parts);
    }
    g_strfreev(lines);
    if (!valid || words.empty()) { pending.clear(); notice("Tracos: sem texto reconhecido; tinta preservada."); return; }
    InsertionOrderRef refs;
    // Never replace a block with an incomplete re-recognition of its historical ink.
    for (size_t i=0;i<region->ink.size();++i) if (!consumed.count(i)) {
        notice("Tracos: revisao incompleta; bloco anterior e tinta preservados."); return;
    }
    for (auto id: consumed) if (job.ink[id].stroke)
        refs.emplace_back(job.ink[id].stroke,job.layer->indexOf(job.ink[id].stroke));
    if (region->text) refs.emplace_back(region->text,job.layer->indexOf(region->text));
    std::vector<ElementPtr> elements;
    std::shared_ptr<Region> nextRegion;
    auto rows=tracos::composeLines(std::move(words));
    std::set<size_t> activeIds;
    for (size_t row=0;row<rows.size();++row) {
        auto next=std::make_shared<Region>();
        next->font=region->font; next->color=region->color;
        next->x=row==0?region->x:std::max(0.0,rows[row].source.left);
        next->y=row==0?region->y:std::max(0.0,rows[row].source.top);
        next->width=std::max(24.0,job.page->getWidth()-next->x-24);
        next->source=rows[row].source;
        auto element=std::make_unique<Text>();
        element->setFont(next->font);element->setColor(next->color);
        element->setX(next->x);element->setY(next->y);
        auto content=rows[row].text();
        if (content.size()>65536) { notice("Tracos: texto muito longo; tinta preservada.");return; }
        auto layout=element->createPangoLayout();
        pango_layout_set_text(layout.get(),content.c_str(),static_cast<int>(content.size()));
        pango_layout_set_width(layout.get(),static_cast<int>(next->width*PANGO_SCALE));
        pango_layout_set_wrap(layout.get(),PANGO_WRAP_WORD_CHAR);
        std::string wrapped;
        for (auto* node=pango_layout_get_lines_readonly(layout.get());node;node=node->next) {
            auto* line=static_cast<PangoLayoutLine*>(node->data);
            auto part=content.substr(line->start_index,line->length);
            while (!part.empty() && (part.back()==' ' || part.back()=='\r' || part.back()=='\n')) part.pop_back();
            if (!wrapped.empty()) wrapped+='\n';
            wrapped+=part;
        }
        element->setText(wrapped);next->text=element.get();next->content=wrapped;
        activeIds.clear();
        for (const auto& word:rows[row].words) for (auto id:word.ids) activeIds.insert(id);
        nextRegion=std::move(next);elements.push_back(std::move(element));
    }
    std::sort(refs.begin(),refs.end());
    std::vector<Element*> created;
    auto* doc = control->getDocument();
    applying = true;
    doc->lock();
    auto originals = job.layer->removeElementsAt(refs);
    for (auto& element:elements) { created.push_back(element.get());job.layer->addElement(std::move(element)); }
    doc->unlock();
    for (auto& original: refs) job.page->fireElementChanged(original.e);
    for (auto* text: created) job.page->fireElementChanged(text);
    control->getUndoRedoHandler()->addUndoAction(std::make_unique<TracosUndoAction>(job.page,job.layer,std::move(originals),std::move(created)));
    applying = false;
    region=std::move(nextRegion);
    std::vector<Ink> remaining;
    for (size_t i=0;i<job.ink.size();++i) {
        auto item=job.ink[i];
        if (activeIds.count(i)) {
            item.stroke=nullptr;
            for (const auto& p:item.recognitionPoints.empty()?item.points:item.recognitionPoints) region->source.include(p.x,p.y);
            region->ink.push_back(std::move(item));
        } else if (!consumed.count(i) && item.stroke) remaining.push_back(std::move(item));
    }
    pending=std::move(remaining);
    notice("Tracos: bloco alinhado. Continue perto da escrita para completar a frase; Ctrl+Z desfaz.");
}
