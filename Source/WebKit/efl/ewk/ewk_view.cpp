/*
    Copyright (C) 2009-2010 ProFUSION embedded systems
    Copyright (C) 2009-2011 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#define __STDC_FORMAT_MACROS
#include "config.h"
#include "ewk_view.h"

#include "BackForwardListImpl.h"
#include "Bridge.h"
#include "Chrome.h"
#include "ChromeClientEfl.h"
#include "ContextMenuController.h"
#include "DocumentLoader.h"
#include "DragClientEfl.h"
#include "EWebKit.h"
#include "EditorClientEfl.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "FrameLoaderClientEfl.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "InspectorClientEfl.h"
#include "IntSize.h"
#include "JSDOMBinding.h"
#include "JSDOMWindow.h"
#include "JSLock.h"
#include "PlatformMouseEvent.h"
#include "PopupMenuClient.h"
#include "ProgressTracker.h"
#include "RenderTheme.h"
#include "c_instance.h"
#include "ewk_private.h"
#include "ewk_protocol_handler.h"

#include <Ecore.h>
#include <Eina.h>
#include <Evas.h>
#include <eina_safety_checks.h>
#include <inttypes.h>
#include <limits>
#include <math.h>
#include <sys/time.h>

#if ENABLE(DEVICE_ORIENTATION)
#include "DeviceMotionClientEfl.h"
#include "DeviceOrientationClientEfl.h"
#endif

#define ZOOM_MIN (0.05)
#define ZOOM_MAX (4.0)

#define DEVICE_PIXEL_RATIO (1.0)

static const char EWK_VIEW_TYPE_STR[] = "EWK_View";

static const size_t EWK_VIEW_REPAINTS_SIZE_INITIAL = 32;
static const size_t EWK_VIEW_REPAINTS_SIZE_STEP = 8;
static const size_t EWK_VIEW_REPAINTS_SIZE_MAX_FREE = 64;

static const size_t EWK_VIEW_SCROLLS_SIZE_INITIAL = 8;
static const size_t EWK_VIEW_SCROLLS_SIZE_STEP = 2;
static const size_t EWK_VIEW_SCROLLS_SIZE_MAX_FREE = 32;

/**
 * @brief Private data that is used internally by EFL WebKit
 * and should never be modified from outside.
 *
 * @internal
 */
struct _Ewk_View_Private_Data {
    WebCore::Page* page;
    WebCore::Settings* page_settings;
    WebCore::Frame* main_frame;
    WebCore::ViewportArguments viewport_arguments;
    Ewk_History* history;
    struct {
        void* context;
        Ewk_View_Resource_Handler_Cb function;
    } custom_resource_handler;
    struct {
        Ewk_Menu menu;
        WebCore::PopupMenuClient* menu_client;
    } popup;
    struct {
        Eina_Rectangle* array;
        size_t count;
        size_t allocated;
    } repaints;
    struct {
        Ewk_Scroll_Request* array;
        size_t count;
        size_t allocated;
    } scrolls;
    unsigned int imh; /**< input method hints */
    struct {
        Eina_Bool view_cleared : 1;
        Eina_Bool need_touch_events : 1;
    } flags;
    struct {
        const char* user_agent;
        const char* user_stylesheet;
        const char* encoding_default;
        const char* encoding_custom;
        const char* theme;
        const char* local_storage_database_path;
        int font_minimum_size;
        int font_minimum_logical_size;
        int font_default_size;
        int font_monospace_size;
        const char* font_standard;
        const char* font_cursive;
        const char* font_monospace;
        const char* font_fantasy;
        const char* font_serif;
        const char* font_sans_serif;
        Eina_Bool auto_load_images : 1;
        Eina_Bool auto_shrink_images : 1;
        Eina_Bool enable_auto_resize_window : 1;
        Eina_Bool enable_developer_extras : 1;
        Eina_Bool enable_scripts : 1;
        Eina_Bool enable_plugins : 1;
        Eina_Bool enable_frame_flattening : 1;
        Eina_Bool encoding_detector : 1;
        Eina_Bool scripts_window_open : 1;
        Eina_Bool scripts_can_close_windows : 1;
        Eina_Bool resizable_textareas : 1;
        Eina_Bool private_browsing : 1;
        Eina_Bool caret_browsing : 1;
        Eina_Bool spatial_navigation : 1;
        Eina_Bool local_storage : 1;
        Eina_Bool offline_app_cache : 1;
        Eina_Bool page_cache : 1;
        struct {
            float min_scale;
            float max_scale;
            Eina_Bool user_scalable : 1;
        } zoom_range;
        float device_pixel_ratio;
        double dom_timer_interval;
    } settings;
    struct {
        struct {
            double start;
            double end;
            double duration;
        } time;
        struct {
            float start;
            float end;
            float range;
        } zoom;
        struct {
            Evas_Coord x, y;
        } center;
        Ecore_Animator* animator;
    } animated_zoom;
};

#ifndef EWK_TYPE_CHECK
#define EWK_VIEW_TYPE_CHECK(o, ...) do { } while (0)
#else
#define EWK_VIEW_TYPE_CHECK(o, ...)                                     \
    do {                                                                \
        const char* _tmp_otype = evas_object_type_get(o);               \
        const Evas_Smart* _tmp_s = evas_object_smart_smart_get(o);      \
        if (EINA_UNLIKELY(!_tmp_s)) {                                   \
            EINA_LOG_CRIT                                               \
                ("%p (%s) is not a smart object!", o,                   \
                _tmp_otype ? _tmp_otype : "(null)");                   \
            return __VA_ARGS__;                                         \
        }                                                               \
        const Evas_Smart_Class* _tmp_sc = evas_smart_class_get(_tmp_s); \
        if (EINA_UNLIKELY(!_tmp_sc)) {                                  \
            EINA_LOG_CRIT                                               \
                ("%p (%s) is not a smart object!", o,                   \
                _tmp_otype ? _tmp_otype : "(null)");                   \
            return __VA_ARGS__;                                         \
        }                                                               \
        if (EINA_UNLIKELY(_tmp_sc->data != EWK_VIEW_TYPE_STR)) {        \
            EINA_LOG_CRIT                                               \
                ("%p (%s) is not of an ewk_view (need %p, got %p)!",    \
                o, _tmp_otype ? _tmp_otype : "(null)",                 \
                EWK_VIEW_TYPE_STR, _tmp_sc->data);                     \
            return __VA_ARGS__;                                         \
        }                                                               \
    } while (0)
#endif

#define EWK_VIEW_SD_GET(o, ptr)                                 \
    Ewk_View_Smart_Data* ptr = static_cast<Ewk_View_Smart_Data*>(evas_object_smart_data_get(o))

#define EWK_VIEW_SD_GET_OR_RETURN(o, ptr, ...)          \
    EWK_VIEW_TYPE_CHECK(o, __VA_ARGS__);                \
    EWK_VIEW_SD_GET(o, ptr);                            \
    if (!ptr) {                                         \
        CRITICAL("no smart data for object %p (%s)",    \
                 o, evas_object_type_get(o));           \
        return __VA_ARGS__;                             \
    }

#define EWK_VIEW_PRIV_GET(sd, ptr)              \
    Ewk_View_Private_Data *ptr = sd->_priv

#define EWK_VIEW_PRIV_GET_OR_RETURN(sd, ptr, ...)               \
    EWK_VIEW_PRIV_GET(sd, ptr);                                 \
    if (!ptr) {                                                 \
        CRITICAL("no private data for object %p (%s)",          \
                 sd->self, evas_object_type_get(sd->self));     \
        return __VA_ARGS__;                                     \
    }

static void _ewk_view_smart_changed(Ewk_View_Smart_Data* sd)
{
    if (sd->changed.any)
        return;
    sd->changed.any = EINA_TRUE;
    evas_object_smart_changed(sd->self);
}

static Eina_Bool _ewk_view_repaints_resize(Ewk_View_Private_Data* priv, size_t size)
{
    void* tmp = realloc(priv->repaints.array, size * sizeof(Eina_Rectangle));
    if (!tmp) {
        CRITICAL("could not realloc repaints array to %zu elements.", size);
        return EINA_FALSE;
    }
    priv->repaints.allocated = size;
    priv->repaints.array = static_cast<Eina_Rectangle*>(tmp);
    return EINA_TRUE;
}

static void _ewk_view_repaint_add(Ewk_View_Private_Data* priv, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
    Eina_Rectangle* r;

    // fprintf(stderr, ">>> repaint requested: %d,%d+%dx%d\n", x, y, w, h);
    if (priv->repaints.allocated == priv->repaints.count) {
        size_t size;
        if (!priv->repaints.allocated)
            size = EWK_VIEW_REPAINTS_SIZE_INITIAL;
        else
            size = priv->repaints.allocated + EWK_VIEW_REPAINTS_SIZE_STEP;
        if (!_ewk_view_repaints_resize(priv, size))
            return;
    }

    r = priv->repaints.array + priv->repaints.count;
    priv->repaints.count++;

    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;

    DBG("add repaint %d, %d+%dx%d", x, y, w, h);
}

static void _ewk_view_repaints_flush(Ewk_View_Private_Data* priv)
{
    priv->repaints.count = 0;
    if (priv->repaints.allocated <= EWK_VIEW_REPAINTS_SIZE_MAX_FREE)
        return;
    _ewk_view_repaints_resize(priv, EWK_VIEW_REPAINTS_SIZE_MAX_FREE);
}

static Eina_Bool _ewk_view_scrolls_resize(Ewk_View_Private_Data* priv, size_t size)
{
    void* tmp = realloc(priv->scrolls.array, size * sizeof(Ewk_Scroll_Request));
    if (!tmp) {
        CRITICAL("could not realloc scrolls array to %zu elements.", size);
        return EINA_FALSE;
    }
    priv->scrolls.allocated = size;
    priv->scrolls.array = static_cast<Ewk_Scroll_Request*>(tmp);
    return EINA_TRUE;
}

static void _ewk_view_scroll_add(Ewk_View_Private_Data* priv, Evas_Coord dx, Evas_Coord dy, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, Eina_Bool main_scroll)
{
    Ewk_Scroll_Request* r;
    Ewk_Scroll_Request* r_end;
    Evas_Coord x2 = x + w, y2 = y + h;

    r = priv->scrolls.array;
    r_end = r + priv->scrolls.count;
    for (; r < r_end; r++) {
        if (r->x == x && r->y == y && r->w == w && r->h == h) {
            DBG("region already scrolled %d,%d+%dx%d %+03d,%+03d add "
                "%+03d,%+03d",
                r->x, r->y, r->w, r->h, r->dx, r->dy, dx, dy);
            r->dx += dx;
            r->dy += dy;
            return;
        }
        if ((x <= r->x && x2 >= r->x2) && (y <= r->y && y2 >= r->y2)) {
            DBG("old viewport (%d,%d+%dx%d %+03d,%+03d) was scrolled itself, "
                "add %+03d,%+03d",
                r->x, r->y, r->w, r->h, r->dx, r->dy, dx, dy);
            r->x += dx;
            r->y += dy;
        }
    }

    if (priv->scrolls.allocated == priv->scrolls.count) {
        size_t size;
        if (!priv->scrolls.allocated)
            size = EWK_VIEW_SCROLLS_SIZE_INITIAL;
        else
            size = priv->scrolls.allocated + EWK_VIEW_SCROLLS_SIZE_STEP;
        if (!_ewk_view_scrolls_resize(priv, size))
            return;
    }

    r = priv->scrolls.array + priv->scrolls.count;
    priv->scrolls.count++;

    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
    r->x2 = x2;
    r->y2 = y2;
    r->dx = dx;
    r->dy = dy;
    r->main_scroll = main_scroll;
    DBG("add scroll in region: %d, %d+%dx%d %+03d, %+03d", x, y, w, h, dx, dy);

    Eina_Rectangle* pr;
    Eina_Rectangle* pr_end;
    size_t count;
    pr = priv->repaints.array;
    count = priv->repaints.count;
    pr_end = pr + count;
    for (; pr < pr_end; pr++) {
        pr->x += dx;
        pr->y += dy;
    }
}

static void _ewk_view_scrolls_flush(Ewk_View_Private_Data* priv)
{
    priv->scrolls.count = 0;
    if (priv->scrolls.allocated <= EWK_VIEW_SCROLLS_SIZE_MAX_FREE)
        return;
    _ewk_view_scrolls_resize(priv, EWK_VIEW_SCROLLS_SIZE_MAX_FREE);
}

// Default Event Handling //////////////////////////////////////////////
static Eina_Bool _ewk_view_smart_focus_in(Ewk_View_Smart_Data* sd)
{
    EWK_VIEW_PRIV_GET(sd, priv);
    WebCore::FocusController* fc = priv->page->focusController();
    DBG("o=%p, fc=%p", sd->self, fc);
    EINA_SAFETY_ON_NULL_RETURN_VAL(fc, EINA_FALSE);

    fc->setActive(true);
    fc->setFocused(true);
    return EINA_TRUE;
}

static Eina_Bool _ewk_view_smart_focus_out(Ewk_View_Smart_Data* sd)
{
    EWK_VIEW_PRIV_GET(sd, priv);
    WebCore::FocusController* fc = priv->page->focusController();
    DBG("o=%p, fc=%p", sd->self, fc);
    EINA_SAFETY_ON_NULL_RETURN_VAL(fc, EINA_FALSE);

    fc->setActive(false);
    fc->setFocused(false);
    return EINA_TRUE;
}

static Eina_Bool _ewk_view_smart_mouse_wheel(Ewk_View_Smart_Data* sd, const Evas_Event_Mouse_Wheel* ev)
{
    return ewk_frame_feed_mouse_wheel(sd->main_frame, ev);
}

static Eina_Bool _ewk_view_smart_mouse_down(Ewk_View_Smart_Data* sd, const Evas_Event_Mouse_Down* ev)
{
    return ewk_frame_feed_mouse_down(sd->main_frame, ev);
}

static Eina_Bool _ewk_view_smart_mouse_up(Ewk_View_Smart_Data* sd, const Evas_Event_Mouse_Up* ev)
{
    return ewk_frame_feed_mouse_up(sd->main_frame, ev);
}

static Eina_Bool _ewk_view_smart_mouse_move(Ewk_View_Smart_Data* sd, const Evas_Event_Mouse_Move* ev)
{
    return ewk_frame_feed_mouse_move(sd->main_frame, ev);
}

static Eina_Bool _ewk_view_smart_key_down(Ewk_View_Smart_Data* sd, const Evas_Event_Key_Down* ev)
{
    Evas_Object* frame = ewk_view_frame_focused_get(sd->self);

    if (!frame)
        frame = sd->main_frame;

    return ewk_frame_feed_key_down(frame, ev);
}

