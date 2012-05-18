/*
 * Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 * Copyright (C) 2009, 2010 ProFUSION embedded systems
 * Copyright (C) 2009, 2010, 2011 Samsung Electronics
 * Copyright (C) 2012 Intel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "EWebKit.h"

#include <ctype.h>
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Ecore_File.h>
#include <Ecore_Getopt.h>
#include <Ecore_X.h>
#include <Edje.h>
#include <Evas.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_WIDTH      800
#define DEFAULT_HEIGHT     600
#define DEFAULT_ZOOM_INIT  1.0

#define info(format, args...)       \
    do {                            \
        if (verbose)                \
            printf(format, ##args); \
    } while (0)

#define MIN_ZOOM_LEVEL 0
#define DEFAULT_ZOOM_LEVEL 5
#define MAX_ZOOM_LEVEL 13

static int currentZoomLevel = DEFAULT_ZOOM_LEVEL;
static float currentZoom = 1.0;

// the zoom values are chosen to be like in Mozilla Firefox 3
static int zoomLevels[] = {30, 50, 67, 80, 90,
                            100,
                            110, 120, 133, 150, 170, 200, 240, 300};

static int verbose = 0;

static Eina_List *windows = NULL;

static char *themePath = NULL;

static const char *backingStores[] = {
    "tiled",
    "single",
    NULL
};

typedef struct _Window_Properties {
    Eina_Bool toolbarsVisible:1;
    Eina_Bool statusbarVisible:1;
    Eina_Bool scrollbarsVisible:1;
    Eina_Bool menubarVisible:1;
} Window_Properties;

Window_Properties windowProperties = { /* Pretend we have them and they are initially visible */
    EINA_TRUE,
    EINA_TRUE,
    EINA_TRUE,
    EINA_TRUE
};

static const Ecore_Getopt options = {
    "EWebLauncher",
    "%prog [options] [url]",
    "0.0.1",
    "(C)2008 INdT (The Nokia Technology Institute)\n"
    "(C)2009, 2010 ProFUSION embedded systems\n"
    "(C)2009, 2010, 2011 Samsung Electronics\n"
    "(C)2012 Intel Corporation\n",
    "GPL",
    "Test Web Browser using the Enlightenment Foundation Libraries of WebKit",
    EINA_TRUE, {
        ECORE_GETOPT_STORE_STR
            ('e', "engine", "ecore-evas engine to use."),
        ECORE_GETOPT_CALLBACK_NOARGS
            ('E', "list-engines", "list ecore-evas engines.",
             ecore_getopt_callback_ecore_evas_list_engines, NULL),
        ECORE_GETOPT_CHOICE
            ('b', "backing-store", "choose backing store to use.", backingStores),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('f', "flattening", "frame flattening.", 0),
        ECORE_GETOPT_STORE_DEF_BOOL
            ('F', "fullscreen", "fullscreen mode.", 0),
        ECORE_GETOPT_CALLBACK_ARGS
            ('g', "geometry", "geometry to use in x:y:w:h form.", "X:Y:W:H",
             ecore_getopt_callback_geometry_parse, NULL),
        ECORE_GETOPT_STORE_STR
            ('t', "theme", "path to read the theme file from."),
        ECORE_GETOPT_STORE_STR
            ('U', "user-agent", "custom user agent string to use."),
        ECORE_GETOPT_COUNT
            ('v', "verbose", "be more verbose."),
        ECORE_GETOPT_VERSION
            ('V', "version"),
        ECORE_GETOPT_COPYRIGHT
            ('R', "copyright"),
        ECORE_GETOPT_LICENSE
            ('L', "license"),
        ECORE_GETOPT_HELP
            ('h', "help"),
        ECORE_GETOPT_SENTINEL
    }
};

typedef struct _ELauncher {
    Ecore_Evas *ee;
    Evas *evas;
    Evas_Object *bg;
    Evas_Object *browser;
    const char *theme;
    const char *userAgent;
    const char *backingStore;
    unsigned char isFlattening;
} ELauncher;

