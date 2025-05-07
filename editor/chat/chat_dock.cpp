#include "chat_dock.h"
#include "core/string/print_string.h"
#include "editor/editor_scale.h"
#include "editor/editor_themes.h"
#include "editor/editor_node.h"
#include "scene/resources/style_box_flat.h"
#include "core/os/time.h"
#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/variant/type_info.h"

// Add template specialization for ChatMode enum to fix compilation
template <>
struct GetTypeInfo<ChatDock::ChatMode> {
    static constexpr Variant::Type VARIANT_TYPE = Variant::INT;
    static constexpr GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE;
    static inline PropertyInfo get_class_info() { return PropertyInfo(VARIANT_TYPE, "ChatMode"); }
};

ChatDock *ChatDock::singleton = nullptr;

void ChatDock::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_send_message"), &ChatDock::_send_message);
    ClassDB::bind_method(D_METHOD("_on_input_text_submitted"), &ChatDock::_on_input_text_submitted);
    ClassDB::bind_method(D_METHOD("add_message", "from", "message"), &ChatDock::add_message);
    ClassDB::bind_method(D_METHOD("_http_request_completed", "status", "code", "headers", "body"), &ChatDock::_http_request_completed);
    ClassDB::bind_method(D_METHOD("_on_mode_changed", "index"), &ChatDock::_on_mode_changed);
    ClassDB::bind_method(D_METHOD("_on_attach_scene_pressed"), &ChatDock::_on_attach_scene_pressed);
    ClassDB::bind_method(D_METHOD("_on_file_selected", "path"), &ChatDock::_on_file_selected);
    ClassDB::bind_method(D_METHOD("_on_apply_changes_pressed"), &ChatDock::_on_apply_changes_pressed);
    
    // Use Class name "ChatDock" rather than static function method
    ClassDB::bind_integer_constant(StringName("ChatDock"), StringName("ChatMode"), StringName("MODE_ASK"), MODE_ASK);
    ClassDB::bind_integer_constant(StringName("ChatDock"), StringName("ChatMode"), StringName("MODE_COMPOSER"), MODE_COMPOSER);
}

void ChatDock::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_THEME_CHANGED: {
            // Apply modern dark theme styling
            Ref<StyleBoxFlat> chat_display_style;
            chat_display_style.instantiate();
            chat_display_style->set_bg_color(get_theme_color(SNAME("base_color"), SNAME("Editor")));
            chat_display_style->set_border_width_all(1);
            chat_display_style->set_border_color(get_theme_color(SNAME("dark_color_2"), SNAME("Editor")));
            chat_display_style->set_corner_radius_all(3);
            chat_display_style->set_corner_radius_all(3);
            chat_display->add_theme_style_override("normal", chat_display_style);

            Ref<StyleBoxFlat> chat_input_style;
            chat_input_style.instantiate();
            chat_input_style->set_bg_color(get_theme_color(SNAME("dark_color_1"), SNAME("Editor")));
            chat_input_style->set_border_width_all(1);
            chat_input_style->set_border_color(get_theme_color(SNAME("dark_color_2"), SNAME("Editor")));
            chat_input_style->set_corner_radius_all(3);
            chat_input->add_theme_style_override("normal", chat_input_style);

            send_button->set_icon(get_theme_icon(SNAME("Forward"), SNAME("EditorIcons")));
            attach_scene_button->set_icon(get_theme_icon(SNAME("PackedScene"), SNAME("EditorIcons")));
            apply_changes_button->set_icon(get_theme_icon(SNAME("Save"), SNAME("EditorIcons")));
        } break;

        case NOTIFICATION_ENTER_TREE: {
            _load_settings();
            _update_ui_for_mode();
            add_message("System", "Welcome to the Godot Chat with Claude AI!");
            add_message("System", "Current mode: " + String(current_mode == MODE_ASK ? "Ask Mode" : "Composer Mode"));
            
            if (api_key.is_empty()) {
                add_message("System", "Please configure your Claude API key in Editor Settings.");
            }
        } break;
    }
}