static Eina_Bool _ewk_view_smart_key_up(Ewk_View_Smart_Data* sd, const Evas_Event_Key_Up* ev)
{
    Evas_Object* frame = ewk_view_frame_focused_get(sd->self);

    if (!frame)
        frame = sd->main_frame;

    return ewk_frame_feed_key_up(frame, ev);
}

static void _ewk_view_smart_add_console_message(Ewk_View_Smart_Data* sd, const char* message, unsigned int lineNumber, const char* sourceID)
{
    INF("console message: %s @%d: %s\n", sourceID, lineNumber, message);
}

static void _ewk_view_smart_run_javascript_alert(Ewk_View_Smart_Data* sd, Evas_Object* frame, const char* message)
{
    INF("javascript alert: %s\n", message);
}

static Eina_Bool _ewk_view_smart_run_javascript_confirm(Ewk_View_Smart_Data* sd, Evas_Object* frame, const char* message)
{
    INF("javascript confirm: %s", message);
    INF("javascript confirm (HARD CODED)? YES");
    return EINA_TRUE;
}

static Eina_Bool _ewk_view_smart_should_interrupt_javascript(Ewk_View_Smart_Data* sd)
{
    INF("should interrupt javascript?\n"
        "\t(HARD CODED) NO");
    return EINA_FALSE;
}

static Eina_Bool _ewk_view_smart_run_javascript_prompt(Ewk_View_Smart_Data* sd, Evas_Object* frame, const char* message, const char* defaultValue, char** value)
{
    *value = strdup("test");
    Eina_Bool ret = EINA_TRUE;
    INF("javascript prompt:\n"
        "\t      message: %s\n"
        "\tdefault value: %s\n"
        "\tgiving answer: %s\n"
        "\t       button: %s", message, defaultValue, *value, ret ? "ok" : "cancel");

    return ret;
}

// Event Handling //////////////////////////////////////////////////////
static void _ewk_view_on_focus_in(void* data, Evas* e, Evas_Object* o, void* event_info)
{
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->focus_in);
    sd->api->focus_in(sd);
}

static void _ewk_view_on_focus_out(void* data, Evas* e, Evas_Object* o, void* event_info)
{
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->focus_out);
    sd->api->focus_out(sd);
}

static void _ewk_view_on_mouse_wheel(void* data, Evas* e, Evas_Object* o, void* event_info)
{
    Evas_Event_Mouse_Wheel* ev = static_cast<Evas_Event_Mouse_Wheel*>(event_info);
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->mouse_wheel);
    sd->api->mouse_wheel(sd, ev);
}

static void _ewk_view_on_mouse_down(void* data, Evas* e, Evas_Object* o, void* event_info)
{
    Evas_Event_Mouse_Down* ev = static_cast<Evas_Event_Mouse_Down*>(event_info);
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->mouse_down);
    sd->api->mouse_down(sd, ev);
}

static void _ewk_view_on_mouse_up(void* data, Evas* e, Evas_Object* o, void* event_info)
{
    Evas_Event_Mouse_Up* ev = static_cast<Evas_Event_Mouse_Up*>(event_info);
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->mouse_up);
    sd->api->mouse_up(sd, ev);
}

static void _ewk_view_on_mouse_move(void* data, Evas* e, Evas_Object* o, void* event_info)
{
    Evas_Event_Mouse_Move* ev = static_cast<Evas_Event_Mouse_Move*>(event_info);
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->mouse_move);
    sd->api->mouse_move(sd, ev);
}

static void _ewk_view_on_key_down(void* data, Evas* e, Evas_Object* o, void* event_info)
{
    Evas_Event_Key_Down* ev = static_cast<Evas_Event_Key_Down*>(event_info);
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->key_down);
    sd->api->key_down(sd, ev);
}

static void _ewk_view_on_key_up(void* data, Evas* e, Evas_Object* o, void* event_info)
{
    Evas_Event_Key_Up* ev = static_cast<Evas_Event_Key_Up*>(event_info);
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->key_up);
    sd->api->key_up(sd, ev);
}

static WTF::PassRefPtr<WebCore::Frame> _ewk_view_core_frame_new(Ewk_View_Smart_Data* sd, Ewk_View_Private_Data* priv, WebCore::HTMLFrameOwnerElement* owner)
{
    WebCore::FrameLoaderClientEfl* flc = new WebCore::FrameLoaderClientEfl(sd->self);
    if (!flc) {
        CRITICAL("Could not create frame loader client.");
        return 0;
    }
    flc->setCustomUserAgent(String::fromUTF8(priv->settings.user_agent));

    return WebCore::Frame::create(priv->page, owner, flc);
}

static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;

static Ewk_View_Private_Data* _ewk_view_priv_new(Ewk_View_Smart_Data* sd)
{
    Ewk_View_Private_Data* priv =
        static_cast<Ewk_View_Private_Data*>(calloc(1, sizeof(Ewk_View_Private_Data)));
    AtomicString s;
    WebCore::KURL url;

    if (!priv) {
        CRITICAL("could not allocate Ewk_View_Private_Data");
        return 0;
    }

    WebCore::Page::PageClients pageClients;
    pageClients.chromeClient = static_cast<WebCore::ChromeClient*>(new WebCore::ChromeClientEfl(sd->self));
    pageClients.editorClient = static_cast<WebCore::EditorClient*>(new WebCore::EditorClientEfl(sd->self));
    pageClients.dragClient = static_cast<WebCore::DragClient*>(new WebCore::DragClientEfl);
    pageClients.inspectorClient = static_cast<WebCore::InspectorClient*>(new WebCore::InspectorClientEfl);
#if ENABLE(DEVICE_ORIENTATION)
    pageClients.deviceMotionClient = static_cast<WebCore::DeviceMotionClient*>(new WebCore::DeviceMotionClientEfl);
    pageClients.deviceOrientationClient = static_cast<WebCore::DeviceOrientationClient*>(new WebCore::DeviceOrientationClientEfl);
#endif
    priv->page = new WebCore::Page(pageClients);
    if (!priv->page) {
        CRITICAL("Could not create WebKit Page");
        goto error_page;
    }

    priv->page_settings = priv->page->settings();
    if (!priv->page_settings) {
        CRITICAL("Could not get page settings.");
        goto error_settings;
    }

    priv->viewport_arguments.width = WebCore::ViewportArguments::ValueAuto;
    priv->viewport_arguments.height = WebCore::ViewportArguments::ValueAuto;
    priv->viewport_arguments.initialScale = WebCore::ViewportArguments::ValueAuto;
    priv->viewport_arguments.minimumScale = WebCore::ViewportArguments::ValueAuto;
    priv->viewport_arguments.maximumScale = WebCore::ViewportArguments::ValueAuto;
    priv->viewport_arguments.targetDensityDpi = WebCore::ViewportArguments::ValueAuto;
    priv->viewport_arguments.userScalable = EINA_TRUE;

    priv->page_settings->setLoadsImagesAutomatically(true);
    priv->page_settings->setDefaultFixedFontSize(12);
    priv->page_settings->setDefaultFontSize(16);
    priv->page_settings->setSerifFontFamily("serif");
    priv->page_settings->setFixedFontFamily("monotype");
    priv->page_settings->setSansSerifFontFamily("sans");
    priv->page_settings->setStandardFontFamily("sans");
    priv->page_settings->setJavaScriptEnabled(true);
    priv->page_settings->setPluginsEnabled(true);
    priv->page_settings->setLocalStorageEnabled(true);
    priv->page_settings->setOfflineWebApplicationCacheEnabled(true);
    priv->page_settings->setUsesPageCache(true);
    priv->page_settings->setUsesEncodingDetector(false);

    url = priv->page_settings->userStyleSheetLocation();
    priv->settings.user_stylesheet = eina_stringshare_add(url.string().utf8().data());

    priv->settings.encoding_default = eina_stringshare_add
                                          (priv->page_settings->defaultTextEncodingName().utf8().data());
    priv->settings.encoding_custom = 0;

    s = priv->page_settings->localStorageDatabasePath();
    priv->settings.local_storage_database_path = eina_stringshare_add(s.string().utf8().data());

    priv->settings.font_minimum_size = priv->page_settings->minimumFontSize();
    priv->settings.font_minimum_logical_size = priv->page_settings->minimumLogicalFontSize();
    priv->settings.font_default_size = priv->page_settings->defaultFontSize();
    priv->settings.font_monospace_size = priv->page_settings->defaultFixedFontSize();

    s = priv->page_settings->standardFontFamily();
    priv->settings.font_standard = eina_stringshare_add(s.string().utf8().data());
    s = priv->page_settings->cursiveFontFamily();
    priv->settings.font_cursive = eina_stringshare_add(s.string().utf8().data());
    s = priv->page_settings->fixedFontFamily();
    priv->settings.font_monospace = eina_stringshare_add(s.string().utf8().data());
    s = priv->page_settings->fantasyFontFamily();
    priv->settings.font_fantasy = eina_stringshare_add(s.string().utf8().data());
    s = priv->page_settings->serifFontFamily();
    priv->settings.font_serif = eina_stringshare_add(s.string().utf8().data());
    s = priv->page_settings->sansSerifFontFamily();
    priv->settings.font_sans_serif = eina_stringshare_add(s.string().utf8().data());

    priv->settings.auto_load_images = priv->page_settings->loadsImagesAutomatically();
    priv->settings.auto_shrink_images = priv->page_settings->shrinksStandaloneImagesToFit();
    priv->settings.enable_auto_resize_window = EINA_TRUE;
    priv->settings.enable_developer_extras = priv->page_settings->developerExtrasEnabled();
    priv->settings.enable_scripts = priv->page_settings->isJavaScriptEnabled();
    priv->settings.enable_plugins = priv->page_settings->arePluginsEnabled();
    priv->settings.enable_frame_flattening = priv->page_settings->frameFlatteningEnabled();
    priv->settings.scripts_window_open = priv->page_settings->javaScriptCanOpenWindowsAutomatically();
    priv->settings.scripts_can_close_windows = priv->page_settings->allowScriptsToCloseWindows();
    priv->settings.resizable_textareas = priv->page_settings->textAreasAreResizable();
    priv->settings.private_browsing = priv->page_settings->privateBrowsingEnabled();
    priv->settings.caret_browsing = priv->page_settings->caretBrowsingEnabled();
    priv->settings.spatial_navigation = priv->page_settings->isSpatialNavigationEnabled();
    priv->settings.local_storage = priv->page_settings->localStorageEnabled();
    priv->settings.offline_app_cache = true; // XXX no function to read setting; this keeps the original setting
    priv->settings.page_cache = priv->page_settings->usesPageCache();
    priv->settings.encoding_detector = priv->page_settings->usesEncodingDetector();

    priv->settings.user_agent = ewk_settings_default_user_agent_get();

    // Since there's no scale separated from zooming in webkit-efl, this functionality of
    // viewport meta tag is implemented using zoom. When scale zoom is supported by webkit-efl,
    // this functionality will be modified by the scale zoom patch.
    priv->settings.zoom_range.min_scale = ZOOM_MIN;
    priv->settings.zoom_range.max_scale = ZOOM_MAX;
    priv->settings.zoom_range.user_scalable = EINA_TRUE;
    priv->settings.device_pixel_ratio = DEVICE_PIXEL_RATIO;

    priv->settings.dom_timer_interval = priv->page_settings->defaultMinDOMTimerInterval();

    priv->main_frame = _ewk_view_core_frame_new(sd, priv, 0).get();
    if (!priv->main_frame) {
        CRITICAL("Could not create main frame.");
        goto error_main_frame;
    }

    priv->history = ewk_history_new(static_cast<WebCore::BackForwardListImpl*>(priv->page->backForwardList()));
    if (!priv->history) {
        CRITICAL("Could not create history instance for view.");
        goto error_history;
    }

    return priv;

error_history:
    // delete priv->main_frame; /* do not delete priv->main_frame */
error_main_frame:
error_settings:
    delete priv->page;
error_page:
    free(priv);
    return 0;
}

static void _ewk_view_priv_del(Ewk_View_Private_Data* priv)
{
    if (!priv)
        return;

    /* do not delete priv->main_frame */

    free(priv->repaints.array);
    free(priv->scrolls.array);

    eina_stringshare_del(priv->settings.user_agent);
    eina_stringshare_del(priv->settings.user_stylesheet);
    eina_stringshare_del(priv->settings.encoding_default);
    eina_stringshare_del(priv->settings.encoding_custom);
    eina_stringshare_del(priv->settings.font_standard);
    eina_stringshare_del(priv->settings.font_cursive);
    eina_stringshare_del(priv->settings.font_monospace);
    eina_stringshare_del(priv->settings.font_fantasy);
    eina_stringshare_del(priv->settings.font_serif);
    eina_stringshare_del(priv->settings.font_sans_serif);
    eina_stringshare_del(priv->settings.local_storage_database_path);

    if (priv->animated_zoom.animator)
        ecore_animator_del(priv->animated_zoom.animator);

    ewk_history_free(priv->history);

    delete priv->page;
    free(priv);
}

static void _ewk_view_smart_add(Evas_Object* o)
{
    const Evas_Smart* smart = evas_object_smart_smart_get(o);
    const Evas_Smart_Class* sc = evas_smart_class_get(smart);
    const Ewk_View_Smart_Class* api = reinterpret_cast<const Ewk_View_Smart_Class*>(sc);
    EINA_SAFETY_ON_NULL_RETURN(api->backing_store_add);
    EWK_VIEW_SD_GET(o, sd);

    if (!sd) {
        sd = static_cast<Ewk_View_Smart_Data*>(calloc(1, sizeof(Ewk_View_Smart_Data)));
        if (!sd) {
            CRITICAL("could not allocate Ewk_View_Smart_Data");
            return;
        }
        evas_object_smart_data_set(o, sd);
    }

    sd->bg_color.r = 255;
    sd->bg_color.g = 255;
    sd->bg_color.b = 255;
    sd->bg_color.a = 255;

    sd->self = o;
    sd->_priv = _ewk_view_priv_new(sd);
    sd->api = api;

    _parent_sc.add(o);

    if (!sd->_priv)
        return;

    EWK_VIEW_PRIV_GET(sd, priv);

    sd->backing_store = api->backing_store_add(sd);
    if (!sd->backing_store) {
        ERR("Could not create backing store object.");
        return;
    }

    evas_object_smart_member_add(sd->backing_store, o);
    evas_object_show(sd->backing_store);
    evas_object_pass_events_set(sd->backing_store, EINA_TRUE);

    sd->events_rect = evas_object_rectangle_add(sd->base.evas);
    evas_object_color_set(sd->events_rect, 0, 0, 0, 0);
    evas_object_smart_member_add(sd->events_rect, o);
    evas_object_show(sd->events_rect);

    sd->main_frame = ewk_frame_add(sd->base.evas);
    if (!sd->main_frame) {
        ERR("Could not create main frame object.");
        return;
    }

    if (!ewk_frame_init(sd->main_frame, o, priv->main_frame)) {
        ERR("Could not initialize main frme object.");
        evas_object_del(sd->main_frame);
        sd->main_frame = 0;

        delete priv->main_frame;
        priv->main_frame = 0;
        return;
    }

    evas_object_name_set(sd->main_frame, "EWK_Frame:main");
    evas_object_smart_member_add(sd->main_frame, o);
    evas_object_show(sd->main_frame);

#define CONNECT(s, c) evas_object_event_callback_add(o, s, c, sd)
    CONNECT(EVAS_CALLBACK_FOCUS_IN, _ewk_view_on_focus_in);
    CONNECT(EVAS_CALLBACK_FOCUS_OUT, _ewk_view_on_focus_out);
    CONNECT(EVAS_CALLBACK_MOUSE_WHEEL, _ewk_view_on_mouse_wheel);
    CONNECT(EVAS_CALLBACK_MOUSE_DOWN, _ewk_view_on_mouse_down);
    CONNECT(EVAS_CALLBACK_MOUSE_UP, _ewk_view_on_mouse_up);
    CONNECT(EVAS_CALLBACK_MOUSE_MOVE, _ewk_view_on_mouse_move);
    CONNECT(EVAS_CALLBACK_KEY_DOWN, _ewk_view_on_key_down);
    CONNECT(EVAS_CALLBACK_KEY_UP, _ewk_view_on_key_up);
#undef CONNECT
}