static void browserDestroy(Ecore_Evas *ee);
static void closeWindow(Ecore_Evas *ee);
static int browserCreate(const char *url, const char *theme, const char *userAgent, Eina_Rectangle geometry, const char *engine, const char *backingStore, unsigned char isFlattening, unsigned char isFullscreen, const char *databasePath);

static void
print_history(Eina_List *list)
{
    Eina_List *l;
    void *d;

    if (!verbose)
       return;

    printf("Session history contains:\n");

    EINA_LIST_FOREACH(list, l, d) {
       Ewk_History_Item *item = (Ewk_History_Item*)d;
       cairo_surface_t *cs = ewk_history_item_icon_surface_get(item);
       char buf[PATH_MAX];
       int s = snprintf(buf, sizeof(buf), "/tmp/favicon-%s.png", ewk_history_item_uri_original_get(item));
       for (s--; s >= (int)sizeof("/tmp/favicon-"); s--) {
           if (!isalnum(buf[s]) && buf[s] != '.')
               buf[s] = '_';
       }
       cs = ewk_history_item_icon_surface_get(item);

       if (cs && cairo_surface_status(cs) == CAIRO_STATUS_SUCCESS)
           cairo_surface_write_to_png(cs, buf);
       else
           buf[0] = '\0';

       printf("* '%s' title='%s' icon='%s'\n",
              ewk_history_item_uri_original_get(item),
              ewk_history_item_title_get(item), buf);
    }
}

static int
nearest_zoom_level_get(float factor)
{
    int i, intFactor = (int)(factor * 100.0);
    for (i = 0; zoomLevels[i] <= intFactor; i++) { }
    printf("factor=%f, intFactor=%d, zoomLevels[%d]=%d, zoomLevels[%d]=%d\n",
           factor, intFactor, i-1, zoomLevels[i-1], i, zoomLevels[i]);
    if (intFactor - zoomLevels[i-1] < zoomLevels[i] - intFactor)
        return i - 1;
    return i;
}

static Eina_Bool
zoom_level_set(Evas_Object *webview, int level)
{
    float factor = ((float) zoomLevels[level]) / 100.0;
    Evas_Coord ox, oy, mx, my, cx, cy;
    evas_pointer_canvas_xy_get(evas_object_evas_get(webview), &mx, &my);
    evas_object_geometry_get(webview, &ox, &oy, NULL, NULL);
    cx = mx - ox;
    cy = my - oy;
    return ewk_view_zoom_animated_set(webview, factor, 0.5, cx, cy);
}

static void
on_ecore_evas_resize(Ecore_Evas *ee)
{
    Evas_Object *webview;
    Evas_Object *bg;
    int w, h;

    ecore_evas_geometry_get(ee, NULL, NULL, &w, &h);

    bg = evas_object_name_find(ecore_evas_get(ee), "bg");
    evas_object_move(bg, 0, 0);
    evas_object_resize(bg, w, h);

    webview = evas_object_name_find(ecore_evas_get(ee), "browser");
    evas_object_move(webview, 10, 10);
    evas_object_resize(webview, w - 20, h - 20);
}

static void
title_set(Ecore_Evas *ee, const char *title, int progress)
{
    const char *appname = "EFL Test Launcher";
    const char *separator = " - ";
    char label[4096];
    int size;

    if (!title || !strcmp(title, "")) {
        ecore_evas_title_set(ee, appname);
        return;
    }

    if (progress < 100)
        size = snprintf(label, sizeof(label), "%s (%d%%)%s%s", title, progress, separator, appname);
    else
        size = snprintf(label, sizeof(label), "%s %s%s", title, separator, appname);

    if (size >= (int)sizeof(label))
        return;

    ecore_evas_title_set(ee, label);
}

static void
on_title_changed(void *user_data, Evas_Object *webview, void *event_info)
{
    ELauncher *app = (ELauncher *)user_data;
    const char *title = (const char *)event_info;

    title_set(app->ee, title, 100);
}