void ChatDock::_send_message() {
    String message = chat_input->get_text();
    if (!message.is_empty()) {
        add_message("You", message);
        
        // Save to history
        Dictionary user_message;
        user_message["role"] = "user";
        user_message["content"] = message;
        chat_history.push_back(user_message);
        
        chat_input->clear();
        chat_input->grab_focus();
        
        if (!api_key.is_empty()) {
            // Cancel any ongoing requests before starting a new one
            if (http_request->get_http_client_status() != HTTPClient::STATUS_DISCONNECTED) {
                http_request->cancel_request();
                // Small delay to ensure cancellation completes
                OS::get_singleton()->delay_usec(100000); // 100ms delay
            }
            _send_to_claude(message);
        } else {
            add_message("System", "Claude API key is not configured. Please set it in Editor Settings.");
        }
    }
}

void ChatDock::_on_input_text_submitted(const String &p_text) {
    _send_message();
}

void ChatDock::add_message(const String &p_from, const String &p_message) {
    String timestamp = Time::get_singleton()->get_time_string_from_system();
    chat_display->append_text(vformat("[color=#808080][%s][/color] [color=#4B9EF9]%s:[/color] %s\n", timestamp, p_from, p_message));
}

String ChatDock::_prepare_scene_context() {
    String context;
    
    if (attached_scenes.size() > 0) {
        for (int i = 0; i < attached_scenes.size(); i++) {
            Dictionary scene_info = attached_scenes[i];
            String file_path = scene_info["path"];
            String file_content = scene_info["content"];
            
            context += "\n\nScene file: " + file_path.get_file() + "\n```tscn\n" + file_content + "\n```\n";
        }
    }
    
    return context;
}

void ChatDock::_send_to_claude(const String &p_message) {
    ERR_FAIL_COND_MSG(api_key.is_empty(), "Claude API key is not configured");
    
    // Construct the request
    Dictionary request_data;
    request_data["model"] = model_name;
    request_data["max_tokens"] = 4096; // Increased maximum token limit
    
    // Set the system message based on the current mode
    String system_message;
    
    if (current_mode == MODE_ASK) {
        system_message = "You are Claude, an AI assistant integrated into the Godot game engine editor. Your role is to help developers with their Godot and GDScript programming questions. Focus on providing accurate, concise information about Godot features, GDScript syntax, game development concepts, and debugging help. When asked for code, provide working GDScript examples that follow Godot best practices. Format code with appropriate syntax highlighting when possible. Be helpful, technical, and focus on Godot-specific solutions.";
        
        // Add attached scene context if available in Ask mode
        String scene_context = _prepare_scene_context();
        if (!scene_context.is_empty()) {
            system_message += "\n\nThe user has attached the following scene file(s) for context:" + scene_context;
        }
    } else { // MODE_COMPOSER
        system_message = "You are Claude, an AI assistant integrated into the Godot game engine editor in Composer Mode. Your task is to help developers modify their scene files (.tscn) and create GDScript (.gd) code. When the user asks for modifications:\n\n"
                        "1. Provide the COMPLETE modified .tscn file content wrapped in ```tscn and ``` tags. Never truncate or abbreviate the TSCN file.\n"
                        "2. Ensure the .tscn file format is maintained perfectly with proper indentation and structure.\n"
                        "3. Be precise with node paths, properties, and resources.\n\n"
                        "For any GDScript (.gd) files needed:\n\n"
                        "4. FIRST specify the path where the script should be saved by writing 'Path: res://path/to/script.gd' on its own line.\n"
                        "5. THEN immediately after provide the script content wrapped in ```gdscript and ``` tags.\n"
                        "6. Repeat this pattern for each script file that needs to be created or modified.\n"
                        "7. Make sure your GDScript follows Godot best practices and coding standards.\n\n"
                        "IMPORTANT: Your scripts will be automatically applied to the specified paths. Always write the full path preceded by 'Path:' before each script block, as the engine will use this to create/update the files.\n\n"
                        "Your response MUST include the full, complete TSCN file content without omissions. This is critical as all changes will be automatically applied to the user's files.";
        
        // Add attached file content if available in Composer mode
        String scene_context = _prepare_scene_context();
        if (!scene_context.is_empty()) {
            system_message += "\n\nThe user has attached the following scene file(s):" + scene_context;
            
            // In composer mode, we want to emphasize the file that will be modified
            if (!attached_file_path.is_empty()) {
                system_message += "\n\nThe scene file to be modified is: " + attached_file_path.get_file();
            }
        } else if (!attached_file_content.is_empty()) {
            // Legacy support for the previous implementation
            system_message += "\n\nHere is the current .tscn file content:\n```tscn\n" + attached_file_content + "\n```";
        }
    }
    
    request_data["system"] = system_message;
    
    // Add message history for context
    Array messages;
    for (int i = 0; i < chat_history.size(); i++) {
        messages.push_back(chat_history[i]);
    }
    
    request_data["messages"] = messages;
    
    String json_data = JSON::stringify(request_data);
    
    // Set up headers
    Vector<String> headers;
    headers.push_back("x-api-key: " + api_key);
    headers.push_back("anthropic-version: 2023-06-01");
    headers.push_back("content-type: application/json");
    
    // Send the request
    if (current_mode == MODE_ASK) {
        chat_display->append_text("[color=#808080]Waiting for Claude's response...[/color]\n");
    } else {
        // More informative message for Composer Mode with attached file reference
        if (!attached_file_path.is_empty()) {
            chat_display->append_text(vformat("[color=#4B9EF9]🔄 Processing modifications for %s...[/color]\n", attached_file_path.get_file()));
        } else {
            chat_display->append_text("[color=#4B9EF9]🔄 Processing scene modifications... Please wait.[/color]\n");
        }
    }
    
    Error err = http_request->request(api_url, headers, HTTPClient::METHOD_POST, json_data);
    
    if (err != OK) {
        add_message("System", "Error sending request to Claude API: " + itos(err));
    }
}