static void _ewk_view_smart_del(Evas_Object* o)
{
    EWK_VIEW_SD_GET(o, sd);
    Ewk_View_Private_Data* priv = sd ? sd->_priv : 0;

    ewk_view_stop(o);
    _parent_sc.del(o);
    _ewk_view_priv_del(priv);
}

static void _ewk_view_smart_resize(Evas_Object* o, Evas_Coord w, Evas_Coord h)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);

    // these should be queued and processed in calculate as well!
    evas_object_resize(sd->backing_store, w, h);

    sd->changed.size = EINA_TRUE;
    _ewk_view_smart_changed(sd);
}

static void _ewk_view_smart_move(Evas_Object* o, Evas_Coord x, Evas_Coord y)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    sd->changed.position = EINA_TRUE;
    _ewk_view_smart_changed(sd);
}

static void _ewk_view_smart_calculate(Evas_Object* o)
{
    EWK_VIEW_SD_GET(o, sd);
    EWK_VIEW_PRIV_GET(sd, priv);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->contents_resize);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->scrolls_process);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->repaints_process);
    Evas_Coord x, y, w, h;

    sd->changed.any = EINA_FALSE;

    if (!sd->main_frame || !priv->main_frame)
        return;

    evas_object_geometry_get(o, &x, &y, &w, &h);

    DBG("o=%p geo=[%d, %d + %dx%d], changed: size=%hhu, "
        "scrolls=%zu, repaints=%zu",
        o, x, y, w, h, sd->changed.size,
        priv->scrolls.count, priv->repaints.count);

    if (sd->changed.size && ((w != sd->view.w) || (h != sd->view.h))) {
        WebCore::FrameView* view = priv->main_frame->view();
        if (view) {
            view->resize(w, h);
            view->forceLayout();
            view->adjustViewSize();
        }
        evas_object_resize(sd->main_frame, w, h);
        evas_object_resize(sd->events_rect, w, h);
        sd->changed.frame_rect = EINA_TRUE;
        sd->view.w = w;
        sd->view.h = h;

        _ewk_view_repaint_add(priv, 0, 0, w, h);

        // This callback is a good place e.g. to change fixed layout size (ewk_view_fixed_layout_size_set).
        evas_object_smart_callback_call(o, "view,resized", 0);
    }
    sd->changed.size = EINA_FALSE;

    if (sd->changed.position && ((x != sd->view.x) || (y != sd->view.y))) {
        evas_object_move(sd->main_frame, x, y);
        evas_object_move(sd->backing_store, x, y);
        evas_object_move(sd->events_rect, x, y);
        sd->changed.frame_rect = EINA_TRUE;
        sd->view.x = x;
        sd->view.y = y;
    }
    sd->changed.position = EINA_FALSE;

    if (!sd->api->scrolls_process(sd))
        ERR("failed to process scrolls.");
    _ewk_view_scrolls_flush(priv);

    if (!sd->api->repaints_process(sd))
        ERR("failed to process repaints.");
    _ewk_view_repaints_flush(priv);

    if (sd->changed.frame_rect) {
        WebCore::FrameView* view = priv->main_frame->view();
        view->frameRectsChanged(); /* force tree to get position from root */
        sd->changed.frame_rect = EINA_FALSE;
    }
}

static void _ewk_view_smart_show(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);

    if (evas_object_clipees_get(sd->base.clipper))
        evas_object_show(sd->base.clipper);
    evas_object_show(sd->backing_store);
}

static void _ewk_view_smart_hide(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);

    evas_object_hide(sd->base.clipper);
    evas_object_hide(sd->backing_store);
}

static Eina_Bool _ewk_view_smart_contents_resize(Ewk_View_Smart_Data* sd, int w, int h)
{
    return EINA_TRUE;
}

static Eina_Bool _ewk_view_smart_zoom_set(Ewk_View_Smart_Data* sd, float zoom, Evas_Coord cx, Evas_Coord cy)
{
    double px, py;
    Evas_Coord x, y, w, h;
    Eina_Bool ret;

    ewk_frame_scroll_size_get(sd->main_frame, &w, &h);
    ewk_frame_scroll_pos_get(sd->main_frame, &x, &y);

    if (w + sd->view.w > 0)
        px = static_cast<double>(x + cx) / (w + sd->view.w);
    else
        px = 0.0;

    if (h + sd->view.h > 0)
        py = static_cast<double>(y + cy) / (h + sd->view.h);
    else
        py = 0.0;

    ret = ewk_frame_zoom_set(sd->main_frame, zoom);

    ewk_frame_scroll_size_get(sd->main_frame, &w, &h);
    x = (w + sd->view.w) * px - cx;
    y = (h + sd->view.h) * py - cy;
    ewk_frame_scroll_set(sd->main_frame, x, y);
    return ret;
}

static void _ewk_view_smart_flush(Ewk_View_Smart_Data* sd)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv);
    _ewk_view_repaints_flush(priv);
    _ewk_view_scrolls_flush(priv);
}

static Eina_Bool _ewk_view_smart_pre_render_region(Ewk_View_Smart_Data* sd, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, float zoom)
{
    WRN("not supported by engine. sd=%p area=%d,%d+%dx%d, zoom=%f",
        sd, x, y, w, h, zoom);
    return EINA_FALSE;
}

static Eina_Bool _ewk_view_smart_pre_render_relative_radius(Ewk_View_Smart_Data* sd, unsigned int n, float zoom)
{
    WRN("not supported by engine. sd=%p, n=%u zoom=%f",
        sd, n, zoom);
    return EINA_FALSE;
}

static void _ewk_view_smart_pre_render_cancel(Ewk_View_Smart_Data* sd)
{
    WRN("not supported by engine. sd=%p", sd);
}

static void _ewk_view_zoom_animated_mark_stop(Ewk_View_Smart_Data* sd)
{
    sd->animated_zoom.zoom.start = 0.0;
    sd->animated_zoom.zoom.end = 0.0;
    sd->animated_zoom.zoom.current = 0.0;
}

static void _ewk_view_zoom_animated_finish(Ewk_View_Smart_Data* sd)
{
    EWK_VIEW_PRIV_GET(sd, priv);
    ecore_animator_del(priv->animated_zoom.animator);
    priv->animated_zoom.animator = 0;
    _ewk_view_zoom_animated_mark_stop(sd);
    evas_object_smart_callback_call(sd->self, "zoom,animated,end", 0);
}

static float _ewk_view_zoom_animated_current(Ewk_View_Private_Data* priv)
{
    double now = ecore_loop_time_get();
    double delta = now - priv->animated_zoom.time.start;

    if (delta > priv->animated_zoom.time.duration)
        delta = priv->animated_zoom.time.duration;
    if (delta < 0.0) // time went back, clock adjusted?
        delta = 0.0;

    delta /= priv->animated_zoom.time.duration;

    return ((priv->animated_zoom.zoom.range * delta)
            + priv->animated_zoom.zoom.start);
}

static Eina_Bool _ewk_view_zoom_animator_cb(void* data)
{
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(data);
    Evas_Coord cx, cy;
    EWK_VIEW_PRIV_GET(sd, priv);
    double now = ecore_loop_time_get();

    cx = priv->animated_zoom.center.x;
    cy = priv->animated_zoom.center.y;

    // TODO: progressively center (cx, cy) -> (view.x + view.h/2, view.y + view.h/2)
    if (cx >= sd->view.w)
        cx = sd->view.w - 1;
    if (cy >= sd->view.h)
        cy = sd->view.h - 1;

    if ((now >= priv->animated_zoom.time.end)
        || (now < priv->animated_zoom.time.start)) {
        _ewk_view_zoom_animated_finish(sd);
        ewk_view_zoom_set(sd->self, priv->animated_zoom.zoom.end, cx, cy);
        sd->api->sc.calculate(sd->self);
        return EINA_FALSE;
    }

    sd->animated_zoom.zoom.current = _ewk_view_zoom_animated_current(priv);
    sd->api->zoom_weak_set(sd, sd->animated_zoom.zoom.current, cx, cy);
    return EINA_TRUE;
}

static void _ewk_view_zoom_animation_start(Ewk_View_Smart_Data* sd)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv);
    if (priv->animated_zoom.animator)
        return;
    priv->animated_zoom.animator = ecore_animator_add
                                       (_ewk_view_zoom_animator_cb, sd);
}

static WebCore::ViewportAttributes _ewk_view_viewport_attributes_compute(const Evas_Object* o)
{
    EWK_VIEW_SD_GET(o, sd);
    EWK_VIEW_PRIV_GET(sd, priv);

    int desktop_width = 980;
    int device_dpi = ewk_util_dpi_get();

    WebCore::IntRect available_rect = enclosingIntRect(priv->page->chrome()->client()->pageRect());
    WebCore::IntRect device_rect = enclosingIntRect(priv->page->chrome()->client()->windowRect());

    WebCore::ViewportAttributes attributes = WebCore::computeViewportAttributes(priv->viewport_arguments, desktop_width, device_rect.width(), device_rect.height(), device_dpi, available_rect.size());

    return attributes;
}

static Eina_Bool _ewk_view_smart_disable_render(Ewk_View_Smart_Data* sd)
{
    WRN("not supported by engine. sd=%p", sd);
    return EINA_FALSE;
}

static Eina_Bool _ewk_view_smart_enable_render(Ewk_View_Smart_Data* sd)
{
    WRN("not supported by engine. sd=%p", sd);
    return EINA_FALSE;
}

Eina_Bool ewk_view_base_smart_set(Ewk_View_Smart_Class* api)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(api, EINA_FALSE);

    if (api->version != EWK_VIEW_SMART_CLASS_VERSION) {
        EINA_LOG_CRIT
            ("Ewk_View_Smart_Class %p is version %lu while %lu was expected.",
            api, api->version, EWK_VIEW_SMART_CLASS_VERSION);
        return EINA_FALSE;
    }

    if (EINA_UNLIKELY(!_parent_sc.add))
        evas_object_smart_clipped_smart_set(&_parent_sc);

    evas_object_smart_clipped_smart_set(&api->sc);
    api->sc.add = _ewk_view_smart_add;
    api->sc.del = _ewk_view_smart_del;
    api->sc.resize = _ewk_view_smart_resize;
    api->sc.move = _ewk_view_smart_move;
    api->sc.calculate = _ewk_view_smart_calculate;
    api->sc.show = _ewk_view_smart_show;
    api->sc.hide = _ewk_view_smart_hide;
    api->sc.data = EWK_VIEW_TYPE_STR; /* used by type checking */

    api->contents_resize = _ewk_view_smart_contents_resize;
    api->zoom_set = _ewk_view_smart_zoom_set;
    api->flush = _ewk_view_smart_flush;
    api->pre_render_region = _ewk_view_smart_pre_render_region;
    api->pre_render_relative_radius = _ewk_view_smart_pre_render_relative_radius;
    api->pre_render_cancel = _ewk_view_smart_pre_render_cancel;
    api->disable_render = _ewk_view_smart_disable_render;
    api->enable_render = _ewk_view_smart_enable_render;

    api->focus_in = _ewk_view_smart_focus_in;
    api->focus_out = _ewk_view_smart_focus_out;
    api->mouse_wheel = _ewk_view_smart_mouse_wheel;
    api->mouse_down = _ewk_view_smart_mouse_down;
    api->mouse_up = _ewk_view_smart_mouse_up;
    api->mouse_move = _ewk_view_smart_mouse_move;
    api->key_down = _ewk_view_smart_key_down;
    api->key_up = _ewk_view_smart_key_up;

    api->add_console_message = _ewk_view_smart_add_console_message;
    api->run_javascript_alert = _ewk_view_smart_run_javascript_alert;
    api->run_javascript_confirm = _ewk_view_smart_run_javascript_confirm;
    api->run_javascript_prompt = _ewk_view_smart_run_javascript_prompt;
    api->should_interrupt_javascript = _ewk_view_smart_should_interrupt_javascript;

    return EINA_TRUE;
}

void ewk_view_fixed_layout_size_set(Evas_Object* o, Evas_Coord w, Evas_Coord h)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv);

    WebCore::FrameView* view = priv->main_frame->view();
    if (w <= 0 && h <= 0) {
        if (!view->useFixedLayout())
            return;
        view->setUseFixedLayout(EINA_FALSE);
    } else {
        WebCore::IntSize size = view->fixedLayoutSize();
        if (size.width() == w && size.height() == h)
            return;
        if (view)
            view->setFixedLayoutSize(WebCore::IntSize(w, h));
    }

    if (!view)
        return;
    view->setUseFixedLayout(EINA_TRUE);
    view->forceLayout();
}

void ewk_view_fixed_layout_size_get(const Evas_Object* o, Evas_Coord* w, Evas_Coord* h)
{
    if (w)
        *w = 0;
    if (h)
        *h = 0;
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv);
    WebCore::FrameView* view = priv->main_frame->view();
    if (view->useFixedLayout()) {
        WebCore::IntSize size = view->fixedLayoutSize();
        if (w)
            *w = size.width();
        if (h)
            *h = size.height();
    }
}

void ewk_view_theme_set(Evas_Object* o, const char* path)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv);
    if (!eina_stringshare_replace(&priv->settings.theme, path))
        return;

    WebCore::FrameView* view = priv->main_frame->view();
    if (view) {
        view->setEdjeTheme(WTF::String(path));
        priv->page->theme()->themeChanged();
    }

}

