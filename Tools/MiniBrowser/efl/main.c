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

static const int DEFAULT_WIDTH = 800;
static const int DEFAULT_HEIGHT = 600;
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
static Eina_Bool frame_flattening_enabled = EINA_FALSE;

static Ewk_View_Smart_Class* miniBrowserViewSmartClass()
{
    static Ewk_View_Smart_Class ewkViewClass = EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION("MiniBrowser_View");
    return &ewkViewClass;
}

typedef struct _Browser_Window {
    Evas_Object *window;
    Evas_Object *webview;
    Evas_Object *url_bar;
    Evas_Object *back_button;
    Evas_Object *forward_button;
} Browser_Window;

static const Ecore_Getopt options = {
    "MiniBrowser",
    "%prog [options] [url]",
    "0.0.1",
    "(C)2012 Samsung Electronics\n",
    "",
    "Test Web Browser using the Enlightenment Foundation Libraries of WebKit2",
    EINA_TRUE, {
        ECORE_GETOPT_STORE_STR
            ('e', "engine", "ecore-evas engine to use."),
        ECORE_GETOPT_CALLBACK_NOARGS
            ('E', "list-engines", "list ecore-evas engines.",
             ecore_getopt_callback_ecore_evas_list_engines, NULL),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('f', "flattening", "frame flattening.", EINA_FALSE),
        ECORE_GETOPT_VERSION
            ('V', "version"),
        ECORE_GETOPT_COPYRIGHT
            ('R', "copyright"),
        ECORE_GETOPT_HELP
            ('h', "help"),
        ECORE_GETOPT_SENTINEL
    }
};

static Browser_Window *window_create(const char *url);

static Browser_Window *browser_window_find(Evas_Object *window)
{
    Eina_List *l;
    void *data;

    if (!window)
        return NULL;

    EINA_LIST_FOREACH(windows, l, data) {
        Browser_Window *browser_window = (Browser_Window *)data;
        if (browser_window->window == window)
            return browser_window;
    }
    return NULL;
}

static Browser_Window *browser_view_find(Evas_Object *view)
{
    Eina_List *l;
    void *data;

    if (!view)
        return NULL;

    EINA_LIST_FOREACH(windows, l, data) {
        Browser_Window *browser_window = (Browser_Window *)data;
        if (browser_window->webview == view)
            return browser_window;
    }
    return NULL;
}

static void window_free(Browser_Window *window)
{
    evas_object_del(window->webview);
    /* The elm_win will take care of freeing its children */
    evas_object_del(window->window);
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
on_key_down(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
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
        if (!ewk_view_back(obj))
            info("Back ignored: No back history\n");
    } else if (!strcmp(ev->key, "F2")) {
        info("Forward (F2) was pressed\n");
        if (!ewk_view_forward(obj))
            info("Forward ignored: No forward history\n");
    } else if (!strcmp(ev->key, "F3")) {
        currentEncoding = (currentEncoding + 1) % (sizeof(encodings) / sizeof(encodings[0]));
        info("Set encoding (F3) pressed. New encoding to %s", encodings[currentEncoding]);
        ewk_view_setting_encoding_custom_set(obj, encodings[currentEncoding]);
    } else if (!strcmp(ev->key, "F5")) {
        info("Reload (F5) was pressed, reloading.\n");
        ewk_view_reload(obj);
    } else if (!strcmp(ev->key, "F6")) {
        info("Stop (F6) was pressed, stop loading.\n");
        ewk_view_stop(obj);
    } else if (!strcmp(ev->key, "n") && ctrlPressed) {
        info("Create new window (Ctrl+n) was pressed.\n");
        Browser_Window *window = window_create(DEFAULT_URL);
        windows = eina_list_append(windows, window);
    } else if (!strcmp(ev->key, "i") && ctrlPressed) {
        info("Show Inspector (Ctrl+i) was pressed.\n");
        ewk_view_inspector_show(obj);
    }
}

static void
view_focus_set(Browser_Window *window, Eina_Bool focus)
{
    /* We steal focus away from elm's focus model and start to do things
     * manually here, so elm now has no clue what's up. Tell elm that its
     * toplevel widget is to be unfocused so elm gives up the focus */
    elm_object_focus_set(elm_object_top_widget_get(window->window), EINA_FALSE);
    evas_object_focus_set(window->webview, focus);
}