void ChatDock::_http_request_completed(int p_status, int p_code, const PackedStringArray &headers, const PackedByteArray &p_data) {
    if (p_status != HTTPRequest::RESULT_SUCCESS) {
        add_message("System", "HTTP Request Error: " + itos(p_status));
        return;
    }
    
    if (p_code != 200) {
        add_message("System", "API Error: " + itos(p_code));
        String response_text;
        for (int i = 0; i < p_data.size(); i++) {
            response_text += (char)p_data[i];
        }
        add_message("System", "Error details: " + response_text);
        return;
    }
    
    // Parse JSON response
    String response_text;
    for (int i = 0; i < p_data.size(); i++) {
        response_text += (char)p_data[i];
    }
    
    JSON json;
    Error err = json.parse(response_text);
    
    if (err != OK) {
        add_message("System", "Error parsing JSON response: " + itos(err));
        return;
    }
    
    Variant result = json.get_data();
    
    if (result.get_type() != Variant::DICTIONARY) {
        add_message("System", "Unexpected response format");
        return;
    }
    
    Dictionary response = result;
    _process_claude_response(response);
}

void ChatDock::_process_claude_response(const Dictionary &p_response) {
    if (!p_response.has("content")) {
        add_message("System", "Invalid response format from Claude API");
        return;
    }
    
    Array content = p_response["content"];
    String message_text;
    
    // Extract message text from the content array
    for (int i = 0; i < content.size(); i++) {
        Dictionary item = content[i];
        if (item.has("type") && String(item["type"]) == "text") {
            message_text = item["text"];
            break;
        }
    }
    
    if (message_text.is_empty()) {
        add_message("System", "No text content in Claude response");
        return;
    }
    
    // Process differently based on mode
    if (current_mode == MODE_COMPOSER) {
        // For Composer Mode - extract TSCN content without displaying code
        String tscn_content;
        _extract_tscn_content(message_text, tscn_content);
        
        // Remove all code blocks from the message to display
        String display_message = _remove_code_blocks(message_text);
        
        // Check if we got valid TSCN content
        if (!tscn_content.is_empty()) {
            last_tscn_content = tscn_content;
            apply_changes_button->set_disabled(false);
            
            // Add a success message with file name
            add_message("System", "✅ TSCN code ready to apply to: " + attached_file_path.get_file());
        } else {
            // No TSCN content found
            add_message("System", "⚠️ No valid TSCN content found in Claude's response. Cannot modify scene.");
        }
        
        // Check if we got scripts
        if (script_paths.size() > 0) {
            add_message("System", vformat("✅ Found %d script file(s) ready to apply", script_paths.size()));
        }
        
        // Add explanation about Apply Changes button
        if (!tscn_content.is_empty() || script_paths.size() > 0) {
            add_message("System", "Click 'Apply Changes' to update your files with these modifications.");
        }
        
        // Add Claude's response without any code blocks
        add_message("Claude", display_message);
    } else {
        // For Ask Mode - show the full response including code
        add_message("Claude", message_text);
    }
    
    // Save to history - save the full message including code
    Dictionary assistant_message;
    assistant_message["role"] = "assistant";
    assistant_message["content"] = message_text; // Original with code
    chat_history.push_back(assistant_message);
}

