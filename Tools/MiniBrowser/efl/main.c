/*
 * Copyright (C) 2012 Samsung Electronics
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "EWebKit2.h"
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Ecore_Getopt.h>
#include <Eet.h>
#include <Eina.h>
#include <Elementary.h>
#include <Evas.h>

extern int efreet_cache_update;

static const char DEFAULT_URL[] = "http://www.google.com/";
static const char APP_NAME[] = "EFL MiniBrowser";
static const int TOOL_BAR_ICON_SIZE = 24;
static const int TOOL_BAR_BUTTON_SIZE = 32;
static const int SEARCH_FIELD_SIZE = 200;
static const int SEARCH_BUTTON_SIZE = 30;
static const int MAX_SEARCH_COUNT = 1000;
static const double TOOLTIP_DELAY_SECONDS = 1.0;
static const double LONGPRESS_INTERVAL_SECONDS = 1.5;
static const double LIST_ITEM_HEIGHT = 24.35;

static Eina_List *windows = NULL;
static char *evas_engine_name = NULL;
static char *user_agent_string = NULL;
static char *extensions_path = NULL;
static char *background_color_string = NULL;
static Eina_Bool encoding_detector_enabled = EINA_FALSE;
static Eina_Bool frame_flattening_enabled = EINA_FALSE;
static Eina_Bool local_storage_enabled = EINA_TRUE;
static Eina_Bool fullscreen_enabled = EINA_FALSE;
static Eina_Bool spell_checking_enabled = EINA_FALSE;
static Eina_Bool web_security_enabled = EINA_TRUE;
static Eina_Bool touch_events_enabled = EINA_FALSE;
static Eina_Bool fixed_layout_enabled = EINA_TRUE;
static Eina_Bool separated_process_enabled = EINA_FALSE;
static Eina_Bool longpress_enabled = EINA_FALSE;
static int window_width = 800;
static int window_height = 600;
static int search_flags = EWK_FIND_OPTIONS_SHOW_HIGHLIGHT | EWK_FIND_OPTIONS_WRAP_AROUND | EWK_FIND_OPTIONS_CASE_INSENSITIVE;
/* Default value of device_pixel_ratio is '0' so that we don't set custom device
 * scale factor unless it's required by the User. */
static double device_pixel_ratio = 0;
static Eina_Bool legacy_behavior_enabled = EINA_FALSE;

#define DEFAULT_ZOOM_LEVEL 5 // Set default zoom level to 1.0 (index 5 on zoomLevels).
// The zoom values are chosen to be like in Mozilla Firefox 3.
const static float zoomLevels[] = {0.3, 0.5, 0.67, 0.8, 0.9, 1.0, 1.1, 1.2, 1.33, 1.5, 1.7, 2.0, 2.4, 3.0};
static int _log_domain_id = -1;

#define INFO(...) EINA_LOG_DOM_INFO(_log_domain_id, __VA_ARGS__)
#define ERROR(...) EINA_LOG_DOM_ERR(_log_domain_id, __VA_ARGS__)

static Eina_Bool
zoom_level_set(Evas_Object *webview, int level)
{
    if (level < 0  || level >= sizeof(zoomLevels) / sizeof(float))
        return EINA_FALSE;

    return ewk_view_page_zoom_set(webview, zoomLevels[level]);
}

static Ewk_View_Smart_Class *miniBrowserViewSmartClass()
{
    static Ewk_View_Smart_Class ewkViewClass = EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION("MiniBrowser_View");
    return &ewkViewClass;
}

typedef struct _Tooltip_Information {
    Ecore_Timer *show_timer;
    Eina_Bool activated;
    Eina_Bool text_set;
    Eina_Bool shown;
} Tooltip_Information;

typedef struct _Color_Selector {
    Ewk_Color_Picker *ewk_picker;
    Evas_Object *elm_selector;
    Evas_Object *elm_selector_window;
} Color_Selector;

typedef struct _Browser_Window {
    Evas_Object *elm_window;
    Evas_Object *ewk_view;
    Evas_Object *horizontal_layout;
    Evas_Object *vertical_layout;
    Evas_Object *url_bar;
    Evas_Object *back_button;
    Evas_Object *forward_button;
    struct {
        Evas_Object *elm_menu;
        Ewk_Popup_Menu *ewk_menu;
    } popup;
    struct {
        Evas_Object *search_bar;
        Evas_Object *search_field;
        Evas_Object *search_field_count;
        Evas_Object *backward_button;
        Evas_Object *forward_button;
        Evas_Object *search_case_check_box;
        Evas_Object *search_word_start_check_box;
        Evas_Object *close_button;
    } search;
    struct {
        Evas_Object *history_box;
        Evas_Object *history_list;
        Eina_List *history_list_items;
    } history;
    int current_zoom_level; 
    Tooltip_Information tooltip;
    Color_Selector color_selector;
    struct {
        Evas_Object *elm_menu;
        Ewk_Context_Menu *ewk_menu;
    } context_menu;
} Browser_Window;

typedef struct _File_Selector_Data {
    Browser_Window* parent;
    Evas_Object *elm_window;
    Ewk_File_Chooser_Request *request;
} File_Selector_Data;

typedef struct _Auth_Data {
    Evas_Object *popup;
    Ewk_Auth_Request *request;
    Evas_Object *username_entry;
    Evas_Object *password_entry;
} Auth_Data;

static const Ecore_Getopt options = {
    "MiniBrowser",
    "%prog [options] [url]",
    "0.0.1",
    "(C)2012 Samsung Electronics\n (C)2012 Intel Corporation\n",
    "",
    "Test Web Browser using the Enlightenment Foundation Libraries (EFL) port of WebKit2",
    EINA_TRUE, {
        ECORE_GETOPT_STORE_STR
            ('e', "engine", "Ecore-evas engine to use."),
        ECORE_GETOPT_STORE_STR
            ('s', "window-size", "Window size in following format (width)x(height)."),
        ECORE_GETOPT_STORE_STR
            ('u', "user-agent", "User agent to set."),
        ECORE_GETOPT_STORE_STR
            ('x', "extensions-path", "The path which extensions are stored."),
        ECORE_GETOPT_STORE_DOUBLE
            ('r', "device-pixel-ratio", "Ratio between the CSS units and device pixels."),
        ECORE_GETOPT_CALLBACK_NOARGS
            ('E', "list-engines", "List ecore-evas engines.",
             ecore_getopt_callback_ecore_evas_list_engines, NULL),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('c', "encoding-detector", "Enable/disable encoding detector.", EINA_FALSE),
        ECORE_GETOPT_STORE_STR
            ('C', "background-color", "Background color of page. ex) -C=255:255:255:255"),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('f', "flattening", "Enable/disable frame flattening.", EINA_FALSE),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('l', "local-storage", "Enable/disable HTML5 local storage.", EINA_TRUE),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('F', "full-screen", "Start in full-screen.", EINA_FALSE),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('t', "text-checking", "Enable/disable text spell checking.", EINA_FALSE),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('T', "touch-events", "Enable/disable touch events.", EINA_FALSE),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('L', "fixed-layout", "Enable/disable fixed layout.", EINA_TRUE),
        ECORE_GETOPT_STORE_DEF_STR
            ('p', "policy-cookies", "Cookies policy:\n  always - always accept,\n  never - never accept,\n  no-third-party - don't accept third-party cookies.", "no-third-party"),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('w', "web-security", "enable/disable web security.", EINA_TRUE),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('S', "separate-process", "Create new window in separated web process.", EINA_FALSE),
        ECORE_GETOPT_VERSION
            ('V', "version"),
        ECORE_GETOPT_COPYRIGHT
            ('R', "copyright"),
        ECORE_GETOPT_HELP
            ('h', "help"),
        ECORE_GETOPT_SENTINEL
    }
};

static Eina_Stringshare *show_file_entry_dialog(Browser_Window *window, const char *label_tag, const char *default_text);
static Browser_Window *window_create(Ewk_View_Configuration* configuration, int width, int height);

static Browser_Window *window_find_with_elm_window(Evas_Object *elm_window)
{
    Eina_List *l;
    void *data;

    if (!elm_window)
        return NULL;

    EINA_LIST_FOREACH(windows, l, data) {
        Browser_Window *window = (Browser_Window *)data;
        if (window->elm_window == elm_window)
            return window;
    }
    return NULL;
}

static Browser_Window *window_find_with_ewk_view(Evas_Object *ewk_view)
{
    Eina_List *l;
    void *data;

    if (!ewk_view)
        return NULL;

    EINA_LIST_FOREACH(windows, l, data) {
        Browser_Window *window = (Browser_Window *)data;
        if (window->ewk_view == ewk_view)
            return window;
    }
    return NULL;
}

static Eina_Bool
on_tooltip_show(void *user_data)
{
    Browser_Window *window = (Browser_Window *)user_data;

    window->tooltip.show_timer = NULL;
    elm_object_tooltip_show(window->elm_window);
    window->tooltip.shown = EINA_TRUE;
    return ECORE_CALLBACK_CANCEL;
}

static void
window_tooltip_hide(Browser_Window *window)
{
    if (window->tooltip.show_timer) {
        ecore_timer_del(window->tooltip.show_timer);
        window->tooltip.show_timer = NULL;
    }

    if (window->tooltip.shown) {
        elm_object_tooltip_hide(window->elm_window);
        window->tooltip.shown = EINA_FALSE;
    }
}

static void
window_tooltip_update(Browser_Window *window)
{
    window_tooltip_hide(window);

    if (window->tooltip.activated && window->tooltip.text_set)
        window->tooltip.show_timer = ecore_timer_add(TOOLTIP_DELAY_SECONDS, on_tooltip_show, window);
}

static void
on_mouse_in(void *user_data, Evas *e, Evas_Object *ewk_view, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    window->tooltip.activated = EINA_TRUE;
    window_tooltip_update(window);
}

static void
on_mouse_move(void *user_data, Evas *e, Evas_Object *ewk_view, void *event_info)
{
    window_tooltip_update((Browser_Window *)user_data);
}

static void
on_mouse_out(void *user_data, Evas *e, Evas_Object *ewk_view, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    window->tooltip.activated = EINA_FALSE;
    window_tooltip_update(window);
}

static void
on_mouse_wheel(void *user_data, Evas *e, Evas_Object *ewk_view, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    const Evas_Modifier *mod = evas_key_modifier_get(e);
    Evas_Event_Mouse_Wheel *ev = (Evas_Event_Mouse_Wheel *)event_info;
    Eina_Bool shiftPressed = evas_key_modifier_is_set(mod, "Shift");
    Eina_Bool ctrlPressed = evas_key_modifier_is_set(mod, "Control");

    if (!shiftPressed && !ctrlPressed)
        return;

    /* navigate history or zoom web page based on mouse wheel scroll action with shift or control key */
    if (shiftPressed) {
        if (ev->z == -1 && ewk_view_forward_possible(ewk_view)) {
            ewk_view_forward(ewk_view);
            elm_object_disabled_set(window->forward_button, !ewk_view_forward_possible(ewk_view));
        } else if (ev->z == 1 && ewk_view_back_possible(ewk_view)) {
            ewk_view_back(ewk_view);
            elm_object_disabled_set(window->back_button, !ewk_view_back_possible(ewk_view));       
        }
    } else if (ctrlPressed) {
        if (ev->z == -1 && zoom_level_set(ewk_view, window->current_zoom_level + 1)) {
            window->current_zoom_level++;
            INFO("Zoom in (Ctrl + 'scroll up') was pressed, zoom level became %.2f", zoomLevels[window->current_zoom_level]);
        } else if (ev->z == 1 && zoom_level_set(ewk_view, window->current_zoom_level - 1)) {
            window->current_zoom_level--;
            INFO("Zoom out (Ctrl + 'scroll down') was pressed, zoom level became %.2f", zoomLevels[window->current_zoom_level]);
        }
    }
}

static void
on_window_resize(void *user_data, Evas *e, Evas_Object *elm_window, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    if (!window) {
        ERROR("Window is NULL.");
        return;
    }

    if (window->context_menu.ewk_menu)
        ewk_context_menu_hide(window->context_menu.ewk_menu);
    if (window->popup.ewk_menu)
        ewk_popup_menu_close(window->popup.ewk_menu);
    if (window->popup.elm_menu)
        elm_menu_close(window->popup.elm_menu);
}

static void
update_view_favicon(Browser_Window *window, Evas_Object *icon)
{
    /* Remove previous icon from URL bar */
    Evas_Object *old_icon = elm_object_part_content_unset(window->url_bar, "icon");
    if (old_icon) {
        evas_object_unref(old_icon);
        evas_object_del(old_icon);
    }

    /* Show new icon in URL bar */
    if (icon) {
        evas_object_size_hint_min_set(icon, 32, 32);
        elm_object_part_content_set(window->url_bar, "icon", icon);
        evas_object_ref(icon);
    }
}