static void
on_mouse_down(void *user_data, Evas *e, Evas_Object *webview, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    Evas_Event_Mouse_Down *ev = (Evas_Event_Mouse_Down *)event_info;

    /* Clear selection from the URL bar */
    elm_entry_select_none(window->url_bar);

    if (ev->button == 1)
        view_focus_set(window, EINA_TRUE);
    else if (ev->button == 2)
        view_focus_set(window, !evas_object_focus_get(webview));
}

static void
title_set(Evas_Object *window, const char *title, int progress)
{
    Eina_Strbuf *buffer;

    if (!title || !*title) {
        elm_win_title_set(window, APP_NAME);
        return;
    }

    buffer = eina_strbuf_new();
    if (progress < 100)
        eina_strbuf_append_printf(buffer, "%s (%d%%) - %s", title, progress, APP_NAME);
    else
        eina_strbuf_append_printf(buffer, "%s - %s", title, APP_NAME);

    elm_win_title_set(window, eina_strbuf_string_get(buffer));
    eina_strbuf_free(buffer);
}

static void
on_title_changed(void *user_data, Evas_Object *webview, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    const char *title = (const char *)event_info;

    title_set(window->window, title, 100);
}

static void
on_url_changed(void *user_data, Evas_Object *webview, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    elm_entry_entry_set(window->url_bar, ewk_view_url_get(window->webview));
}

static void
on_back_forward_list_changed(void *user_data, Evas_Object *webview, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    /* Update navigation buttons state */
    elm_object_disabled_set(window->back_button, !ewk_view_back_possible(webview));
    elm_object_disabled_set(window->forward_button, !ewk_view_forward_possible(webview));
}

static void
on_new_window(void *user_data, Evas_Object *webview, void *event_info)
{
    Evas_Object **new_view = (Evas_Object **)event_info;
    Browser_Window *window = window_create(NULL);
    *new_view = window->webview;
    windows = eina_list_append(windows, window);
}

static void
on_close_window(void *user_data, Evas_Object *webview, void *event_info)
{
    window_close((Browser_Window *)user_data);
}

static void
on_progress(void *user_data, Evas_Object *webview, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    double progress = *(double *)event_info;

    title_set(window->window, ewk_view_title_get(window->webview), progress * 100);
}

static void
on_error(void *user_data, Evas_Object *webview, void *event_info)
{
    Eina_Strbuf* buffer;
    const Ewk_Error *error = (const Ewk_Error *)event_info;

    /* This is a cancellation, do not display the error page */
    if (ewk_error_cancellation_get(error))
        return;

    buffer = eina_strbuf_new();
    eina_strbuf_append_printf(buffer, "<html><body><div style=\"color:#ff0000\">ERROR!</div><br><div>Code: %d<br>Description: %s<br>URL: %s</div></body</html>",
        ewk_error_code_get(error), ewk_error_description_get(error), ewk_error_url_get(error));

    ewk_view_html_string_load(webview, eina_strbuf_string_get(buffer), 0, ewk_error_url_get(error));
    eina_strbuf_free(buffer);
}

static void
on_download_request(void *user_data, Evas_Object *webview, void *event_info)
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

typedef struct {
    Evas_Object *parent;
    Evas_Object *window;
    Ewk_File_Chooser_Request *request;
} FileSelectorData;

static void on_filepicker_parent_deletion(void *user_data, Evas *evas, Evas_Object *window, void *event);

static void close_file_picker(FileSelectorData* fs_data)
{
    evas_object_event_callback_del(fs_data->parent, EVAS_CALLBACK_DEL, on_filepicker_parent_deletion);
    evas_object_del(fs_data->window);
    ewk_file_chooser_request_unref(fs_data->request);
    free(fs_data);
}

static void
on_filepicker_parent_deletion(void *user_data, Evas *evas, Evas_Object *window, void *event)
{
    close_file_picker((FileSelectorData *)user_data);
}

static void
on_filepicker_deletion(void *user_data, Evas_Object *window, void *event_info)
{
    close_file_picker((FileSelectorData *)user_data);
}

static void
on_fileselector_done(void *user_data, Evas_Object *file_selector, void *event_info)
{
    FileSelectorData *fs_data = (FileSelectorData *)user_data;

    const char *selected = (const char *)event_info;
    if (selected && *selected)
        ewk_file_chooser_request_file_choose(fs_data->request, selected);

    close_file_picker(fs_data);
}