// New helper method to remove code blocks from messages
String ChatDock::_remove_code_blocks(const String &p_text) {
    String result = p_text;
    
    // Remove TSCN code blocks
    int start_pos = result.find("```tscn");
    while (start_pos != -1) {
        int end_pos = result.find("```", start_pos + 7);
        if (end_pos != -1) {
            result = result.substr(0, start_pos) + 
                    "[TSCN code extracted]" + 
                    result.substr(end_pos + 3);
        } else {
            break;
        }
        start_pos = result.find("```tscn", start_pos + 1);
    }
    
    // Remove GDScript code blocks too
    start_pos = result.find("```gdscript");
    while (start_pos != -1) {
        int end_pos = result.find("```", start_pos + 11);
        if (end_pos != -1) {
            result = result.substr(0, start_pos) + 
                    "[GDScript code extracted]" + 
                    result.substr(end_pos + 3);
        } else {
            break;
        }
        start_pos = result.find("```gdscript", start_pos + 1);
    }
    
    // Also check for ```gd blocks
    start_pos = result.find("```gd");
    while (start_pos != -1) {
        int end_pos = result.find("```", start_pos + 5);
        if (end_pos != -1) {
            result = result.substr(0, start_pos) + 
                    "[GDScript code extracted]" + 
                    result.substr(end_pos + 3);
        } else {
            break;
        }
        start_pos = result.find("```gd", start_pos + 1);
    }
    
    return result;
}

void ChatDock::_extract_tscn_content(const String &p_response, String &r_tscn_content) {
    r_tscn_content = "";
    
    // Clear previous script information
    script_paths.clear();
    script_contents.clear();
    
    // Look for content between ```tscn and ``` tags
    int start_pos = p_response.find("```tscn");
    if (start_pos == -1) {
        // Try alternative format
        start_pos = p_response.find("```TSCN");
    }
    
    if (start_pos != -1) {
        start_pos += 7; // Length of ```tscn
        int end_pos = p_response.find("```", start_pos);
        
        if (end_pos != -1) {
            r_tscn_content = p_response.substr(start_pos, end_pos - start_pos).strip_edges();
        }
    }
    
    // Extract script content separately
    _extract_script_content(p_response);
}

void ChatDock::_extract_script_content(const String &p_response) {
    // Extract GDScript files with path information
    // Look for pattern "Path: res://path/to/script.gd" followed by code blocks
    int pos = 0;
    while (true) {
        pos = p_response.find("Path:", pos);
        if (pos == -1) break;
        
        int path_end = p_response.find("\n", pos);
        if (path_end == -1) break;
        
        String script_path = p_response.substr(pos + 5, path_end - pos - 5).strip_edges();
        
        // Find the next code block after this path
        int script_start = p_response.find("```", path_end);
        if (script_start == -1) break;
        
        // Skip the opening tag and find the content
        int content_start = script_start;
        bool is_script = false;
        
        if (p_response.substr(script_start + 3, 8) == "gdscript") {
            content_start = script_start + 11; // Length of "```gdscript"
            is_script = true;
        } else if (p_response.substr(script_start + 3, 2) == "gd") {
            content_start = script_start + 5; // Length of "```gd"
            is_script = true;
        }
        
        if (!is_script) {
            // Not a script block, continue searching
            pos = script_start + 3;
            continue;
        }
        
        // Find the end of the code block
        int script_end = p_response.find("```", content_start);
        if (script_end == -1) break;
        
        String script_content = p_response.substr(content_start, script_end - content_start).strip_edges();
        
        // Add to our script arrays
        script_paths.push_back(script_path);
        script_contents.push_back(script_content);
        
        add_message("System", "📄 Found script: " + script_path);
        
        // Move past this script block
        pos = script_end + 3;
    }
    
    // If no explicit path information, look for standalone script blocks
    if (script_paths.size() == 0) {
        // Look for GDScript content
        pos = 0;
        int script_count = 0;
        
        while (true) {
            // Find script code blocks
            int gd_start = p_response.find("```gdscript", pos);
            int tag_length = 0;
            
            if (gd_start == -1) {
                // Try alternative format
                gd_start = p_response.find("```gd", pos);
                if (gd_start == -1) break;
                
                tag_length = 5; // Length of ```gd
            } else {
                tag_length = 11; // Length of ```gdscript
            }
            
            int content_start = gd_start + tag_length;
            
            // Find the end of the block
            int gd_end = p_response.find("```", content_start);
            if (gd_end == -1) break;
            
            String script_content = p_response.substr(content_start, gd_end - content_start).strip_edges();
            script_count++;
            
            // Generate a default path for the script
            String default_path = _get_default_script_path(script_count);
            
            script_paths.push_back(default_path);
            script_contents.push_back(script_content);
            
            add_message("System", "📄 Generated script will be saved as: " + default_path);
            
            // Move to the next position
            pos = gd_end + 3;
        }
    }
}