static void
on_progress(void *user_data, Evas_Object *webview, void *event_info)
{
    ELauncher *app = (ELauncher *)user_data;
    double *progress = (double *)event_info;

    title_set(app->ee, ewk_view_title_get(app->browser), *progress * 100);
}

static void
on_load_finished(void *user_data, Evas_Object *webview, void *event_info)
{
    const Ewk_Frame_Load_Error *err = (const Ewk_Frame_Load_Error *)event_info;

    if (!err)
        info("Succeeded loading page.\n");
    else if (err->is_cancellation)
        info("Load was cancelled.\n");
    else
        info("Failed loading page: %d %s \"%s\", url=%s\n",
             err->code, err->domain, err->description, err->failing_url);

    currentZoom = ewk_view_zoom_get(webview);
    currentZoomLevel = nearest_zoom_level_get(currentZoom);
    info("WebCore Zoom=%f, currentZoomLevel=%d\n", currentZoom, currentZoomLevel);
}

static void
on_load_error(void *user_data, Evas_Object *webview, void *event_info)
{
    const Ewk_Frame_Load_Error *err = (const Ewk_Frame_Load_Error *)event_info;
    char message[1024];
    snprintf(message, 1024, "<html><body><div style=\"color:#ff0000\">ERROR!</div><br><div>Code: %d<br>Domain: %s<br>Description: %s<br>URL: %s</div></body</html>",
             err->code, err->domain, err->description, err->failing_url);
    ewk_frame_contents_set(err->frame, message, 0, "text/html", "UTF-8", err->failing_url);
}

static void
on_toolbars_visible_set(void* user_data, Evas_Object* webview, void* event_info)
{
    Eina_Bool *visible = (Eina_Bool *)event_info;
    if (*visible) {
        info("Toolbars visible changed: show");
        windowProperties.toolbarsVisible = EINA_TRUE;
    } else {
        info("Toolbars visible changed: hide");
        windowProperties.toolbarsVisible = EINA_FALSE;
    }
}

static void
on_toolbars_visible_get(void* user_data, Evas_Object* webview, void* event_info)
{
    Eina_Bool *visible = (Eina_Bool *)event_info;
    *visible = windowProperties.toolbarsVisible;
}

static void
on_statusbar_visible_set(void* user_data, Evas_Object* webview, void* event_info)
{
    Eina_Bool *visible = (Eina_Bool *)event_info;
    if (*visible) {
        info("Statusbar visible changed: show");
        windowProperties.statusbarVisible = EINA_TRUE;
    } else {
        info("Statusbar visible changed: hide");
        windowProperties.statusbarVisible = EINA_FALSE;
    }
}

static void
on_statusbar_visible_get(void* user_data, Evas_Object* webview, void* event_info)
{
    Eina_Bool *visible = (Eina_Bool *)event_info;
    *visible = windowProperties.statusbarVisible;
}

static void
on_scrollbars_visible_set(void* user_data, Evas_Object* webview, void* event_info)
{
    Eina_Bool *visible = (Eina_Bool *)event_info;
    if (*visible) {
        info("Scrollbars visible changed: show");
        windowProperties.scrollbarsVisible = EINA_TRUE;
    } else {
        info("Scrollbars visible changed: hide");
        windowProperties.scrollbarsVisible = EINA_FALSE;
    }
}

static void
on_scrollbars_visible_get(void* user_data, Evas_Object* webview, void* event_info)
{
    Eina_Bool *visible = (Eina_Bool *)event_info;
    *visible = windowProperties.scrollbarsVisible;
}

static void
on_menubar_visible_set(void* user_data, Evas_Object* webview, void* event_info)
{
    Eina_Bool *visible = (Eina_Bool *)event_info;
    if (*visible) {
        info("Menubar visible changed: show");
        windowProperties.menubarVisible = EINA_TRUE;
    } else {
        info("Menubar visible changed: hide");
        windowProperties.menubarVisible = EINA_FALSE;
    }
}