const char* ewk_view_theme_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);
    return priv->settings.theme;
}

Evas_Object* ewk_view_frame_main_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    return sd->main_frame;
}

Evas_Object* ewk_view_frame_focused_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);

    WebCore::Frame* core = priv->page->focusController()->focusedFrame();
    if (!core)
        return 0;

    WebCore::FrameLoaderClientEfl* client = static_cast<WebCore::FrameLoaderClientEfl*>(core->loader()->client());
    if (!client)
        return 0;
    return client->webFrame();
}

Eina_Bool ewk_view_uri_set(Evas_Object* o, const char* uri)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_uri_set(sd->main_frame, uri);
}

const char* ewk_view_uri_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    return ewk_frame_uri_get(sd->main_frame);
}

const char* ewk_view_title_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    return ewk_frame_title_get(sd->main_frame);
}

Eina_Bool ewk_view_editable_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_editable_get(sd->main_frame);
}

void ewk_view_bg_color_set(Evas_Object* o, int r, int g, int b, int a)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->bg_color_set);

    if (a < 0) {
        WRN("Alpha less than zero (%d).", a);
        a = 0;
    } else if (a > 255) {
        WRN("Alpha is larger than 255 (%d).", a);
        a = 255;
    }

#define CHECK_PREMUL_COLOR(c, a)                                        \
    if (c < 0) {                                                        \
        WRN("Color component " #c " is less than zero (%d).", c);         \
        c = 0;                                                          \
    } else if (c > a) {                                                 \
        WRN("Color component " #c " is greater than alpha (%d, alpha=%d).", \
            c, a);                                                      \
        c = a;                                                          \
    }
    CHECK_PREMUL_COLOR(r, a);
    CHECK_PREMUL_COLOR(g, a);
    CHECK_PREMUL_COLOR(b, a);
#undef CHECK_PREMUL_COLOR

    sd->bg_color.r = r;
    sd->bg_color.g = g;
    sd->bg_color.b = b;
    sd->bg_color.a = a;

    sd->api->bg_color_set(sd, r, g, b, a);

    WebCore::FrameView* view = sd->_priv->main_frame->view();
    if (view) {
        WebCore::Color color;

        if (!a)
            color = WebCore::Color(0, 0, 0, 0);
        else if (a == 255)
            color = WebCore::Color(r, g, b, a);
        else
            color = WebCore::Color(r * 255 / a, g * 255 / a, b * 255 / a, a);

        view->updateBackgroundRecursively(color, !a);
    }
}

void ewk_view_bg_color_get(const Evas_Object* o, int* r, int* g, int* b, int* a)
{
    if (r)
        *r = 0;
    if (g)
        *g = 0;
    if (b)
        *b = 0;
    if (a)
        *a = 0;
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    if (r)
        *r = sd->bg_color.r;
    if (g)
        *g = sd->bg_color.g;
    if (b)
        *b = sd->bg_color.b;
    if (a)
        *a = sd->bg_color.a;
}

Eina_Bool ewk_view_text_search(const Evas_Object* o, const char* string, Eina_Bool case_sensitive, Eina_Bool forward, Eina_Bool wrap)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(string, EINA_FALSE);
    WTF::TextCaseSensitivity sensitive;
    WebCore::FindDirection direction;

    if (case_sensitive)
        sensitive = WTF::TextCaseSensitive;
    else
        sensitive = WTF::TextCaseInsensitive;

    if (forward)
        direction = WebCore::FindDirectionForward;
    else
        direction = WebCore::FindDirectionBackward;

    return priv->page->findString(String::fromUTF8(string), sensitive, direction, wrap);
}

/**
 * Mark matches the given text string in document.
 *
 * @param o view object where to search text.
 * @param string reference string to match.
 * @param case_sensitive if match should be case sensitive or not.
 * @param highlight if matches should be highlighted.
 * @param limit maximum amount of matches, or zero to unlimited.
 *
 * @return number of matches.
 */
unsigned int ewk_view_text_matches_mark(Evas_Object* o, const char* string, Eina_Bool case_sensitive, Eina_Bool highlight, unsigned int limit)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(string, 0);
    WTF::TextCaseSensitivity sensitive;

    if (case_sensitive)
        sensitive = WTF::TextCaseSensitive;
    else
        sensitive = WTF::TextCaseInsensitive;

    return priv->page->markAllMatchesForText(String::fromUTF8(string), sensitive, highlight, limit);
}

Eina_Bool ewk_view_text_matches_unmark_all(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    priv->page->unmarkAllTextMatches();
    return EINA_TRUE;
}

Eina_Bool ewk_view_text_matches_highlight_set(Evas_Object* o, Eina_Bool highlight)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_text_matches_highlight_set(sd->main_frame, highlight);
}

Eina_Bool ewk_view_text_matches_highlight_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_text_matches_highlight_get(sd->main_frame);
}

Eina_Bool ewk_view_editable_set(Evas_Object* o, Eina_Bool editable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_editable_set(sd->main_frame, editable);
}

char* ewk_view_selection_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);
    CString s = priv->page->focusController()->focusedOrMainFrame()->editor()->selectedText().utf8();
    if (s.isNull())
        return 0;
    return strdup(s.data());
}

static Eina_Bool _ewk_view_editor_command(Ewk_View_Private_Data* priv, const char* command, const char* value = 0)
{
    return priv->page->focusController()->focusedOrMainFrame()->editor()->command(WTF::String::fromUTF8(command)).execute(value);
}

Eina_Bool ewk_view_execute_editor_command(Evas_Object* o, const Ewk_Editor_Command command, const char* value)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);

    switch (command) {
    case EWK_EDITOR_COMMAND_INSERT_IMAGE:
        return _ewk_view_editor_command(priv, "InsertImage", value);
    case EWK_EDITOR_COMMAND_INSERT_TEXT:
        return _ewk_view_editor_command(priv, "InsertText", value);
    case EWK_EDITOR_COMMAND_SELECT_NONE:
        return _ewk_view_editor_command(priv, "Unselect");
    case EWK_EDITOR_COMMAND_SELECT_ALL:
        return _ewk_view_editor_command(priv, "SelectAll");
    case EWK_EDITOR_COMMAND_SELECT_PARAGRAPH:
        return _ewk_view_editor_command(priv, "SelectParagraph");
    case EWK_EDITOR_COMMAND_SELECT_SENTENCE:
        return _ewk_view_editor_command(priv, "SelectSentence");
    case EWK_EDITOR_COMMAND_SELECT_LINE:
        return _ewk_view_editor_command(priv, "SelectLine");
    case EWK_EDITOR_COMMAND_SELECT_WORD:
        return _ewk_view_editor_command(priv, "SelectWord");
    default:
        return EINA_FALSE;
    }
}

Eina_Bool ewk_view_context_menu_forward_event(Evas_Object* o, const Evas_Event_Mouse_Down* ev)
{
#if ENABLE(CONTEXT_MENUS)
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    Eina_Bool mouse_press_handled = EINA_FALSE;

    priv->page->contextMenuController()->clearContextMenu();
    WebCore::Frame* main_frame = priv->page->mainFrame();
    Evas_Coord x, y;
    evas_object_geometry_get(sd->self, &x, &y, 0, 0);

    WebCore::PlatformMouseEvent event(ev, WebCore::IntPoint(x, y));

    if (main_frame->view()) {
        mouse_press_handled =
            main_frame->eventHandler()->handleMousePressEvent(event);
    }

    if (main_frame->eventHandler()->sendContextMenuEvent(event))
        return EINA_FALSE;

    WebCore::ContextMenu* coreMenu =
        priv->page->contextMenuController()->contextMenu();
    if (!coreMenu) {
        // WebCore decided not to create a context menu, return true if event
        // was handled by handleMouseReleaseEvent
        return mouse_press_handled;
    }

    return EINA_TRUE;
#else
    return EINA_FALSE;
#endif
}

double ewk_view_load_progress_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, -1.0);
    return priv->page->progress()->estimatedProgress();
}

Eina_Bool ewk_view_stop(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_stop(sd->main_frame);
}

Eina_Bool ewk_view_reload(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_reload(sd->main_frame);
}

Eina_Bool ewk_view_reload_full(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_reload_full(sd->main_frame);
}

Eina_Bool ewk_view_back(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_back(sd->main_frame);
}

Eina_Bool ewk_view_forward(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_forward(sd->main_frame);
}

Eina_Bool ewk_view_navigate(Evas_Object* o, int steps)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_navigate(sd->main_frame, steps);
}

Eina_Bool ewk_view_back_possible(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_back_possible(sd->main_frame);
}

Eina_Bool ewk_view_forward_possible(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_forward_possible(sd->main_frame);
}

Eina_Bool ewk_view_navigate_possible(Evas_Object* o, int steps)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_navigate_possible(sd->main_frame, steps);
}

Eina_Bool ewk_view_history_enable_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return static_cast<WebCore::BackForwardListImpl*>(priv->page->backForwardList())->enabled();
}

Eina_Bool ewk_view_history_enable_set(Evas_Object* o, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    static_cast<WebCore::BackForwardListImpl*>(priv->page->backForwardList())->setEnabled(enable);
    return EINA_TRUE;
}

Ewk_History* ewk_view_history_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);
    if (!static_cast<WebCore::BackForwardListImpl*>(priv->page->backForwardList())->enabled()) {
        ERR("asked history, but it's disabled! Returning 0!");
        return 0;
    }
    return priv->history;
}

float ewk_view_zoom_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, -1.0);
    return ewk_frame_zoom_get(sd->main_frame);
}

Eina_Bool ewk_view_zoom_set(Evas_Object* o, float zoom, Evas_Coord cx, Evas_Coord cy)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET(sd, priv);

    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api->zoom_set, EINA_FALSE);

    if (!priv->settings.zoom_range.user_scalable) {
        WRN("userScalable is false");
        return EINA_FALSE;
    }

    if (zoom < priv->settings.zoom_range.min_scale) {
        WRN("zoom level is < %f : %f", priv->settings.zoom_range.min_scale, zoom);
        return EINA_FALSE;
    }
    if (zoom > priv->settings.zoom_range.max_scale) {
        WRN("zoom level is > %f : %f", priv->settings.zoom_range.max_scale, zoom);
        return EINA_FALSE;
    }

    _ewk_view_zoom_animated_mark_stop(sd);
    return sd->api->zoom_set(sd, zoom, cx, cy);
}

Eina_Bool ewk_view_zoom_weak_smooth_scale_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return sd->zoom_weak_smooth_scale;
}

void ewk_view_zoom_weak_smooth_scale_set(Evas_Object* o, Eina_Bool smooth_scale)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    smooth_scale = !!smooth_scale;
    if (sd->zoom_weak_smooth_scale == smooth_scale)
        return;
    sd->zoom_weak_smooth_scale = smooth_scale;
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->zoom_weak_smooth_scale_set);
    sd->api->zoom_weak_smooth_scale_set(sd, smooth_scale);
}

Eina_Bool ewk_view_zoom_weak_set(Evas_Object* o, float zoom, Evas_Coord cx, Evas_Coord cy)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET(sd, priv);

    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api->zoom_weak_set, EINA_FALSE);

    if (!priv->settings.zoom_range.user_scalable) {
        WRN("userScalable is false");
        return EINA_FALSE;
    }

    if (zoom < priv->settings.zoom_range.min_scale) {
        WRN("zoom level is < %f : %f", priv->settings.zoom_range.min_scale, zoom);
        return EINA_FALSE;
    }
    if (zoom > priv->settings.zoom_range.max_scale) {
        WRN("zoom level is > %f : %f", priv->settings.zoom_range.max_scale, zoom);
        return EINA_FALSE;
    }

    sd->animated_zoom.zoom.start = ewk_frame_zoom_get(sd->main_frame);
    sd->animated_zoom.zoom.end = zoom;
    sd->animated_zoom.zoom.current = zoom;
    return sd->api->zoom_weak_set(sd, zoom, cx, cy);
}

Eina_Bool ewk_view_zoom_animated_mark_start(Evas_Object* o, float zoom)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    sd->animated_zoom.zoom.start = zoom;
    return EINA_TRUE;
}

Eina_Bool ewk_view_zoom_animated_mark_end(Evas_Object* o, float zoom)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    sd->animated_zoom.zoom.end = zoom;
    return EINA_TRUE;
}

Eina_Bool ewk_view_zoom_animated_mark_current(Evas_Object* o, float zoom)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    sd->animated_zoom.zoom.current = zoom;
    return EINA_TRUE;
}

Eina_Bool ewk_view_zoom_animated_mark_stop(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    _ewk_view_zoom_animated_mark_stop(sd);
    return EINA_TRUE;
}

Eina_Bool ewk_view_zoom_animated_set(Evas_Object* o, float zoom, float duration, Evas_Coord cx, Evas_Coord cy)
{
    double now;
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api->zoom_weak_set, EINA_FALSE);

    if (!priv->settings.zoom_range.user_scalable) {
        WRN("userScalable is false");
        return EINA_FALSE;
    }

    if (zoom < priv->settings.zoom_range.min_scale) {
        WRN("zoom level is < %f : %f", priv->settings.zoom_range.min_scale, zoom);
        return EINA_FALSE;
    }
    if (zoom > priv->settings.zoom_range.max_scale) {
        WRN("zoom level is > %f : %f", priv->settings.zoom_range.max_scale, zoom);
        return EINA_FALSE;
    }

    if (priv->animated_zoom.animator)
        priv->animated_zoom.zoom.start = _ewk_view_zoom_animated_current(priv);
    else {
        priv->animated_zoom.zoom.start = ewk_frame_zoom_get(sd->main_frame);
        _ewk_view_zoom_animation_start(sd);
    }

    if (cx < 0)
        cx = 0;
    if (cy < 0)
        cy = 0;

    now = ecore_loop_time_get();
    priv->animated_zoom.time.start = now;
    priv->animated_zoom.time.end = now + duration;
    priv->animated_zoom.time.duration = duration;
    priv->animated_zoom.zoom.end = zoom;
    priv->animated_zoom.zoom.range = (priv->animated_zoom.zoom.end - priv->animated_zoom.zoom.start);
    priv->animated_zoom.center.x = cx;
    priv->animated_zoom.center.y = cy;
    sd->animated_zoom.zoom.current = priv->animated_zoom.zoom.start;
    sd->animated_zoom.zoom.start = priv->animated_zoom.zoom.start;
    sd->animated_zoom.zoom.end = priv->animated_zoom.zoom.end;

    return EINA_TRUE;
}

Eina_Bool ewk_view_zoom_text_only_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_zoom_text_only_get(sd->main_frame);
}

Eina_Bool ewk_view_zoom_text_only_set(Evas_Object* o, Eina_Bool setting)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return ewk_frame_zoom_text_only_set(sd->main_frame, setting);
}