static void
on_icon_changed_cb(Ewk_Favicon_Database *database, const char *url, void *user_data)
{
    Browser_Window *window = (Browser_Window *)user_data;
    Evas_Object *ewk_view = window->ewk_view;

    if (strcmp(url, ewk_view_url_get(ewk_view)))
        return;

    Evas_Object *favicon = ewk_favicon_database_icon_get(database, url, evas_object_evas_get(ewk_view));
    update_view_favicon(window, favicon);

    if (favicon)
        evas_object_unref(favicon);
}

static void window_free(Browser_Window *window)
{
    evas_object_event_callback_del(window->ewk_view, EVAS_CALLBACK_MOUSE_IN, on_mouse_in);
    evas_object_event_callback_del(window->ewk_view, EVAS_CALLBACK_MOUSE_OUT, on_mouse_out);
    evas_object_event_callback_del(window->ewk_view, EVAS_CALLBACK_MOUSE_MOVE, on_mouse_move);
    evas_object_event_callback_del(window->ewk_view, EVAS_CALLBACK_MOUSE_WHEEL, on_mouse_wheel);

    evas_object_event_callback_del(window->elm_window, EVAS_CALLBACK_RESIZE, on_window_resize);

    ewk_favicon_database_icon_change_callback_del(ewk_context_favicon_database_get(ewk_view_context_get(window->ewk_view)), on_icon_changed_cb);

    evas_object_del(window->ewk_view);
    /* The elm_win will take care of freeing its children */
    evas_object_del(window->elm_window);

    if (window->tooltip.show_timer)
        ecore_timer_del(window->tooltip.show_timer);

    if (window->color_selector.elm_selector_window)
        evas_object_del(window->color_selector.elm_selector_window);

    free(window);
}

static void window_close(Browser_Window *window)
{
    windows = eina_list_remove(windows, window);
    window_free(window);

    if (!windows)
        elm_exit();
}

static void
search_icon_show(Browser_Window *window)
{
    Evas_Object *icon = elm_icon_add(window->elm_window);
    elm_icon_standard_set(icon, "edit-find");
    elm_object_part_content_set(window->search.search_field, "icon", icon);
    evas_object_size_hint_min_set(icon, 20, 20);
    evas_object_size_hint_max_set(icon, 20, 20);
    elm_entry_icon_visible_set(window->search.search_field, EINA_TRUE);
    evas_object_show(icon);
}

static void
search_box_show(Browser_Window *window)
{
    evas_object_size_hint_min_set(window->search.search_bar, SEARCH_FIELD_SIZE + 2 * SEARCH_BUTTON_SIZE, SEARCH_BUTTON_SIZE);

    search_icon_show(window);
    evas_object_show(window->search.search_bar);
    evas_object_show(window->search.search_field);
    evas_object_show(window->search.search_field_count);
    evas_object_show(window->search.backward_button);
    evas_object_show(window->search.forward_button);
    evas_object_show(window->search.search_case_check_box);
    evas_object_show(window->search.search_word_start_check_box);
    evas_object_show(window->search.close_button);

    /* Grab focus from the view */
    evas_object_focus_set(window->ewk_view, EINA_FALSE);
    elm_object_focus_set(window->search.search_field, EINA_TRUE);
}

static void
search_box_hide(Browser_Window *window)
{
    ewk_view_text_find_highlight_clear(window->ewk_view);

    evas_object_size_hint_min_set(window->search.search_bar, SEARCH_FIELD_SIZE + 2 * SEARCH_BUTTON_SIZE, 0);
    evas_object_hide(window->search.search_bar);
    evas_object_hide(window->search.search_field);
    evas_object_hide(window->search.search_field_count);
    evas_object_hide(window->search.backward_button);
    evas_object_hide(window->search.forward_button);
    evas_object_hide(window->search.search_case_check_box);
    evas_object_hide(window->search.search_word_start_check_box);
    evas_object_hide(window->search.close_button);

    /* Give focus back to the view */
    elm_object_focus_set(window->search.search_field, EINA_FALSE);
    evas_object_focus_set(window->ewk_view, EINA_TRUE);
}

static void 
history_list_hide(Browser_Window *window)
{
    /* Hide history list */
    evas_object_hide(window->history.history_box);
    evas_object_hide(window->history.history_list);
    
    /* Dereference the list items and clear the history list */
    void *data;    
    EINA_LIST_FREE(window->history.history_list_items, data) {
        ewk_object_unref(data);
    }

    elm_genlist_clear(window->history.history_list);
    
    /* Give focus back to the view */
    elm_object_focus_set(window->history.history_box, EINA_FALSE);
    elm_object_focus_set(window->history.history_list, EINA_FALSE);
    evas_object_focus_set(window->ewk_view, EINA_TRUE);

    /* Reset flags */
    longpress_enabled = EINA_FALSE;
} 

static void save_page_contents_callback(Ewk_Page_Contents_Type type, const char *data, void *user_data)
{
    Eet_File *ef;
    Eina_Stringshare *fileName = (Eina_Stringshare *)user_data;

    if (!eina_str_has_extension(fileName, ".mht")) {
        Eina_Strbuf *fileNameWithMht = eina_strbuf_new();
        eina_strbuf_append_printf(fileNameWithMht, "%s.mht", fileName);
        ef = eet_open(eina_strbuf_string_get(fileNameWithMht), EET_FILE_MODE_WRITE);
        INFO("Saving file to: %s", eina_strbuf_string_get(fileNameWithMht));
        eina_strbuf_free(fileNameWithMht);
    } else {
        ef = eet_open(fileName, EET_FILE_MODE_WRITE);
        INFO("Saving file to: %s", fileName);
    }

    if (!ef) {
        ERROR("Could not create File");
        return;
    }

    eet_write(ef, "MHTML data", data, strlen(data), 0 /* compress */);
    eet_close(ef);
    INFO("SUCCESS: saved.");

    eina_stringshare_del(fileName);
}

static void 
script_execute_callback(Evas_Object *ewk_view, const char *return_value, void *user_data)
{
    Browser_Window *window = (Browser_Window *)user_data;

    Eina_Strbuf *text_buffer = eina_strbuf_new();

    if (return_value) {
        eina_strbuf_append(text_buffer, return_value);
        INFO("selected text is: %s", eina_strbuf_string_get(text_buffer));
        elm_entry_entry_set(window->search.search_field, eina_strbuf_string_get(text_buffer));   
    }
    eina_strbuf_free(text_buffer);
    search_box_show(window);
}

static void
toggle_window_fullscreen(Browser_Window *window, Eina_Bool isFullScreen)
{
    if (isFullScreen) {
        evas_object_hide(window->horizontal_layout);
        elm_box_unpack(window->vertical_layout, window->horizontal_layout);
        elm_win_fullscreen_set(window->elm_window, EINA_TRUE);
    } else {
        elm_win_fullscreen_set(window->elm_window, EINA_FALSE);
        elm_box_pack_start(window->vertical_layout, window->horizontal_layout);
        evas_object_show(window->horizontal_layout);
    }
}

static void
on_key_down(void *user_data, Evas *e, Evas_Object *ewk_view, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    Evas_Event_Key_Down *ev = (Evas_Event_Key_Down*) event_info;

    static const char *encodings[] = {
        "ISO-8859-1",
        "UTF-8",
        NULL
    };
    static int currentEncoding = -1;
    const Evas_Modifier *mod = evas_key_modifier_get(e);
    Eina_Bool ctrlPressed = evas_key_modifier_is_set(mod, "Control");
    Eina_Bool altPressed = evas_key_modifier_is_set(mod, "Alt");
    Eina_Bool shiftPressed = evas_key_modifier_is_set(mod, "Shift");

    if (!strcmp(ev->key, "Left") && altPressed) {
        INFO("Back (Alt+Left) was pressed");
        if (!ewk_view_back(ewk_view))
            INFO("Back ignored: No back history");
    } else if (!strcmp(ev->key, "Right") && altPressed) {
        INFO("Forward (Alt+Right) was pressed");
        if (!ewk_view_forward(ewk_view))
            INFO("Forward ignored: No forward history");
    } else if (!strcmp(ev->key, "Home") && altPressed) {
        INFO("Home (Alt+Home) was pressed");
        ewk_view_url_set(window->ewk_view, DEFAULT_URL);
    } else if (!strcmp(ev->key, "F3")) {
        currentEncoding = (currentEncoding + 1) % (sizeof(encodings) / sizeof(encodings[0]));
        INFO("Set encoding (F3) pressed. New encoding to %s", encodings[currentEncoding]);
        ewk_view_custom_encoding_set(ewk_view, encodings[currentEncoding]);
    } else if ((!strcmp(ev->key, "F5") && ctrlPressed) || (!strcmp(ev->key, "r") && (shiftPressed & ctrlPressed))) {
        INFO("Reload ignoring cache (Ctrl+F5 or Ctrl+Shift+r) was pressed, reloading and bypassing cache...");
        ewk_view_reload_bypass_cache(ewk_view);
    } else if (!strcmp(ev->key, "F5") || (!strcmp(ev->key, "r") && ctrlPressed)) {
        INFO("Reload (F5 or Ctrl+r) was pressed, reloading...");
        ewk_view_reload(ewk_view);
    } else if (!strcmp(ev->key, "F6")) {
        INFO("Stop (F6) was pressed, stop loading.");
        ewk_view_stop(ewk_view);
    } else if (!strcmp(ev->key, "F7")) {
        Ewk_Pagination_Mode mode =  ewk_view_pagination_mode_get(ewk_view);
        mode = (mode + 1) % (EWK_PAGINATION_MODE_BOTTOM_TO_TOP + 1);
        if (ewk_view_pagination_mode_set(ewk_view, mode))
            INFO("Change Pagination Mode (F7) was pressed, changed to: %d", mode);
        else
            INFO("Change Pagination Mode (F7) was pressed, but NOT changed!");
    } else if (!strcmp(ev->key, "F11")) {
        INFO("Fullscreen (F11) was pressed, toggling window/fullscreen.");
        toggle_window_fullscreen(window, !elm_win_fullscreen_get(window->elm_window));
    } else if (!strcmp(ev->key, "n") && ctrlPressed) {
        INFO("Create new window (Ctrl+n) was pressed.");
        Browser_Window *window = window_create(NULL, 0, 0);
        ewk_view_url_set(window->ewk_view, DEFAULT_URL);
        // 0 equals default width and height.
        windows = eina_list_append(windows, window);
    } else if (!strcmp(ev->key, "i") && ctrlPressed) {
        INFO("Show Inspector (Ctrl+i) was pressed.");
        ewk_view_inspector_show(ewk_view);
    } else if (!strcmp(ev->key, "f") && ctrlPressed) {
        INFO("Show Search Box (Ctrl+f) was pressed.");
        const char get_data_script[] = "window.getSelection().toString();";
        ewk_view_script_execute(ewk_view, get_data_script, script_execute_callback, (void*)(window));
    } else if (!strcmp(ev->key, "Escape")) {
        if (evas_object_visible_get(window->search.search_bar))
            search_box_hide(window);
        else if (evas_object_visible_get(window->history.history_box))
            history_list_hide(window);
        else if (elm_win_fullscreen_get(window->elm_window))
            ewk_view_fullscreen_exit(ewk_view);
        else
            ewk_view_stop(ewk_view);
    } else if (ctrlPressed && (!strcmp(ev->key, "minus") || !strcmp(ev->key, "KP_Subtract"))) {
        if (zoom_level_set(ewk_view, window->current_zoom_level - 1))
            window->current_zoom_level--;
        INFO("Zoom out (Ctrl + '-') was pressed, zoom level became %.2f", zoomLevels[window->current_zoom_level]);
    } else if (ctrlPressed && (!strcmp(ev->key, "equal") || !strcmp(ev->key, "KP_Add"))) {
        if (zoom_level_set(ewk_view, window->current_zoom_level + 1))
            window->current_zoom_level++;
        INFO("Zoom in (Ctrl + '+') was pressed, zoom level became %.2f", zoomLevels[window->current_zoom_level]);
    } else if (ctrlPressed && !strcmp(ev->key, "0")) {
        if (zoom_level_set(ewk_view, DEFAULT_ZOOM_LEVEL))
            window->current_zoom_level = DEFAULT_ZOOM_LEVEL;
        INFO("Zoom to default (Ctrl + '0') was pressed, zoom level became %.2f", zoomLevels[window->current_zoom_level]);
    } else if (ctrlPressed && !strcmp(ev->key, "s")) {
        Eina_Strbuf *default_file = eina_strbuf_new();
        const char *home_path = getenv("HOME");
        const char *title = ewk_view_title_get(window->ewk_view);
        eina_strbuf_append_printf(default_file, "%s/%s.mht", home_path ? home_path : "/home", title ? title : "title");
        INFO("Pressed (CTRL + S) : Saving Current Page.");
        Eina_Stringshare *save_file_name = show_file_entry_dialog(window, "SAVE", eina_strbuf_string_get(default_file));
        if (!save_file_name)
            return;
        ewk_view_page_contents_get(ewk_view, EWK_PAGE_CONTENTS_TYPE_MHTML, save_page_contents_callback, (void *)save_file_name);
        eina_strbuf_free(default_file);
    } else if (ctrlPressed && !strcmp(ev->key, "l")) {
        const char *home_path =  getenv("HOME");
        Eina_Stringshare *open_file_name = show_file_entry_dialog(window, "LOAD", home_path ? home_path : "/home");
        if (!open_file_name)
            return;
        Eina_Strbuf *uri_path = eina_strbuf_new();
        eina_strbuf_append_printf(uri_path, "file://%s", open_file_name);
        INFO("pressed (CTRL + L) : Loading Page %s", eina_strbuf_string_get(uri_path));
        ewk_view_url_set(ewk_view, eina_strbuf_string_get(uri_path));
        eina_strbuf_free(uri_path);
        eina_stringshare_del(open_file_name);
    }
}