static void
on_menubar_visible_get(void* user_data, Evas_Object* webview, void* event_info)
{
    Eina_Bool *visible = (Eina_Bool *)event_info;
    *visible = windowProperties.menubarVisible;
}

static void
on_tooltip_text_set(void* user_data, Evas_Object* webview, void* event_info)
{
    const char *text = (const char *)event_info;
    if (text && *text != '\0')
        info("%s\n", text);
}

static void
on_inputmethod_changed(void* user_data, Evas_Object* webview, void* event_info)
{
    Eina_Bool active = (Eina_Bool)(long)event_info;
    unsigned int imh;
    info("Keyboard changed: %d\n", active);

    if (!active)
        return;

    imh = ewk_view_imh_get(webview);
    info("    Keyboard flags: %#.2x\n", imh);

}

static void
on_mouse_down(void* data, Evas* e, Evas_Object* webview, void* event_info)
{
    Evas_Event_Mouse_Down *ev = (Evas_Event_Mouse_Down*) event_info;
    if (ev->button == 2)
        evas_object_focus_set(webview, !evas_object_focus_get(webview));
}

static void
on_focus_out(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    info("the webview lost keyboard focus\n");
}

static void
on_focus_in(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    info("the webview gained keyboard focus\n");
}

static void
on_key_down(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    Evas_Event_Key_Down *ev = (Evas_Event_Key_Down*) event_info;
    ELauncher *app = data;
    static const char *encodings[] = {
        "ISO-8859-1",
        "UTF-8",
        NULL
    };
    static int currentEncoding = -1;
    Eina_Bool ctrlPressed = evas_key_modifier_is_set(evas_key_modifier_get(e), "Control");

    if (!strcmp(ev->key, "Escape")) {
        closeWindow(app->ee);
    } else if (!strcmp(ev->key, "F1")) {
        info("Back (F1) was pressed\n");
        if (ewk_view_back_possible(obj)) {
            Ewk_History *history = ewk_view_history_get(obj);
            Eina_List *list = ewk_history_back_list_get(history);
            print_history(list);
            ewk_history_item_list_free(list);
            ewk_view_back(obj);
        } else
            info("Back ignored: No back history\n");
    } else if (!strcmp(ev->key, "F2")) {
        info("Forward (F2) was pressed\n");
        if (ewk_view_forward_possible(obj)) {
            Ewk_History *history = ewk_view_history_get(obj);
            Eina_List *list = ewk_history_forward_list_get(history);
            print_history(list);
            ewk_history_item_list_free(list);
            ewk_view_forward(obj);
        } else
            info("Forward ignored: No forward history\n");
    } else if (!strcmp(ev->key, "F3")) {
        currentEncoding++;
        currentEncoding %= (sizeof(encodings) / sizeof(encodings[0]));
        info("Set encoding (F3) pressed. New encoding to %s", encodings[currentEncoding]);
        ewk_view_setting_encoding_custom_set(obj, encodings[currentEncoding]);
    } else if (!strcmp(ev->key, "F4")) {
        Evas_Object *frame = ewk_view_frame_main_get(obj);
        Evas_Coord x, y;
        Ewk_Hit_Test *ht;

        evas_pointer_canvas_xy_get(evas_object_evas_get(obj), &x, &y);
        ht = ewk_frame_hit_test_new(frame, x, y);
        if (!ht)
            printf("No hit test returned for point %d,%d\n", x, y);
        else {
            printf("Hit test for point %d,%d\n"
                   "  pos=%3d,%3d\n"
                   "  bounding_box=%d,%d + %dx%d\n"
                   "  title='%s'\n"
                   "  alternate_text='%s'\n"
                   "  frame=%p (%s)\n"
                   "  link {\n"
                   "    text='%s'\n"
                   "    url='%s'\n"
                   "    title='%s'\n"
                   "    target frame=%p (%s)\n"
                   "  }\n"
                   "context:\n"
                   "%s"
                   "%s"
                   "%s"
                   "%s"
                   "%s\n",
                   x, y,
                   ht->x, ht->y,
                   ht->bounding_box.x, ht->bounding_box.y, ht->bounding_box.w, ht->bounding_box.h,
                   ht->title,
                   ht->alternate_text,
                   ht->frame, evas_object_name_get(ht->frame),
                   ht->link.text,
                   ht->link.url,
                   ht->link.title,
                   ht->link.target_frame, evas_object_name_get(ht->link.target_frame),
                   ht->context & EWK_HIT_TEST_RESULT_CONTEXT_LINK ? "  LINK\n" : "",
                   ht->context & EWK_HIT_TEST_RESULT_CONTEXT_IMAGE ? "  IMAGE\n" : "",
                   ht->context & EWK_HIT_TEST_RESULT_CONTEXT_MEDIA ? "   MEDIA\n" : "",
                   ht->context & EWK_HIT_TEST_RESULT_CONTEXT_SELECTION ? "  SELECTION\n" : "",
                   ht->context & EWK_HIT_TEST_RESULT_CONTEXT_EDITABLE ? "  EDITABLE" : "");
            ewk_frame_hit_test_free(ht);
        }

    } else if (!strcmp(ev->key, "F5")) {
        info("Reload (F5) was pressed, reloading.\n");
        ewk_view_reload(obj);
    } else if (!strcmp(ev->key, "F6")) {
        info("Stop (F6) was pressed, stop loading.\n");
        ewk_view_stop(obj);
    } else if (!strcmp(ev->key, "F12")) {
        Eina_Bool status = ewk_view_setting_spatial_navigation_get(obj);
        ewk_view_setting_spatial_navigation_set(obj, !status);
        info("Command::keyboard navigation toggle\n");
    } else if (!strcmp(ev->key, "F7")) {
        info("Zoom out (F7) was pressed.\n");
        if (currentZoomLevel > MIN_ZOOM_LEVEL && zoom_level_set(obj, currentZoomLevel - 1))
            currentZoomLevel--;
    } else if (!strcmp(ev->key, "F8")) {
        info("Zoom in (F8) was pressed.\n");
        if (currentZoomLevel < MAX_ZOOM_LEVEL && zoom_level_set(obj, currentZoomLevel + 1))
            currentZoomLevel++;
    } else if (!strcmp(ev->key, "F9")) {
        info("Create new window (F9) was pressed.\n");
        Eina_Rectangle geometry = {0, 0, 0, 0};
        browserCreate("http://www.google.com",
                       app->theme, app->userAgent, geometry, NULL,
                       app->backingStore, app->isFlattening, 0, NULL);
    } else if (!strcmp(ev->key, "g") && ctrlPressed ) {
        Evas_Coord x, y, w, h;
        Evas_Object *frame = ewk_view_frame_main_get(obj);
        float zoom = zoomLevels[currentZoomLevel] / 100.0;

        ewk_frame_visible_content_geometry_get(frame, EINA_FALSE, &x, &y, &w, &h);
        x -= w;
        y -= h;
        w *= 4;
        h *= 4;
        info("Pre-render %d,%d + %dx%d\n", x, y, w, h);
        ewk_view_pre_render_region(obj, x, y, w, h, zoom);
    } else if (!strcmp(ev->key, "r") && ctrlPressed) {
        info("Pre-render 1 extra column/row with current zoom");
        ewk_view_pre_render_relative_radius(obj, 1);
    } else if (!strcmp(ev->key, "p") && ctrlPressed) {
        info("Pre-rendering start");
        ewk_view_pre_render_start(obj);
    } else if (!strcmp(ev->key, "d") && ctrlPressed) {
        info("Render suspended");
        ewk_view_disable_render(obj);
    } else if (!strcmp(ev->key, "e") && ctrlPressed) {
        info("Render resumed");
        ewk_view_enable_render(obj);
    } else if (!strcmp(ev->key, "s") && ctrlPressed) {
        Evas_Object *frame = ewk_view_frame_main_get(obj);
        Ewk_Security_Origin *origin = ewk_frame_security_origin_get(frame);
        printf("Security origin information:\n"
               "  protocol=%s\n"
               "  host=%s\n"
               "  port=%d\n"
               "  web database quota=%" PRIu64 "\n",
               ewk_security_origin_protocol_get(origin),
               ewk_security_origin_host_get(origin),
               ewk_security_origin_port_get(origin),
               ewk_security_origin_web_database_quota_get(origin));

        Eina_List *databaseList = ewk_security_origin_web_database_get_all(origin);
        Eina_List *listIterator = 0;
        Ewk_Web_Database *database;
        EINA_LIST_FOREACH(databaseList, listIterator, database)
            printf("Database information:\n"
                   "  name=%s\n"
                   "  display name=%s\n"
                   "  filename=%s\n"
                   "  expected size=%" PRIu64 "\n"
                   "  size=%" PRIu64 "\n",
                   ewk_web_database_name_get(database),
                   ewk_web_database_display_name_get(database),
                   ewk_web_database_filename_get(database),
                   ewk_web_database_expected_size_get(database),
                   ewk_web_database_size_get(database));

        ewk_security_origin_free(origin);
        ewk_web_database_list_free(databaseList);
    }
}