String ChatDock::_get_default_script_path(int p_index) {
    // Generate a default path for scripts
    String default_path;
    if (attached_file_path.is_empty()) {
        default_path = "res://generated_script_" + itos(p_index) + ".gd";
    } else {
        // Base the script name on the attached scene
        String scene_name = attached_file_path.get_file().get_basename();
        default_path = attached_file_path.get_base_dir() + "/" + scene_name + "_script_" + itos(p_index) + ".gd";
    }
    return default_path;
}

bool ChatDock::_create_script_file(const String &p_path, const String &p_content) {
    // Create the directory if it doesn't exist
    String dir_path = p_path.get_base_dir();
    if (!dir_path.is_empty()) {
        Ref<DirAccess> dir = DirAccess::create_for_path(dir_path);
        if (dir.is_valid()) {
            dir->make_dir_recursive(dir_path);
        }
    }
    
    // Create a backup if the file already exists
    if (FileAccess::exists(p_path)) {
        String backup_path = p_path + ".backup";
        String existing_content;
        if (_load_file_content(p_path, existing_content)) {
            _save_file_content(backup_path, existing_content);
            add_message("System", "Created backup of existing script: " + backup_path.get_file());
        }
    }
    
    // Save the script file
    return _save_file_content(p_path, p_content);
}

bool ChatDock::_load_file_content(const String &p_path, String &r_content) {
    Error err;
    Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
    
    if (err != OK) {
        add_message("System", "Error opening file: " + itos(err));
        return false;
    }
    
    r_content = f->get_as_text();
    return true;
}

bool ChatDock::_save_file_content(const String &p_path, const String &p_content) {
    Error err;
    Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
    
    if (err != OK) {
        add_message("System", "Error opening file for writing: " + itos(err));
        return false;
    }
    
    f->store_string(p_content);
    return true;
}

void ChatDock::_on_mode_changed(int p_index) {
    current_mode = (ChatMode)p_index;
    _save_settings();
    _update_ui_for_mode();
    
    // Clear chat history when changing modes
    chat_history.clear();
    chat_display->clear();
    
    add_message("System", "Switched to " + String(current_mode == MODE_ASK ? "Ask Mode" : "Composer Mode"));
    
    if (current_mode == MODE_COMPOSER) {
        add_message("System", "Attach a .tscn file to get started with modifications.");
    } else {
        // Don't clear attached scenes when switching to Ask mode
        // This way, users can still reference the scenes in their questions
    }
}

void ChatDock::_on_attach_scene_pressed() {
    file_dialog->popup_centered_ratio(0.7);
}