static void
on_file_chooser_request(void *user_data, Evas_Object *webview, void *event_info)
{
    Browser_Window *app_data = (Browser_Window *)user_data;
    Ewk_File_Chooser_Request *request = (Ewk_File_Chooser_Request *)event_info;

    // Show basic file picker which does not currently support multiple files
    // or MIME type filtering.
    Evas_Object *window = elm_win_add(app_data->window, "file-picker-window", ELM_WIN_DIALOG_BASIC);
    elm_win_title_set(window, "File picker");
    elm_win_modal_set(window, EINA_TRUE);

    FileSelectorData* fs_data = (FileSelectorData*)malloc(sizeof(FileSelectorData));
    fs_data->parent = app_data->window;
    fs_data->window = window;
    fs_data->request = ewk_file_chooser_request_ref(request);
    evas_object_smart_callback_add(window, "delete,request", on_filepicker_deletion, fs_data);
    evas_object_event_callback_add(app_data->window, EVAS_CALLBACK_DEL, on_filepicker_parent_deletion, fs_data);

    Evas_Object *file_selector = elm_fileselector_add(window);
    const char *home_path = getenv("HOME");
    elm_fileselector_path_set(file_selector, home_path ? home_path : "/home");
    evas_object_size_hint_weight_set(file_selector, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(window, file_selector);
    evas_object_show(file_selector);

    evas_object_smart_callback_add(file_selector, "done", on_fileselector_done, fs_data);

    evas_object_resize(window, 400, 400);
    elm_win_center(window, EINA_TRUE, EINA_TRUE);
    evas_object_show(window);
}

static void
on_download_finished(void *user_data, Evas_Object *webview, void *event_info)
{
    Ewk_Download_Job *download = (Ewk_Download_Job *)event_info;
    info("Download finished: %s\n",  ewk_download_job_destination_get(download));
}

static void
on_download_failed(void *user_data, Evas_Object *webview, void *event_info)
{
    info("Download failed!\n");
}

static void
on_favicon_received(const char *page_url, Evas_Object *icon, void *event_info)
{
    Browser_Window *app_data = (Browser_Window *)event_info;
    if (strcmp(page_url, ewk_view_url_get(app_data->webview)))
        return;

    /* Remove previous icon from URL bar */
    Evas_Object *old_icon = elm_object_part_content_unset(app_data->url_bar, "icon");
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
        elm_object_part_content_set(app_data->url_bar, "icon", icon);
        evas_object_ref(icon);
    }
}

static void
on_view_icon_changed(void *user_data, Evas_Object *webview, void *event_info)
{
    Browser_Window *app_data = (Browser_Window *)user_data;
    /* Retrieve the view's favicon */
    Ewk_Context *context = ewk_view_context_get(webview);
    Ewk_Favicon_Database *icon_database = ewk_context_favicon_database_get(context);

    const char *page_url = ewk_view_url_get(webview);
    Evas *evas = evas_object_evas_get(webview);
    ewk_favicon_database_async_icon_get(icon_database, page_url, evas, on_favicon_received, app_data);
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
    Browser_Window *app_data = (Browser_Window *)user_data;

    const char *user_url = elm_entry_entry_get(url_bar);
    char *url = url_from_user_input(user_url);
    ewk_view_url_set(app_data->webview, url);
    free(url);

    /* Give focus back to the view */
    view_focus_set(app_data, EINA_TRUE);
}

static void
on_url_bar_clicked(void *user_data, Evas_Object *url_bar, void *event_info)
{
    Browser_Window *app_data = (Browser_Window *)user_data;

    /* Grab focus from the view */
    evas_object_focus_set(app_data->webview, EINA_FALSE);
    elm_object_focus_set(url_bar, EINA_TRUE);
}

static void
on_back_button_clicked(void *user_data, Evas_Object *back_button, void *event_info)
{
    Browser_Window *app_data = (Browser_Window *)user_data;

    ewk_view_back(app_data->webview);
    /* Update back button state */
    elm_object_disabled_set(back_button, !ewk_view_back_possible(app_data->webview));
}

static void
on_forward_button_clicked(void *user_data, Evas_Object *forward_button, void *event_info)
{
    Browser_Window *app_data = (Browser_Window *)user_data;

    ewk_view_forward(app_data->webview);
    /* Update forward button state */
    elm_object_disabled_set(forward_button, !ewk_view_forward_possible(app_data->webview));
}

