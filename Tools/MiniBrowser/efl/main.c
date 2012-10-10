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
#include "url_bar.h"
#include "url_utils.h"
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Eina.h>
#include <Evas.h>

static const int DEFAULT_WIDTH = 800;
static const int DEFAULT_HEIGHT = 600;
static const char DEFAULT_URL[] = "http://www.google.com/";
static const char APP_NAME[] = "EFL MiniBrowser";

#define info(format, args...)       \
    do {                            \
        if (verbose)                \
            printf(format, ##args); \
    } while (0)

static int verbose = 1;
static Eina_List *windows = NULL;
static char *evas_engine_name = NULL;
static Eina_Bool frame_flattening_enabled = EINA_FALSE;

typedef struct _Browser_Window {
    Ecore_Evas *ee;
    Evas *evas;
    Evas_Object *webview;
    Url_Bar *url_bar;
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

static Browser_Window *browser_window_find(Ecore_Evas *ee)
{
    Eina_List *l;
    void *data;

    if (!ee)
        return NULL;

    EINA_LIST_FOREACH(windows, l, data) {
        Browser_Window *window = (Browser_Window *)data;
        if (window->ee == ee)
            return window;
    }
    return NULL;
}

static void window_free(Browser_Window *window)
{
    url_bar_del(window->url_bar);
    ecore_evas_free(window->ee);
    free(window);
}

static void window_close(Browser_Window *window)
{
    windows = eina_list_remove(windows, window);
    window_free(window);

    if (!windows)
        ecore_main_loop_quit();
}

static Eina_Bool main_signal_exit(void *data, int ev_type, void *ev)
{
    Browser_Window *window;
    EINA_LIST_FREE(windows, window)
        window_free(window);

    ecore_main_loop_quit();
    return EINA_TRUE;
}

static void on_ecore_evas_delete(Ecore_Evas *ee)
{
    window_close(browser_window_find(ee));
}

static void on_ecore_evas_resize(Ecore_Evas *ee)
{
    Browser_Window *window;
    Evas_Object *webview;
    int w, h;

    window = browser_window_find(ee);
    if (!window)
        return;

    ecore_evas_geometry_get(ee, NULL, NULL, &w, &h);

    /* Resize URL bar */
    url_bar_width_set(window->url_bar, w);

    evas_object_move(window->webview, 0, URL_BAR_HEIGHT);
    evas_object_resize(window->webview, w, h - URL_BAR_HEIGHT);
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
    }
}

static void
on_mouse_down(void *data, Evas *e, Evas_Object *webview, void *event_info)
{
    Evas_Event_Mouse_Down *ev = (Evas_Event_Mouse_Down *)event_info;

    if (ev->button == 1)
        evas_object_focus_set(webview, EINA_TRUE);
    else if (ev->button == 2)
        evas_object_focus_set(webview, !evas_object_focus_get(webview));
}

static void
title_set(Ecore_Evas *ee, const char *title, int progress)
{
    Eina_Strbuf* buffer;

    if (!title || !*title) {
        ecore_evas_title_set(ee, APP_NAME);
        return;
    }

    buffer = eina_strbuf_new();
    if (progress < 100)
        eina_strbuf_append_printf(buffer, "%s (%d%%) - %s", title, progress, APP_NAME);
    else
        eina_strbuf_append_printf(buffer, "%s - %s", title, APP_NAME);

    ecore_evas_title_set(ee, eina_strbuf_string_get(buffer));
    eina_strbuf_free(buffer);
}

static void
on_title_changed(void *user_data, Evas_Object *webview, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    const char *title = (const char *)event_info;

    title_set(window->ee, title, 100);
}

static void
on_url_changed(void *user_data, Evas_Object *webview, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    url_bar_url_set(window->url_bar, ewk_view_url_get(window->webview));
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
on_close_window(void *user_data, void *event_info)
{
    window_close((Browser_Window *)user_data);
}

static void
on_progress(void *user_data, Evas_Object *webview, void *event_info)
{
    Browser_Window *window = (Browser_Window *)user_data;
    double progress = *(double *)event_info;

    title_set(window->ee, ewk_view_title_get(window->webview), progress * 100);
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
        mktemp(unique_path);
        eina_strbuf_append(destination_path, unique_path);
    }

    ewk_download_job_destination_set(download, eina_strbuf_string_get(destination_path));
    info("Downloading: %s\n", eina_strbuf_string_get(destination_path));
    eina_strbuf_free(destination_path);
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

static int
quit(Eina_Bool success, const char *msg)
{
    ewk_shutdown();

    if (msg)
        fputs(msg, (success) ? stdout : stderr);

    if (!success)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static Browser_Window *window_create(const char *url)
{
    Browser_Window *window = malloc(sizeof(Browser_Window));
    if (!window) {
        info("ERROR: could not create browser window.\n");
        return NULL;
    }

    window->ee = ecore_evas_new(evas_engine_name, 0, 0, DEFAULT_WIDTH, DEFAULT_HEIGHT, 0);
    if (!window->ee) {
        info("ERROR: could not construct ecore-evas.\n");
        free(window);
        return NULL;
    }

    ecore_evas_title_set(window->ee, APP_NAME);
    ecore_evas_callback_resize_set(window->ee, on_ecore_evas_resize);
    ecore_evas_borderless_set(window->ee, 0);
    ecore_evas_show(window->ee);
    ecore_evas_callback_delete_request_set(window->ee, on_ecore_evas_delete);

    window->evas = ecore_evas_get(window->ee);
    if (!window->evas) {
        info("ERROR: could not get evas from evas-ecore.\n");
        free(window);
        return NULL;
    }

    /* Create webview */
    window->webview = ewk_view_add(window->evas);
    ewk_view_theme_set(window->webview, THEME_DIR"/default.edj");
    ewk_settings_enable_frame_flattening_set(ewk_view_settings_get(window->webview), frame_flattening_enabled);

    Ewk_Settings *settings = ewk_view_settings_get(window->webview);
    ewk_settings_file_access_from_file_urls_allowed_set(settings, EINA_TRUE);

    evas_object_smart_callback_add(window->webview, "close,window", on_close_window, window);
    evas_object_smart_callback_add(window->webview, "create,window", on_new_window, window);
    evas_object_smart_callback_add(window->webview, "download,failed", on_download_failed, window);
    evas_object_smart_callback_add(window->webview, "download,finished", on_download_finished, window);
    evas_object_smart_callback_add(window->webview, "download,request", on_download_request, window);
    evas_object_smart_callback_add(window->webview, "load,error", on_error, window);
    evas_object_smart_callback_add(window->webview, "load,progress", on_progress, window);
    evas_object_smart_callback_add(window->webview, "title,changed", on_title_changed, window);
    evas_object_smart_callback_add(window->webview, "url,changed", on_url_changed, window);

    evas_object_event_callback_add(window->webview, EVAS_CALLBACK_KEY_DOWN, on_key_down, window);
    evas_object_event_callback_add(window->webview, EVAS_CALLBACK_MOUSE_DOWN, on_mouse_down, window);

    evas_object_size_hint_weight_set(window->webview, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_move(window->webview, 0, URL_BAR_HEIGHT);
    evas_object_resize(window->webview, DEFAULT_WIDTH, DEFAULT_HEIGHT - URL_BAR_HEIGHT);
    evas_object_show(window->webview);
    evas_object_focus_set(window->webview, EINA_TRUE);

    window->url_bar = url_bar_add(window->webview, DEFAULT_WIDTH);

    if (url)
        ewk_view_url_set(window->webview, url);

    return window;
}

int main(int argc, char *argv[])
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

    ecore_app_args_set(argc, (const char **) argv);
    args = ecore_getopt_parse(&options, values, argc, argv);

    if (args < 0)
        return quit(EINA_FALSE, "ERROR: could not parse options.\n");

    if (quitOption)
        return quit(EINA_TRUE, NULL);

    if (args < argc) {
        char *url = url_from_user_input(argv[args]);
        window = window_create(url);
        free(url);
    } else
        window = window_create(DEFAULT_URL);

    if (!window)
        return quit(EINA_FALSE, "ERROR: could not create browser window.\n");

    windows = eina_list_append(windows, window);

    Ecore_Event_Handler *handle = ecore_event_handler_add(ECORE_EVENT_SIGNAL_EXIT, main_signal_exit, 0);

    ecore_main_loop_begin();

    ecore_event_handler_del(handle);

    return quit(EINA_TRUE, NULL);
}