void ChatDock::_on_file_selected(const String &p_path) {
    String extension = p_path.get_extension().to_lower();
    
    // Check if file is a supported format
    if (extension != "tscn") {
        if (extension == "blend") {
            add_message("System", "Error: .blend files cannot be used directly. Godot needs to import them first.");
            add_message("System", "Please use a .tscn file that has been created from an imported .blend file.");
        } else {
            add_message("System", "Error: Only .tscn files are supported for attachment.");
            add_message("System", "The selected file (" + p_path.get_file() + ") has extension ." + extension + " which cannot be processed.");
        }
        return;
    }
    
    String file_content;
    
    // Try to load the file content with error handling
    if (!_load_file_content(p_path, file_content)) {
        add_message("System", "Failed to load file: " + p_path.get_file());
        return;
    }
    
    // Validate the content is proper TSCN format
    if (!file_content.begins_with("[gd_scene") && !file_content.begins_with("[gd_resource")) {
        add_message("System", "Error: The file does not appear to be a valid TSCN file.");
        add_message("System", "Please select a different file.");
        return;
    }
    
    // Create a dictionary to store scene information
    Dictionary scene_info;
    scene_info["path"] = p_path;
    scene_info["content"] = file_content;
    
    // Check if this scene is already attached
    bool already_attached = false;
    for (int i = 0; i < attached_scenes.size(); i++) {
        Dictionary existing = attached_scenes[i];
        if (existing["path"] == p_path) {
            already_attached = true;
            attached_scenes.remove_at(i); // Remove and replace with updated content
            break;
        }
    }
    
    // Add to attached scenes
    attached_scenes.push_back(scene_info);
    
    // In composer mode, also set as the active file to modify
    if (current_mode == MODE_COMPOSER) {
        attached_file_path = p_path;
        attached_file_content = file_content;
    }
    
    add_message("System", "✅ Attached scene: " + p_path.get_file() + (already_attached ? " (updated)" : ""));
    
    // In composer mode, provide immediate guidance
    if (current_mode == MODE_COMPOSER) {
        add_message("System", "Now you can ask Claude to modify this scene. Be specific about what changes you want.");
    }
}

void ChatDock::_on_apply_changes_pressed() {
    if (attached_file_path.is_empty()) {
        add_message("System", "Error: No file is attached. Please attach a TSCN file first.");
        return;
    }
    
    bool changes_applied = false;
    
    // 1. Apply TSCN changes if available
    if (!last_tscn_content.is_empty()) {
        // Validate the TSCN content before saving
        if (!last_tscn_content.begins_with("[gd_scene") && !last_tscn_content.begins_with("[gd_resource")) {
            add_message("System", "Error: The generated TSCN content does not appear to be valid.");
            add_message("System", "Please try again with a simpler modification request.");
        } else {
            // Ensure the file still exists and is accessible
            if (!FileAccess::exists(attached_file_path)) {
                add_message("System", "Error: The file " + attached_file_path.get_file() + " no longer exists or is inaccessible.");
                return;
            }
            
            // Create a backup of the original file
            String backup_path = attached_file_path + ".backup";
            if (_load_file_content(attached_file_path, attached_file_content)) {
                _save_file_content(backup_path, attached_file_content);
                add_message("System", "Created backup of original file at: " + backup_path.get_file());
            }
            
            // Try to save the new content
            bool tscn_success = false;
            
            // Try multiple times with error handling
            for (int attempt = 0; attempt < 3; attempt++) {
                Error err = OK;
                Ref<FileAccess> f = FileAccess::open(attached_file_path, FileAccess::WRITE, &err);
                
                if (err != OK) {
                    if (attempt < 2) {
                        // Wait a bit and try again
                        OS::get_singleton()->delay_usec(500000); // 500ms delay
                        continue;
                    } else {
                        add_message("System", "Error: Failed to open file for writing: " + attached_file_path.get_file());
                        add_message("System", "Is the file currently open in an editor? Please close it and try again.");
                        return;
                    }
                }
                
                f->store_string(last_tscn_content);
                tscn_success = true;
                break;
            }
            
            if (tscn_success) {
                add_message("System", "✅ Changes successfully applied to " + attached_file_path.get_file());
                changes_applied = true;
                
                // Update the attached file content with the new content
                attached_file_content = last_tscn_content;
                
                // Update the content in attached_scenes array
                for (int i = 0; i < attached_scenes.size(); i++) {
                    Dictionary scene_info = attached_scenes[i];
                    if (scene_info["path"] == attached_file_path) {
                        scene_info["content"] = last_tscn_content;
                        attached_scenes.set(i, scene_info);
                        break;
                    }
                }
            } else {
                add_message("System", "Error: Failed to apply changes to " + attached_file_path.get_file());
            }
        }
    }
    
    // 2. Apply GDScript changes if available
    int scripts_created = 0;
    if (script_paths.size() > 0 && script_contents.size() == script_paths.size()) {
        for (int i = 0; i < script_paths.size(); i++) {
            String script_path = script_paths[i];
            String script_content = script_contents[i];
            
            // Convert relative paths to project paths if needed
            if (!script_path.begins_with("res://")) {
                // Assume it's relative to the scene
                script_path = attached_file_path.get_base_dir() + "/" + script_path;
            }
            
            // Create the script file
            if (_create_script_file(script_path, script_content)) {
                add_message("System", "✅ Created/updated script: " + script_path.get_file());
                scripts_created++;
                changes_applied = true;
            } else {
                add_message("System", "❌ Failed to save script: " + script_path);
            }
        }
    }
    
    // Final status message
    if (changes_applied) {
        // Show summary of changes
        if (scripts_created > 0) {
            add_message("System", vformat("Created/updated %d script files.", scripts_created));
        }
        
        add_message("System", "To see the changes in the editor, close and reopen the scene.");
        
        // Disable the apply button until next change
        apply_changes_button->set_disabled(true);
        
        // Clear script data after successful application
        script_paths.clear();
        script_contents.clear();
        last_tscn_content = "";
    } else {
        add_message("System", "No changes were applied. Make sure Claude generated valid content.");
    }
}