static void
on_browser_del(void *data, Evas *evas, Evas_Object *browser, void *event)
{
    ELauncher *app = (ELauncher*) data;

    evas_object_event_callback_del(app->browser, EVAS_CALLBACK_KEY_DOWN, on_key_down);
    evas_object_event_callback_del(app->browser, EVAS_CALLBACK_MOUSE_DOWN, on_mouse_down);
    evas_object_event_callback_del(app->browser, EVAS_CALLBACK_FOCUS_IN, on_focus_in);
    evas_object_event_callback_del(app->browser, EVAS_CALLBACK_FOCUS_OUT, on_focus_out);
    evas_object_event_callback_del(app->browser, EVAS_CALLBACK_DEL, on_browser_del);
}

static int
quit(Eina_Bool success, const char *msg)
{
    edje_shutdown();
    ecore_evas_shutdown();
    ecore_file_shutdown();

    if (msg)
        fputs(msg, (success) ? stdout : stderr);

    if (themePath) {
        free(themePath);
        themePath = NULL;
    }

    if (!success)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static int
browserCreate(const char *url, const char *theme, const char *userAgent, Eina_Rectangle geometry, const char *engine, const char *backingStore, unsigned char isFlattening, unsigned char isFullscreen, const char *databasePath)
{
    if ((geometry.w <= 0) && (geometry.h <= 0)) {
        geometry.w = DEFAULT_WIDTH;
        geometry.h = DEFAULT_HEIGHT;
    }

    ELauncher *app = (ELauncher*) malloc(sizeof(ELauncher));
    if (!app)
        return quit(EINA_FALSE, "ERROR: could not create EWebLauncher window\n");

    app->ee = ecore_evas_new(engine, 0, 0, geometry.w, geometry.h, NULL);

    if (!app->ee)
        return quit(EINA_FALSE, "ERROR: could not construct evas-ecore\n");

    if (isFullscreen)
        ecore_evas_fullscreen_set(app->ee, EINA_TRUE);

    ecore_evas_title_set(app->ee, "EFL Test Launcher");
    ecore_evas_callback_resize_set(app->ee, on_ecore_evas_resize);
    ecore_evas_callback_delete_request_set(app->ee, closeWindow);

    app->evas = ecore_evas_get(app->ee);

    if (!app->evas)
        return quit(EINA_FALSE, "ERROR: could not get evas from evas-ecore\n");

    app->theme = theme;
    app->userAgent = userAgent;
    app->backingStore = backingStore;
    app->isFlattening = isFlattening;

    app->bg = evas_object_rectangle_add(app->evas);
    evas_object_name_set(app->bg, "bg");
    evas_object_color_set(app->bg, 255, 0, 255, 255);
    evas_object_move(app->bg, 0, 0);
    evas_object_resize(app->bg, geometry.w, geometry.h);
    evas_object_layer_set(app->bg, EVAS_LAYER_MIN);
    evas_object_show(app->bg);

    if (backingStore && !strcasecmp(backingStore, "tiled")) {
        app->browser = ewk_view_tiled_add(app->evas);
        info("backing store: tiled\n");
    } else {
        app->browser = ewk_view_single_add(app->evas);
        info("backing store: single\n");
    }

    ewk_view_theme_set(app->browser, theme);
    if (userAgent)
        ewk_view_setting_user_agent_set(app->browser, userAgent);
    ewk_view_setting_local_storage_database_path_set(app->browser, databasePath);
    ewk_view_setting_enable_frame_flattening_set(app->browser, isFlattening);
    
    evas_object_name_set(app->browser, "browser");

    evas_object_smart_callback_add(app->browser, "title,changed", on_title_changed, app);
    evas_object_smart_callback_add(app->browser, "load,progress", on_progress, app);
    evas_object_smart_callback_add(app->browser, "load,finished", on_load_finished, app);
    evas_object_smart_callback_add(app->browser, "load,error", on_load_error, app);

    evas_object_smart_callback_add(app->browser, "toolbars,visible,set", on_toolbars_visible_set, app);
    evas_object_smart_callback_add(app->browser, "toolbars,visible,get", on_toolbars_visible_get, app);
    evas_object_smart_callback_add(app->browser, "statusbar,visible,set", on_statusbar_visible_set, app);
    evas_object_smart_callback_add(app->browser, "statusbar,visible,get", on_statusbar_visible_get, app);
    evas_object_smart_callback_add(app->browser, "scrollbars,visible,set", on_scrollbars_visible_set, app);
    evas_object_smart_callback_add(app->browser, "scrollbars,visible,get", on_scrollbars_visible_get, app);
    evas_object_smart_callback_add(app->browser, "menubar,visible,set", on_menubar_visible_set, app);
    evas_object_smart_callback_add(app->browser, "menubar,visible,get", on_menubar_visible_get, app);
    evas_object_smart_callback_add(app->browser, "tooltip,text,set", on_tooltip_text_set, app);
    evas_object_smart_callback_add(app->browser, "inputmethod,changed", on_inputmethod_changed, app);

/*     ewk_callback_resize_requested_add(app->browser, on_resize_requested, app->ee); */

    evas_object_event_callback_add(app->browser, EVAS_CALLBACK_KEY_DOWN, on_key_down, app);
    evas_object_event_callback_add(app->browser, EVAS_CALLBACK_MOUSE_DOWN, on_mouse_down, app);
    evas_object_event_callback_add(app->browser, EVAS_CALLBACK_FOCUS_IN, on_focus_in, app);
    evas_object_event_callback_add(app->browser, EVAS_CALLBACK_FOCUS_OUT, on_focus_out, app);
    evas_object_event_callback_add(app->browser, EVAS_CALLBACK_DEL, on_browser_del, app);

    evas_object_move(app->browser, 10, 10);
    evas_object_resize(app->browser, geometry.w - 20, geometry.h - 20);

    if (url && (url[0] != '\0'))
        ewk_view_uri_set(app->browser, url);

    evas_object_show(app->browser);
    ecore_evas_show(app->ee);

    evas_object_focus_set(app->browser, EINA_TRUE);

    windows = eina_list_append(windows, app);

    return 1;
}

static void
browserDestroy(Ecore_Evas *ee)
{
    ecore_evas_free(ee);
    if (!eina_list_count(windows))
        ecore_main_loop_quit();
}

static void
closeWindow(Ecore_Evas *ee)
{
    Eina_List *l;
    void *app;
    EINA_LIST_FOREACH(windows, l, app)
    {
        if (((ELauncher*) app)->ee == ee)
            break;
    }
    windows = eina_list_remove(windows, app);
    browserDestroy(ee);
    free(app);
}

static Eina_Bool
main_signal_exit(void *data, int ev_type, void *ev)
{
    ELauncher *app;
    while (windows) {
        app = (ELauncher*) eina_list_data_get(windows);
        ecore_evas_free(app->ee);
        windows = eina_list_remove(windows, app);
    }
    if (!eina_list_count(windows))
        ecore_main_loop_quit();
    return EINA_TRUE;
}

static char *
findThemePath(const char *theme)
{
    const char *defaultTheme = DATA_DIR"/default.edj";
    char *rpath;
    struct stat st;

    if (!theme)
        theme = defaultTheme;

    rpath = ecore_file_realpath(theme);
    if (!strlen(rpath) || stat(rpath, &st)) {
        free(rpath);
        return NULL;
    }

    return rpath;
}

int
main(int argc, char *argv[])
{
    const char *default_url = "http://www.google.com/";

    Eina_Rectangle geometry = {0, 0, 0, 0};
    char *url = NULL;
    char *userAgent = NULL;
    const char *tmp;
    const char *proxyUri;
    char path[PATH_MAX];

    char *engine = NULL;
    char *theme = NULL;
    char *backingStore = (char *)backingStores[1];

    unsigned char quitOption = 0;
    unsigned char isFlattening = 0;
    unsigned char isFullscreen = 0;
    int args;

    Ecore_Getopt_Value values[] = {
        ECORE_GETOPT_VALUE_STR(engine),
        ECORE_GETOPT_VALUE_BOOL(quitOption),
        ECORE_GETOPT_VALUE_STR(backingStore),
        ECORE_GETOPT_VALUE_BOOL(isFlattening),
        ECORE_GETOPT_VALUE_BOOL(isFullscreen),
        ECORE_GETOPT_VALUE_PTR_CAST(geometry),
        ECORE_GETOPT_VALUE_STR(theme),
        ECORE_GETOPT_VALUE_STR(userAgent),
        ECORE_GETOPT_VALUE_INT(verbose),
        ECORE_GETOPT_VALUE_BOOL(quitOption),
        ECORE_GETOPT_VALUE_BOOL(quitOption),
        ECORE_GETOPT_VALUE_BOOL(quitOption),
        ECORE_GETOPT_VALUE_BOOL(quitOption),
        ECORE_GETOPT_VALUE_NONE
    };

    if (!ecore_evas_init())
        return EXIT_FAILURE;

    if (!edje_init()) {
        ecore_evas_shutdown();
        return EXIT_FAILURE;
    }

    if (!ecore_file_init()) {
        edje_shutdown();
        ecore_evas_shutdown();
        return EXIT_FAILURE;
    }

    ecore_app_args_set(argc, (const char**) argv);
    args = ecore_getopt_parse(&options, values, argc, argv);

    if (args < 0)
       return quit(EINA_FALSE, "ERROR: could not parse options.\n");

    if (quitOption)
        return quit(EINA_TRUE, NULL);

    if (args < argc)
        url = argv[args];
    else
        url = (char*) default_url;

    themePath = findThemePath(theme);
    if (!themePath)
        return quit(EINA_FALSE, "ERROR: could not find theme.\n");

    ewk_init();
    tmp = getenv("TMPDIR");
    if (!tmp)
        tmp = "/tmp";
    snprintf(path, sizeof(path), "%s/.ewebkit-%u", tmp, getuid());
    if (!ecore_file_mkpath(path))
        return quit(EINA_FALSE, "ERROR: could not create settings database directory.\n");

    ewk_settings_icon_database_path_set(path);
    ewk_settings_web_database_path_set(path);

    proxyUri = getenv("http_proxy");
    if (proxyUri)
        ewk_network_proxy_uri_set(proxyUri);

    browserCreate(url, themePath, userAgent, geometry, engine, backingStore, isFlattening, isFullscreen, path);
    ecore_event_handler_add(ECORE_EVENT_SIGNAL_EXIT, main_signal_exit, &windows);

    ecore_main_loop_begin();

    ewk_shutdown();

    return quit(EINA_TRUE, NULL);
}
