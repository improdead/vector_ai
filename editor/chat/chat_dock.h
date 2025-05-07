#ifndef CHAT_DOCK_H
#define CHAT_DOCK_H

#include "core/object/class_db.h"
#include "scene/gui/box_container.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/button.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/file_dialog.h"
#include "scene/main/http_request.h"
#include "editor/editor_settings.h"
#include "core/io/json.h"
#include "core/config/project_settings.h"
#include "editor/chat/chat_dock_constants.h"

class ChatDock : public VBoxContainer {
    GDCLASS(ChatDock, VBoxContainer);

public:
    enum ChatMode {
        MODE_ASK = CHAT_DOCK_MODE_ASK,
        MODE_COMPOSER = CHAT_DOCK_MODE_COMPOSER
    };

private:
    static ChatDock *singleton;
    RichTextLabel *chat_display;
    LineEdit *chat_input;
    Button *send_button;
    ScrollContainer *scroll_container;
    HTTPRequest *http_request;
    OptionButton *mode_selector;
    Button *attach_scene_button;
    Button *apply_changes_button;
    FileDialog *file_dialog;
    
    String api_key;
    String api_url;
    String model_name;
    
    ChatMode current_mode;
    String attached_file_path;
    String attached_file_content;
    String last_tscn_content;
    
    Array chat_history;
    Array attached_scenes;
    
    // For storing script information
    Array script_paths;    // Paths to GDScript files to create/update
    Array script_contents; // Contents of GDScript files to create/update

    void _send_message();
    void _on_input_text_submitted(const String &p_text);
    void _notification(int p_what);
    void _http_request_completed(int p_status, int p_code, const PackedStringArray &headers, const PackedByteArray &p_data);
    void _send_to_claude(const String &p_message);
    void _process_claude_response(const Dictionary &p_response);
    void _load_settings();
    void _save_settings();
    void _on_mode_changed(int p_index);
    void _on_attach_scene_pressed();
    void _on_file_selected(const String &p_path);
    void _on_apply_changes_pressed();
    void _update_ui_for_mode();
    void _extract_tscn_content(const String &p_response, String &r_tscn_content);
    String _remove_code_blocks(const String &p_text);
    bool _load_file_content(const String &p_path, String &r_content);
    bool _save_file_content(const String &p_path, const String &p_content);
    String _prepare_scene_context();
    void _extract_script_content(const String &p_response);
    bool _create_script_file(const String &p_path, const String &p_content);
    String _get_default_script_path(int p_index);

protected:
    static void _bind_methods();

public:
    static ChatDock *get_singleton() { return singleton; }
    void add_message(const String &p_from, const String &p_message);
    
    ChatDock();
    ~ChatDock();
};

#endif // CHAT_DOCK_H 