void ChatDock::_update_ui_for_mode() {
    // Update UI based on current mode
    if (current_mode == MODE_ASK) {
        chat_input->set_placeholder(TTR("Ask Claude about Godot or GDScript..."));
        attach_scene_button->set_tooltip_text(TTR("Attach Scene for Context"));
        apply_changes_button->set_visible(false);
    } else { // MODE_COMPOSER
        chat_input->set_placeholder(TTR("Ask Claude to modify your TSCN file..."));
        attach_scene_button->set_tooltip_text(TTR("Attach TSCN File to Modify"));
        apply_changes_button->set_visible(true);
        apply_changes_button->set_disabled(true); // Disabled until we have TSCN content to apply
    }
    
    // Scene attachment button is now always visible in both modes
    attach_scene_button->set_visible(true);
}

void ChatDock::_load_settings() {
    // Load settings from EditorSettings
    Ref<EditorSettings> editor_settings = EditorSettings::get_singleton();
    
    if (editor_settings.is_valid()) {
        // Register settings if they don't exist
        if (!editor_settings->has_setting("chat/api_key")) {
            editor_settings->set_setting("chat/api_key", "");
            editor_settings->set_initial_value("chat/api_key", "", false);
            editor_settings->add_property_hint(PropertyInfo(Variant::STRING, "chat/api_key", PROPERTY_HINT_PASSWORD));
        }
        
        if (!editor_settings->has_setting("chat/api_url")) {
            editor_settings->set_setting("chat/api_url", "https://api.anthropic.com/v1/messages");
            editor_settings->set_initial_value("chat/api_url", "https://api.anthropic.com/v1/messages", false);
        }
        
        if (!editor_settings->has_setting("chat/model_name")) {
            editor_settings->set_setting("chat/model_name", "claude-3-sonnet-20240229");
            editor_settings->set_initial_value("chat/model_name", "claude-3-sonnet-20240229", false);
        }
        
        if (!editor_settings->has_setting("chat/current_mode")) {
            editor_settings->set_setting("chat/current_mode", (int)MODE_ASK);
            editor_settings->set_initial_value("chat/current_mode", (int)MODE_ASK, false);
        }
        
        // Load settings
        api_key = editor_settings->get_setting("chat/api_key");
        api_url = editor_settings->get_setting("chat/api_url");
        model_name = editor_settings->get_setting("chat/model_name");
        current_mode = (ChatMode)(int)editor_settings->get_setting("chat/current_mode");
        
        // Update the mode selector
        if (mode_selector) {
            mode_selector->select(current_mode);
        }
    }
}

void ChatDock::_save_settings() {
    // Save settings to EditorSettings
    Ref<EditorSettings> editor_settings = EditorSettings::get_singleton();
    
    if (editor_settings.is_valid()) {
        editor_settings->set_setting("chat/api_key", api_key);
        editor_settings->set_setting("chat/api_url", api_url);
        editor_settings->set_setting("chat/model_name", model_name);
        editor_settings->set_setting("chat/current_mode", (int)current_mode);
        editor_settings->save();
    }
}