Eina_Bool ewk_view_pre_render_region(Evas_Object* o, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, float zoom)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api->pre_render_region, EINA_FALSE);
    float cur_zoom;
    Evas_Coord cw, ch;

    /* When doing animated zoom it's not possible to call pre-render since it
     * would screw up parameters that animation is currently using
     */
    if (priv->animated_zoom.animator)
        return EINA_FALSE;

    cur_zoom = ewk_frame_zoom_get(sd->main_frame);

    if (cur_zoom < 0.00001)
        return EINA_FALSE;
    if (!ewk_frame_contents_size_get(sd->main_frame, &cw, &ch))
        return EINA_FALSE;

    cw *= zoom / cur_zoom;
    ch *= zoom / cur_zoom;
    DBG("region %d,%d+%dx%d @ %f contents=%dx%d", x, y, w, h, zoom, cw, ch);

    if (x + w > cw)
        w = cw - x;

    if (y + h > ch)
        h = ch - y;

    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }

    return sd->api->pre_render_region(sd, x, y, w, h, zoom);
}

Eina_Bool ewk_view_pre_render_relative_radius(Evas_Object* o, unsigned int n)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api->pre_render_relative_radius, EINA_FALSE);
    float cur_zoom;

    if (priv->animated_zoom.animator)
        return EINA_FALSE;

    cur_zoom = ewk_frame_zoom_get(sd->main_frame);
    return sd->api->pre_render_relative_radius(sd, n, cur_zoom);
}

unsigned int ewk_view_imh_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);
    return priv->imh;
}

void ewk_view_pre_render_cancel(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->pre_render_cancel);
    sd->api->pre_render_cancel(sd);
}

Eina_Bool ewk_view_enable_render(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api->enable_render, EINA_FALSE);
    return sd->api->enable_render(sd);
}

Eina_Bool ewk_view_disable_render(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api->disable_render, EINA_FALSE);
    return sd->api->disable_render(sd);
}

const char* ewk_view_setting_user_agent_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);
    return priv->settings.user_agent;
}

Eina_Bool ewk_view_setting_user_agent_set(Evas_Object* o, const char* user_agent)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    if (eina_stringshare_replace(&priv->settings.user_agent, user_agent)) {
        WebCore::FrameLoaderClientEfl* client = static_cast<WebCore::FrameLoaderClientEfl*>(priv->main_frame->loader()->client());
        client->setCustomUserAgent(String::fromUTF8(user_agent));
    }
    return EINA_TRUE;
}

const char* ewk_view_setting_user_stylesheet_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);
    return priv->settings.user_stylesheet;
}

Eina_Bool ewk_view_setting_user_stylesheet_set(Evas_Object* o, const char* uri)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    if (eina_stringshare_replace(&priv->settings.user_stylesheet, uri)) {
        WebCore::KURL kurl(WebCore::KURL(), String::fromUTF8(uri));
        priv->page_settings->setUserStyleSheetLocation(kurl);
    }
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_auto_load_images_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.auto_load_images;
}

Eina_Bool ewk_view_setting_auto_load_images_set(Evas_Object* o, Eina_Bool automatic)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    automatic = !!automatic;
    if (priv->settings.auto_load_images != automatic) {
        priv->page_settings->setLoadsImagesAutomatically(automatic);
        priv->settings.auto_load_images = automatic;
    }
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_auto_shrink_images_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.auto_shrink_images;
}

Eina_Bool ewk_view_setting_auto_shrink_images_set(Evas_Object* o, Eina_Bool automatic)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    automatic = !!automatic;
    if (priv->settings.auto_shrink_images != automatic) {
        priv->page_settings->setShrinksStandaloneImagesToFit(automatic);
        priv->settings.auto_shrink_images = automatic;
    }
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_enable_auto_resize_window_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.enable_auto_resize_window;
}

Eina_Bool ewk_view_setting_enable_auto_resize_window_set(Evas_Object* o, Eina_Bool resizable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    priv->settings.enable_auto_resize_window = resizable;
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_enable_scripts_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.enable_scripts;
}

Eina_Bool ewk_view_setting_enable_scripts_set(Evas_Object* o, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    enable = !!enable;
    if (priv->settings.enable_scripts != enable) {
        priv->page_settings->setJavaScriptEnabled(enable);
        priv->settings.enable_scripts = enable;
    }
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_enable_plugins_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.enable_plugins;
}

Eina_Bool ewk_view_setting_enable_plugins_set(Evas_Object* o, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    enable = !!enable;
    if (priv->settings.enable_plugins != enable) {
        priv->page_settings->setPluginsEnabled(enable);
        priv->settings.enable_plugins = enable;
    }
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_enable_frame_flattening_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.enable_frame_flattening;
}

Eina_Bool ewk_view_setting_enable_frame_flattening_set(Evas_Object* o, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    enable = !!enable;
    if (priv->settings.enable_frame_flattening != enable) {
        priv->page_settings->setFrameFlatteningEnabled(enable);
        priv->settings.enable_frame_flattening = enable;
    }
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_scripts_window_open_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.scripts_window_open;
}

Eina_Bool ewk_view_setting_scripts_window_open_set(Evas_Object* o, Eina_Bool allow)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    allow = !!allow;
    if (priv->settings.scripts_window_open != allow) {
        priv->page_settings->setJavaScriptCanOpenWindowsAutomatically(allow);
        priv->settings.scripts_window_open = allow;
    }
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_scripts_can_close_windows_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.scripts_can_close_windows;
}

Eina_Bool ewk_view_setting_scripts_can_close_windows_set(Evas_Object* o, Eina_Bool allow)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    allow = !!allow;
    if (priv->settings.scripts_can_close_windows != allow) {
        priv->page_settings->setAllowScriptsToCloseWindows(allow);
        priv->settings.scripts_can_close_windows = allow;
    }
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_resizable_textareas_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.resizable_textareas;
}

Eina_Bool ewk_view_setting_resizable_textareas_set(Evas_Object* o, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    enable = !!enable;
    if (priv->settings.resizable_textareas != enable) {
        priv->page_settings->setTextAreasAreResizable(enable);
        priv->settings.resizable_textareas = enable;
    }
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_private_browsing_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.private_browsing;
}

Eina_Bool ewk_view_setting_private_browsing_set(Evas_Object* o, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    enable = !!enable;
    if (priv->settings.private_browsing != enable) {
        priv->page_settings->setPrivateBrowsingEnabled(enable);
        priv->settings.private_browsing = enable;
    }
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_offline_app_cache_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.offline_app_cache;
}

Eina_Bool ewk_view_setting_offline_app_cache_set(Evas_Object* o, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    enable = !!enable;
    if (priv->settings.offline_app_cache != enable) {
        priv->page_settings->setOfflineWebApplicationCacheEnabled(enable);
        priv->settings.offline_app_cache = enable;
    }
    return EINA_TRUE;
}


Eina_Bool ewk_view_setting_caret_browsing_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.caret_browsing;
}

Eina_Bool ewk_view_setting_caret_browsing_set(Evas_Object* o, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    enable = !!enable;
    if (priv->settings.caret_browsing != enable) {
        priv->page_settings->setCaretBrowsingEnabled(enable);
        priv->settings.caret_browsing = enable;
    }
    return EINA_TRUE;
}

const char* ewk_view_setting_encoding_custom_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);
    Evas_Object* main_frame = ewk_view_frame_main_get(o);
    WebCore::Frame* core_frame = ewk_frame_core_get(main_frame);

    String overrideEncoding = core_frame->loader()->documentLoader()->overrideEncoding();

    if (overrideEncoding.isEmpty())
        return 0;

    eina_stringshare_replace(&priv->settings.encoding_custom, overrideEncoding.utf8().data());
    return priv->settings.encoding_custom;
}

Eina_Bool ewk_view_setting_encoding_custom_set(Evas_Object* o, const char* encoding)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    Evas_Object* main_frame = ewk_view_frame_main_get(o);
    WebCore::Frame* core_frame = ewk_frame_core_get(main_frame);
    DBG("%s", encoding);
    if (eina_stringshare_replace(&priv->settings.encoding_custom, encoding))
        core_frame->loader()->reloadWithOverrideEncoding(String::fromUTF8(encoding));
    return EINA_TRUE;
}

const char* ewk_view_setting_encoding_default_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);
    return priv->settings.encoding_default;
}

Eina_Bool ewk_view_setting_encoding_default_set(Evas_Object* o, const char* encoding)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    if (eina_stringshare_replace(&priv->settings.encoding_default, encoding))
        priv->page_settings->setDefaultTextEncodingName(String::fromUTF8(encoding));
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_encoding_detector_set(Evas_Object* o, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    enable = !!enable;
    if (priv->settings.encoding_detector != enable) {
        priv->page_settings->setUsesEncodingDetector(enable);
        priv->settings.encoding_detector = enable;
    }
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_encoding_detector_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.encoding_detector;
}

Eina_Bool ewk_view_setting_enable_developer_extras_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.enable_developer_extras;
}

Eina_Bool ewk_view_setting_enable_developer_extras_set(Evas_Object* o, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    enable = !!enable;
    if (priv->settings.enable_developer_extras != enable) {
        priv->page_settings->setDeveloperExtrasEnabled(enable);
        priv->settings.enable_developer_extras = enable;
    }
    return EINA_TRUE;
}

int ewk_view_setting_font_minimum_size_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);
    return priv->settings.font_minimum_size;
}

Eina_Bool ewk_view_setting_font_minimum_size_set(Evas_Object* o, int size)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    if (priv->settings.font_minimum_size != size) {
        priv->page_settings->setMinimumFontSize(size);
        priv->settings.font_minimum_size = size;
    }
    return EINA_TRUE;
}

int ewk_view_setting_font_minimum_logical_size_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);
    return priv->settings.font_minimum_logical_size;
}

Eina_Bool ewk_view_setting_font_minimum_logical_size_set(Evas_Object* o, int size)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    if (priv->settings.font_minimum_logical_size != size) {
        priv->page_settings->setMinimumLogicalFontSize(size);
        priv->settings.font_minimum_logical_size = size;
    }
    return EINA_TRUE;
}

int ewk_view_setting_font_default_size_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);
    return priv->settings.font_default_size;
}

Eina_Bool ewk_view_setting_font_default_size_set(Evas_Object* o, int size)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    if (priv->settings.font_default_size != size) {
        priv->page_settings->setDefaultFontSize(size);
        priv->settings.font_default_size = size;
    }
    return EINA_TRUE;
}

int ewk_view_setting_font_monospace_size_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);
    return priv->settings.font_monospace_size;
}

Eina_Bool ewk_view_setting_font_monospace_size_set(Evas_Object* o, int size)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    if (priv->settings.font_monospace_size != size) {
        priv->page_settings->setDefaultFixedFontSize(size);
        priv->settings.font_monospace_size = size;
    }
    return EINA_TRUE;
}

const char* ewk_view_font_family_name_get(const Evas_Object* o, Ewk_Font_Family font_family)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);

    switch (font_family) {
    case EWK_FONT_FAMILY_STANDARD:
        return priv->settings.font_standard;
    case EWK_FONT_FAMILY_CURSIVE:
        return priv->settings.font_cursive;
    case EWK_FONT_FAMILY_FANTASY:
        return priv->settings.font_fantasy;
    case EWK_FONT_FAMILY_MONOSPACE:
        return priv->settings.font_monospace;
    case EWK_FONT_FAMILY_SERIF:
        return priv->settings.font_serif;
    case EWK_FONT_FAMILY_SANS_SERIF:
        return priv->settings.font_sans_serif;
    }
    return 0;
}

Eina_Bool ewk_view_font_family_name_set(Evas_Object* o, Ewk_Font_Family font_family, const char* name)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);

    switch (font_family) {
    case EWK_FONT_FAMILY_STANDARD:
        if (eina_stringshare_replace(&priv->settings.font_standard, name))
            priv->page_settings->setStandardFontFamily(AtomicString::fromUTF8(name));
        break;
    case EWK_FONT_FAMILY_CURSIVE:
        if (eina_stringshare_replace(&priv->settings.font_cursive, name))
            priv->page_settings->setCursiveFontFamily(AtomicString::fromUTF8(name));
        break;
    case EWK_FONT_FAMILY_FANTASY:
        if (eina_stringshare_replace(&priv->settings.font_fantasy, name))
            priv->page_settings->setFantasyFontFamily(AtomicString::fromUTF8(name));
        break;
    case EWK_FONT_FAMILY_MONOSPACE:
        if (eina_stringshare_replace(&priv->settings.font_monospace, name))
            priv->page_settings->setFixedFontFamily(AtomicString::fromUTF8(name));
        break;
    case EWK_FONT_FAMILY_SERIF:
        if (eina_stringshare_replace(&priv->settings.font_serif, name))
            priv->page_settings->setSerifFontFamily(AtomicString::fromUTF8(name));
        break;
    case EWK_FONT_FAMILY_SANS_SERIF:
        if (eina_stringshare_replace(&priv->settings.font_sans_serif, name))
            priv->page_settings->setSansSerifFontFamily(AtomicString::fromUTF8(name));
        break;
    default:
        return EINA_FALSE;
    }

    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_spatial_navigation_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.spatial_navigation;
}

Eina_Bool ewk_view_setting_spatial_navigation_set(Evas_Object* o, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    enable = !!enable;
    if (priv->settings.spatial_navigation != enable) {
        priv->page_settings->setSpatialNavigationEnabled(enable);
        priv->settings.spatial_navigation = enable;
    }
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_local_storage_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.local_storage;
}

Eina_Bool ewk_view_setting_local_storage_set(Evas_Object* o, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    enable = !!enable;
    if (priv->settings.local_storage != enable) {
        priv->page_settings->setLocalStorageEnabled(enable);
        priv->settings.local_storage = enable;
    }
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_page_cache_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.page_cache;
}

Eina_Bool ewk_view_setting_page_cache_set(Evas_Object* o, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    enable = !!enable;
    if (priv->settings.page_cache != enable) {
        priv->page_settings->setUsesPageCache(enable);
        priv->settings.page_cache = enable;
    }
    return EINA_TRUE;
}

const char* ewk_view_setting_local_storage_database_path_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);
    return priv->settings.local_storage_database_path;
}

Eina_Bool ewk_view_setting_local_storage_database_path_set(Evas_Object* o, const char* path)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    if (eina_stringshare_replace(&priv->settings.local_storage_database_path, path))
        priv->page_settings->setLocalStorageDatabasePath(String::fromUTF8(path));
    return EINA_TRUE;
}