static void
view_focus_set(Browser_Window *window, Eina_Bool focus)
{
    /* We steal focus away from elm's focus model and start to do things
     * manually here, so elm now has no clue what's up. Tell elm that its
     * toplevel widget is to be unfocused so elm gives up the focus */
    elm_object_focus_set(elm_object_top_widget_get(window->elm_window), EINA_FALSE);
    evas_object_focus_set(window->ewk_view, focus);
}

static void
on_mouse_down(void *user_data, Evas *e, Evas_Object *ewk_view, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    Evas_Event_Mouse_Down *ev = (Evas_Event_Mouse_Down *)event_info;

    /* Clear selection from the URL bar */
    elm_entry_select_none(window->url_bar);

    if (longpress_enabled)
        history_list_hide(window);
    if (ev->button == 1)
        view_focus_set(window, EINA_TRUE);
    else if (ev->button == 2)
        view_focus_set(window, !evas_object_focus_get(ewk_view));
}

static void
title_set(Evas_Object *elm_window, const char *title, int progress)
{
    Eina_Strbuf *buffer;

    if (!title || !*title) {
        elm_win_title_set(elm_window, APP_NAME);
        return;
    }

    buffer = eina_strbuf_new();
    if (progress < 100)
        eina_strbuf_append_printf(buffer, "%s (%d%%) - %s", title, progress, APP_NAME);
    else
        eina_strbuf_append_printf(buffer, "%s - %s", title, APP_NAME);

    elm_win_title_set(elm_window, eina_strbuf_string_get(buffer));
    eina_strbuf_free(buffer);
}

static void
on_title_changed(void *user_data, Evas_Object *ewk_view, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    const char *title = (const char *)event_info;

    title_set(window->elm_window, title, 100);
}

static void
on_url_changed(void *user_data, Evas_Object *ewk_view, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    const char *url = ewk_view_url_get(window->ewk_view);
    char *converted_url = elm_entry_utf8_to_markup(url); 
    elm_entry_entry_set(window->url_bar, converted_url);

    on_icon_changed_cb(ewk_context_favicon_database_get(ewk_view_context_get(ewk_view)), url, user_data);

    free(converted_url);

    search_box_hide(window);
}

static void
on_back_forward_list_changed(void *user_data, Evas_Object *ewk_view, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    /* Update navigation buttons state */
    elm_object_disabled_set(window->back_button, !ewk_view_back_possible(ewk_view));
    elm_object_disabled_set(window->forward_button, !ewk_view_forward_possible(ewk_view));
}

static void
on_progress(void *user_data, Evas_Object *ewk_view, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    double progress = *(double *)event_info;

    title_set(window->elm_window, ewk_view_title_get(window->ewk_view), progress * 100);
}

static void
on_error(void *user_data, Evas_Object *ewk_view, void *event_info)
{
    Eina_Strbuf *buffer;
    const Ewk_Error *error = (const Ewk_Error *)event_info;

    /* This is a cancellation, do not display the error page */
    if (ewk_error_cancellation_get(error))
        return;

    buffer = eina_strbuf_new();
    eina_strbuf_append_printf(buffer, "<html><body><div style=\"color:#ff0000\">ERROR!</div><br><div>Code: %d<br>Description: %s<br>URL: %s</div></body</html>",
        ewk_error_code_get(error), ewk_error_description_get(error), ewk_error_url_get(error));

    ewk_view_html_string_load(ewk_view, eina_strbuf_string_get(buffer), 0, ewk_error_url_get(error));
    eina_strbuf_free(buffer);
}

static void
on_download_request(Ewk_Download_Job *download, void *user_data)
{
    Browser_Window *window = (Browser_Window *)user_data;

    Eina_Strbuf *destination_path = eina_strbuf_new();

    const char *home_path = getenv("HOME");
    Eina_Stringshare *save_file_path = show_file_entry_dialog(window, "DOWNLOAD", home_path ? home_path : "/tmp");

    if (save_file_path)
        eina_strbuf_append_printf(destination_path, "%s", save_file_path);
    else
        eina_strbuf_append(destination_path, "/tmp");

    const char *suggested_name = ewk_download_job_suggested_filename_get(download);
    if (suggested_name && *suggested_name)
        eina_strbuf_append_printf(destination_path, "/%s", suggested_name);
    else {
        // Generate a unique file name since no name was suggested.
        eina_strbuf_append(destination_path, "/downloaded-file.XXXXXX");
        char *url = NULL;
        url = eina_strbuf_string_steal(destination_path);
        if (mkstemp(url) == -1) {
            ERROR("Could not generate a unique file name.");
            return;
        }
        eina_strbuf_append(destination_path, url);
    }

    ewk_download_job_destination_set(download, eina_strbuf_string_get(destination_path));
    INFO("Downloading: %s", eina_strbuf_string_get(destination_path));
    eina_strbuf_free(destination_path);
    eina_stringshare_del(save_file_path);
}

static void on_filepicker_parent_deletion(void *user_data, Evas *evas, Evas_Object *elm_window, void *event);

static void close_file_picker(File_Selector_Data *fs_data)
{
    evas_object_event_callback_del(fs_data->parent->elm_window, EVAS_CALLBACK_DEL, on_filepicker_parent_deletion);
    evas_object_del(fs_data->elm_window);
    ewk_object_unref(fs_data->request);
    free(fs_data);
}

static void
on_filepicker_parent_deletion(void *user_data, Evas *evas, Evas_Object *elm_window, void *event)
{
    close_file_picker((File_Selector_Data *)user_data);
}

static void
on_filepicker_deletion(void *user_data, Evas_Object *elm_window, void *event_info)
{
    close_file_picker((File_Selector_Data *)user_data);
}

static void
on_fileselector_done(void *user_data, Evas_Object *file_selector, void *event_info)
{
    File_Selector_Data *fs_data = (File_Selector_Data *)user_data;

    const char *selected = (const char *)event_info;
    if (selected && *selected)
        ewk_file_chooser_request_file_choose(fs_data->request, selected);

    close_file_picker(fs_data);
}

