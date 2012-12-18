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
#include "url_utils.h"
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Eina.h>
#include <Elementary.h>
#include <Evas.h>

static const char DEFAULT_URL[] = "http://www.google.com/";
static const char APP_NAME[] = "EFL MiniBrowser";
static const int TOOL_BAR_ICON_SIZE = 24;
static const int TOOL_BAR_BUTTON_SIZE = 32;

#define info(format, args...)       \
    do {                            \
        if (verbose)                \
            printf(format, ##args); \
    } while (0)

static int verbose = 1;
static Eina_List *windows = NULL;
static char *evas_engine_name = NULL;
static Eina_Bool encoding_detector_enabled = EINA_FALSE;
static Eina_Bool frame_flattening_enabled = EINA_FALSE;
static Eina_Bool local_storage_enabled = EINA_TRUE;
static int window_width = 800;
static int window_height = 600;
/* Default value of device_pixel_ratio is '0' so that we don't set custom device
 * scale factor unless it's required by the User. */
static double device_pixel_ratio = 0;
static Eina_Bool legacy_behavior_enabled = EINA_FALSE;

#define DEFAULT_ZOOM_LEVEL 5 // Set default zoom level to 1.0 (index 5 on zoomLevels).
// The zoom values are chosen to be like in Mozilla Firefox 3.
const static float zoomLevels[] = {0.3, 0.5, 0.67, 0.8, 0.9, 1.0, 1.1, 1.2, 1.33, 1.5, 1.7, 2.0, 2.4, 3.0};

static Eina_Bool
zoom_level_set(Evas_Object *webview, int level)
{
    if (level < 0  || level >= sizeof(zoomLevels) / sizeof(float))
        return EINA_FALSE;

    Evas_Coord ox, oy, mx, my, cx, cy;
    evas_pointer_canvas_xy_get(evas_object_evas_get(webview), &mx, &my); // Get current mouse position on window.
    evas_object_geometry_get(webview, &ox, &oy, NULL, NULL); // Get webview's position on window.
    cx = mx - ox; // current x position = mouse x position - webview x position
    cy = my - oy; // current y position = mouse y position - webview y position

    Eina_Bool result = ewk_view_scale_set(webview, zoomLevels[level], cx, cy);
    return result;
}

static Ewk_View_Smart_Class *miniBrowserViewSmartClass()
{
    static Ewk_View_Smart_Class ewkViewClass = EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION("MiniBrowser_View");
    return &ewkViewClass;
}

typedef struct _Browser_Window {
    Evas_Object *elm_window;
    Evas_Object *ewk_view;
    Evas_Object *url_bar;
    Evas_Object *back_button;
    Evas_Object *forward_button;
    int current_zoom_level; 
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
            ('e', "engine", "ecore-evas engine to use."),
        ECORE_GETOPT_STORE_STR
            ('s', "window-size", "window size in following format (width)x(height)."),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('b', "legacy", "Legacy mode", EINA_FALSE),
        ECORE_GETOPT_STORE_DOUBLE
            ('r', "device-pixel-ratio", "Ratio between the CSS units and device pixels."),
        ECORE_GETOPT_CALLBACK_NOARGS
            ('E', "list-engines", "list ecore-evas engines.",
             ecore_getopt_callback_ecore_evas_list_engines, NULL),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('c', "encoding-detector", "enable/disable encoding detector", EINA_FALSE),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('f', "flattening", "frame flattening.", EINA_FALSE),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('l', "local-storage", "HTML5 local storage support (enabled by default).", EINA_TRUE),
        ECORE_GETOPT_VERSION
            ('V', "version"),
        ECORE_GETOPT_COPYRIGHT
            ('R', "copyright"),
        ECORE_GETOPT_HELP
            ('h', "help"),
        ECORE_GETOPT_SENTINEL
    }
};

static Browser_Window *window_create(Evas_Object* opener, const char *url, int width, int height);

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

