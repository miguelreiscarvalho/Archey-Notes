#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include <gio/gio.h>
#include "model/PageRef.h"
#include "model/Point.h"
class Control;
class Layer;
class Stroke;

class TracosController: public std::enable_shared_from_this<TracosController> {
public:
    explicit TracosController(Control* control);
    ~TracosController();
    bool setEnabled(bool value);
    bool isEnabled() const { return enabled; }
    void penDown();
    void penCancelled();
    void strokeFinished(const PageRef& page, Layer* layer, Stroke* stroke);
    void historyChanged();
    void reset();
    void convertSelection();
private:
    struct Ink {
        const Stroke* stroke;
        std::vector<Point> points;
        int64_t time;
        std::vector<Point> recognitionPoints; // optional projection from rendered text back to source ink
    };
    struct Job;
    struct Region;
    std::shared_ptr<Region> region;
    Control* control;
    bool enabled = false;
    bool writing = false;
    bool applying = false;
    unsigned timer = 0;
    uint64_t revision = 0;
    PageRef page;
    Layer* layer = nullptr;
    std::vector<Ink> pending;
    GSubprocess* process = nullptr;
    GCancellable* cancellation = nullptr;
    std::vector<int64_t> gaps;
    int64_t lastUp = 0;
    void cancelRequest();
    bool stillPresent(const PageRef& page, Layer* layer, const std::vector<Ink>& ink) const;
    void request();
    void accept(const Job& job, const char* response);
    void notice(const char* message) const;
};