static void
on_refresh_button_clicked(void *user_data, Evas_Object *refresh_button, void *event_info)
{
    Browser_Window *app_data = (Browser_Window *)user_data;

    Evas *evas = evas_object_evas_get(refresh_button);
    Eina_Bool ctrlPressed = evas_key_modifier_is_set(evas_key_modifier_get(evas), "Control");
    if (ctrlPressed) {
        info("Reloading and bypassing cache...\n");
        ewk_view_reload_bypass_cache(app_data->webview);
    } else {
        info("Reloading...\n");
        ewk_view_reload(app_data->webview);
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
    Browser_Window *window = browser_view_find(smartData->self);

    Evas_Object *alert_popup = elm_popup_add(window->window);
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
    Browser_Window *window = browser_view_find(smartData->self);

    Eina_Bool ok = EINA_FALSE;

    Evas_Object *confirm_popup = elm_popup_add(window->window);
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
    Browser_Window *window = browser_view_find(smartData->self);

    Eina_Bool ok = EINA_FALSE;

    Evas_Object *prompt_popup = elm_popup_add(window->window);
    evas_object_size_hint_weight_set(prompt_popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_object_part_text_set(prompt_popup, "title,text", "Prompt");

    /* Popup Content */
    Evas_Object *box = elm_box_add(window->window);
    elm_box_padding_set(box, 0, 4);
    evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_show(box);

    Evas_Object *prompt = elm_label_add(window->window);
    elm_object_text_set(prompt, message);
    evas_object_size_hint_weight_set(prompt, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(prompt, EVAS_HINT_FILL, 0.5);
    elm_box_pack_end(box, prompt);
    evas_object_show(prompt);

    Evas_Object *entry = elm_entry_add(window->window);
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

typedef struct {
    Evas_Object *popup;
    Ewk_Auth_Request *request;
    Evas_Object *username_entry;
    Evas_Object *password_entry;
} AuthData;

static void
auth_popup_close(AuthData *auth_data)
{
    ewk_auth_request_unref(auth_data->request);
    evas_object_del(auth_data->popup);
    free(auth_data);
}

static void
on_auth_cancel(void *user_data, Evas_Object *obj, void *event_info)
{
    AuthData *auth_data = (AuthData *)user_data;

    ewk_auth_request_cancel(auth_data->request);

    auth_popup_close(auth_data);
}

static void
on_auth_ok(void *user_data, Evas_Object *obj, void *event_info)
{
    AuthData *auth_data = (AuthData *)user_data;

    const char *username = elm_entry_entry_get(auth_data->username_entry);
    const char *password = elm_entry_entry_get(auth_data->password_entry);
    ewk_auth_request_authenticate(auth_data->request, username, password);

    auth_popup_close(auth_data);
}

static void
on_authentication_request(void *user_data, Evas_Object *obj, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    Ewk_Auth_Request *request = ewk_auth_request_ref((Ewk_Auth_Request *)event_info);

    AuthData *auth_data = (AuthData *)malloc(sizeof(AuthData));
    auth_data->request = request;

    Evas_Object *auth_popup = elm_popup_add(window->window);
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
    const char* host = ewk_auth_request_host_get(request);
    const char* realm = ewk_auth_request_realm_get(request);
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
    const char* message = (const char*)event_info;

    elm_object_tooltip_text_set(window->webview, message);
    elm_object_tooltip_show(window->webview);
}

static void
on_tooltip_text_unset(void *user_data, Evas_Object *obj, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;

    elm_object_tooltip_unset(window->webview);
}

static void
on_home_button_clicked(void *user_data, Evas_Object *home_button, void *event_info)
{
    Browser_Window *app_data = (Browser_Window *)user_data;

    ewk_view_url_set(app_data->webview, DEFAULT_URL);
}

static void
on_window_deletion(void *user_data, Evas_Object *window, void *event_info)
{
    window_close(browser_window_find(window));
}

static Evas_Object *
create_toolbar_button(Evas_Object *window, const char *icon_name)
{
    Evas_Object *button = elm_button_add(window);

    Evas_Object *icon = elm_icon_add(window);
    elm_icon_standard_set(icon, icon_name);
    evas_object_size_hint_max_set(icon, TOOL_BAR_ICON_SIZE, TOOL_BAR_ICON_SIZE);
    evas_object_color_set(icon, 44, 44, 102, 128);
    evas_object_show(icon);
    elm_object_part_content_set(button, "icon", icon);
    evas_object_size_hint_min_set(button, TOOL_BAR_BUTTON_SIZE, TOOL_BAR_BUTTON_SIZE);

    return button;
}

static Browser_Window *window_create(const char *url)
{
    Browser_Window *app_data = malloc(sizeof(Browser_Window));
    if (!app_data) {
        info("ERROR: could not create browser window.\n");
        return NULL;
    }

    /* Create window */
    app_data->window = elm_win_add(NULL, "minibrowser-window", ELM_WIN_BASIC);
    elm_win_title_set(app_data->window, APP_NAME);
    evas_object_smart_callback_add(app_data->window, "delete,request", on_window_deletion, &app_data);

    /* Create window background */
    Evas_Object *bg = elm_bg_add(app_data->window);
    elm_bg_color_set(bg, 193, 192, 191);
    evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(app_data->window, bg);
    evas_object_show(bg);

    /* Create vertical layout */
    Evas_Object *vertical_layout = elm_box_add(app_data->window);
    elm_box_padding_set(vertical_layout, 0, 2);
    evas_object_size_hint_weight_set(vertical_layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(app_data->window, vertical_layout);
    evas_object_show(vertical_layout);

    /* Create horizontal layout for top bar */
    Evas_Object *horizontal_layout = elm_box_add(app_data->window);
    elm_box_horizontal_set(horizontal_layout, EINA_TRUE);
    evas_object_size_hint_weight_set(horizontal_layout, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(horizontal_layout, EVAS_HINT_FILL, 0.0);
    elm_box_pack_end(vertical_layout, horizontal_layout);
    evas_object_show(horizontal_layout);

    /* Create Back button */
    app_data->back_button = create_toolbar_button(app_data->window, "arrow_left");
    evas_object_smart_callback_add(app_data->back_button, "clicked", on_back_button_clicked, app_data);
    elm_object_disabled_set(app_data->back_button, EINA_TRUE);
    evas_object_size_hint_weight_set(app_data->back_button, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(app_data->back_button, 0.0, 0.5);
    elm_box_pack_end(horizontal_layout, app_data->back_button);
    evas_object_show(app_data->back_button);

    /* Create Forward button */
    app_data->forward_button = create_toolbar_button(app_data->window, "arrow_right");
    evas_object_smart_callback_add(app_data->forward_button, "clicked", on_forward_button_clicked, app_data);
    elm_object_disabled_set(app_data->forward_button, EINA_TRUE);
    evas_object_size_hint_weight_set(app_data->forward_button, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(app_data->forward_button, 0.0, 0.5);
    elm_box_pack_end(horizontal_layout, app_data->forward_button);
    evas_object_show(app_data->forward_button);

    /* Create URL bar */
    app_data->url_bar = elm_entry_add(app_data->window);
    elm_entry_scrollable_set(app_data->url_bar, EINA_TRUE);
    elm_entry_scrollbar_policy_set(app_data->url_bar, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
    elm_entry_single_line_set(app_data->url_bar, EINA_TRUE);
    elm_entry_cnp_mode_set(app_data->url_bar, ELM_CNP_MODE_PLAINTEXT);
    elm_entry_text_style_user_push(app_data->url_bar, "DEFAULT='font_size=18'");
    evas_object_smart_callback_add(app_data->url_bar, "activated", on_url_bar_activated, app_data);
    evas_object_smart_callback_add(app_data->url_bar, "clicked", on_url_bar_clicked, app_data);
    evas_object_size_hint_weight_set(app_data->url_bar, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(app_data->url_bar, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_end(horizontal_layout, app_data->url_bar);
    evas_object_show(app_data->url_bar);

    /* Create Refresh button */
    Evas_Object *refresh_button = create_toolbar_button(app_data->window, "refresh");
    evas_object_smart_callback_add(refresh_button, "clicked", on_refresh_button_clicked, app_data);
    evas_object_size_hint_weight_set(refresh_button, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(refresh_button, 1.0, 0.5);
    elm_box_pack_end(horizontal_layout, refresh_button);
    evas_object_show(refresh_button);

    /* Create Home button */
    Evas_Object *home_button = create_toolbar_button(app_data->window, "home");
    evas_object_smart_callback_add(home_button, "clicked", on_home_button_clicked, app_data);
    evas_object_size_hint_weight_set(home_button, 0.0, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(home_button, 1.0, 0.5);
    elm_box_pack_end(horizontal_layout, home_button);
    evas_object_show(home_button);

    /* Create webview */
    Ewk_View_Smart_Class *ewkViewClass = miniBrowserViewSmartClass();
    ewkViewClass->run_javascript_alert = on_javascript_alert;
    ewkViewClass->run_javascript_confirm = on_javascript_confirm;
    ewkViewClass->run_javascript_prompt = on_javascript_prompt;

    Evas *evas = evas_object_evas_get(app_data->window);
    Evas_Smart *smart = evas_smart_class_new(&ewkViewClass->sc);
    app_data->webview = ewk_view_smart_add(evas, smart, ewk_context_default_get());
    ewk_view_theme_set(app_data->webview, THEME_DIR "/default.edj");

    Ewk_Settings *settings = ewk_view_settings_get(app_data->webview);
    ewk_settings_file_access_from_file_urls_allowed_set(settings, EINA_TRUE);
    ewk_settings_frame_flattening_enabled_set(settings, frame_flattening_enabled);
    ewk_settings_developer_extras_enabled_set(settings, EINA_TRUE);

    evas_object_smart_callback_add(app_data->webview, "authentication,request", on_authentication_request, app_data);
    evas_object_smart_callback_add(app_data->webview, "close,window", on_close_window, app_data);
    evas_object_smart_callback_add(app_data->webview, "create,window", on_new_window, app_data);
    evas_object_smart_callback_add(app_data->webview, "download,failed", on_download_failed, app_data);
    evas_object_smart_callback_add(app_data->webview, "download,finished", on_download_finished, app_data);
    evas_object_smart_callback_add(app_data->webview, "download,request", on_download_request, app_data);
    evas_object_smart_callback_add(app_data->webview, "file,chooser,request", on_file_chooser_request, app_data);
    evas_object_smart_callback_add(app_data->webview, "icon,changed", on_view_icon_changed, app_data);
    evas_object_smart_callback_add(app_data->webview, "load,error", on_error, app_data);
    evas_object_smart_callback_add(app_data->webview, "load,progress", on_progress, app_data);
    evas_object_smart_callback_add(app_data->webview, "title,changed", on_title_changed, app_data);
    evas_object_smart_callback_add(app_data->webview, "url,changed", on_url_changed, app_data);
    evas_object_smart_callback_add(app_data->webview, "back,forward,list,changed", on_back_forward_list_changed, app_data);
    evas_object_smart_callback_add(app_data->webview, "tooltip,text,set", on_tooltip_text_set, app_data);
    evas_object_smart_callback_add(app_data->webview, "tooltip,text,unset", on_tooltip_text_unset, app_data);

    evas_object_event_callback_add(app_data->webview, EVAS_CALLBACK_KEY_DOWN, on_key_down, app_data);
    evas_object_event_callback_add(app_data->webview, EVAS_CALLBACK_MOUSE_DOWN, on_mouse_down, app_data);

    evas_object_size_hint_weight_set(app_data->webview, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(app_data->webview, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_end(vertical_layout, app_data->webview);
    evas_object_show(app_data->webview);

    if (url)
        ewk_view_url_set(app_data->webview, url);

    evas_object_resize(app_data->window, DEFAULT_WIDTH, DEFAULT_HEIGHT);
    evas_object_show(app_data->window);

    view_focus_set(app_data, EINA_TRUE);

    return app_data;
}

EAPI_MAIN int
elm_main(int argc, char *argv[])
{
    int args = 1;
    unsigned char quitOption = 0;
    Browser_Window *window;

    Ecore_Getopt_Value values[] = {
        ECORE_GETOPT_VALUE_STR(evas_engine_name),
        ECORE_GETOPT_VALUE_BOOL(quitOption),
        ECORE_GETOPT_VALUE_BOOL(frame_flattening_enabled),
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

    if (args < argc) {
        char *url = url_from_user_input(argv[args]);
        window = window_create(url);
        free(url);
    } else
        window = window_create(DEFAULT_URL);

    if (!window)
        return quit(EINA_FALSE, "ERROR: could not create browser window.\n");

    windows = eina_list_append(windows, window);

    elm_run();

    return quit(EINA_TRUE, NULL);
}
ELM_MAIN()