ChatDock::ChatDock() {
    singleton = this;
    set_name("Chat");
    set_v_size_flags(SIZE_EXPAND_FILL);
    set_h_size_flags(SIZE_EXPAND_FILL);
    
    // Mode selector
    HBoxContainer *mode_container = memnew(HBoxContainer);
    add_child(mode_container);
    
    mode_selector = memnew(OptionButton);
    mode_selector->add_item("Ask Mode", MODE_ASK);
    mode_selector->add_item("Composer Mode", MODE_COMPOSER);
    mode_selector->connect("item_selected", callable_mp(this, &ChatDock::_on_mode_changed));
    mode_container->add_child(mode_selector);
    
    // Spacer
    Control *spacer = memnew(Control);
    spacer->set_h_size_flags(SIZE_EXPAND_FILL);
    mode_container->add_child(spacer);
    
    // Attach scene button (now visible in both modes)
    attach_scene_button = memnew(Button);
    attach_scene_button->set_tooltip_text(TTR("Attach Scene"));
    attach_scene_button->set_flat(true);
    attach_scene_button->connect("pressed", callable_mp(this, &ChatDock::_on_attach_scene_pressed));
    mode_container->add_child(attach_scene_button);
    
    // Apply changes button (only visible in Composer mode)
    apply_changes_button = memnew(Button);
    apply_changes_button->set_text(TTR("Apply Changes"));
    apply_changes_button->set_tooltip_text(TTR("Apply Changes to TSCN File"));
    apply_changes_button->connect("pressed", callable_mp(this, &ChatDock::_on_apply_changes_pressed));
    apply_changes_button->set_disabled(true);
    apply_changes_button->set_visible(false);
    mode_container->add_child(apply_changes_button);
    
    // File dialog for selecting TSCN files
    file_dialog = memnew(FileDialog);
    file_dialog->set_file_mode(FileDialog::FILE_MODE_OPEN_FILE);
    file_dialog->set_access(FileDialog::ACCESS_FILESYSTEM);
    file_dialog->add_filter("*.tscn", "Scene Files");
    file_dialog->connect("file_selected", callable_mp(this, &ChatDock::_on_file_selected));
    add_child(file_dialog);
    
    // Chat display area with scroll container
    scroll_container = memnew(ScrollContainer);
    scroll_container->set_v_size_flags(SIZE_EXPAND_FILL);
    scroll_container->set_h_size_flags(SIZE_EXPAND_FILL);
    add_child(scroll_container);
    
    chat_display = memnew(RichTextLabel);
    chat_display->set_v_size_flags(SIZE_EXPAND_FILL);
    chat_display->set_h_size_flags(SIZE_EXPAND_FILL);
    chat_display->set_use_bbcode(true);
    chat_display->set_selection_enabled(true);
    chat_display->set_context_menu_enabled(true);
    chat_display->set_scroll_follow(true);
    chat_display->set_custom_minimum_size(Size2(200 * EDSCALE, 0));
    scroll_container->add_child(chat_display);
    
    // Input container
    HBoxContainer *input_container = memnew(HBoxContainer);
    input_container->set_h_size_flags(SIZE_EXPAND_FILL);
    add_child(input_container);
    
    // Chat input
    chat_input = memnew(LineEdit);
    chat_input->set_h_size_flags(SIZE_EXPAND_FILL);
    chat_input->set_placeholder(TTR("Ask Claude about Godot or GDScript..."));
    chat_input->connect("text_submitted", callable_mp(this, &ChatDock::_on_input_text_submitted));
    input_container->add_child(chat_input);
    
    // Send button
    send_button = memnew(Button);
    send_button->set_flat(true);
    send_button->connect("pressed", callable_mp(this, &ChatDock::_send_message));
    input_container->add_child(send_button);
    
    // Set up HTTP request
    http_request = memnew(HTTPRequest);
    http_request->set_use_threads(true);
    http_request->connect("request_completed", callable_mp(this, &ChatDock::_http_request_completed));
    add_child(http_request);
    
    // Initialize variables
    current_mode = MODE_ASK;
    attached_file_path = "";
    attached_file_content = "";
    last_tscn_content = "";
    
    // Initialize arrays
    chat_history = Array();
    attached_scenes = Array();
}

ChatDock::~ChatDock() {
    if (singleton == this) {
        singleton = nullptr;
    }
} 