static void
on_file_chooser_request(void *user_data, Evas_Object *ewk_view, void *event_info)
{
    Evas_Object *bg;
    Browser_Window *window = (Browser_Window *)user_data;
    Ewk_File_Chooser_Request *request = (Ewk_File_Chooser_Request *)event_info;

    // Show basic file picker which does not currently support multiple files
    // or MIME type filtering.
    Evas_Object *elm_window = elm_win_add(window->elm_window, "file-picker-window", ELM_WIN_DIALOG_BASIC);
    elm_win_title_set(elm_window, "File picker");
    elm_win_modal_set(elm_window, EINA_TRUE);

    bg = elm_bg_add(elm_window);
    evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(elm_window, bg);
    evas_object_show(bg);

    File_Selector_Data *fs_data = (File_Selector_Data *)malloc(sizeof(File_Selector_Data));
    fs_data->parent = window;
    fs_data->elm_window = elm_window;
    fs_data->request = ewk_object_ref(request);
    evas_object_smart_callback_add(elm_window, "delete,request", on_filepicker_deletion, fs_data);
    evas_object_event_callback_add(window->elm_window, EVAS_CALLBACK_DEL, on_filepicker_parent_deletion, fs_data);

    Evas_Object *file_selector = elm_fileselector_add(elm_window);
    const char *home_path = getenv("HOME");
    elm_fileselector_path_set(file_selector, home_path ? home_path : "/home");
    evas_object_size_hint_weight_set(file_selector, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(elm_window, file_selector);
    evas_object_show(file_selector);

    evas_object_smart_callback_add(file_selector, "done", on_fileselector_done, fs_data);

    evas_object_resize(elm_window, 400, 400);
    elm_win_center(elm_window, EINA_TRUE, EINA_TRUE);
    evas_object_show(elm_window);
}

static void
on_download_finished(Ewk_Download_Job *download, void *user_data)
{
    INFO("Download finished: %s",  ewk_download_job_destination_get(download));
}

static void
on_download_failed(Ewk_Download_Job_Error *download_error, void *user_data)
{
    Ewk_Error *error = download_error->error;

    INFO("Download failed! Error code: %d, Error description: %s, Error URL: %s", ewk_error_code_get(error), ewk_error_description_get(error), ewk_error_url_get(error));
}

static void
on_color_changed(void *data, Evas_Object *obj, void *event_info)
{
    int r, g, b, a;

    elm_colorselector_color_get(obj, &r, &g, &b, &a);
    evas_object_color_set(data, r, g, b, a);
}

static void
on_color_item_selected(void *data, Evas_Object *obj, void *event_info)
{
    int r, g, b, a;
    Elm_Object_Item *color_item = (Elm_Object_Item *)event_info;

    elm_colorselector_palette_item_color_get(color_item, &r, &g, &b, &a);
    evas_object_color_set(data, r, g, b, a);
}

static void
on_color_picker_ok_clicked(void *data, Evas_Object *obj, void *event_info)
{
    int r, g, b, a;
    Color_Selector *color_selector = (Color_Selector *)data;

    elm_colorselector_color_get(color_selector->elm_selector, &r, &g, &b, &a);
    ewk_color_picker_color_set(color_selector->ewk_picker, r, g, b, a);
}

static void
on_color_picker_cancel_clicked(void *data, Evas_Object *obj, void *event_info)
{
    int r, g, b, a;

    ewk_color_picker_color_get(data, &r, &g, &b, &a);
    ewk_color_picker_color_set(data, r, g, b, a);
}

static Eina_Bool
on_color_picker_dismiss(Ewk_View_Smart_Data *sd)
{
    Browser_Window *window = window_find_with_ewk_view(sd->self);

    evas_object_del(window->color_selector.elm_selector_window);
    window->color_selector.elm_selector_window = NULL;

    return EINA_TRUE;
}

static Eina_Bool
on_color_picker_request(Ewk_View_Smart_Data *sd, Ewk_Color_Picker *color_picker)
{
    int r, g, b, a;
    Evas_Object *background, *rect, *box, *button_box, *rect_frame, *cs_frame, *ok_button, *cancel_button;

    Browser_Window *window = window_find_with_ewk_view(sd->self);
    window->color_selector.elm_selector_window = elm_win_add(window->elm_window, "color selector", ELM_WIN_BASIC);
    window->color_selector.ewk_picker = color_picker;

    elm_win_title_set(window->color_selector.elm_selector_window, "Color selector");

    /* Show color view */
    background = elm_bg_add(window->color_selector.elm_selector_window);
    evas_object_size_hint_weight_set(background, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(window->color_selector.elm_selector_window, background);
    evas_object_show(background);

    box = elm_box_add(window->color_selector.elm_selector_window);
    evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(window->color_selector.elm_selector_window, box);
    evas_object_show(box);

    rect_frame = elm_frame_add(window->color_selector.elm_selector_window);
    evas_object_size_hint_weight_set(rect_frame, EVAS_HINT_EXPAND, 0.3);
    evas_object_size_hint_align_set(rect_frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_object_text_set(rect_frame, "Color View");
    elm_box_pack_end(box, rect_frame);
    evas_object_show(rect_frame);

    rect = evas_object_rectangle_add(evas_object_evas_get(window->color_selector.elm_selector_window));
    elm_object_content_set(rect_frame, rect);
    ewk_color_picker_color_get(window->color_selector.ewk_picker, &r, &g, &b, &a);
    evas_object_color_set(rect, r, g, b, a);
    evas_object_show(rect);

    /* Show color selector */
    cs_frame = elm_frame_add(window->color_selector.elm_selector_window);
    evas_object_size_hint_weight_set(cs_frame, EVAS_HINT_EXPAND, 0.7);
    evas_object_size_hint_align_set(cs_frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_object_text_set(cs_frame, "Color Selector");
    elm_box_pack_end(box, cs_frame);
    evas_object_show(cs_frame);

    window->color_selector.elm_selector = elm_colorselector_add(window->color_selector.elm_selector_window);
    elm_object_content_set(cs_frame, window->color_selector.elm_selector);
    evas_object_show(window->color_selector.elm_selector);

    /* OK, Cancel Buttons */
    button_box = elm_box_add(window->color_selector.elm_selector_window);
    elm_box_horizontal_set(button_box, EINA_TRUE);
    evas_object_size_hint_min_set(button_box, 200, 50);
    elm_box_pack_end(box, button_box);
    evas_object_show(button_box);

    ok_button = elm_button_add(window->color_selector.elm_selector_window);
    elm_object_text_set(ok_button, "OK");
    elm_box_pack_end(button_box, ok_button);
    evas_object_show(ok_button);

    cancel_button = elm_button_add(window->color_selector.elm_selector_window);
    elm_object_text_set(cancel_button, "Cancel");
    elm_box_pack_end(button_box, cancel_button);
    evas_object_show(cancel_button);

    evas_object_smart_callback_add(ok_button, "clicked", on_color_picker_ok_clicked, &(window->color_selector));
    evas_object_smart_callback_add(cancel_button, "clicked", on_color_picker_cancel_clicked, window->color_selector.ewk_picker);
    evas_object_smart_callback_add(window->color_selector.elm_selector_window, "delete,request", on_color_picker_cancel_clicked, window->color_selector.ewk_picker);
    evas_object_smart_callback_add(window->color_selector.elm_selector, "changed", on_color_changed, rect);
    evas_object_smart_callback_add(window->color_selector.elm_selector, "color,item,selected", on_color_item_selected, rect);

    elm_win_center(window->color_selector.elm_selector_window, EINA_TRUE, EINA_TRUE);
    evas_object_resize(window->color_selector.elm_selector_window, 350, 500);
    evas_object_show(window->color_selector.elm_selector_window);

    return EINA_TRUE;
}

static int
quit(Eina_Bool success, const char *msg)
{
    if (msg)
        success ? INFO("%s", msg) : ERROR("%s", msg);

    eina_log_domain_unregister(_log_domain_id);
    ewk_shutdown();
    elm_shutdown();

    if (!success)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static Eina_Bool
has_scheme(const char *url)
{
    return !!strstr(url, "://");
}

static char *
url_from_user_input(const char *arg)
{
    /* If it is already a URL, return the argument as is. */
    if (has_scheme(arg) || eina_str_has_prefix(arg, "javascript:") || !strcasecmp(arg, "about:blank"))
        return strdup(arg);

    Eina_Strbuf *buf = eina_strbuf_manage_new(eina_file_path_sanitize(arg));

    /* Check if the path exists. */
    if (ecore_file_exists(eina_strbuf_string_get(buf))) {
        /* File exists, convert local path to a URL. */
        eina_strbuf_prepend(buf, "file://");
    } else {
        /* The path does not exist, convert it to a URL by
           prepending http:// scheme:
           www.google.com -> http://www.google.com */
        eina_strbuf_string_free(buf);
        eina_strbuf_append_printf(buf, "http://%s", arg);
    }
    char *url = eina_strbuf_string_steal(buf);
    eina_strbuf_free(buf);

    return url;
}

static Eina_Bool
url_load_from_user_input(Evas_Object *ewk_view, const char *url)
{
    if (!ewk_view || !url)
        return EINA_FALSE;

    if (eina_str_has_prefix(url, "javascript:"))
        return ewk_view_script_execute(ewk_view, strstr(url, "javascript:"), 0, 0);

    return ewk_view_url_set(ewk_view, url);
}

static void
on_url_bar_activated(void *user_data, Evas_Object *url_bar, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    const char *markup_url = elm_entry_entry_get(url_bar);
    char *user_url = elm_entry_markup_to_utf8(markup_url);
    char *url = url_from_user_input(user_url);
    url_load_from_user_input(window->ewk_view, url);

    free(user_url);
    free(url);

    /* Give focus back to the view */
    view_focus_set(window, EINA_TRUE);
}

static void
on_url_bar_clicked(void *user_data, Evas_Object *url_bar, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    /* Grab focus from the view */
    evas_object_focus_set(window->ewk_view, EINA_FALSE);
    elm_object_focus_set(url_bar, EINA_TRUE);
}

static void
on_search_field_aborted(void *user_data, Evas_Object *search_field, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    search_box_hide(window);

    /* Give focus back to the view */
    view_focus_set(window, EINA_TRUE);
}

static void
on_search_field_activated(void *user_data, Evas_Object *search_field, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    const char *markup_text = elm_entry_entry_get(search_field);
    char *text = elm_entry_markup_to_utf8(markup_text);
    ewk_view_text_find(window->ewk_view, text, search_flags, MAX_SEARCH_COUNT);
    free(text);

    /* Grab focus from the view */
    evas_object_focus_set(window->ewk_view, EINA_FALSE);
    elm_object_focus_set(search_field, EINA_TRUE);
}

static void
on_search_field_clicked(void *user_data, Evas_Object *search_field, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    /* Grab focus from the view */
    evas_object_focus_set(window->ewk_view, EINA_FALSE);
    elm_object_focus_set(search_field, EINA_TRUE);
}

static void
on_back_button_clicked(void *user_data, Evas_Object *back_button, void *event_info)
{
    if (longpress_enabled)
        return;
    
    Browser_Window *window = (Browser_Window *)user_data;

    ewk_view_back(window->ewk_view);
    /* Update back button state */
    elm_object_disabled_set(back_button, !ewk_view_back_possible(window->ewk_view));
}

static void
on_forward_button_clicked(void *user_data, Evas_Object *forward_button, void *event_info)
{
    if (longpress_enabled)
        return;

    Browser_Window *window = (Browser_Window *)user_data;

    ewk_view_forward(window->ewk_view);
    /* Update forward button state */
    elm_object_disabled_set(forward_button, !ewk_view_forward_possible(window->ewk_view));
}

static void
on_search_backward_button_clicked(void *user_data, Evas_Object *search_backward_button, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    char *text = elm_entry_markup_to_utf8(elm_entry_entry_get(window->search.search_field));
    ewk_view_text_find(window->ewk_view, text, search_flags | EWK_FIND_OPTIONS_BACKWARDS, MAX_SEARCH_COUNT);
    free(text);
}

static void
on_search_forward_button_clicked(void *user_data, Evas_Object *search_forward_button, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    char *text = elm_entry_markup_to_utf8(elm_entry_entry_get(window->search.search_field));
    ewk_view_text_find(window->ewk_view, text, search_flags, MAX_SEARCH_COUNT);
    free(text);
}

static void
on_search_case_option_changed(void *user_data, Evas_Object *search_case_check_box, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    char *text = elm_entry_markup_to_utf8(elm_entry_entry_get(window->search.search_field));
    
    /* Bit toggle the case sensitive flag */
    search_flags = search_flags ^ EWK_FIND_OPTIONS_CASE_INSENSITIVE;
    
    ewk_view_text_find(window->ewk_view, text, search_flags, MAX_SEARCH_COUNT);
    free(text);
}

static void
on_search_word_start_option_changed(void *user_data, Evas_Object *search_word_start_check_box, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    char *text = elm_entry_markup_to_utf8(elm_entry_entry_get(window->search.search_field));
    
    /* Bit toggle the word start flag */
    search_flags = search_flags ^ EWK_FIND_OPTIONS_AT_WORD_STARTS;
    
    ewk_view_text_find(window->ewk_view, text, search_flags, MAX_SEARCH_COUNT);
    free(text);
}

static void
on_search_close_button_clicked(void *user_data, Evas_Object *search_close_button, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    search_box_hide(window);
}

static void
on_refresh_button_clicked(void *user_data, Evas_Object *refresh_button, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    Evas *evas = evas_object_evas_get(refresh_button);
    Eina_Bool ctrlPressed = evas_key_modifier_is_set(evas_key_modifier_get(evas), "Control");
    if (ctrlPressed) {
        INFO("Reloading and bypassing cache...");
        ewk_view_reload_bypass_cache(window->ewk_view);
    } else {
        INFO("Reloading...");
        ewk_view_reload(window->ewk_view);
    }
}

static void
on_stop_button_clicked(void *user_data, Evas_Object *stop_button, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    INFO("Stop was Pressed. Aborting load...");
    ewk_view_stop(window->ewk_view);
}

static char *
list_item_label_get(void *data, Evas_Object *obj, const char *part)
{
    int len = strlen((char *)data);
    char *buf = (char *)malloc(sizeof(char) * (len + 1));
    snprintf(buf, len + 1, "%s", (char *)data);
    return buf;
}

static void
on_list_item_select(void *user_data, Evas_Object *obj, void *event_info)
{
    Browser_Window *window = evas_object_data_get(obj, "Browser_Window");
    ewk_view_navigate_to(window->ewk_view, user_data); 
    history_list_hide(window);
    evas_object_data_del(obj, "Browser_Window");
}

static void
navigation_button_longpress_process(void *user_data, Eina_Bool forward_navigation_enabled)
{
    if (longpress_enabled)
        return;

    longpress_enabled = EINA_TRUE;
    Browser_Window *window = (Browser_Window *)user_data;
    
    Ewk_Back_Forward_List *list = ewk_view_back_forward_list_get(window->ewk_view);
    const Eina_List *l;
    void *data;
    static Elm_Genlist_Item_Class *list_item = NULL;
    const char *title = NULL;
    int item_count;
    int x;
    int y;
    int width;
    int height;
    size_t index;
    
    evas_object_data_set(window->history.history_list, "Browser_Window", window);

    if (forward_navigation_enabled)
        window->history.history_list_items = ewk_back_forward_list_forward_items_copy(list); 
    else
        window->history.history_list_items = ewk_back_forward_list_back_items_copy(list);

    if (!list_item) {
        list_item = elm_genlist_item_class_new();
        list_item->item_style = "default";
        list_item->func.text_get = list_item_label_get;
        list_item->func.content_get = NULL;
        list_item->func.state_get = NULL;
        list_item->func.del = NULL;
    }

    item_count = eina_list_count(window->history.history_list_items);
    INFO("navigation_button_longpress_process : Item count = %d forward_navigation_enabled = %d", item_count, forward_navigation_enabled);

    EINA_LIST_FOREACH(window->history.history_list_items, l, data) {
        title = ewk_back_forward_list_item_title_get(data);
        INFO(" title = %s", title);
        elm_genlist_item_append(window->history.history_list, list_item, (void *)(title), NULL, ELM_GENLIST_ITEM_NONE, on_list_item_select, data);
    }
    
    if (item_count > 0) {
        evas_object_geometry_get(window->elm_window, &x, &y, &width, &height);
        elm_list_go(window->history.history_list);
        evas_object_resize(window->history.history_box , width / 3 , LIST_ITEM_HEIGHT * item_count);

        if (forward_navigation_enabled) {
            evas_object_move(window->history.history_box , x + TOOL_BAR_BUTTON_SIZE + 1, y + TOOL_BAR_BUTTON_SIZE);
            evas_object_move(window->history.history_list , x + TOOL_BAR_BUTTON_SIZE + 1, y + TOOL_BAR_BUTTON_SIZE);  
        } else {
            evas_object_move(window->history.history_box , x, y + TOOL_BAR_BUTTON_SIZE);
            evas_object_move(window->history.history_list , x, y + TOOL_BAR_BUTTON_SIZE);  
        }
    
        elm_genlist_mode_set(window->history.history_list, ELM_LIST_COMPRESS);
        evas_object_show(window->history.history_box);
        evas_object_show(window->history.history_list);
        evas_object_focus_set(window->ewk_view, EINA_FALSE);
        elm_object_focus_set(window->history.history_list, EINA_TRUE);
    } else
        longpress_enabled = EINA_FALSE;
}

static void
on_forward_button_longpress(void *user_data, Evas_Object *forward_button, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    navigation_button_longpress_process(user_data, EINA_TRUE);
    elm_object_disabled_set(forward_button, !ewk_view_back_possible(window->ewk_view));
}

static void
on_back_button_longpress(void *user_data, Evas_Object *back_button, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    navigation_button_longpress_process(user_data, EINA_FALSE);
    elm_object_disabled_set(back_button, !ewk_view_back_possible(window->ewk_view));
}

static void
quit_event_loop(void *user_data, Evas_Object *obj, void *event_info)
{
    ecore_main_loop_quit();
}

static void
on_ok_clicked(void *user_data, Evas_Object *obj, void *event_info)
{
    Eina_Bool *confirmed = (Eina_Bool *)user_data;
    *confirmed = EINA_TRUE;

    ecore_main_loop_quit();
}

static Eina_Stringshare *
show_file_entry_dialog(Browser_Window *window, const char *label_tag, const char *default_text)
{
    Evas_Object *file_popup = elm_popup_add(window->elm_window);
    evas_object_size_hint_weight_set(file_popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show(file_popup);

    Evas_Object *vbox = elm_box_add(file_popup);
    evas_object_size_hint_weight_set(vbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(vbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_object_content_set(file_popup, vbox);
    evas_object_show(vbox);

    Evas_Object *label = elm_label_add(window->elm_window);
    elm_object_text_set(label, label_tag);
    evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(label, EVAS_HINT_FILL, 0.5);
    evas_object_color_set(label, 23, 45, 67, 142);
    elm_box_pack_end(vbox, label);
    evas_object_show(label);

    Evas_Object *fs_entry = elm_fileselector_entry_add(file_popup);
    elm_fileselector_is_save_set(fs_entry, EINA_TRUE);
    evas_object_size_hint_align_set(fs_entry, EVAS_HINT_FILL, 0);
    elm_fileselector_path_set(fs_entry, default_text);
    elm_object_text_set(fs_entry, "FileChooser");
    elm_box_pack_end(vbox, fs_entry);
    evas_object_show(fs_entry);

    Evas_Object *hbox = elm_box_add(file_popup);
    evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_box_horizontal_set(hbox, EINA_TRUE);
    evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_end(vbox, hbox);
    evas_object_show(hbox);

    Eina_Bool ok = EINA_FALSE;
    Evas_Object *ok_button = elm_button_add(file_popup);
    elm_object_text_set(ok_button, "OK");
    evas_object_smart_callback_add(ok_button, "clicked", on_ok_clicked, &ok);
    elm_box_pack_end(hbox, ok_button);
    evas_object_show(ok_button);

    Evas_Object *cancel_button = elm_button_add(file_popup);
    elm_object_text_set(cancel_button, "Cancel");
    evas_object_smart_callback_add(cancel_button, "clicked", quit_event_loop, NULL);
    elm_box_pack_end(hbox, cancel_button);
    evas_object_show(cancel_button);

    ecore_main_loop_begin();

    Eina_Stringshare *file_path = ok ? eina_stringshare_add(elm_fileselector_path_get(fs_entry)) : NULL;
    evas_object_del(file_popup);
    return file_path;
}

static void
on_javascript_alert(Ewk_View_Smart_Data *smartData, const char *message)
{
    Browser_Window *window = window_find_with_ewk_view(smartData->self);

    Evas_Object *alert_popup = elm_popup_add(window->elm_window);
    evas_object_size_hint_weight_set(alert_popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_object_text_set(alert_popup, message);
    elm_object_part_text_set(alert_popup, "title,text", "Alert");

    /* Popup buttons */
    Evas_Object *button = elm_button_add(alert_popup);
    elm_object_text_set(button, "OK");
    elm_object_part_content_set(alert_popup, "button1", button);
    evas_object_smart_callback_add(button, "clicked", quit_event_loop, NULL);
    elm_object_focus_set(button, EINA_TRUE);
    evas_object_show(alert_popup);

    /* Make modal */
    ecore_main_loop_begin();

    evas_object_del(alert_popup);
}

static Eina_Bool
on_javascript_confirm(Ewk_View_Smart_Data *smartData, const char *message)
{
    Browser_Window *window = window_find_with_ewk_view(smartData->self);

    Eina_Bool ok = EINA_FALSE;

    Evas_Object *confirm_popup = elm_popup_add(window->elm_window);
    evas_object_size_hint_weight_set(confirm_popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_object_text_set(confirm_popup, message);
    elm_object_part_text_set(confirm_popup, "title,text", "Confirmation");

    /* Popup buttons */
    Evas_Object *cancel_button = elm_button_add(confirm_popup);
    elm_object_text_set(cancel_button, "Cancel");
    elm_object_part_content_set(confirm_popup, "button1", cancel_button);
    evas_object_smart_callback_add(cancel_button, "clicked", quit_event_loop, NULL);
    Evas_Object *ok_button = elm_button_add(confirm_popup);
    elm_object_text_set(ok_button, "OK");
    elm_object_part_content_set(confirm_popup, "button2", ok_button);
    evas_object_smart_callback_add(ok_button, "clicked", on_ok_clicked, &ok);
    elm_object_focus_set(ok_button, EINA_TRUE);
    evas_object_show(confirm_popup);

    /* Make modal */
    ecore_main_loop_begin();

    evas_object_del(confirm_popup);

    return ok;
}

static const char *
on_javascript_prompt(Ewk_View_Smart_Data *smartData, const char *message, const char *default_value)
{
    Browser_Window *window = window_find_with_ewk_view(smartData->self);

    Eina_Bool ok = EINA_FALSE;

    Evas_Object *prompt_popup = elm_popup_add(window->elm_window);
    evas_object_size_hint_weight_set(prompt_popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_object_part_text_set(prompt_popup, "title,text", "Prompt");

    /* Popup Content */
    Evas_Object *box = elm_box_add(window->elm_window);
    elm_box_padding_set(box, 0, 4);
    evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_show(box);

    Evas_Object *prompt = elm_label_add(window->elm_window);
    elm_object_text_set(prompt, message);
    evas_object_size_hint_weight_set(prompt, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(prompt, EVAS_HINT_FILL, 0.5);
    elm_box_pack_end(box, prompt);
    evas_object_show(prompt);

    Evas_Object *entry = elm_entry_add(window->elm_window);
    elm_entry_scrollable_set(entry, EINA_TRUE);
    elm_entry_single_line_set(entry, EINA_TRUE);
    elm_entry_text_style_user_push(entry, "DEFAULT='font_size=18'");
    elm_entry_entry_set(entry, default_value ? default_value : "");
    elm_entry_select_all(entry);
    evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, 0.5);
    elm_box_pack_end(box, entry);
    elm_object_focus_set(entry, EINA_TRUE);
    evas_object_show(entry);

    elm_object_content_set(prompt_popup, box);

    /* Popup buttons */
    Evas_Object *cancel_button = elm_button_add(prompt_popup);
    elm_object_text_set(cancel_button, "Cancel");
    elm_object_part_content_set(prompt_popup, "button1", cancel_button);
    evas_object_smart_callback_add(cancel_button, "clicked", quit_event_loop, NULL);
    Evas_Object *ok_button = elm_button_add(prompt_popup);
    elm_object_text_set(ok_button, "OK");
    elm_object_part_content_set(prompt_popup, "button2", ok_button);
    evas_object_smart_callback_add(ok_button, "clicked", on_ok_clicked, &ok);
    evas_object_show(prompt_popup);

    /* Make modal */
    ecore_main_loop_begin();

    /* The return string need to be stringshared */
    const char *prompt_text = ok ? eina_stringshare_add(elm_entry_entry_get(entry)) : NULL;
    evas_object_del(prompt_popup);

    return prompt_text;
}

static Eina_Bool
on_javascript_before_unload_confirm(Ewk_View_Smart_Data *smartData, const char *message)
{
    return on_javascript_confirm(smartData, "Will you leave this page?");
}

static void
on_popup_menu_item_clicked(void *user_data, Evas_Object *obj, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    Elm_Object_Item *item = (Elm_Object_Item *)event_info;

    INFO("Selected popup menu index: %u", elm_menu_item_index_get(item));
    ewk_popup_menu_selected_index_set(window->popup.ewk_menu, elm_menu_item_index_get(item));

    // Close popup menu.
    ewk_popup_menu_close(window->popup.ewk_menu);
}

static void
popup_menu_populate(Evas_Object *elm_menu, Ewk_Popup_Menu *ewk_menu, void *user_data)
{
    const Eina_List* ewk_items = ewk_popup_menu_items_get(ewk_menu);

    void *data;
    const Eina_List *l;
    EINA_LIST_FOREACH(ewk_items, l, data) {
        Ewk_Popup_Menu_Item *ewk_item = (Ewk_Popup_Menu_Item *)data;
        switch (ewk_popup_menu_item_type_get(ewk_item)) {
        case EWK_POPUP_MENU_SEPARATOR:
            elm_menu_item_separator_add(elm_menu, NULL);
            break;
        case EWK_POPUP_MENU_ITEM:
            if (ewk_popup_menu_item_is_label_get(ewk_item)) {
                Elm_Object_Item *item = elm_menu_item_add(elm_menu, NULL, NULL, ewk_popup_menu_item_text_get(ewk_item), NULL, NULL);
                elm_object_item_disabled_set(item, EINA_TRUE);
            } else {
                Elm_Object_Item *item = elm_menu_item_add(elm_menu, NULL, NULL, ewk_popup_menu_item_text_get(ewk_item), on_popup_menu_item_clicked, user_data);
                const char *tooltip_text = ewk_popup_menu_item_tooltip_get(ewk_item);
                if (tooltip_text && tooltip_text[0] != '\0')
                    elm_object_item_tooltip_text_set(item, tooltip_text);
                elm_object_item_disabled_set(item, !ewk_popup_menu_item_enabled_get(ewk_item));
                elm_menu_item_selected_set(item, ewk_popup_menu_item_selected_get(ewk_item));
            }
            break;
        default:
            INFO("Unrecognized popup menu item type!");
            break;
        }
    }
}

static Eina_Bool
on_popup_menu_show(Ewk_View_Smart_Data *smartData, Eina_Rectangle rect, Ewk_Text_Direction text_direction, double page_scale_factor, Ewk_Popup_Menu *ewk_menu)
{
    Browser_Window *window = window_find_with_ewk_view(smartData->self);

    if (window->popup.elm_menu)
        evas_object_del(window->popup.elm_menu);

    window->popup.elm_menu = elm_menu_add(window->elm_window);
    window->popup.ewk_menu = ewk_menu;

    popup_menu_populate(window->popup.elm_menu, ewk_menu, window);

    INFO("Showing popup menu at (%d, %d)", rect.x, rect.y);
    elm_menu_move(window->popup.elm_menu, rect.x, rect.y);
    evas_object_show(window->popup.elm_menu);

    return EINA_TRUE;
}

static Eina_Bool
on_popup_menu_hide(Ewk_View_Smart_Data *smartData)
{
    Browser_Window *window = window_find_with_ewk_view(smartData->self);

    if (!window->popup.elm_menu)
        return EINA_FALSE;

    elm_menu_close(window->popup.elm_menu);
    window->popup.ewk_menu = NULL;

    return EINA_TRUE;
}

static Eina_Bool on_window_geometry_get(Ewk_View_Smart_Data *sd, Evas_Coord *x, Evas_Coord *y, Evas_Coord *width, Evas_Coord *height)
{
    Browser_Window *window = window_find_with_ewk_view(sd->self);

    evas_object_geometry_get(window->elm_window, x, y, width, height);

    return EINA_TRUE;
}

static Eina_Bool on_window_geometry_set(Ewk_View_Smart_Data *sd, Evas_Coord x, Evas_Coord y, Evas_Coord width, Evas_Coord height)
{
    Browser_Window *window = window_find_with_ewk_view(sd->self);

    evas_object_move(window->elm_window, x, y);
    evas_object_resize(window->elm_window, width, height);

    return EINA_TRUE;
}

typedef struct {
    Evas_Object *ewk_view;
    Evas_Object *permission_popup;
} PermissionData;

static void
on_fullscreen_accept(void *user_data, Evas_Object *obj, void *event_info)
{
    PermissionData *permission_data = (PermissionData *)user_data;
    Browser_Window *window = window_find_with_ewk_view(permission_data->ewk_view);

    elm_win_resize_object_del(window->elm_window, permission_data->permission_popup);
    evas_object_del(permission_data->permission_popup);
    evas_object_focus_set(permission_data->ewk_view, EINA_TRUE);
    free(permission_data);
}

static void
on_fullscreen_deny(void *user_data, Evas_Object *obj, void *event_info)
{
    PermissionData *permission_data = (PermissionData *)user_data;
    Browser_Window *window = window_find_with_ewk_view(permission_data->ewk_view);

    ewk_view_fullscreen_exit(permission_data->ewk_view);
    elm_win_resize_object_del(window->elm_window, permission_data->permission_popup);
    evas_object_del(permission_data->permission_popup);
    evas_object_focus_set(permission_data->ewk_view, EINA_TRUE);
    free(permission_data);
}

static Eina_Bool on_fullscreen_enter(Ewk_View_Smart_Data *sd, Ewk_Security_Origin *origin)
{
    Browser_Window *window = window_find_with_ewk_view(sd->self);

    /* Go fullscreen */
    toggle_window_fullscreen(window, EINA_TRUE);

    /* Show user popup */
    Evas_Object *permission_popup = elm_popup_add(window->elm_window);
    evas_object_size_hint_weight_set(permission_popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

    Eina_Strbuf *message = eina_strbuf_new();
    eina_strbuf_append_printf(message, "%s is now fullscreen.<br>Press ESC at any time to exit fullscreen.<br>Allow fullscreen?", ewk_security_origin_host_get(origin));
    elm_object_text_set(permission_popup, eina_strbuf_string_get(message));
    eina_strbuf_free(message);
    elm_object_part_text_set(permission_popup, "title,text", "Fullscreen Permission");

    /* Popup buttons */
    PermissionData *permission_data = (PermissionData *)malloc(sizeof(PermissionData));
    permission_data->ewk_view = window->ewk_view;
    permission_data->permission_popup = permission_popup;
    Evas_Object *accept_button = elm_button_add(permission_popup);
    elm_object_text_set(accept_button, "Accept");
    elm_object_part_content_set(permission_popup, "button1", accept_button);
    evas_object_smart_callback_add(accept_button, "clicked", on_fullscreen_accept, permission_data);

    Evas_Object *deny_button = elm_button_add(permission_popup);
    elm_object_text_set(deny_button, "Deny");
    elm_object_part_content_set(permission_popup, "button2", deny_button);
    evas_object_smart_callback_add(deny_button, "clicked", on_fullscreen_deny, permission_data);
    elm_win_resize_object_add(window->elm_window, permission_popup);
    evas_object_show(permission_popup);

    return EINA_TRUE;
}

static Eina_Bool on_fullscreen_exit(Ewk_View_Smart_Data *sd)
{
    Browser_Window *window = window_find_with_ewk_view(sd->self);

    toggle_window_fullscreen(window, EINA_FALSE);

    return EINA_TRUE;
}

static Evas_Object *
on_window_create(Ewk_View_Smart_Data *smartData, Ewk_View_Configuration* configuration, const Ewk_Window_Features *window_features)
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    ewk_window_features_geometry_get(window_features, &x, &y, &width, &height);

    if (!width)
        width = window_width;

    if (!height)
        height = window_height;

    Browser_Window *window = window_create(configuration, width, height);
    Evas_Object *new_view = window->ewk_view;

    windows = eina_list_append(windows, window);

    INFO("minibrowser: location(%d,%d) size=(%d,%d)", x, y, width, height);

    return new_view;
}

static void
on_window_close(Ewk_View_Smart_Data *smartData)
{
    Browser_Window *window = window_find_with_ewk_view(smartData->self);
    window_close(window);
}

static void
context_menu_item_selected_cb(void *data, Evas_Object *obj, void *event_info)
{
    if (!data) {
        ERROR("Context menu callback data is NULL.");
        return;
    }

    Ewk_Context_Menu_Item *ewk_item = (Ewk_Context_Menu_Item *)data;
    INFO("Selected context menu item: %s.", ewk_context_menu_item_title_get(ewk_item));
    ewk_context_menu_item_select(ewk_context_menu_item_parent_menu_get(ewk_item), ewk_item);
    ewk_context_menu_hide(ewk_context_menu_item_parent_menu_get(ewk_item));
}

static void
context_menu_populate(Evas_Object* context_menu, Ewk_Context_Menu *ewk_menu, Elm_Object_Item *parent_item)
{
    if (!context_menu || !ewk_menu) {
        ERROR("Necessary objects are NULL.");
        return;
    }

    const Eina_List *list = ewk_context_menu_items_get(ewk_menu);
    const Eina_List *l;
    void *data;

    Ewk_Context_Menu_Item *ewk_item;
    Elm_Object_Item *elm_menu_item;
    Evas_Object *elm_check_item;

    EINA_LIST_FOREACH(list, l, data) {
        ewk_item = (Ewk_Context_Menu_Item *)data;
        switch (ewk_context_menu_item_type_get(ewk_item)) {
        case EWK_ACTION_TYPE:
            elm_menu_item = elm_menu_item_add(context_menu, parent_item, NULL, ewk_context_menu_item_title_get(ewk_item), context_menu_item_selected_cb, ewk_item);
            break;
        case EWK_CHECKABLE_ACTION_TYPE:
            elm_check_item = elm_check_add(context_menu);
            elm_menu_item = elm_menu_item_add(context_menu, parent_item, NULL, ewk_context_menu_item_title_get(ewk_item), context_menu_item_selected_cb, ewk_item);
            elm_object_item_content_set(elm_menu_item, elm_check_item);
            elm_check_state_set(elm_check_item, ewk_context_menu_item_checked_get(ewk_item));
            break;
        case EWK_SUBMENU_TYPE:
            elm_menu_item = elm_menu_item_add(context_menu, parent_item, NULL, ewk_context_menu_item_title_get(ewk_item), NULL, ewk_item);
            if (elm_menu_item)
                context_menu_populate(context_menu, ewk_context_menu_item_submenu_get(ewk_item), elm_menu_item);
            break;
        default:
            continue;
        }
        elm_object_item_disabled_set(elm_menu_item, !ewk_context_menu_item_enabled_get(ewk_item));
    }
}

static Eina_Bool
on_context_menu_show(Ewk_View_Smart_Data *sd, Evas_Coord x, Evas_Coord y, Ewk_Context_Menu *menu)
{
    Browser_Window *window = window_find_with_ewk_view(sd->self);

    if (!window || !menu) {
        ERROR("Necessary objects are NULL.");
        return EINA_FALSE;
    }

    window->context_menu.elm_menu = elm_menu_add(window->elm_window);

    if (!window->context_menu.elm_menu) {
        ERROR("Could not create menu widget.");
        return EINA_FALSE;
    }

    window->context_menu.ewk_menu = menu;

    context_menu_populate(window->context_menu.elm_menu, menu, NULL);

    INFO("Showing context menu at (%d, %d).", x, y);
    elm_menu_move(window->context_menu.elm_menu, x, y);
    evas_object_show(window->context_menu.elm_menu);

    return EINA_TRUE;
}

static Eina_Bool
on_context_menu_hide(Ewk_View_Smart_Data *sd)
{
    Browser_Window *window = window_find_with_ewk_view(sd->self);

    if (!window || !window->context_menu.elm_menu) {
        ERROR("Necessary objects are NULL.");
        return EINA_FALSE;
    }

    elm_menu_close(window->context_menu.elm_menu);
    evas_object_del(window->context_menu.elm_menu);
    window->context_menu.elm_menu = NULL;
    window->context_menu.ewk_menu = NULL;

    return EINA_TRUE;
}

static void
auth_popup_close(Auth_Data *auth_data)
{
    ewk_object_unref(auth_data->request);
    evas_object_del(auth_data->popup);
    free(auth_data);
}

static void
on_auth_cancel(void *user_data, Evas_Object *obj, void *event_info)
{
    Auth_Data *auth_data = (Auth_Data *)user_data;

    ewk_auth_request_cancel(auth_data->request);

    auth_popup_close(auth_data);
}

static void
on_auth_ok(void *user_data, Evas_Object *obj, void *event_info)
{
    Auth_Data *auth_data = (Auth_Data *)user_data;

    const char *username = elm_entry_entry_get(auth_data->username_entry);
    const char *password = elm_entry_entry_get(auth_data->password_entry);
    ewk_auth_request_authenticate(auth_data->request, username, password);

    auth_popup_close(auth_data);
}

static void
on_authentication_request(void *user_data, Evas_Object *obj, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    Ewk_Auth_Request *request = ewk_object_ref((Ewk_Auth_Request *)event_info);

    Auth_Data *auth_data = (Auth_Data *)malloc(sizeof(Auth_Data));
    auth_data->request = request;

    Evas_Object *auth_popup = elm_popup_add(window->elm_window);
    auth_data->popup = auth_popup;
    evas_object_size_hint_weight_set(auth_popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_object_part_text_set(auth_popup, "title,text", "Authentication Required");

    /* Popup Content */
    Evas_Object *vbox = elm_box_add(auth_popup);
    elm_box_padding_set(vbox, 0, 4);
    evas_object_size_hint_weight_set(vbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(vbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_object_content_set(auth_popup, vbox);
    evas_object_show(vbox);

    /* Authentication message */
    Evas_Object *label = elm_label_add(auth_popup);
    elm_label_line_wrap_set(label, ELM_WRAP_WORD);
    Eina_Strbuf *auth_text = eina_strbuf_new();
    const char *host = ewk_auth_request_host_get(request);
    const char *realm = ewk_auth_request_realm_get(request);
    eina_strbuf_append_printf(auth_text, "A username and password are being requested by %s. The site says: \"%s\"", host, realm ? realm : "");
    elm_object_text_set(label, eina_strbuf_string_get(auth_text));
    eina_strbuf_free(auth_text);
    evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_end(vbox, label);
    evas_object_show(label);

    /* Credential table */
    Evas_Object *table = elm_table_add(auth_popup);
    elm_table_padding_set(table, 2, 2);
    elm_table_homogeneous_set(table, EINA_TRUE);
    evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_end(vbox, table);
    evas_object_show(table);

    /* Username row */
    Evas_Object *username_label = elm_label_add(auth_popup);
    elm_object_text_set(username_label, "Username:");
    evas_object_size_hint_weight_set(username_label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(username_label, 1, EVAS_HINT_FILL);
    elm_table_pack(table, username_label, 0, 0, 1, 1);
    evas_object_show(username_label);

    Evas_Object *username_entry = elm_entry_add(auth_popup);
    auth_data->username_entry = username_entry;
    elm_entry_scrollable_set(username_entry, EINA_TRUE);
    elm_entry_single_line_set(username_entry, EINA_TRUE);
    elm_entry_text_style_user_push(username_entry, "DEFAULT='font_size=18'");
    const char *suggested_username = ewk_auth_request_suggested_username_get(request);
    elm_entry_entry_set(username_entry, suggested_username ? suggested_username : "");
    evas_object_size_hint_weight_set(username_entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(username_entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_table_pack(table, username_entry, 1, 0, 2, 1);
    elm_object_focus_set(username_entry, EINA_TRUE);
    evas_object_show(username_entry);

    /* Password row */
    Evas_Object *password_label = elm_label_add(auth_popup);
    elm_object_text_set(password_label, "Password:");
    evas_object_size_hint_weight_set(password_label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(password_label, 1, EVAS_HINT_FILL);
    elm_table_pack(table, password_label, 0, 1, 1, 1);
    evas_object_show(password_label);

    Evas_Object *password_entry = elm_entry_add(auth_popup);
    auth_data->password_entry = password_entry;
    elm_entry_scrollable_set(password_entry, EINA_TRUE);
    elm_entry_single_line_set(password_entry, EINA_TRUE);
    elm_entry_password_set(password_entry, EINA_TRUE);
    elm_entry_text_style_user_push(password_entry, "DEFAULT='font_size=18'");
    evas_object_size_hint_weight_set(password_entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(password_entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_table_pack(table, password_entry, 1, 1, 2, 1);
    evas_object_show(password_entry);

    /* Popup buttons */
    Evas_Object *cancel_button = elm_button_add(auth_popup);
    elm_object_text_set(cancel_button, "Cancel");
    elm_object_part_content_set(auth_popup, "button1", cancel_button);
    evas_object_smart_callback_add(cancel_button, "clicked", on_auth_cancel, auth_data);
    Evas_Object *ok_button = elm_button_add(auth_popup);
    elm_object_text_set(ok_button, "OK");
    elm_object_part_content_set(auth_popup, "button2", ok_button);
    evas_object_smart_callback_add(ok_button, "clicked", on_auth_ok, auth_data);
    evas_object_show(auth_popup);
}

static void
on_search_text_found(void *user_data, Evas_Object *obj, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    const int *match_count = (const int *)(event_info);

    if (*match_count) {
        Eina_Strbuf *search_text = eina_strbuf_new();
        eina_strbuf_append_printf(search_text, "  %d  Matches Found " , *match_count);
        elm_object_text_set(window->search.search_field_count, eina_strbuf_string_get(search_text));
        eina_strbuf_free(search_text);
    } else
        elm_object_text_set(window->search.search_field_count, " No Matches Found");

    evas_object_focus_set(window->ewk_view, EINA_FALSE);
    elm_object_focus_set(window->search.search_field_count, EINA_FALSE);
    elm_object_focus_set(window->search.search_field, EINA_TRUE);
}

static void
on_tooltip_text_set(void *user_data, Evas_Object *obj, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    const char *message = (const char*)event_info;

    elm_object_tooltip_text_set(window->elm_window, message);
    window->tooltip.text_set = EINA_TRUE;
    window_tooltip_update(window);
}

static void
on_tooltip_text_unset(void *user_data, Evas_Object *obj, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    window_tooltip_hide(window);
    elm_object_tooltip_unset(window->elm_window);
    window->tooltip.text_set = EINA_FALSE;
}

static void
on_navigation_policy_decision(void *user_data, Evas_Object *obj, void *event_info)
{
    Ewk_Navigation_Policy_Decision *decision = (Ewk_Navigation_Policy_Decision *)event_info;

    if (ewk_navigation_policy_mouse_button_get(decision) == EWK_EVENT_MOUSE_BUTTON_MIDDLE) {
        Browser_Window *window = window_create(NULL, 0, 0);
        ewk_view_url_set(window->ewk_view, ewk_url_request_url_get(ewk_navigation_policy_request_get(decision)));
        windows = eina_list_append(windows, window);
        INFO("Mouse middle button pressed, open link in new window");

        ewk_navigation_policy_decision_reject(decision); 
    }
}

static void
on_home_button_clicked(void *user_data, Evas_Object *home_button, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    ewk_view_url_set(window->ewk_view, DEFAULT_URL);
}

static void
on_window_deletion(void *user_data, Evas_Object *elm_window, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    ewk_view_try_close(window->ewk_view);
}

static Evas_Object *
create_toolbar_button(Evas_Object *elm_window, const char *icon_name)
{
    Evas_Object *button = elm_button_add(elm_window);

    Evas_Object *icon = elm_icon_add(elm_window);
    elm_icon_standard_set(icon, icon_name);
    evas_object_size_hint_max_set(icon, TOOL_BAR_ICON_SIZE, TOOL_BAR_ICON_SIZE);
    evas_object_color_set(icon, 44, 44, 102, 128);
    evas_object_show(icon);
    elm_object_part_content_set(button, "icon", icon);
    evas_object_size_hint_min_set(button, TOOL_BAR_BUTTON_SIZE, TOOL_BAR_BUTTON_SIZE);

    return button;
}

static Browser_Window *window_create(Ewk_View_Configuration* configuration, int width, int height)
{
    Browser_Window *window = calloc(1, sizeof(Browser_Window));
    if (!window) {
        ERROR("Could not create browser window.");
        return NULL;
    }

    /* Initialize tooltip information */
    window->tooltip.show_timer = NULL;
    window->tooltip.activated = EINA_FALSE;
    window->tooltip.text_set = EINA_FALSE;
    window->tooltip.shown = EINA_FALSE;

    /* Create window */
    window->elm_window = elm_win_add(NULL, "minibrowser-window", ELM_WIN_BASIC);
    elm_win_title_set(window->elm_window, APP_NAME);
    evas_object_smart_callback_add(window->elm_window, "delete,request", on_window_deletion, window);

    /* Create window background */
    Evas_Object *bg = elm_bg_add(window->elm_window);
    elm_bg_color_set(bg, 193, 192, 191);
    evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(window->elm_window, bg);
    evas_object_show(bg);

    /* Create vertical layout */
    window->vertical_layout = elm_box_add(window->elm_window);
    elm_box_padding_set(window->vertical_layout, 0, 2);
    evas_object_size_hint_weight_set(window->vertical_layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(window->elm_window, window->vertical_layout);
    evas_object_show(window->vertical_layout);

    /* Create horizontal layout for top bar */
    window->horizontal_layout = elm_box_add(window->elm_window);
    elm_box_horizontal_set(window->horizontal_layout, EINA_TRUE);
    evas_object_size_hint_weight_set(window->horizontal_layout, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(window->horizontal_layout, EVAS_HINT_FILL, 0.0);
    elm_box_pack_end(window->vertical_layout, window->horizontal_layout);
    evas_object_show(window->horizontal_layout);

    /* Create Back button */
    window->back_button = create_toolbar_button(window->elm_window, "arrow_left");
    evas_object_smart_callback_add(window->back_button, "clicked", on_back_button_clicked, window);
    evas_object_smart_callback_add(window->back_button, "repeated", on_back_button_longpress, window);
    elm_object_tooltip_text_set(window->back_button, "Click to go back, longpress to see session history");
    elm_object_tooltip_window_mode_set(window->back_button, EINA_TRUE);
    elm_object_disabled_set(window->back_button, EINA_TRUE);
    evas_object_size_hint_weight_set(window->back_button, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(window->back_button, 0.0, 0.5);
    elm_box_pack_end(window->horizontal_layout, window->back_button);
    evas_object_show(window->back_button);

    /* Create Forward button */
    window->forward_button = create_toolbar_button(window->elm_window, "arrow_right");
    evas_object_smart_callback_add(window->forward_button, "clicked", on_forward_button_clicked, window);
    evas_object_smart_callback_add(window->forward_button, "repeated", on_forward_button_longpress, window);
    elm_object_tooltip_text_set(window->forward_button, "Click to go forward, longpress to see session history");
    elm_object_tooltip_window_mode_set(window->forward_button, EINA_TRUE);
    elm_object_disabled_set(window->forward_button, EINA_TRUE);
    evas_object_size_hint_weight_set(window->forward_button, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(window->forward_button, 0.0, 0.5);
    elm_box_pack_end(window->horizontal_layout, window->forward_button);
    evas_object_show(window->forward_button);

    /* Create URL bar */
    window->url_bar = elm_entry_add(window->elm_window);
    elm_entry_scrollable_set(window->url_bar, EINA_TRUE);
    elm_scroller_policy_set(window->url_bar, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
    elm_entry_single_line_set(window->url_bar, EINA_TRUE);
    elm_entry_cnp_mode_set(window->url_bar, ELM_CNP_MODE_PLAINTEXT);
    elm_entry_text_style_user_push(window->url_bar, "DEFAULT='font_size=18'");
    evas_object_smart_callback_add(window->url_bar, "activated", on_url_bar_activated, window);
    evas_object_smart_callback_add(window->url_bar, "clicked", on_url_bar_clicked, window);
    evas_object_size_hint_weight_set(window->url_bar, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(window->url_bar, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_end(window->horizontal_layout, window->url_bar);
    evas_object_show(window->url_bar);

    /* Create Refresh button */
    Evas_Object *refresh_button = create_toolbar_button(window->elm_window, "refresh");
    evas_object_smart_callback_add(refresh_button, "clicked", on_refresh_button_clicked, window);
    elm_object_tooltip_text_set(refresh_button, "Reload page");
    elm_object_tooltip_window_mode_set(refresh_button, EINA_TRUE);
    evas_object_size_hint_weight_set(refresh_button, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(refresh_button, 1.0, 0.5);
    elm_box_pack_end(window->horizontal_layout, refresh_button);
    evas_object_show(refresh_button);

    /* Create Stop button */
    Evas_Object *stop_button = create_toolbar_button(window->elm_window, "close");
    evas_object_smart_callback_add(stop_button, "clicked", on_stop_button_clicked, window);
    elm_object_tooltip_text_set(stop_button, "Stop page load");
    elm_object_tooltip_window_mode_set(stop_button, EINA_TRUE);
    evas_object_size_hint_weight_set(stop_button, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(stop_button, 1.0, 0.5);
    elm_box_pack_end(window->horizontal_layout, stop_button);
    evas_object_show(stop_button);

    /* Create Home button */
    Evas_Object *home_button = create_toolbar_button(window->elm_window, "home");
    evas_object_smart_callback_add(home_button, "clicked", on_home_button_clicked, window);
    elm_object_tooltip_text_set(home_button, "Home page");
    elm_object_tooltip_window_mode_set(home_button, EINA_TRUE);
    evas_object_size_hint_weight_set(home_button, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(home_button, 1.0, 0.5);
    elm_box_pack_end(window->horizontal_layout, home_button);
    evas_object_show(home_button);

    /* Create Search bar */
    window->search.search_bar = elm_box_add(window->elm_window);
    elm_box_horizontal_set(window->search.search_bar, EINA_TRUE);
    evas_object_size_hint_min_set(window->search.search_bar, SEARCH_FIELD_SIZE + 2 * SEARCH_BUTTON_SIZE, 0);
    evas_object_size_hint_align_set(window->search.search_bar, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_end(window->vertical_layout, window->search.search_bar);

    /* Create Search field */
    window->search.search_field = elm_entry_add(window->elm_window);
    elm_entry_scrollable_set(window->search.search_field, EINA_TRUE);
    elm_scroller_policy_set(window->search.search_field, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
    elm_entry_single_line_set(window->search.search_field, EINA_TRUE);
    elm_entry_cnp_mode_set(window->search.search_field, ELM_CNP_MODE_PLAINTEXT);
    elm_entry_text_style_user_push(window->search.search_field, "DEFAULT='font_size=14'");
    evas_object_smart_callback_add(window->search.search_field, "activated", on_search_field_activated, window);
    evas_object_smart_callback_add(window->search.search_field, "changed", on_search_field_activated, window);
    evas_object_smart_callback_add(window->search.search_field, "aborted", on_search_field_aborted, window);
    evas_object_smart_callback_add(window->search.search_field, "clicked", on_search_field_clicked, window);
    evas_object_size_hint_weight_set(window->search.search_field, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(window->search.search_field, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_end(window->search.search_bar, window->search.search_field);

    /* Create Search backward button */
    window->search.backward_button = create_toolbar_button(window->elm_window, "arrow_up");
    evas_object_smart_callback_add(window->search.backward_button, "clicked", on_search_backward_button_clicked, window);
    elm_object_disabled_set(window->search.backward_button, EINA_FALSE);
    evas_object_size_hint_weight_set(window->search.backward_button, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(window->search.backward_button, 1.0, 0.5);
    evas_object_size_hint_min_set(window->search.backward_button, SEARCH_BUTTON_SIZE, SEARCH_BUTTON_SIZE);
    elm_box_pack_end(window->search.search_bar, window->search.backward_button);

    /* Create Search forwardward button */
    window->search.forward_button = create_toolbar_button(window->elm_window, "arrow_down");
    evas_object_smart_callback_add(window->search.forward_button, "clicked", on_search_forward_button_clicked, window);
    elm_object_disabled_set(window->search.forward_button, EINA_FALSE);
    evas_object_size_hint_weight_set(window->search.forward_button, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(window->search.forward_button, 1.0, 0.5);
    evas_object_size_hint_min_set(window->search.forward_button, SEARCH_BUTTON_SIZE, SEARCH_BUTTON_SIZE);
    elm_box_pack_end(window->search.search_bar, window->search.forward_button);

    /* Create Search count field */
    window->search.search_field_count = elm_label_add(window->elm_window);
    evas_object_size_hint_weight_set(window->search.search_field_count, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(window->search.search_field_count, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_object_text_set(window->search.search_field_count, "");
    elm_box_pack_end(window->search.search_bar, window->search.search_field_count);

    /* Create Search Case Sensitive Option check box */
    window->search.search_case_check_box = elm_check_add(window->elm_window);
    elm_object_text_set(window->search.search_case_check_box, "Case Sensitive");
    evas_object_smart_callback_add(window->search.search_case_check_box, "changed", on_search_case_option_changed, window);
    elm_box_pack_end(window->search.search_bar, window->search.search_case_check_box);

    /* Create Search Word Start Option check box */
    window->search.search_word_start_check_box = elm_check_add(window->elm_window);
    elm_object_text_set(window->search.search_word_start_check_box, "Only Word Start");
    evas_object_smart_callback_add(window->search.search_word_start_check_box, "changed", on_search_word_start_option_changed, window);
    elm_box_pack_end(window->search.search_bar, window->search.search_word_start_check_box);

    /* Create Search close button */
    window->search.close_button = create_toolbar_button(window->elm_window, "close");
    evas_object_smart_callback_add(window->search.close_button, "clicked", on_search_close_button_clicked, window);
    elm_object_disabled_set(window->search.close_button, EINA_FALSE);
    evas_object_size_hint_weight_set(window->search.close_button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(window->search.close_button, 1.0, 0.5);
    evas_object_size_hint_min_set(window->search.close_button, SEARCH_BUTTON_SIZE, SEARCH_BUTTON_SIZE);
    elm_box_pack_end(window->search.search_bar, window->search.close_button);

    /* Create history box */
    window->history.history_box = elm_box_add(window->elm_window);
    evas_object_size_hint_aspect_set(window->history.history_box, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
    evas_object_size_hint_weight_set(window->history.history_box, EVAS_HINT_EXPAND , EVAS_HINT_EXPAND);

    /* Create history list */
    window->history.history_list = elm_genlist_add(window->elm_window);
    evas_object_size_hint_weight_set(window->history.history_list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(window->history.history_list, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_win_resize_object_add(window->elm_window, window->history.history_box);
    elm_box_pack_end(window->history.history_box, window->history.history_list);
    
    /* Create ewk_view */
    Ewk_View_Smart_Class *ewkViewClass = miniBrowserViewSmartClass();
    ewkViewClass->run_javascript_alert = on_javascript_alert;
    ewkViewClass->run_javascript_confirm = on_javascript_confirm;
    ewkViewClass->run_javascript_prompt = on_javascript_prompt;
    ewkViewClass->run_javascript_before_unload_confirm = on_javascript_before_unload_confirm;
    ewkViewClass->window_geometry_get = on_window_geometry_get;
    ewkViewClass->window_geometry_set = on_window_geometry_set;
    ewkViewClass->fullscreen_enter = on_fullscreen_enter;
    ewkViewClass->fullscreen_exit = on_fullscreen_exit;
    ewkViewClass->window_create = on_window_create;
    ewkViewClass->window_close = on_window_close;
    ewkViewClass->popup_menu_show = on_popup_menu_show;
    ewkViewClass->popup_menu_hide = on_popup_menu_hide;
    ewkViewClass->context_menu_show = on_context_menu_show;
    ewkViewClass->context_menu_hide = on_context_menu_hide;
    ewkViewClass->input_picker_color_request = on_color_picker_request;
    ewkViewClass->input_picker_color_dismiss = on_color_picker_dismiss;

    Evas *evas = evas_object_evas_get(window->elm_window);
    Evas_Smart *smart = evas_smart_class_new(&ewkViewClass->sc);
    window->ewk_view = ewk_view_add_with_configuration(evas, smart, configuration);

    Ewk_Context *context = ewk_view_context_get(window->ewk_view);
    ewk_favicon_database_icon_change_callback_add(ewk_context_favicon_database_get(context), on_icon_changed_cb, window);

    ewk_view_theme_set(window->ewk_view, DEFAULT_THEME_DIR "/default.edj");
    if (device_pixel_ratio)
        ewk_view_device_pixel_ratio_set(window->ewk_view, (float)device_pixel_ratio);
    ewk_view_user_agent_set(window->ewk_view, user_agent_string);
    ewk_view_layout_fixed_set(window->ewk_view, fixed_layout_enabled);

    if (touch_events_enabled) {
        ewk_view_touch_events_enabled_set(window->ewk_view, EINA_TRUE);
        ewk_view_mouse_events_enabled_set(window->ewk_view, EINA_FALSE);
    }

    if (background_color_string) {
        int red, green, blue, alpha;

        if (sscanf(background_color_string, "%d:%d:%d:%d", &red, &green, &blue, &alpha))
            ewk_view_bg_color_set(window->ewk_view, red, green, blue, alpha);
    }

    /* Set the zoom level to default */
    window->current_zoom_level = DEFAULT_ZOOM_LEVEL;

    Ewk_Settings *settings = ewk_page_group_settings_get(ewk_view_page_group_get(window->ewk_view));
    ewk_settings_file_access_from_file_urls_allowed_set(settings, EINA_TRUE);
    ewk_settings_encoding_detector_enabled_set(settings, encoding_detector_enabled);
    ewk_settings_frame_flattening_enabled_set(settings, frame_flattening_enabled);
    ewk_settings_local_storage_enabled_set(settings, local_storage_enabled);
    INFO("HTML5 local storage is %s for this view.", local_storage_enabled ? "enabled" : "disabled");
    elm_win_fullscreen_set(window->elm_window, fullscreen_enabled);
    ewk_settings_developer_extras_enabled_set(settings, EINA_TRUE);
    ewk_settings_preferred_minimum_contents_width_set(settings, 0);
    ewk_text_checker_continuous_spell_checking_enabled_set(spell_checking_enabled);
    ewk_settings_web_security_enabled_set(settings, web_security_enabled);
    evas_object_smart_callback_add(window->ewk_view, "authentication,request", on_authentication_request, window);
    evas_object_smart_callback_add(window->ewk_view, "file,chooser,request", on_file_chooser_request, window);
    evas_object_smart_callback_add(window->ewk_view, "load,error", on_error, window);
    evas_object_smart_callback_add(window->ewk_view, "load,provisional,failed", on_error, window);
    evas_object_smart_callback_add(window->ewk_view, "load,progress", on_progress, window);
    evas_object_smart_callback_add(window->ewk_view, "title,changed", on_title_changed, window);
    evas_object_smart_callback_add(window->ewk_view, "url,changed", on_url_changed, window);
    evas_object_smart_callback_add(window->ewk_view, "back,forward,list,changed", on_back_forward_list_changed, window);
    evas_object_smart_callback_add(window->ewk_view, "text,found", on_search_text_found, window);
    evas_object_smart_callback_add(window->ewk_view, "tooltip,text,set", on_tooltip_text_set, window);
    evas_object_smart_callback_add(window->ewk_view, "tooltip,text,unset", on_tooltip_text_unset, window);
    evas_object_smart_callback_add(window->ewk_view, "policy,decision,navigation", on_navigation_policy_decision, window);

    evas_object_event_callback_add(window->ewk_view, EVAS_CALLBACK_KEY_DOWN, on_key_down, window);
    evas_object_event_callback_add(window->ewk_view, EVAS_CALLBACK_MOUSE_DOWN, on_mouse_down, window);

    evas_object_size_hint_weight_set(window->ewk_view, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(window->ewk_view, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_before(window->vertical_layout, window->ewk_view, window->search.search_bar);
    evas_object_show(window->ewk_view);

    evas_object_resize(window->elm_window, width ? width : window_width, height ? height : window_height);
    evas_object_show(window->elm_window);
    search_box_hide(window);

    view_focus_set(window, EINA_TRUE);
    // Prevent window from stealing web view's focus on start up.
    elm_object_focus_allow_set(window->elm_window, EINA_FALSE);

    evas_object_event_callback_add(window->ewk_view, EVAS_CALLBACK_MOUSE_IN, on_mouse_in, window);
    evas_object_event_callback_add(window->ewk_view, EVAS_CALLBACK_MOUSE_OUT, on_mouse_out, window);
    evas_object_event_callback_add(window->ewk_view, EVAS_CALLBACK_MOUSE_MOVE, on_mouse_move, window);
    evas_object_event_callback_add(window->ewk_view, EVAS_CALLBACK_MOUSE_WHEEL, on_mouse_wheel, window);
    evas_object_event_callback_add(window->elm_window, EVAS_CALLBACK_RESIZE, on_window_resize, window);
    
    elm_button_autorepeat_set(window->back_button, EINA_TRUE);
    elm_button_autorepeat_set(window->forward_button, EINA_TRUE);
    elm_button_autorepeat_initial_timeout_set(window->back_button, LONGPRESS_INTERVAL_SECONDS);
    elm_button_autorepeat_initial_timeout_set(window->forward_button, LONGPRESS_INTERVAL_SECONDS);

    return window;
}

static Ewk_Cookie_Accept_Policy
parse_cookies_policy(const char *input_string)
{
    if (!strcmp(input_string, "always")) 
        return EWK_COOKIE_ACCEPT_POLICY_ALWAYS;
    if (!strcmp(input_string, "never"))
        return EWK_COOKIE_ACCEPT_POLICY_NEVER;
    if (strcmp(input_string, "no-third-party"))
        INFO("Unrecognized type for cookies policy: %s.", input_string);
    return EWK_COOKIE_ACCEPT_POLICY_NO_THIRD_PARTY;
}

static void
parse_window_size(const char *input_string, int *width, int *height)
{
    static const unsigned max_length = 4;
    int parsed_width;
    int parsed_height;
    char **arr;
    unsigned elements;

    arr = eina_str_split_full(input_string, "x", 0, &elements);

    if (elements == 2 && (strlen(arr[0]) <= max_length) && (strlen(arr[1]) <= max_length)) {
        parsed_width = atoi(arr[0]);
        if (width && parsed_width)
            *width = parsed_width;

        parsed_height = atoi(arr[1]);
        if (height && parsed_height)
            *height = parsed_height;
    }

    free(arr[0]);
    free(arr);
}

EAPI_MAIN int
elm_main(int argc, char *argv[])
{
    int args = 1;
    unsigned char quitOption = 0;
    Browser_Window *window;
    char *window_size_string = NULL;
    char *cookies_policy_string = NULL;

    Ecore_Getopt_Value values[] = {
        ECORE_GETOPT_VALUE_STR(evas_engine_name),
        ECORE_GETOPT_VALUE_STR(window_size_string),
        ECORE_GETOPT_VALUE_STR(user_agent_string),
        ECORE_GETOPT_VALUE_STR(extensions_path),
        ECORE_GETOPT_VALUE_DOUBLE(device_pixel_ratio),
        ECORE_GETOPT_VALUE_BOOL(quitOption),
        ECORE_GETOPT_VALUE_BOOL(encoding_detector_enabled),
        ECORE_GETOPT_VALUE_STR(background_color_string),
        ECORE_GETOPT_VALUE_BOOL(frame_flattening_enabled),
        ECORE_GETOPT_VALUE_BOOL(local_storage_enabled),
        ECORE_GETOPT_VALUE_BOOL(fullscreen_enabled),
        ECORE_GETOPT_VALUE_BOOL(spell_checking_enabled),
        ECORE_GETOPT_VALUE_BOOL(touch_events_enabled),
        ECORE_GETOPT_VALUE_BOOL(fixed_layout_enabled),
        ECORE_GETOPT_VALUE_STR(cookies_policy_string),
        ECORE_GETOPT_VALUE_BOOL(web_security_enabled),
        ECORE_GETOPT_VALUE_BOOL(separated_process_enabled),
        ECORE_GETOPT_VALUE_BOOL(quitOption),
        ECORE_GETOPT_VALUE_BOOL(quitOption),
        ECORE_GETOPT_VALUE_BOOL(quitOption),
        ECORE_GETOPT_VALUE_NONE
    };

    efreet_cache_update = 0;

    if (!ewk_init())
        return EXIT_FAILURE;

    _log_domain_id = eina_log_domain_register("minibrowser", EINA_COLOR_YELLOW);

    ewk_view_smart_class_set(miniBrowserViewSmartClass());

    ecore_app_args_set(argc, (const char **) argv);
    args = ecore_getopt_parse(&options, values, argc, argv);

    if (args < 0)
        return quit(EINA_FALSE, "Could not parse options.");

    if (quitOption)
        return quit(EINA_TRUE, NULL);

    if (evas_engine_name)
        elm_config_accel_preference_set(evas_engine_name);
    else {
        evas_engine_name = "opengl:depth24:stencil8";
        elm_config_accel_preference_set(evas_engine_name);
    }

    Ewk_Context *context = ewk_context_default_get();

    if (separated_process_enabled)
        ewk_context_process_model_set(context, EWK_PROCESS_MODEL_MULTIPLE_SECONDARY);

    // Enable favicon database.
    ewk_context_favicon_database_directory_set(context, NULL);

    // Use a proper cache model.
    ewk_context_cache_model_set(context, EWK_CACHE_MODEL_PRIMARY_WEBBROWSER);

    if (cookies_policy_string) {
        Ewk_Cookie_Accept_Policy cookie_policy = parse_cookies_policy(cookies_policy_string);
        ewk_cookie_manager_accept_policy_set(ewk_context_cookie_manager_get(context), cookie_policy);

        if (cookie_policy == EWK_COOKIE_ACCEPT_POLICY_ALWAYS) {
            const char cookie_storage_directory[] = "/tmp/ewebkit2_minibrowser_cookie/";
            mkdir(cookie_storage_directory, S_IRWXU);
            char storage_name[64];
            snprintf(storage_name, sizeof(storage_name), "%stxt-cookie", cookie_storage_directory);
            ewk_cookie_manager_persistent_storage_set(ewk_context_cookie_manager_get(context), storage_name, EWK_COOKIE_PERSISTENT_STORAGE_TEXT);
        }
    }

    if (window_size_string)
        parse_window_size(window_size_string, &window_width, &window_height);

    window = window_create(NULL, 0, 0);
    if (!window)
        return quit(EINA_FALSE, "Could not create browser window.");

    // Set callbacks for download events.
    ewk_context_download_callbacks_set(context, on_download_request, on_download_failed, 0, on_download_finished, window);

    if (args < argc) {
        char *url = url_from_user_input(argv[args]);
        url_load_from_user_input(window->ewk_view, url);
        free(url);
    } else
        ewk_view_url_set(window->ewk_view, DEFAULT_URL);

    windows = eina_list_append(windows, window);

    elm_run();

    return quit(EINA_TRUE, NULL);
}
ELM_MAIN()

