#ifndef CHAT_DOCK_CONSTANTS_H
#define CHAT_DOCK_CONSTANTS_H

#include "core/object/object.h"
#include "core/variant/type_info.h"

// Constants for the ChatDock class modes
enum ChatDockMode {
    CHAT_DOCK_MODE_ASK,
    CHAT_DOCK_MODE_COMPOSER,
};

// Register enum with Godot type system
template <>
struct GetTypeInfo<ChatDockMode> {
    static constexpr Variant::Type VARIANT_TYPE = Variant::INT;
    static constexpr GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE;
    static inline PropertyInfo get_class_info() { return PropertyInfo(VARIANT_TYPE, "ChatDockMode"); }
};

#endif // CHAT_DOCK_CONSTANTS_H 