static void window_free(Browser_Window *window)
{
    evas_object_del(window->ewk_view);
    /* The elm_win will take care of freeing its children */
    evas_object_del(window->elm_window);
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
    Eina_Bool ctrlPressed = evas_key_modifier_is_set(evas_key_modifier_get(e), "Control");

    if (!strcmp(ev->key, "F1")) {
        info("Back (F1) was pressed\n");
        if (!ewk_view_back(ewk_view))
            info("Back ignored: No back history\n");
    } else if (!strcmp(ev->key, "F2")) {
        info("Forward (F2) was pressed\n");
        if (!ewk_view_forward(ewk_view))
            info("Forward ignored: No forward history\n");
    } else if (!strcmp(ev->key, "F3")) {
        currentEncoding = (currentEncoding + 1) % (sizeof(encodings) / sizeof(encodings[0]));
        info("Set encoding (F3) pressed. New encoding to %s", encodings[currentEncoding]);
        ewk_view_custom_encoding_set(ewk_view, encodings[currentEncoding]);
    } else if (!strcmp(ev->key, "F5")) {
        info("Reload (F5) was pressed, reloading.\n");
        ewk_view_reload(ewk_view);
    } else if (!strcmp(ev->key, "F6")) {
        info("Stop (F6) was pressed, stop loading.\n");
        ewk_view_stop(ewk_view);
    } else if  (!strcmp(ev->key, "F7")) {
        Ewk_Pagination_Mode mode =  ewk_view_pagination_mode_get(ewk_view);
        mode = (++mode) % (EWK_PAGINATION_MODE_BOTTOM_TO_TOP + 1);
        if (ewk_view_pagination_mode_set(ewk_view, mode))
            info("Change Pagination Mode (F7) was pressed, changed to: %d\n", mode);
        else
            info("Change Pagination Mode (F7) was pressed, but NOT changed!");
    } else if (!strcmp(ev->key, "n") && ctrlPressed) {
        info("Create new window (Ctrl+n) was pressed.\n");
        Browser_Window *window = window_create(0, DEFAULT_URL, 0, 0);
        // 0 equals default width and height.
        windows = eina_list_append(windows, window);
    } else if (!strcmp(ev->key, "i") && ctrlPressed) {
        info("Show Inspector (Ctrl+i) was pressed.\n");
        ewk_view_inspector_show(ewk_view);
    } else if (!strcmp(ev->key, "Escape")) {
        if (elm_win_fullscreen_get(window->elm_window))
            ewk_view_fullscreen_exit(ewk_view);
    } else if (ctrlPressed && (!strcmp(ev->key, "minus") || !strcmp(ev->key, "KP_Subtract"))) {
        if (zoom_level_set(ewk_view, window->current_zoom_level - 1))
            window->current_zoom_level--;
        info("Zoom out (Ctrl + '-') was pressed, zoom level became %.2f\n", zoomLevels[window->current_zoom_level]);
    } else if (ctrlPressed && (!strcmp(ev->key, "equal") || !strcmp(ev->key, "KP_Add"))) {
        if (zoom_level_set(ewk_view, window->current_zoom_level + 1))
            window->current_zoom_level++;
        info("Zoom in (Ctrl + '+') was pressed, zoom level became %.2f\n", zoomLevels[window->current_zoom_level]);
    } else if (ctrlPressed && !strcmp(ev->key, "0")) {
        if (zoom_level_set(ewk_view, DEFAULT_ZOOM_LEVEL))
            window->current_zoom_level = DEFAULT_ZOOM_LEVEL;
        info("Zoom to default (Ctrl + '0') was pressed, zoom level became %.2f\n", zoomLevels[window->current_zoom_level]);
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

    char *url = elm_entry_utf8_to_markup(ewk_view_url_get(window->ewk_view));
    elm_entry_entry_set(window->url_bar, url);

    free(url);
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
on_download_request(void *user_data, Evas_Object *ewk_view, void *event_info)
{
    Ewk_Download_Job *download = (Ewk_Download_Job *)event_info;

    // FIXME: The destination folder should be selected by the user but we set it to '/tmp' for now.
    Eina_Strbuf *destination_path = eina_strbuf_new();

    const char *suggested_name = ewk_download_job_suggested_filename_get(download);
    if (suggested_name && *suggested_name)
        eina_strbuf_append_printf(destination_path, "/tmp/%s", suggested_name);
    else {
        // Generate a unique file name since no name was suggested.
        char unique_path[] = "/tmp/downloaded-file.XXXXXX";
        eina_strbuf_append(destination_path, mktemp(unique_path));
    }

    ewk_download_job_destination_set(download, eina_strbuf_string_get(destination_path));
    info("Downloading: %s\n", eina_strbuf_string_get(destination_path));
    eina_strbuf_free(destination_path);
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
    Browser_Window *window = (Browser_Window *)user_data;
    Ewk_File_Chooser_Request *request = (Ewk_File_Chooser_Request *)event_info;

    // Show basic file picker which does not currently support multiple files
    // or MIME type filtering.
    Evas_Object *elm_window = elm_win_add(window->elm_window, "file-picker-window", ELM_WIN_DIALOG_BASIC);
    elm_win_title_set(elm_window, "File picker");
    elm_win_modal_set(elm_window, EINA_TRUE);

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
on_download_finished(void *user_data, Evas_Object *ewk_view, void *event_info)
{
    Ewk_Download_Job *download = (Ewk_Download_Job *)event_info;
    info("Download finished: %s\n",  ewk_download_job_destination_get(download));
}

static void
on_download_failed(void *user_data, Evas_Object *ewk_view, void *event_info)
{
    info("Download failed!\n");
}

static void
on_favicon_received(const char *page_url, Evas_Object *icon, void *event_info)
{
    Browser_Window *window = (Browser_Window *)event_info;
    if (strcmp(page_url, ewk_view_url_get(window->ewk_view)))
        return;

    /* Remove previous icon from URL bar */
    Evas_Object *old_icon = elm_object_part_content_unset(window->url_bar, "icon");
    if (old_icon) {
        evas_object_unref(old_icon);
        evas_object_del(old_icon);
    }

    /* Show new icon in URL bar */
    if (icon) {
        /* Workaround for icon display bug:
         * http://trac.enlightenment.org/e/ticket/1616 */
        evas_object_size_hint_min_set(icon, 48, 24);
        evas_object_image_filled_set(icon, EINA_FALSE);
        evas_object_image_fill_set(icon, 24, 0, 24, 24);
        elm_object_part_content_set(window->url_bar, "icon", icon);
        evas_object_ref(icon);
    }
}

static void
on_view_icon_changed(void *user_data, Evas_Object *ewk_view, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    /* Retrieve the view's favicon */
    Ewk_Context *context = ewk_view_context_get(ewk_view);
    Ewk_Favicon_Database *icon_database = ewk_context_favicon_database_get(context);

    const char *page_url = ewk_view_url_get(ewk_view);
    Evas *evas = evas_object_evas_get(ewk_view);
    ewk_favicon_database_async_icon_get(icon_database, page_url, evas, on_favicon_received, window);
}

static int
quit(Eina_Bool success, const char *msg)
{
    ewk_shutdown();
    elm_shutdown();

    if (msg)
        fputs(msg, (success) ? stdout : stderr);

    if (!success)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void
on_url_bar_activated(void *user_data, Evas_Object *url_bar, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    const char *markup_url = elm_entry_entry_get(url_bar);
    char *user_url = elm_entry_markup_to_utf8(markup_url);
    char *url = url_from_user_input(user_url);
    ewk_view_url_set(window->ewk_view, url);

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
on_back_button_clicked(void *user_data, Evas_Object *back_button, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    ewk_view_back(window->ewk_view);
    /* Update back button state */
    elm_object_disabled_set(back_button, !ewk_view_back_possible(window->ewk_view));
}

static void
on_forward_button_clicked(void *user_data, Evas_Object *forward_button, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    ewk_view_forward(window->ewk_view);
    /* Update forward button state */
    elm_object_disabled_set(forward_button, !ewk_view_forward_possible(window->ewk_view));
}

static void
on_refresh_button_clicked(void *user_data, Evas_Object *refresh_button, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    Evas *evas = evas_object_evas_get(refresh_button);
    Eina_Bool ctrlPressed = evas_key_modifier_is_set(evas_key_modifier_get(evas), "Control");
    if (ctrlPressed) {
        info("Reloading and bypassing cache...\n");
        ewk_view_reload_bypass_cache(window->ewk_view);
    } else {
        info("Reloading...\n");
        ewk_view_reload(window->ewk_view);
    }
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

    evas_object_del(permission_data->permission_popup);
    free(permission_data);
}

static void
on_fullscreen_deny(void *user_data, Evas_Object *obj, void *event_info)
{
    PermissionData *permission_data = (PermissionData *)user_data;

    ewk_view_fullscreen_exit(permission_data->ewk_view);
    evas_object_del(permission_data->permission_popup);
    free(permission_data);
}

static Eina_Bool on_fullscreen_enter(Ewk_View_Smart_Data *sd, Ewk_Security_Origin *origin)
{
    Browser_Window *window = window_find_with_ewk_view(sd->self);

    /* Go fullscreen */
    elm_win_fullscreen_set(window->elm_window, EINA_TRUE);

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

    evas_object_show(permission_popup);

    return EINA_TRUE;
}

static Eina_Bool on_fullscreen_exit(Ewk_View_Smart_Data *sd)
{
    Browser_Window *window = window_find_with_ewk_view(sd->self);

    elm_win_fullscreen_set(window->elm_window, EINA_FALSE);

    return EINA_TRUE;
}

static Evas_Object *
on_window_create(Ewk_View_Smart_Data *smartData, const char *url, const Ewk_Window_Features *window_features)
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

    Browser_Window *window = window_create(smartData->self, url, width, height);
    Evas_Object *new_view = window->ewk_view;

    windows = eina_list_append(windows, window);

    info("minibrowser: location(%d,%d) size=(%d,%d) url=%s\n", x, y, width, height, url);

    return new_view;
}

static void
on_window_close(Ewk_View_Smart_Data *smartData)
{
    Browser_Window *window = window_find_with_ewk_view(smartData->self);
    window_close(window);
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
on_tooltip_text_set(void *user_data, Evas_Object *obj, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    const char *message = (const char*)event_info;

    elm_object_tooltip_text_set(window->ewk_view, message);
    elm_object_tooltip_show(window->ewk_view);
}

static void
on_tooltip_text_unset(void *user_data, Evas_Object *obj, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    elm_object_tooltip_unset(window->ewk_view);
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
    window_close(window_find_with_elm_window(elm_window));
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

static Browser_Window *window_create(Evas_Object *opener, const char *url, int width, int height)
{
    Browser_Window *window = malloc(sizeof(Browser_Window));
    if (!window) {
        info("ERROR: could not create browser window.\n");
        return NULL;
    }

    /* Create window */
    window->elm_window = elm_win_add(NULL, "minibrowser-window", ELM_WIN_BASIC);
    elm_win_title_set(window->elm_window, APP_NAME);
    evas_object_smart_callback_add(window->elm_window, "delete,request", on_window_deletion, &window);

    /* Create window background */
    Evas_Object *bg = elm_bg_add(window->elm_window);
    elm_bg_color_set(bg, 193, 192, 191);
    evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(window->elm_window, bg);
    evas_object_show(bg);

    /* Create vertical layout */
    Evas_Object *vertical_layout = elm_box_add(window->elm_window);
    elm_box_padding_set(vertical_layout, 0, 2);
    evas_object_size_hint_weight_set(vertical_layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(window->elm_window, vertical_layout);
    evas_object_show(vertical_layout);

    /* Create horizontal layout for top bar */
    Evas_Object *horizontal_layout = elm_box_add(window->elm_window);
    elm_box_horizontal_set(horizontal_layout, EINA_TRUE);
    evas_object_size_hint_weight_set(horizontal_layout, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(horizontal_layout, EVAS_HINT_FILL, 0.0);
    elm_box_pack_end(vertical_layout, horizontal_layout);
    evas_object_show(horizontal_layout);

    /* Create Back button */
    window->back_button = create_toolbar_button(window->elm_window, "arrow_left");
    evas_object_smart_callback_add(window->back_button, "clicked", on_back_button_clicked, window);
    elm_object_disabled_set(window->back_button, EINA_TRUE);
    evas_object_size_hint_weight_set(window->back_button, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(window->back_button, 0.0, 0.5);
    elm_box_pack_end(horizontal_layout, window->back_button);
    evas_object_show(window->back_button);

    /* Create Forward button */
    window->forward_button = create_toolbar_button(window->elm_window, "arrow_right");
    evas_object_smart_callback_add(window->forward_button, "clicked", on_forward_button_clicked, window);
    elm_object_disabled_set(window->forward_button, EINA_TRUE);
    evas_object_size_hint_weight_set(window->forward_button, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(window->forward_button, 0.0, 0.5);
    elm_box_pack_end(horizontal_layout, window->forward_button);
    evas_object_show(window->forward_button);

    /* Create URL bar */
    window->url_bar = elm_entry_add(window->elm_window);
    elm_entry_scrollable_set(window->url_bar, EINA_TRUE);
    elm_entry_scrollbar_policy_set(window->url_bar, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
    elm_entry_single_line_set(window->url_bar, EINA_TRUE);
    elm_entry_cnp_mode_set(window->url_bar, ELM_CNP_MODE_PLAINTEXT);
    elm_entry_text_style_user_push(window->url_bar, "DEFAULT='font_size=18'");
    evas_object_smart_callback_add(window->url_bar, "activated", on_url_bar_activated, window);
    evas_object_smart_callback_add(window->url_bar, "clicked", on_url_bar_clicked, window);
    evas_object_size_hint_weight_set(window->url_bar, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(window->url_bar, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_end(horizontal_layout, window->url_bar);
    evas_object_show(window->url_bar);

    /* Create Refresh button */
    Evas_Object *refresh_button = create_toolbar_button(window->elm_window, "refresh");
    evas_object_smart_callback_add(refresh_button, "clicked", on_refresh_button_clicked, window);
    evas_object_size_hint_weight_set(refresh_button, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(refresh_button, 1.0, 0.5);
    elm_box_pack_end(horizontal_layout, refresh_button);
    evas_object_show(refresh_button);

    /* Create Home button */
    Evas_Object *home_button = create_toolbar_button(window->elm_window, "home");
    evas_object_smart_callback_add(home_button, "clicked", on_home_button_clicked, window);
    evas_object_size_hint_weight_set(home_button, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(home_button, 1.0, 0.5);
    elm_box_pack_end(horizontal_layout, home_button);
    evas_object_show(home_button);

    /* Create ewk_view */
    Ewk_View_Smart_Class *ewkViewClass = miniBrowserViewSmartClass();
    ewkViewClass->run_javascript_alert = on_javascript_alert;
    ewkViewClass->run_javascript_confirm = on_javascript_confirm;
    ewkViewClass->run_javascript_prompt = on_javascript_prompt;
    ewkViewClass->window_geometry_get = on_window_geometry_get;
    ewkViewClass->window_geometry_set = on_window_geometry_set;
    ewkViewClass->fullscreen_enter = on_fullscreen_enter;
    ewkViewClass->fullscreen_exit = on_fullscreen_exit;
    ewkViewClass->window_create = on_window_create;
    ewkViewClass->window_close = on_window_close;

    Evas *evas = evas_object_evas_get(window->elm_window);
    if (legacy_behavior_enabled) {
        // Use raw WK2 api to create a view using legacy mode.
        window->ewk_view = (Evas_Object*)WKViewCreate(evas, 0, 0);
    } else {
        Evas_Smart *smart = evas_smart_class_new(&ewkViewClass->sc);
        Ewk_Context* context = opener ? ewk_view_context_get(opener) : ewk_context_default_get();
        window->ewk_view = ewk_view_smart_add(evas, smart, context);
    }
    ewk_view_theme_set(window->ewk_view, THEME_DIR "/default.edj");
    if (device_pixel_ratio)
        ewk_view_device_pixel_ratio_set(window->ewk_view, (float)device_pixel_ratio);

    /* Set the zoom level to default */
    window->current_zoom_level = DEFAULT_ZOOM_LEVEL;

    Ewk_Settings *settings = ewk_view_settings_get(window->ewk_view);
    ewk_settings_file_access_from_file_urls_allowed_set(settings, EINA_TRUE);
    ewk_settings_encoding_detector_enabled_set(settings, encoding_detector_enabled);
    ewk_settings_frame_flattening_enabled_set(settings, frame_flattening_enabled);
    ewk_settings_local_storage_enabled_set(settings, local_storage_enabled);
    info("HTML5 local storage is %s for this view.\n", local_storage_enabled ? "enabled" : "disabled");
    ewk_settings_developer_extras_enabled_set(settings, EINA_TRUE);
    ewk_settings_preferred_minimum_contents_width_set(settings, 0);

    evas_object_smart_callback_add(window->ewk_view, "authentication,request", on_authentication_request, window);
    evas_object_smart_callback_add(window->ewk_view, "download,failed", on_download_failed, window);
    evas_object_smart_callback_add(window->ewk_view, "download,finished", on_download_finished, window);
    evas_object_smart_callback_add(window->ewk_view, "download,request", on_download_request, window);
    evas_object_smart_callback_add(window->ewk_view, "file,chooser,request", on_file_chooser_request, window);
    evas_object_smart_callback_add(window->ewk_view, "icon,changed", on_view_icon_changed, window);
    evas_object_smart_callback_add(window->ewk_view, "load,error", on_error, window);
    evas_object_smart_callback_add(window->ewk_view, "load,progress", on_progress, window);
    evas_object_smart_callback_add(window->ewk_view, "title,changed", on_title_changed, window);
    evas_object_smart_callback_add(window->ewk_view, "url,changed", on_url_changed, window);
    evas_object_smart_callback_add(window->ewk_view, "back,forward,list,changed", on_back_forward_list_changed, window);
    evas_object_smart_callback_add(window->ewk_view, "tooltip,text,set", on_tooltip_text_set, window);
    evas_object_smart_callback_add(window->ewk_view, "tooltip,text,unset", on_tooltip_text_unset, window);

    evas_object_event_callback_add(window->ewk_view, EVAS_CALLBACK_KEY_DOWN, on_key_down, window);
    evas_object_event_callback_add(window->ewk_view, EVAS_CALLBACK_MOUSE_DOWN, on_mouse_down, window);

    evas_object_size_hint_weight_set(window->ewk_view, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(window->ewk_view, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_end(vertical_layout, window->ewk_view);
    evas_object_show(window->ewk_view);

    if (url)
        ewk_view_url_set(window->ewk_view, url);

    evas_object_resize(window->elm_window, width ? width : window_width, height ? height : window_height);
    evas_object_show(window->elm_window);

    view_focus_set(window, EINA_TRUE);

    return window;
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

    Ecore_Getopt_Value values[] = {
        ECORE_GETOPT_VALUE_STR(evas_engine_name),
        ECORE_GETOPT_VALUE_STR(window_size_string),
        ECORE_GETOPT_VALUE_BOOL(legacy_behavior_enabled),
        ECORE_GETOPT_VALUE_DOUBLE(device_pixel_ratio),
        ECORE_GETOPT_VALUE_BOOL(quitOption),
        ECORE_GETOPT_VALUE_BOOL(encoding_detector_enabled),
        ECORE_GETOPT_VALUE_BOOL(frame_flattening_enabled),
        ECORE_GETOPT_VALUE_BOOL(local_storage_enabled),
        ECORE_GETOPT_VALUE_BOOL(quitOption),
        ECORE_GETOPT_VALUE_BOOL(quitOption),
        ECORE_GETOPT_VALUE_BOOL(quitOption),
        ECORE_GETOPT_VALUE_NONE
    };

    if (!ewk_init())
        return EXIT_FAILURE;

    ewk_view_smart_class_set(miniBrowserViewSmartClass());

    ecore_app_args_set(argc, (const char **) argv);
    args = ecore_getopt_parse(&options, values, argc, argv);

    if (args < 0)
        return quit(EINA_FALSE, "ERROR: could not parse options.\n");

    if (quitOption)
        return quit(EINA_TRUE, NULL);

    if (evas_engine_name)
        elm_config_preferred_engine_set(evas_engine_name);
#if defined(WTF_USE_ACCELERATED_COMPOSITING) && defined(HAVE_ECORE_X)
    else {
        evas_engine_name = "opengl_x11";
        elm_config_preferred_engine_set(evas_engine_name);
    }
#endif

    // Enable favicon database.
    Ewk_Context *context = ewk_context_default_get();
    ewk_context_favicon_database_directory_set(context, NULL);

    if (window_size_string)
        parse_window_size(window_size_string, &window_width, &window_height);

    if (args < argc) {
        char *url = url_from_user_input(argv[args]);
        window = window_create(0, url, 0, 0);
        free(url);
    } else
        window = window_create(0, DEFAULT_URL, 0, 0);

    if (!window)
        return quit(EINA_FALSE, "ERROR: could not create browser window.\n");

    windows = eina_list_append(windows, window);

    elm_run();

    return quit(EINA_TRUE, NULL);
}
ELM_MAIN()