Eina_Bool ewk_view_setting_minimum_timer_interval_set(Evas_Object* o, double interval)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    if (fabs(priv->settings.dom_timer_interval - interval) >= std::numeric_limits<double>::epsilon()) {
        priv->page_settings->setMinDOMTimerInterval(interval);
        priv->settings.dom_timer_interval = interval;
    }
    return EINA_TRUE;
}

double ewk_view_setting_minimum_timer_interval_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->settings.dom_timer_interval;
}

Ewk_View_Smart_Data* ewk_view_smart_data_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    return sd;
}

/**
 * Gets the internal array of repaint requests.
 *
 * This array should not be modified anyhow. It should be processed
 * immediately as any further ewk_view call might change it, like
 * those that add repaints or flush them, so be sure that your code
 * does not call any of those while you process the repaints,
 * otherwise copy the array.
 *
 * @param priv private handle pointer of the view to get repaints.
 * @param count where to return the number of elements of returned array, may be @c 0.
 *
 * @return reference to array of requested repaints.
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
const Eina_Rectangle* ewk_view_repaints_get(const Ewk_View_Private_Data* priv, size_t* count)
{
    if (count)
        *count = 0;
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv, 0);
    if (count)
        *count = priv->repaints.count;
    return priv->repaints.array;
}

/**
 * Gets the internal array of scroll requests.
 *
 * This array should not be modified anyhow. It should be processed
 * immediately as any further ewk_view call might change it, like
 * those that add scrolls or flush them, so be sure that your code
 * does not call any of those while you process the scrolls,
 * otherwise copy the array.
 *
 * @param priv private handle pointer of the view to get scrolls.
 * @param count where to return the number of elements of returned array, may be @c 0.
 *
 * @return reference to array of requested scrolls.
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
const Ewk_Scroll_Request* ewk_view_scroll_requests_get(const Ewk_View_Private_Data* priv, size_t* count)
{
    if (count)
        *count = 0;
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv, 0);
    if (count)
        *count = priv->scrolls.count;
    return priv->scrolls.array;
}

/**
 * Add a new repaint request to queue.
 *
 * The repaints are assumed to be relative to current viewport.
 *
 * @param priv private handle pointer of the view to add repaint request.
 * @param x horizontal position relative to current view port (scrolled).
 * @param y vertical position relative to current view port (scrolled).
 * @param w width of area to be repainted
 * @param h height of area to be repainted
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
void ewk_view_repaint_add(Ewk_View_Private_Data* priv, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
    EINA_SAFETY_ON_NULL_RETURN(priv);
    _ewk_view_repaint_add(priv, x, y, w, h);
}

/**
 * Do layout if required, applied recursively.
 *
 * @param priv private handle pointer of the view to layout.
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
void ewk_view_layout_if_needed_recursive(Ewk_View_Private_Data* priv)
{
    EINA_SAFETY_ON_NULL_RETURN(priv);

    WebCore::FrameView* v = priv->main_frame->view();
    if (!v) {
        ERR("no main frame view");
        return;
    }
    v->updateLayoutAndStyleIfNeededRecursive();
}

void ewk_view_scrolls_process(Ewk_View_Smart_Data* sd)
{
    EINA_SAFETY_ON_NULL_RETURN(sd);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv);
    if (!sd->api->scrolls_process(sd))
        ERR("failed to process scrolls.");
    _ewk_view_scrolls_flush(priv);
}

/**
 * @brief Structure that keeps the paint context.
 *
 * @internal
 *
 * @note This is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
struct _Ewk_View_Paint_Context {
    WebCore::GraphicsContext* gc;
    WebCore::FrameView* view;
    cairo_t* cr;
};

Ewk_View_Paint_Context* ewk_view_paint_context_new(Ewk_View_Private_Data* priv, cairo_t* cr)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(cr, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv->main_frame, 0);
    WebCore::FrameView* view = priv->main_frame->view();
    EINA_SAFETY_ON_NULL_RETURN_VAL(view, 0);
    Ewk_View_Paint_Context* ctxt = static_cast<Ewk_View_Paint_Context*>(malloc(sizeof(*ctxt)));
    EINA_SAFETY_ON_NULL_RETURN_VAL(ctxt, 0);

    ctxt->gc = new WebCore::GraphicsContext(cr);
    if (!ctxt->gc) {
        free(ctxt);
        return 0;
    }
    ctxt->view = view;
    ctxt->cr = cairo_reference(cr);
    return ctxt;
}

void ewk_view_paint_context_free(Ewk_View_Paint_Context* ctxt)
{
    EINA_SAFETY_ON_NULL_RETURN(ctxt);
    delete ctxt->gc;
    cairo_destroy(ctxt->cr);
    free(ctxt);
}

void ewk_view_paint_context_save(Ewk_View_Paint_Context* ctxt)
{
    EINA_SAFETY_ON_NULL_RETURN(ctxt);
    cairo_save(ctxt->cr);
    ctxt->gc->save();
}

void ewk_view_paint_context_restore(Ewk_View_Paint_Context* ctxt)
{
    EINA_SAFETY_ON_NULL_RETURN(ctxt);
    ctxt->gc->restore();
    cairo_restore(ctxt->cr);
}

void ewk_view_paint_context_clip(Ewk_View_Paint_Context* ctxt, const Eina_Rectangle* area)
{
    EINA_SAFETY_ON_NULL_RETURN(ctxt);
    EINA_SAFETY_ON_NULL_RETURN(area);
    ctxt->gc->clip(WebCore::IntRect(area->x, area->y, area->w, area->h));
}

void ewk_view_paint_context_paint(Ewk_View_Paint_Context* ctxt, const Eina_Rectangle* area)
{
    EINA_SAFETY_ON_NULL_RETURN(ctxt);
    EINA_SAFETY_ON_NULL_RETURN(area);

    WebCore::IntRect rect(area->x, area->y, area->w, area->h);

    if (ctxt->view->isTransparent())
        ctxt->gc->clearRect(rect);
    ctxt->view->paint(ctxt->gc, rect);
}

void ewk_view_paint_context_paint_contents(Ewk_View_Paint_Context* ctxt, const Eina_Rectangle* area)
{
    EINA_SAFETY_ON_NULL_RETURN(ctxt);
    EINA_SAFETY_ON_NULL_RETURN(area);

    WebCore::IntRect rect(area->x, area->y, area->w, area->h);

    if (ctxt->view->isTransparent())
        ctxt->gc->clearRect(rect);

    ctxt->view->paintContents(ctxt->gc, rect);
}

void ewk_view_paint_context_scale(Ewk_View_Paint_Context* ctxt, float scale_x, float scale_y)
{
    EINA_SAFETY_ON_NULL_RETURN(ctxt);

    ctxt->gc->scale(WebCore::FloatSize(scale_x, scale_y));
}

void ewk_view_paint_context_translate(Ewk_View_Paint_Context* ctxt, float x, float y)
{
    EINA_SAFETY_ON_NULL_RETURN(ctxt);

    ctxt->gc->translate(x, y);
}

Eina_Bool ewk_view_paint(Ewk_View_Private_Data* priv, cairo_t* cr, const Eina_Rectangle* area)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(cr, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(area, EINA_FALSE);
    WebCore::FrameView* view = priv->main_frame->view();
    EINA_SAFETY_ON_NULL_RETURN_VAL(view, EINA_FALSE);

    if (view->needsLayout())
        view->forceLayout();
    WebCore::GraphicsContext gc(cr);
    WebCore::IntRect rect(area->x, area->y, area->w, area->h);

    cairo_save(cr);
    gc.save();
    gc.clip(rect);
    if (view->isTransparent())
        gc.clearRect(rect);
    view->paint(&gc, rect);
    gc.restore();
    cairo_restore(cr);

    return EINA_TRUE;
}

Eina_Bool ewk_view_paint_contents(Ewk_View_Private_Data* priv, cairo_t* cr, const Eina_Rectangle* area)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(cr, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(area, EINA_FALSE);
    WebCore::FrameView* view = priv->main_frame->view();
    EINA_SAFETY_ON_NULL_RETURN_VAL(view, EINA_FALSE);

    WebCore::GraphicsContext gc(cr);
    WebCore::IntRect rect(area->x, area->y, area->w, area->h);

    cairo_save(cr);
    gc.save();
    gc.clip(rect);
    if (view->isTransparent())
        gc.clearRect(rect);
    view->paintContents(&gc,  rect);
    gc.restore();
    cairo_restore(cr);

    return EINA_TRUE;
}


/* internal methods ****************************************************/
/**
 * @internal
 * Reports the view is ready to be displayed as all elements are aready.
 *
 * Emits signal: "ready" with no parameters.
 */
void ewk_view_ready(Evas_Object* o)
{
    DBG("o=%p", o);
    evas_object_smart_callback_call(o, "ready", 0);
}

/**
 * @internal
 * Reports the state of input method changed. This is triggered, for example
 * when a input field received/lost focus
 *
 * Emits signal: "inputmethod,changed" with a boolean indicating whether it's
 * enabled or not.
 */
void ewk_view_input_method_state_set(Evas_Object* o, Eina_Bool active)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EWK_VIEW_PRIV_GET(sd, priv);
    WebCore::Frame* focusedFrame = priv->page->focusController()->focusedOrMainFrame();

    priv->imh = 0;
    if (focusedFrame
        && focusedFrame->document()
        && focusedFrame->document()->focusedNode()
        && focusedFrame->document()->focusedNode()->hasTagName(WebCore::HTMLNames::inputTag)) {
        WebCore::HTMLInputElement* inputElement;

        inputElement = static_cast<WebCore::HTMLInputElement*>(focusedFrame->document()->focusedNode());
        if (inputElement) {
            // for password fields, active == false
            if (!active) {
                active = inputElement->isPasswordField();
                priv->imh = inputElement->isPasswordField() * EWK_IMH_PASSWORD;
            } else {
                // Set input method hints for "number", "tel", "email", and "url" input elements.
                priv->imh |= inputElement->isTelephoneField() * EWK_IMH_TELEPHONE;
                priv->imh |= inputElement->isNumberField() * EWK_IMH_NUMBER;
                priv->imh |= inputElement->isEmailField() * EWK_IMH_EMAIL;
                priv->imh |= inputElement->isURLField() * EWK_IMH_URL;
            }
        }
    }

    evas_object_smart_callback_call(o, "inputmethod,changed", (void*)active);
}

/**
 * @internal
 * The view title was changed by the frame loader.
 *
 * Emits signal: "title,changed" with pointer to new title string.
 */
void ewk_view_title_set(Evas_Object* o, const char* title)
{
    DBG("o=%p, title=%s", o, title ? title : "(null)");
    evas_object_smart_callback_call(o, "title,changed", (void*)title);
}

/**
 * @internal
 * Reports that main frame's uri changed.
 *
 * Emits signal: "uri,changed" with pointer to the new uri string.
 */
void ewk_view_uri_changed(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    const char* uri = ewk_frame_uri_get(sd->main_frame);
    DBG("o=%p, uri=%s", o, uri ? uri : "(null)");
    evas_object_smart_callback_call(o, "uri,changed", (void*)uri);
}

/**
 * @internal
 * Reports that a DOM document object has finished loading for @p frame.
 *
 * @param o View which contains the frame.
 * @param frame The frame whose load has triggered the event.
 *
 * Emits signal: "load,document,finished" with @p frame as the parameter.
 */
void ewk_view_load_document_finished(Evas_Object* o, Evas_Object* frame)
{
    evas_object_smart_callback_call(o, "load,document,finished", frame);
}

/**
 * @internal
 * Reports the view started loading something.
 *
 * @param o View.
 *
 * Emits signal: "load,started" with no parameters.
 */
void ewk_view_load_started(Evas_Object* o)
{
    DBG("o=%p", o);
    evas_object_smart_callback_call(o, "load,started", 0);
}

/**
 * Reports the frame started loading something.
 *
 * @param o View.
 *
 * Emits signal: "load,started" on main frame with no parameters.
 */
void ewk_view_frame_main_load_started(Evas_Object* o)
{
    DBG("o=%p", o);
    Evas_Object* frame = ewk_view_frame_main_get(o);
    evas_object_smart_callback_call(frame, "load,started", 0);
}

/**
 * @internal
 * Reports the main frame started provisional load.
 *
 * @param o View.
 *
 * Emits signal: "load,provisional" on View with no parameters.
 */
void ewk_view_load_provisional(Evas_Object* o)
{
    DBG("o=%p", o);
    evas_object_smart_callback_call(o, "load,provisional", 0);
}

/**
 * @internal
 * Reports view can be shown after a new window is created.
 *
 * @param o Frame.
 *
 * Emits signal: "load,newwindow,show" on view with no parameters.
 */
void ewk_view_load_show(Evas_Object* o)
{
    DBG("o=%p", o);
    evas_object_smart_callback_call(o, "load,newwindow,show", 0);
}


/**
 * @internal
 * Reports the main frame was cleared.
 *
 * @param o View.
 */
void ewk_view_frame_main_cleared(Evas_Object* o)
{
    DBG("o=%p", o);
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->flush);
    sd->api->flush(sd);
}

/**
 * @internal
 * Reports the main frame received an icon.
 *
 * @param o View.
 *
 * Emits signal: "icon,received" with no parameters.
 */
void ewk_view_frame_main_icon_received(Evas_Object* o)
{
    DBG("o=%p", o);
    Evas_Object* frame = ewk_view_frame_main_get(o);
    evas_object_smart_callback_call(frame, "icon,received", 0);
}

/**
 * @internal
 * Reports load finished, optionally with error information.
 *
 * Emits signal: "load,finished" with pointer to #Ewk_Frame_Load_Error
 * if any error, or @c 0 if successful load.
 *
 * @note there should not be any error stuff here, but trying to be
 *       compatible with previous WebKit.
 */
void ewk_view_load_finished(Evas_Object* o, const Ewk_Frame_Load_Error* error)
{
    DBG("o=%p, error=%p", o, error);
    evas_object_smart_callback_call(o, "load,finished", (void*)error);
}

/**
 * @internal
 * Reports load failed with error information.
 *
 * Emits signal: "load,error" with pointer to Ewk_Frame_Load_Error.
 */
void ewk_view_load_error(Evas_Object* o, const Ewk_Frame_Load_Error* error)
{
    DBG("o=%p, error=%p", o, error);
    evas_object_smart_callback_call(o, "load,error", (void*)error);
}

/**
 * @internal
 * Reports load progress changed.
 *
 * Emits signal: "load,progress" with pointer to a double from 0.0 to 1.0.
 */
void ewk_view_load_progress_changed(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv);

    // Evas_Coord w, h;
    double progress = priv->page->progress()->estimatedProgress();

    DBG("o=%p (p=%0.3f)", o, progress);

    evas_object_smart_callback_call(o, "load,progress", &progress);
}

/**
 * @internal
 * Reports view @param o should be restored to default conditions
 *
 * @param o View.
 * @param frame Frame that originated restore.
 *
 * Emits signal: "restore" with frame.
 */
