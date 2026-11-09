#pragma once

#include <optional>
#include <string_view>
#include <gio/gio.h>

namespace xoj::ui {
// Chrome actions belong to their window widgets, rather than the document ActionDatabase.
// Share parameter metadata with menu validation so unknown actions still fail the test.
inline auto windowActionParameterType(std::string_view name) -> std::optional<const GVariantType*> {
    if (name == "interface-mode") return G_VARIANT_TYPE_STRING;
    if (name == "radial-menu" || name == "radial-menu-at-pointer" || name == "scribble-erase" ||
        name == "document-tab-close" || name == "document-tab-next")
        return std::optional<const GVariantType*>(static_cast<const GVariantType*>(nullptr));
    return std::nullopt;
}
}