void ewk_view_restore_state(Evas_Object* o, Evas_Object* frame)
{
    evas_object_smart_callback_call(o, "restore", frame);
}

/**
 * @internal
 * Delegates to browser the creation of a new window. If it is not implemented,
 * current view is returned, so navigation might continue in same window. If
 * browser supports the creation of new windows, a new Ewk_Window_Features is
 * created and passed to browser. If it intends to keep the request for opening
 * the window later it must increments the Ewk_Winwdow_Features ref count by
 * calling ewk_window_features_ref(window_features). Otherwise this struct will
 * be freed after returning to this function.
 *
 * @param o Current view.
 * @param javascript @c EINA_TRUE if the new window is originated from javascript,
 * @c EINA_FALSE otherwise
 * @param window_features Features of the new window being created. If it's @c
 * NULL, it will be created a window with default features.
 *
 * @return New view, in case smart class implements the creation of new windows;
 * else, current view @param o or @c 0 on failure.
 *
 * @see ewk_window_features_ref().
 */
Evas_Object* ewk_view_window_create(Evas_Object* o, Eina_Bool javascript, const WebCore::WindowFeatures* coreFeatures)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);

    if (!sd->api->window_create)
        return o;

    Ewk_Window_Features* window_features = ewk_window_features_new_from_core(coreFeatures);
    if (!window_features)
        return 0;

    Evas_Object* view = sd->api->window_create(sd, javascript, window_features);
    ewk_window_features_unref(window_features);

    return view;
}

/**
 * @internal
 * Reports a window should be closed. It's client responsibility to decide if
 * the window should in fact be closed. So, if only windows created by javascript
 * are allowed to be closed by this call, browser needs to save the javascript
 * flag when the window is created. Since a window can close itself (for example
 * with a 'self.close()' in Javascript) browser must postpone the deletion to an
 * idler.
 *
 * @param o View to be closed.
 */
void ewk_view_window_close(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);

    ewk_view_stop(o);
    if (!sd->api->window_close)
        return;
    sd->api->window_close(sd);
}

/**
 * @internal
 * Reports mouse has moved over a link.
 *
 * Emits signal: "link,hover,in"
 */
void ewk_view_mouse_link_hover_in(Evas_Object* o, void* data)
{
    evas_object_smart_callback_call(o, "link,hover,in", data);
}

/**
 * @internal
 * Reports mouse is not over a link anymore.
 *
 * Emits signal: "link,hover,out"
 */
void ewk_view_mouse_link_hover_out(Evas_Object* o)
{
    evas_object_smart_callback_call(o, "link,hover,out", 0);
}

/**
 * @internal
 * Set toolbar visible.
 *
 * Emits signal: "toolbars,visible,set" with a pointer to a boolean.
 */
void ewk_view_toolbars_visible_set(Evas_Object* o, Eina_Bool visible)
{
    DBG("o=%p (visible=%d)", o, !!visible);
    evas_object_smart_callback_call(o, "toolbars,visible,set", &visible);
}

/**
 * @internal
 * Get toolbar visibility.
 *
 * @param o View.
 * @param visible boolean pointer in which to save the result. It defaults
 * to @c FALSE, i.e. if browser does no listen to emitted signal, it means
 * there are no toolbars and therefore they are not visible.
 *
 * Emits signal: "toolbars,visible,get" with a pointer to a boolean.
 */
void ewk_view_toolbars_visible_get(Evas_Object* o, Eina_Bool* visible)
{
    DBG("%s, o=%p", __func__, o);
    *visible = EINA_FALSE;
    evas_object_smart_callback_call(o, "toolbars,visible,get", visible);
}

/**
 * @internal
 * Set statusbar visible.
 *
 * @param o View.
 * @param visible @c TRUE if statusbar are visible, @c FALSE otherwise.
 *
 * Emits signal: "statusbar,visible,set" with a pointer to a boolean.
 */
void ewk_view_statusbar_visible_set(Evas_Object* o, Eina_Bool visible)
{
    DBG("o=%p (visible=%d)", o, !!visible);
    evas_object_smart_callback_call(o, "statusbar,visible,set", &visible);
}

/**
 * @internal
 * Get statusbar visibility.
 *
 * @param o View.
 * @param visible boolean pointer in which to save the result. It defaults
 * to @c FALSE, i.e. if browser does no listen to emitted signal, it means
 * there is no statusbar and therefore it is not visible.
 *
 * Emits signal: "statusbar,visible,get" with a pointer to a boolean.
 */
void ewk_view_statusbar_visible_get(Evas_Object* o, Eina_Bool* visible)
{
    DBG("%s, o=%p", __func__, o);
    *visible = EINA_FALSE;
    evas_object_smart_callback_call(o, "statusbar,visible,get", visible);
}

/**
 * @internal
 * Set text of statusbar
 *
 * @param o View.
 * @param text New text to put on statusbar.
 *
 * Emits signal: "statusbar,text,set" with a string.
 */
void ewk_view_statusbar_text_set(Evas_Object* o, const char* text)
{
    DBG("o=%p (text=%s)", o, text);
    INF("status bar text set: %s", text);
    evas_object_smart_callback_call(o, "statusbar,text,set", (void*)text);
}

/**
 * @internal
 * Set scrollbars visible.
 *
 * @param o View.
 * @param visible @c TRUE if scrollbars are visible, @c FALSE otherwise.
 *
 * Emits signal: "scrollbars,visible,set" with a pointer to a boolean.
 */
void ewk_view_scrollbars_visible_set(Evas_Object* o, Eina_Bool visible)
{
    DBG("o=%p (visible=%d)", o, !!visible);
    evas_object_smart_callback_call(o, "scrollbars,visible,set", &visible);
}

/**
 * @internal
 * Get scrollbars visibility.
 *
 * @param o View.
 * @param visible boolean pointer in which to save the result. It defaults
 * to @c FALSE, i.e. if browser does no listen to emitted signal, it means
 * there are no scrollbars and therefore they are not visible.
 *
 * Emits signal: "scrollbars,visible,get" with a pointer to a boolean.
 */
void ewk_view_scrollbars_visible_get(Evas_Object* o, Eina_Bool* visible)
{
    DBG("%s, o=%p", __func__, o);
    *visible = EINA_FALSE;
    evas_object_smart_callback_call(o, "scrollbars,visible,get", visible);
}

/**
 * @internal
 * Set menubar visible.
 *
 * @param o View.
 * @param visible @c TRUE if menubar is visible, @c FALSE otherwise.
 *
 * Emits signal: "menubar,visible,set" with a pointer to a boolean.
 */
void ewk_view_menubar_visible_set(Evas_Object* o, Eina_Bool visible)
{
    DBG("o=%p (visible=%d)", o, !!visible);
    evas_object_smart_callback_call(o, "menubar,visible,set", &visible);
}

/**
 * @internal
 * Get menubar visibility.
 *
 * @param o View.
 * @param visible boolean pointer in which to save the result. It defaults
 * to @c FALSE, i.e. if browser does no listen to emitted signal, it means
 * there is no menubar and therefore it is not visible.
 *
 * Emits signal: "menubar,visible,get" with a pointer to a boolean.
 */
void ewk_view_menubar_visible_get(Evas_Object* o, Eina_Bool* visible)
{
    DBG("%s, o=%p", __func__, o);
    *visible = EINA_FALSE;
    evas_object_smart_callback_call(o, "menubar,visible,get", visible);
}

/**
 * @internal
 * Set tooltip text and display if it is currently hidden.
 *
 * @param o View.
 * @param text Text to set tooltip to.
 *
 * Emits signal: "tooltip,text,set" with a string. If tooltip must be actually
 * removed, text will be 0 or '\0'
 */
void ewk_view_tooltip_text_set(Evas_Object* o, const char* text)
{
    DBG("o=%p text=%s", o, text);
    evas_object_smart_callback_call(o, "tooltip,text,set", (void*)text);
}

/**
 * @internal
 *
 * @param o View.
 * @param message String to show on console.
 * @param lineNumber Line number.
 * @sourceID Source id.
 *
 */
void ewk_view_add_console_message(Evas_Object* o, const char* message, unsigned int lineNumber, const char* sourceID)
{
    DBG("o=%p message=%s lineNumber=%u sourceID=%s", o, message, lineNumber, sourceID);
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->add_console_message);
    sd->api->add_console_message(sd, message, lineNumber, sourceID);
}

bool ewk_view_focus_can_cycle(Evas_Object* o, Ewk_Focus_Direction direction)
{
    DBG("o=%p direction=%d", o, direction);
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api, false);

    if (!sd->api->focus_can_cycle)
        return false;

    return sd->api->focus_can_cycle(sd, direction);
}

void ewk_view_run_javascript_alert(Evas_Object* o, Evas_Object* frame, const char* message)
{
    DBG("o=%p frame=%p message=%s", o, frame, message);
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);

    if (!sd->api->run_javascript_alert)
        return;

    sd->api->run_javascript_alert(sd, frame, message);
}

Eina_Bool ewk_view_run_javascript_confirm(Evas_Object* o, Evas_Object* frame, const char* message)
{
    DBG("o=%p frame=%p message=%s", o, frame, message);
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api, EINA_FALSE);

    if (!sd->api->run_javascript_confirm)
        return EINA_FALSE;

    return sd->api->run_javascript_confirm(sd, frame, message);
}

Eina_Bool ewk_view_run_javascript_prompt(Evas_Object* o, Evas_Object* frame, const char* message, const char* defaultValue, char** value)
{
    DBG("o=%p frame=%p message=%s", o, frame, message);
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api, EINA_FALSE);

    if (!sd->api->run_javascript_prompt)
        return EINA_FALSE;

    return sd->api->run_javascript_prompt(sd, frame, message, defaultValue, value);
}

/**
 * @internal
 * Delegates to client to decide whether a script must be stopped because it's
 * running for too long. If client does not implement it, it goes to default
 * implementation, which logs and returns EINA_FALSE. Client may remove log by
 * setting this function 0, which will just return EINA_FALSE.
 *
 * @param o View.
 *
 * @return @c EINA_TRUE if script should be stopped; @c EINA_FALSE otherwise
 */
Eina_Bool ewk_view_should_interrupt_javascript(Evas_Object* o)
{
    DBG("o=%p", o);
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api, EINA_FALSE);

    if (!sd->api->should_interrupt_javascript)
        return EINA_FALSE;

    return sd->api->should_interrupt_javascript(sd);
}

/**
 * @internal
 * This is called whenever the web site shown in @param frame is asking to store data
 * to the database @param databaseName and the quota allocated to that web site
 * is exceeded. Browser may use this to increase the size of quota before the
 * originating operationa fails.
 *
 * @param o View.
 * @param frame The frame whose web page exceeded its database quota.
 * @param databaseName Database name.
 * @param current_size Current size of this database
 * @param expected_size The expected size of this database in order to fulfill
 * site's requirement.
 */
uint64_t ewk_view_exceeded_database_quota(Evas_Object* o, Evas_Object* frame, const char* databaseName, uint64_t current_size, uint64_t expected_size)
{
    DBG("o=%p", o);
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api, 0);
    if (!sd->api->exceeded_database_quota)
        return 0;

    INF("current_size=%" PRIu64 " expected_size=%" PRIu64, current_size, expected_size);
    return sd->api->exceeded_database_quota(sd, frame, databaseName, current_size, expected_size);
}

/**
 * @internal
 * Open panel to choose a file.
 *
 * @param o View.
 * @param frame Frame in which operation is required.
 * @param allows_multiple_files @c EINA_TRUE when more than one file may be
 * selected, @c EINA_FALSE otherwise
 * @param accept_types accept mime types
 * @selected_filenames List of files selected.
 *
 * @return @EINA_FALSE if user canceled file selection; @EINA_TRUE if confirmed.
 */
Eina_Bool ewk_view_run_open_panel(Evas_Object* o, Evas_Object* frame, Eina_Bool allows_multiple_files, const char* accept_types, Eina_List** selected_filenames)
{
    DBG("o=%p frame=%p allows_multiple_files=%d", o, frame, allows_multiple_files);
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api, EINA_FALSE);
    Eina_Bool confirm;

    if (!sd->api->run_open_panel)
        return EINA_FALSE;

    *selected_filenames = 0;

    confirm = sd->api->run_open_panel(sd, frame, allows_multiple_files, accept_types, selected_filenames);
    if (!confirm && *selected_filenames)
        ERR("Canceled file selection, but selected filenames != 0. Free names before return.");
    return confirm;
}

void ewk_view_repaint(Evas_Object* o, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
    DBG("o=%p, region=%d,%d + %dx%d", o, x, y, w, h);
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv);

    if (!priv->main_frame->contentRenderer()) {
        ERR("no main frame content renderer.");
        return;
    }

    _ewk_view_repaint_add(priv, x, y, w, h);
    _ewk_view_smart_changed(sd);
}

void ewk_view_scroll(Evas_Object* o, Evas_Coord dx, Evas_Coord dy, Evas_Coord sx, Evas_Coord sy, Evas_Coord sw, Evas_Coord sh, Evas_Coord cx, Evas_Coord cy, Evas_Coord cw, Evas_Coord ch, Eina_Bool main_frame)
{
    DBG("o=%p, delta: %d,%d, scroll: %d,%d+%dx%d, clip: %d,%d+%dx%d",
        o, dx, dy, sx, sy, sw, sh, cx, cy, cw, ch);

    if ((sx != cx) || (sy != cy) || (sw != cw) || (sh != ch))
        WRN("scroll region and clip are different! %d,%d+%dx%d and %d,%d+%dx%d",
            sx, sy, sw, sh, cx, cy, cw, ch);

    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv);
    EINA_SAFETY_ON_TRUE_RETURN(!dx && !dy);

    _ewk_view_scroll_add(priv, dx, dy, sx, sy, sw, sh, main_frame);

    _ewk_view_smart_changed(sd);
}

WebCore::Page* ewk_view_core_page_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);
    return priv->page;
}

/**
 * Creates a new frame for given url and owner element.
 *
 * Emits "frame,created" with the new frame object on success.
 */
WTF::PassRefPtr<WebCore::Frame> ewk_view_frame_create(Evas_Object* o, Evas_Object* frame, const WTF::String& name, WebCore::HTMLFrameOwnerElement* ownerElement, const WebCore::KURL& url, const WTF::String& referrer)
{
    DBG("o=%p, frame=%p, name=%s, ownerElement=%p, url=%s, referrer=%s",
        o, frame, name.utf8().data(), ownerElement,
        url.string().utf8().data(), referrer.utf8().data());

    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, 0);

    WTF::RefPtr<WebCore::Frame> cf = _ewk_view_core_frame_new
                                         (sd, priv, ownerElement);
    if (!cf) {
        ERR("Could not create child core frame '%s'", name.utf8().data());
        return 0;
    }

    if (!ewk_frame_child_add(frame, cf, name, url, referrer)) {
        ERR("Could not create child frame object '%s'", name.utf8().data());
        return 0;
    }

    // The creation of the frame may have removed itself already.
    if (!cf->page() || !cf->tree() || !cf->tree()->parent())
        return 0;

    sd->changed.frame_rect = EINA_TRUE;
    _ewk_view_smart_changed(sd);

    evas_object_smart_callback_call(o, "frame,created", frame);
    return cf.release();
}

WTF::PassRefPtr<WebCore::Widget> ewk_view_plugin_create(Evas_Object* o, Evas_Object* frame, const WebCore::IntSize& pluginSize, WebCore::HTMLPlugInElement* element, const WebCore::KURL& url, const WTF::Vector<WTF::String>& paramNames, const WTF::Vector<WTF::String>& paramValues, const WTF::String& mimeType, bool loadManually)
{
    DBG("o=%p, frame=%p, size=%dx%d, element=%p, url=%s, mimeType=%s",
        o, frame, pluginSize.width(), pluginSize.height(), element,
        url.string().utf8().data(), mimeType.utf8().data());

    EWK_VIEW_SD_GET_OR_RETURN(o, sd, 0);
    sd->changed.frame_rect = EINA_TRUE;
    _ewk_view_smart_changed(sd);

    return ewk_frame_plugin_create
               (frame, pluginSize, element, url, paramNames, paramValues,
               mimeType, loadManually);
}

/**
 * @internal
 *
 * Creates a new popup with options when a select widget was clicked.
 *
 * @param client PopupMenuClient instance that allows communication with webkit.
 * @param selected Selected item.
 * @param rect Menu's position.
 *
 * Emits: "popup,create" with a list of Ewk_Menu containing each item's data
 */
void ewk_view_popup_new(Evas_Object* o, WebCore::PopupMenuClient* client, int selected, const WebCore::IntRect& rect)
{
    INF("o=%p", o);
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv);

    if (priv->popup.menu_client)
        ewk_view_popup_destroy(o);

    priv->popup.menu_client = client;

    // populate items
    const int size = client->listSize();
    for (int i = 0; i < size; ++i) {
        Ewk_Menu_Item* item = static_cast<Ewk_Menu_Item*>(malloc(sizeof(*item)));
        if (client->itemIsSeparator(i))
            item->type = EWK_MENU_SEPARATOR;
        else if (client->itemIsLabel(i))
            item->type = EWK_MENU_GROUP;
        else
            item->type = EWK_MENU_OPTION;
        item->text = eina_stringshare_add(client->itemText(i).utf8().data());

        priv->popup.menu.items = eina_list_append(priv->popup.menu.items, item);
    }

    priv->popup.menu.x = rect.x();
    priv->popup.menu.y = rect.y();
    priv->popup.menu.width = rect.width();
    priv->popup.menu.height = rect.height();
    evas_object_smart_callback_call(o, "popup,create", &priv->popup.menu);
}

Eina_Bool ewk_view_popup_destroy(Evas_Object* o)
{
    INF("o=%p", o);
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);

    if (!priv->popup.menu_client)
        return EINA_FALSE;

    evas_object_smart_callback_call(o, "popup,willdelete", &priv->popup.menu);

    void* itemv;
    EINA_LIST_FREE(priv->popup.menu.items, itemv) {
        Ewk_Menu_Item* item = static_cast<Ewk_Menu_Item*>(itemv);
        eina_stringshare_del(item->text);
        free(item);
    }
    priv->popup.menu_client->popupDidHide();
    priv->popup.menu_client = 0;

    return EINA_TRUE;
}

void ewk_view_popup_selected_set(Evas_Object* o, int index)
{
    INF("o=%p", o);
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv);
    EINA_SAFETY_ON_NULL_RETURN(priv->popup.menu_client);

    priv->popup.menu_client->valueChanged(index);
}

/**
 * @internal
 * Request a download to user.
 *
 * @param o View.
 * @oaram download Ewk_Download struct to be sent.
 *
 * Emits: "download,request" with an Ewk_Download containing the details of the
 * requested download. The download per se must be handled outside of webkit.
 */
void ewk_view_download_request(Evas_Object* o, Ewk_Download* download)
{
    DBG("view=%p", o);
    evas_object_smart_callback_call(o, "download,request", download);
}

/**
 * @internal
 * Reports the JS window object was cleared.
 *
 * @param o view.
 * @param frame the frame.
 */
void ewk_view_js_window_object_clear(Evas_Object* o, Evas_Object* frame)
{
    evas_object_smart_callback_call(o, "js,windowobject,clear", frame);
}

/**
 * @internal
 * Reports the viewport has changed.
 *
 * @param arguments viewport argument.
 *
 * Emits signal: "viewport,changed" with no parameters.
 */
void ewk_view_viewport_attributes_set(Evas_Object* o, const WebCore::ViewportArguments& arguments)
{
    EWK_VIEW_SD_GET(o, sd);
    EWK_VIEW_PRIV_GET(sd, priv);

    priv->viewport_arguments = arguments;
    evas_object_smart_callback_call(o, "viewport,changed", 0);
}

void ewk_view_viewport_attributes_get(const Evas_Object* o, int* w, int* h, float* init_scale, float* max_scale, float* min_scale, float* device_pixel_ratio, Eina_Bool* user_scalable)
{
    WebCore::ViewportAttributes attributes = _ewk_view_viewport_attributes_compute(o);

    if (w)
        *w = attributes.layoutSize.width();
    if (h)
        *h = attributes.layoutSize.height();
    if (init_scale)
        *init_scale = attributes.initialScale;
    if (max_scale)
        *max_scale = attributes.maximumScale;
    if (min_scale)
        *min_scale = attributes.minimumScale;
    if (device_pixel_ratio)
        *device_pixel_ratio = attributes.devicePixelRatio;
    if (user_scalable)
        *user_scalable = static_cast<bool>(attributes.userScalable);
}

Eina_Bool ewk_view_zoom_range_set(Evas_Object* o, float min_scale, float max_scale)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);

    if (max_scale < min_scale) {
        WRN("min_scale is larger than max_scale");
        return EINA_FALSE;
    }

    priv->settings.zoom_range.min_scale = min_scale;
    priv->settings.zoom_range.max_scale = max_scale;

    return EINA_TRUE;
}

float ewk_view_zoom_range_min_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, -1.0);

    return priv->settings.zoom_range.min_scale;
}

float ewk_view_zoom_range_max_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, -1.0);

    return priv->settings.zoom_range.max_scale;
}

Eina_Bool ewk_view_user_scalable_set(Evas_Object* o, Eina_Bool user_scalable)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);

    priv->settings.zoom_range.user_scalable = user_scalable;

    return EINA_TRUE;
}

Eina_Bool ewk_view_user_scalable_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);

    return priv->settings.zoom_range.user_scalable;
}

float ewk_view_device_pixel_ratio_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, -1.0);

    return priv->settings.device_pixel_ratio;
}

void ewk_view_did_first_visually_nonempty_layout(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv);
    if (!priv->flags.view_cleared) {
        ewk_view_frame_main_cleared(o);
        ewk_view_enable_render(o);
        priv->flags.view_cleared = EINA_TRUE;
    }
}

/**
 * @internal
 * Dispatch finished loading.
 *
 * @param o view.
 */
void ewk_view_dispatch_did_finish_loading(Evas_Object* o)
{
    /* If we reach this point and rendering is still disabled, WebCore will not
     * trigger the didFirstVisuallyNonEmptyLayout signal anymore. So, we
     * forcefully re-enable the rendering.
     */
    ewk_view_did_first_visually_nonempty_layout(o);
}

void ewk_view_transition_to_commited_for_newpage(Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv);

    ewk_view_disable_render(o);
    priv->flags.view_cleared = EINA_FALSE;
}


/**
 * @internal
 * Reports a requeset will be loaded. It's client responsibility to decide if
 * request would be used. If @return is true, loader will try to load. Else,
 * Loader ignore action of request.
 *
 * @param o View to load
 * @param request Request which contain url to navigate
 */
Eina_Bool ewk_view_navigation_policy_decision(Evas_Object* o, Ewk_Frame_Resource_Request* request)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_TRUE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->api, EINA_TRUE);

    if (!sd->api->navigation_policy_decision)
        return EINA_TRUE;

    return sd->api->navigation_policy_decision(sd, request);
}

Eina_Bool ewk_view_js_object_add(Evas_Object* o, Ewk_JS_Object* obj, const char* obj_name)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    if (obj->view) // object has already been added to another ewk_view
        return EINA_FALSE;
    obj->name = eina_stringshare_add(obj_name);
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);

    JSC::JSLock lock(JSC::SilenceAssertionsOnly);
    WebCore::JSDOMWindow* window = toJSDOMWindow(priv->main_frame, WebCore::mainThreadNormalWorld());
    JSC::Bindings::RootObject* root;
    root = priv->main_frame->script()->bindingRootObject();

    if (!window) {
        ERR("Warning: couldn't get window object");
        return EINA_FALSE;
    }

    JSC::ExecState* exec = window->globalExec();

    obj->view = o;
    JSC::JSObject* runtimeObject = (JSC::JSObject*)JSC::Bindings::CInstance::create((NPObject*)obj, root)->createRuntimeObject(exec);
    JSC::Identifier id = JSC::Identifier(exec, obj_name);

    JSC::PutPropertySlot slot;
    window->put(exec, id, runtimeObject, slot);
    return EINA_TRUE;
#else
    return EINA_FALSE;
#endif // ENABLE(NETSCAPE_PLUGIN_API)
}

Eina_Bool ewk_view_protocol_handler_set(Evas_Object* o, const char** protocols, Ewk_View_Resource_Handler_Cb handler, void* context)
{
    EWK_VIEW_SD_GET(o, sd);
    EWK_VIEW_PRIV_GET(sd, priv);

    if (!handler)
        return EINA_FALSE;

    priv->custom_resource_handler.function = handler;
    priv->custom_resource_handler.context = context;

    return ewk_custom_protocol_handler_set(protocols);
}

Eina_Bool ewk_view_protocol_handler_unset(Evas_Object* o)
{
    EWK_VIEW_SD_GET(o, sd);
    EWK_VIEW_PRIV_GET(sd, priv);
    Eina_Bool ret = ewk_custom_protocol_handler_all_unset();

    if (ret) {
        priv->custom_resource_handler.function = 0;
        priv->custom_resource_handler.context = 0;
    }

    return ret;
}

void* ewk_view_protocol_handler_resource_get(Evas_Object* o, size_t* bytesRead, char** mime, const char* file)
{
    EWK_VIEW_SD_GET(o, sd);
    EWK_VIEW_PRIV_GET(sd, priv);

    Ewk_View_Resource_Handler_Cb function = priv->custom_resource_handler.function;
    if (function)
        return function(file, bytesRead, mime, priv->custom_resource_handler.context);

    return 0;
}

/**
 * @internal
 * Reports that the contents have resized. The ewk_view calls contents_resize,
 * which can be reimplemented as needed.
 *
 * @param o view.
 * @param w new content width.
 * @param h new content height.
 */
void ewk_view_contents_size_changed(Evas_Object* o, int w, int h)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->contents_resize);

    if (!sd->api->contents_resize(sd, w, h))
        ERR("failed to resize contents to %dx%d", w, h);
}

/**
 * @internal
 * Gets page size from frameview.
 *
 * @param o view.
 *
 * @return page size.
 */
WebCore::FloatRect ewk_view_page_rect_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET(o, sd);
    EWK_VIEW_PRIV_GET(sd, priv);

    WebCore::Frame* main_frame = priv->page->mainFrame();
    return main_frame->view()->frameRect();
}

#if ENABLE(TOUCH_EVENTS)
void ewk_view_need_touch_events_set(Evas_Object* o, bool needed)
{
    EWK_VIEW_SD_GET(o, sd);
    EWK_VIEW_PRIV_GET(sd, priv);

    priv->flags.need_touch_events = needed;
}

Eina_Bool ewk_view_need_touch_events_get(const Evas_Object* o)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);
    return priv->flags.need_touch_events;
}
#endif

Eina_Bool ewk_view_mode_set(Evas_Object* o, Ewk_View_Mode view_mode)
{
    EWK_VIEW_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(sd, priv, EINA_FALSE);

    switch (view_mode) {
    case EWK_VIEW_MODE_WINDOWED:
        priv->page->setViewMode(WebCore::Page::ViewModeWindowed);
        break;
    case EWK_VIEW_MODE_FLOATING:
        priv->page->setViewMode(WebCore::Page::ViewModeFloating);
        break;
    case EWK_VIEW_MODE_FULLSCREEN:
        priv->page->setViewMode(WebCore::Page::ViewModeFullscreen);
        break;
    case EWK_VIEW_MODE_MAXIMIZED:
        priv->page->setViewMode(WebCore::Page::ViewModeMaximized);
        break;
    case EWK_VIEW_MODE_MINIMIZED:
        priv->page->setViewMode(WebCore::Page::ViewModeMinimized);
        break;
    default:
        return EINA_FALSE;
    }

    return EINA_TRUE;
}

Ewk_View_Mode ewk_view_mode_get(const Evas_Object* o)
{
    Ewk_View_Mode mode = EWK_VIEW_MODE_WINDOWED;

    EWK_VIEW_SD_GET(o, sd);
    if (!sd)
        return mode;
    EWK_VIEW_PRIV_GET(sd, priv);
    if (!priv)
        return mode;

    switch (priv->page->viewMode()) {
    case WebCore::Page::ViewModeFloating:
        mode = EWK_VIEW_MODE_FLOATING;
        break;
    case WebCore::Page::ViewModeFullscreen:
        mode = EWK_VIEW_MODE_FULLSCREEN;
        break;
    case WebCore::Page::ViewModeMaximized:
        mode = EWK_VIEW_MODE_MAXIMIZED;
        break;
    case WebCore::Page::ViewModeMinimized:
        mode = EWK_VIEW_MODE_MINIMIZED;
        break;
    default:
        break;
    }

    return mode;
}

/**
 * @internal
 * Reports the view that editor client selection has changed.
 *
 * @param o View.
 *
 * Emits signal: "editorclientselection,changed" with no parameters.
 */
void ewk_view_editor_client_selection_changed(Evas_Object* o)
{
    evas_object_smart_callback_call(o, "editorclient,selection,changed", 0);
}

/**
 * @internal
 * Reports to the view that editor client's contents were changed.
 *
 * @param o View.
 *
 * Emits signal: "editorclient,contents,changed" with no parameters.
 */
void ewk_view_editor_client_contents_changed(Evas_Object* o)
{
    evas_object_smart_callback_call(o, "editorclient,contents,changed", 0);
}
