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
#include "LayoutTypes.h"
#include "PlatformMouseEvent.h"
#include "PopupMenuClient.h"
#include "ProgressTracker.h"
#include "RenderTheme.h"
#include "Settings.h"
#include "c_instance.h"
#include "ewk_logging.h"
#include "ewk_private.h"
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

static const Evas_Smart_Cb_Description _ewk_view_callback_names[] = {
    { "download,request", "p" },
    { "editorclient,contents,changed", "" },
    { "editorclient,selection,changed", "" },
    { "frame,created", "p" },
    { "icon,received", "" },
    { "inputmethod,changed", "b" },
    { "js,windowobject,clear", "" },
    { "link,hover,in", "p" },
    { "link,hover,out", "" },
    { "load,document,finished", "p" },
    { "load,error", "p" },
    { "load,finished", "p" },
    { "load,newwindow,show", "" },
    { "load,progress", "d" },
    { "load,provisional", "" },
    { "load,started", "" },
    { "menubar,visible,get", "b" },
    { "menubar,visible,set", "b" },
    { "popup,created", "p" },
    { "popup,willdelete", "p" },
    { "ready", "" },
    { "scrollbars,visible,get", "b" },
    { "scrollbars,visible,set", "b" },
    { "statusbar,text,set", "s" },
    { "statusbar,visible,get", "b" },
    { "statusbar,visible,set", "b" },
    { "title,changed", "s" },
    { "toolbars,visible,get", "b" },
    { "toolbars,visible,set", "b" },
    { "tooltip,text,set", "s" },
    { "uri,changed", "s" },
    { "view,resized", "" },
    { "zoom,animated,end", "" },
    { 0, 0 }
};

/**
 * @brief Private data that is used internally by EFL WebKit
 * and should never be modified from outside.
 *
 * @internal
 */
struct _Ewk_View_Private_Data {
    WebCore::Page* page;
    WebCore::Settings* pageSettings;
    WebCore::Frame* mainFrame;
    WebCore::ViewportArguments viewportArguments;
    Ewk_History* history;
    struct {
        Ewk_Menu menu;
        WebCore::PopupMenuClient* menuClient;
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
        bool viewCleared : 1;
        bool needTouchEvents : 1;
    } flags;
    struct {
        const char* userAgent;
        const char* userStylesheet;
        const char* encodingDefault;
        const char* encodingCustom;
        const char* theme;
        const char* localStorageDatabasePath;
        int fontMinimumSize;
        int fontMinimumLogicalSize;
        int fontDefaultSize;
        int fontMonospaceSize;
        const char* fontStandard;
        const char* fontCursive;
        const char* fontMonospace;
        const char* fontFantasy;
        const char* fontSerif;
        const char* fontSansSerif;
        bool autoLoadImages : 1;
        bool autoShrinkImages : 1;
        bool enableAutoResizeWindow : 1;
        bool enableDeveloperExtras : 1;
        bool enableScripts : 1;
        bool enablePlugins : 1;
        bool enableFrameFlattening : 1;
        bool encodingDetector : 1;
        bool scriptsCanOpenWindows : 1;
        bool scriptsCanCloseWindows : 1;
        bool resizableTextareas : 1;
        bool privateBrowsing : 1;
        bool caretBrowsing : 1;
        bool spatialNavigation : 1;
        bool localStorage : 1;
        bool offlineAppCache : 1;
        bool pageCache : 1;
        struct {
            float minScale;
            float maxScale;
            Eina_Bool userScalable : 1;
        } zoomRange;
        float devicePixelRatio;
        double domTimerInterval;
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
    } animatedZoom;
};

#ifndef EWK_TYPE_CHECK
#define EWK_VIEW_TYPE_CHECK(ewkView, ...) do { } while (0)
#else
#define EWK_VIEW_TYPE_CHECK(ewkView, ...)                                     \
    do {                                                                \
        const char* _tmp_otype = evas_object_type_get(ewkView);               \
        const Evas_Smart* _tmp_s = evas_object_smart_smart_get(ewkView);      \
        if (EINA_UNLIKELY(!_tmp_s)) {                                   \
            EINA_LOG_CRIT                                               \
                ("%p (%s) is not a smart object!", ewkView,                   \
                _tmp_otype ? _tmp_otype : "(null)");                   \
            return __VA_ARGS__;                                         \
        }                                                               \
        const Evas_Smart_Class* _tmp_sc = evas_smart_class_get(_tmp_s); \
        if (EINA_UNLIKELY(!_tmp_sc)) {                                  \
            EINA_LOG_CRIT                                               \
                ("%p (%s) is not a smart object!", ewkView,                   \
                _tmp_otype ? _tmp_otype : "(null)");                   \
            return __VA_ARGS__;                                         \
        }                                                               \
        if (EINA_UNLIKELY(_tmp_sc->data != EWK_VIEW_TYPE_STR)) {        \
            EINA_LOG_CRIT                                               \
                ("%p (%s) is not of an ewk_view (need %p, got %p)!",    \
                ewkView, _tmp_otype ? _tmp_otype : "(null)",                 \
                EWK_VIEW_TYPE_STR, _tmp_sc->data);                     \
            return __VA_ARGS__;                                         \
        }                                                               \
    } while (0)
#endif

#define EWK_VIEW_SD_GET(ewkView, ptr)                                 \
    Ewk_View_Smart_Data* ptr = static_cast<Ewk_View_Smart_Data*>(evas_object_smart_data_get(ewkView))

#define EWK_VIEW_SD_GET_OR_RETURN(ewkView, ptr, ...)          \
    EWK_VIEW_TYPE_CHECK(ewkView, __VA_ARGS__);                \
    EWK_VIEW_SD_GET(ewkView, ptr);                            \
    if (!ptr) {                                         \
        CRITICAL("no smart data for object %p (%s)",    \
                 ewkView, evas_object_type_get(ewkView));           \
        return __VA_ARGS__;                             \
    }

#define EWK_VIEW_PRIV_GET(smartData, ptr)              \
    Ewk_View_Private_Data *ptr = smartData->_priv

#define EWK_VIEW_PRIV_GET_OR_RETURN(smartData, ptr, ...)               \
    EWK_VIEW_PRIV_GET(smartData, ptr);                                 \
    if (!ptr) {                                                 \
        CRITICAL("no private data for object %p (%s)",          \
                 smartData->self, evas_object_type_get(smartData->self));     \
        return __VA_ARGS__;                                     \
    }

static void _ewk_view_smart_changed(Ewk_View_Smart_Data* smartData)
{
    if (smartData->changed.any)
        return;
    smartData->changed.any = true;
    evas_object_smart_changed(smartData->self);
}

static Eina_Bool _ewk_view_repaints_resize(Ewk_View_Private_Data* priv, size_t size)
{
    void* tmp = realloc(priv->repaints.array, size * sizeof(Eina_Rectangle));
    if (!tmp) {
        CRITICAL("could not realloc repaints array to %zu elements.", size);
        return false;
    }
    priv->repaints.allocated = size;
    priv->repaints.array = static_cast<Eina_Rectangle*>(tmp);
    return true;
}

static void _ewk_view_repaint_add(Ewk_View_Private_Data* priv, Evas_Coord x, Evas_Coord y, Evas_Coord width, Evas_Coord height)
{
    Eina_Rectangle* rect;

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

    rect = priv->repaints.array + priv->repaints.count;
    priv->repaints.count++;

    rect->x = x;
    rect->y = y;
    rect->w = width;
    rect->h = height;

    DBG("add repaint %d, %d+%dx%d", x, y, width, height);
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
        return false;
    }
    priv->scrolls.allocated = size;
    priv->scrolls.array = static_cast<Ewk_Scroll_Request*>(tmp);
    return true;
}

static void _ewk_view_scroll_add(Ewk_View_Private_Data* priv, Evas_Coord deltaX, Evas_Coord deltaY, Evas_Coord x, Evas_Coord y, Evas_Coord width, Evas_Coord height, Eina_Bool mainScroll)
{
    Ewk_Scroll_Request* rect;
    Ewk_Scroll_Request* rect_end;
    Evas_Coord x2 = x + width, y2 = y + height;

    rect = priv->scrolls.array;
    rect_end = rect + priv->scrolls.count;
    for (; rect < rect_end; rect++) {
        if (rect->x == x && rect->y == y && rect->w == width && rect->h == height) {
            DBG("region already scrolled %d,%d+%dx%d %+03d,%+03d add "
                "%+03d,%+03d",
                rect->x, rect->y, rect->w, rect->h, rect->dx, rect->dy, deltaX, deltaY);
            rect->dx += deltaX;
            rect->dy += deltaY;
            return;
        }
        if ((x <= rect->x && x2 >= rect->x2) && (y <= rect->y && y2 >= rect->y2)) {
            DBG("old viewport (%d,%d+%dx%d %+03d,%+03d) was scrolled itself, "
                "add %+03d,%+03d",
                rect->x, rect->y, rect->w, rect->h, rect->dx, rect->dy, deltaX, deltaY);
            rect->x += deltaX;
            rect->y += deltaY;
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

    rect = priv->scrolls.array + priv->scrolls.count;
    priv->scrolls.count++;

    rect->x = x;
    rect->y = y;
    rect->w = width;
    rect->h = height;
    rect->x2 = x2;
    rect->y2 = y2;
    rect->dx = deltaX;
    rect->dy = deltaY;
    rect->main_scroll = mainScroll;
    DBG("add scroll in region: %d, %d+%dx%d %+03d, %+03d", x, y, width, height, deltaX, deltaY);

    Eina_Rectangle* pr;
    Eina_Rectangle* pr_end;
    size_t count;
    pr = priv->repaints.array;
    count = priv->repaints.count;
    pr_end = pr + count;
    for (; pr < pr_end; pr++) {
        pr->x += deltaX;
        pr->y += deltaY;
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
static Eina_Bool _ewk_view_smart_focus_in(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_PRIV_GET(smartData, priv);
    WebCore::FocusController* fc = priv->page->focusController();
    DBG("ewkView=%p, fc=%p", smartData->self, fc);
    EINA_SAFETY_ON_NULL_RETURN_VAL(fc, false);

    fc->setActive(true);
    fc->setFocused(true);
    return true;
}

static Eina_Bool _ewk_view_smart_focus_out(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_PRIV_GET(smartData, priv);
    WebCore::FocusController* focusController = priv->page->focusController();
    DBG("ewkView=%p, fc=%p", smartData->self, focusController);
    EINA_SAFETY_ON_NULL_RETURN_VAL(focusController, false);

    focusController->setActive(false);
    focusController->setFocused(false);
    return true;
}

static Eina_Bool _ewk_view_smart_mouse_wheel(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Wheel* wheelEvent)
{
    return ewk_frame_feed_mouse_wheel(smartData->main_frame, wheelEvent);
}

static Eina_Bool _ewk_view_smart_mouse_down(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Down* downEvent)
{
    return ewk_frame_feed_mouse_down(smartData->main_frame, downEvent);
}

static Eina_Bool _ewk_view_smart_mouse_up(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Up* upEvent)
{
    return ewk_frame_feed_mouse_up(smartData->main_frame, upEvent);
}

static Eina_Bool _ewk_view_smart_mouse_move(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Move* moveEvent)
{
    return ewk_frame_feed_mouse_move(smartData->main_frame, moveEvent);
}

static Eina_Bool _ewk_view_smart_key_down(Ewk_View_Smart_Data* smartData, const Evas_Event_Key_Down* downEvent)
{
    Evas_Object* frame = ewk_view_frame_focused_get(smartData->self);

    if (!frame)
        frame = smartData->main_frame;

    return ewk_frame_feed_key_down(frame, downEvent);
}

static Eina_Bool _ewk_view_smart_key_up(Ewk_View_Smart_Data* smartData, const Evas_Event_Key_Up* upEvent)
{
    Evas_Object* frame = ewk_view_frame_focused_get(smartData->self);

    if (!frame)
        frame = smartData->main_frame;

    return ewk_frame_feed_key_up(frame, upEvent);
}

static void _ewk_view_smart_add_console_message(Ewk_View_Smart_Data* smartData, const char* message, unsigned int lineNumber, const char* sourceID)
{
    INF("console message: %s @%d: %s\n", sourceID, lineNumber, message);
}

static void _ewk_view_smart_run_javascript_alert(Ewk_View_Smart_Data* smartData, Evas_Object* frame, const char* message)
{
    INF("javascript alert: %s\n", message);
}

static Eina_Bool _ewk_view_smart_run_javascript_confirm(Ewk_View_Smart_Data* smartData, Evas_Object* frame, const char* message)
{
    INF("javascript confirm: %s", message);
    INF("javascript confirm (HARD CODED)? YES");
    return true;
}

static Eina_Bool _ewk_view_smart_should_interrupt_javascript(Ewk_View_Smart_Data* smartData)
{
    INF("should interrupt javascript?\n"
        "\t(HARD CODED) NO");
    return false;
}

static Eina_Bool _ewk_view_smart_run_javascript_prompt(Ewk_View_Smart_Data* smartData, Evas_Object* frame, const char* message, const char* defaultValue, char** value)
{
    *value = strdup("test");
    Eina_Bool result = true;
    INF("javascript prompt:\n"
        "\t      message: %s\n"
        "\tdefault value: %s\n"
        "\tgiving answer: %s\n"
        "\t       button: %s", message, defaultValue, *value, result ? "ok" : "cancel");

    return result;
}

// Event Handling //////////////////////////////////////////////////////
static void _ewk_view_on_focus_in(void* data, Evas* eventType, Evas_Object* callback, void* eventInfo)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->focus_in);
    smartData->api->focus_in(smartData);
}

static void _ewk_view_on_focus_out(void* data, Evas* eventType, Evas_Object* callback, void* eventInfo)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->focus_out);
    smartData->api->focus_out(smartData);
}

static void _ewk_view_on_mouse_wheel(void* data, Evas* eventType, Evas_Object* callback, void* eventInfo)
{
    Evas_Event_Mouse_Wheel* wheelEvent = static_cast<Evas_Event_Mouse_Wheel*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_wheel);
    smartData->api->mouse_wheel(smartData, wheelEvent);
}

static void _ewk_view_on_mouse_down(void* data, Evas* eventType, Evas_Object* callback, void* eventInfo)
{
    Evas_Event_Mouse_Down* downEvent = static_cast<Evas_Event_Mouse_Down*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_down);
    smartData->api->mouse_down(smartData, downEvent);
}

static void _ewk_view_on_mouse_up(void* data, Evas* eventType, Evas_Object* callback, void* eventInfo)
{
    Evas_Event_Mouse_Up* upEvent = static_cast<Evas_Event_Mouse_Up*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_up);
    smartData->api->mouse_up(smartData, upEvent);
}

static void _ewk_view_on_mouse_move(void* data, Evas* eventType, Evas_Object* callback, void* eventInfo)
{
    Evas_Event_Mouse_Move* moveEvent = static_cast<Evas_Event_Mouse_Move*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_move);
    smartData->api->mouse_move(smartData, moveEvent);
}

static void _ewk_view_on_key_down(void* data, Evas* eventType, Evas_Object* callback, void* eventInfo)
{
    Evas_Event_Key_Down* downEvent = static_cast<Evas_Event_Key_Down*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->key_down);
    smartData->api->key_down(smartData, downEvent);
}

static void _ewk_view_on_key_up(void* data, Evas* eventType, Evas_Object* callback, void* eventInfo)
{
    Evas_Event_Key_Up* upEvent = static_cast<Evas_Event_Key_Up*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->key_up);
    smartData->api->key_up(smartData, upEvent);
}

static WTF::PassRefPtr<WebCore::Frame> _ewk_view_core_frame_new(Ewk_View_Smart_Data* smartData, Ewk_View_Private_Data* priv, WebCore::HTMLFrameOwnerElement* owner)
{
    WebCore::FrameLoaderClientEfl* frameLoaderClient = new WebCore::FrameLoaderClientEfl(smartData->self);
    if (!frameLoaderClient) {
        CRITICAL("Could not create frame loader client.");
        return 0;
    }
    frameLoaderClient->setCustomUserAgent(String::fromUTF8(priv->settings.userAgent));

    return WebCore::Frame::create(priv->page, owner, frameLoaderClient);
}

static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;

static Ewk_View_Private_Data* _ewk_view_priv_new(Ewk_View_Smart_Data* smartData)
{
    Ewk_View_Private_Data* priv =
        static_cast<Ewk_View_Private_Data*>(calloc(1, sizeof(Ewk_View_Private_Data)));
    AtomicString string;
    WebCore::KURL url;

    if (!priv) {
        CRITICAL("could not allocate Ewk_View_Private_Data");
        return 0;
    }

    WebCore::Page::PageClients pageClients;
    pageClients.chromeClient = new WebCore::ChromeClientEfl(smartData->self);
    pageClients.editorClient = new WebCore::EditorClientEfl(smartData->self);
    pageClients.dragClient = new WebCore::DragClientEfl;
    pageClients.inspectorClient = new WebCore::InspectorClientEfl;
#if ENABLE(DEVICE_ORIENTATION)
    pageClients.deviceMotionClient = new WebCore::DeviceMotionClientEfl;
    pageClients.deviceOrientationClient = new WebCore::DeviceOrientationClientEfl;
#endif
    priv->page = new WebCore::Page(pageClients);
    if (!priv->page) {
        CRITICAL("Could not create WebKit Page");
        goto error_page;
    }

    priv->pageSettings = priv->page->settings();
    if (!priv->pageSettings) {
        CRITICAL("Could not get page settings.");
        goto error_settings;
    }

    priv->viewportArguments.width = WebCore::ViewportArguments::ValueAuto;
    priv->viewportArguments.height = WebCore::ViewportArguments::ValueAuto;
    priv->viewportArguments.initialScale = WebCore::ViewportArguments::ValueAuto;
    priv->viewportArguments.minimumScale = WebCore::ViewportArguments::ValueAuto;
    priv->viewportArguments.maximumScale = WebCore::ViewportArguments::ValueAuto;
    priv->viewportArguments.targetDensityDpi = WebCore::ViewportArguments::ValueAuto;
    priv->viewportArguments.userScalable = true;

    priv->pageSettings->setLoadsImagesAutomatically(true);
    priv->pageSettings->setDefaultTextEncodingName("iso-8859-1");
    priv->pageSettings->setDefaultFixedFontSize(12);
    priv->pageSettings->setDefaultFontSize(16);
    priv->pageSettings->setSerifFontFamily("serif");
    priv->pageSettings->setFixedFontFamily("monotype");
    priv->pageSettings->setSansSerifFontFamily("sans");
    priv->pageSettings->setStandardFontFamily("sans");
    priv->pageSettings->setScriptEnabled(true);
    priv->pageSettings->setPluginsEnabled(true);
    priv->pageSettings->setLocalStorageEnabled(true);
    priv->pageSettings->setOfflineWebApplicationCacheEnabled(true);
    priv->pageSettings->setUsesPageCache(true);
    priv->pageSettings->setUsesEncodingDetector(false);

    url = priv->pageSettings->userStyleSheetLocation();
    priv->settings.userStylesheet = eina_stringshare_add(url.string().utf8().data());

    priv->settings.encodingDefault = eina_stringshare_add
                                          (priv->pageSettings->defaultTextEncodingName().utf8().data());
    priv->settings.encodingCustom = 0;

    string = priv->pageSettings->localStorageDatabasePath();
    priv->settings.localStorageDatabasePath = eina_stringshare_add(string.string().utf8().data());

    priv->settings.fontMinimumSize = priv->pageSettings->minimumFontSize();
    priv->settings.fontMinimumLogicalSize = priv->pageSettings->minimumLogicalFontSize();
    priv->settings.fontDefaultSize = priv->pageSettings->defaultFontSize();
    priv->settings.fontMonospaceSize = priv->pageSettings->defaultFixedFontSize();

    string = priv->pageSettings->standardFontFamily();
    priv->settings.fontStandard = eina_stringshare_add(string.string().utf8().data());
    string = priv->pageSettings->cursiveFontFamily();
    priv->settings.fontCursive = eina_stringshare_add(string.string().utf8().data());
    string = priv->pageSettings->fixedFontFamily();
    priv->settings.fontMonospace = eina_stringshare_add(string.string().utf8().data());
    string = priv->pageSettings->fantasyFontFamily();
    priv->settings.fontFantasy = eina_stringshare_add(string.string().utf8().data());
    string = priv->pageSettings->serifFontFamily();
    priv->settings.fontSerif = eina_stringshare_add(string.string().utf8().data());
    string = priv->pageSettings->sansSerifFontFamily();
    priv->settings.fontSansSerif = eina_stringshare_add(string.string().utf8().data());

    priv->settings.autoLoadImages = priv->pageSettings->loadsImagesAutomatically();
    priv->settings.autoShrinkImages = priv->pageSettings->shrinksStandaloneImagesToFit();
    priv->settings.enableAutoResizeWindow = true;
    priv->settings.enableDeveloperExtras = priv->pageSettings->developerExtrasEnabled();
    priv->settings.enableScripts = priv->pageSettings->isScriptEnabled();
    priv->settings.enablePlugins = priv->pageSettings->arePluginsEnabled();
    priv->settings.enableFrameFlattening = priv->pageSettings->frameFlatteningEnabled();
    priv->settings.scriptsCanOpenWindows = priv->pageSettings->javaScriptCanOpenWindowsAutomatically();
    priv->settings.scriptsCanCloseWindows = priv->pageSettings->allowScriptsToCloseWindows();
    priv->settings.resizableTextareas = priv->pageSettings->textAreasAreResizable();
    priv->settings.privateBrowsing = priv->pageSettings->privateBrowsingEnabled();
    priv->settings.caretBrowsing = priv->pageSettings->caretBrowsingEnabled();
    priv->settings.spatialNavigation = priv->pageSettings->isSpatialNavigationEnabled();
    priv->settings.localStorage = priv->pageSettings->localStorageEnabled();
    priv->settings.offlineAppCache = true; // XXX no function to read setting; this keeps the original setting
    priv->settings.pageCache = priv->pageSettings->usesPageCache();
    priv->settings.encodingDetector = priv->pageSettings->usesEncodingDetector();

    priv->settings.userAgent = ewk_settings_default_user_agent_get();

    // Since there's no scale separated from zooming in webkit-efl, this functionality of
    // viewport meta tag is implemented using zoom. When scale zoom is supported by webkit-efl,
    // this functionality will be modified by the scale zoom patch.
    priv->settings.zoomRange.minScale = ZOOM_MIN;
    priv->settings.zoomRange.maxScale = ZOOM_MAX;
    priv->settings.zoomRange.userScalable = true;
    priv->settings.devicePixelRatio = DEVICE_PIXEL_RATIO;

    priv->settings.domTimerInterval = priv->pageSettings->defaultMinDOMTimerInterval();

    priv->mainFrame = _ewk_view_core_frame_new(smartData, priv, 0).get();
    if (!priv->mainFrame) {
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

    eina_stringshare_del(priv->settings.userAgent);
    eina_stringshare_del(priv->settings.userStylesheet);
    eina_stringshare_del(priv->settings.encodingDefault);
    eina_stringshare_del(priv->settings.encodingCustom);
    eina_stringshare_del(priv->settings.fontStandard);
    eina_stringshare_del(priv->settings.fontCursive);
    eina_stringshare_del(priv->settings.fontMonospace);
    eina_stringshare_del(priv->settings.fontFantasy);
    eina_stringshare_del(priv->settings.fontSerif);
    eina_stringshare_del(priv->settings.fontSansSerif);
    eina_stringshare_del(priv->settings.localStorageDatabasePath);

    if (priv->animatedZoom.animator)
        ecore_animator_del(priv->animatedZoom.animator);

    ewk_history_free(priv->history);

    delete priv->page;
    free(priv);
}

static void _ewk_view_smart_add(Evas_Object* ewkView)
{
    const Evas_Smart* smart = evas_object_smart_smart_get(ewkView);
    const Evas_Smart_Class* smartClass = evas_smart_class_get(smart);
    const Ewk_View_Smart_Class* api = reinterpret_cast<const Ewk_View_Smart_Class*>(smartClass);
    EINA_SAFETY_ON_NULL_RETURN(api->backing_store_add);
    EWK_VIEW_SD_GET(ewkView, smartData);

    if (!smartData) {
        smartData = static_cast<Ewk_View_Smart_Data*>(calloc(1, sizeof(Ewk_View_Smart_Data)));
        if (!smartData) {
            CRITICAL("could not allocate Ewk_View_Smart_Data");
            return;
        }
        evas_object_smart_data_set(ewkView, smartData);
    }

    smartData->bg_color.r = 255;
    smartData->bg_color.g = 255;
    smartData->bg_color.b = 255;
    smartData->bg_color.a = 255;

    smartData->self = ewkView;
    smartData->_priv = _ewk_view_priv_new(smartData);
    smartData->api = api;

    _parent_sc.add(ewkView);

    if (!smartData->_priv)
        return;

    EWK_VIEW_PRIV_GET(smartData, priv);

    smartData->backing_store = api->backing_store_add(smartData);
    if (!smartData->backing_store) {
        ERR("Could not create backing store object.");
        return;
    }

    evas_object_smart_member_add(smartData->backing_store, ewkView);
    evas_object_show(smartData->backing_store);
    evas_object_pass_events_set(smartData->backing_store, true);

    smartData->events_rect = evas_object_rectangle_add(smartData->base.evas);
    evas_object_color_set(smartData->events_rect, 0, 0, 0, 0);
    evas_object_smart_member_add(smartData->events_rect, ewkView);
    evas_object_show(smartData->events_rect);

    smartData->main_frame = ewk_frame_add(smartData->base.evas);
    if (!smartData->main_frame) {
        ERR("Could not create main frame object.");
        return;
    }

    if (!ewk_frame_init(smartData->main_frame, ewkView, priv->mainFrame)) {
        ERR("Could not initialize main frme object.");
        evas_object_del(smartData->main_frame);
        smartData->main_frame = 0;

        delete priv->mainFrame;
        priv->mainFrame = 0;
        return;
    }

    evas_object_name_set(smartData->main_frame, "EWK_Frame:main");
    evas_object_smart_member_add(smartData->main_frame, ewkView);
    evas_object_show(smartData->main_frame);

#define CONNECT(s, c) evas_object_event_callback_add(ewkView, s, c, smartData)
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

static void _ewk_view_smart_del(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET(ewkView, smartData);
    Ewk_View_Private_Data* priv = smartData ? smartData->_priv : 0;

    ewk_view_stop(ewkView);
    _parent_sc.del(ewkView);
    _ewk_view_priv_del(priv);
}

static void _ewk_view_smart_resize(Evas_Object* ewkView, Evas_Coord w, Evas_Coord h)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);

    // these should be queued and processed in calculate as well!
    evas_object_resize(smartData->backing_store, w, h);

    smartData->changed.size = true;
    _ewk_view_smart_changed(smartData);
}

static void _ewk_view_smart_move(Evas_Object* ewkView, Evas_Coord x, Evas_Coord y)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    smartData->changed.position = true;
    _ewk_view_smart_changed(smartData);
}

static void _ewk_view_smart_calculate(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET(ewkView, smartData);
    EWK_VIEW_PRIV_GET(smartData, priv);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->contents_resize);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->scrolls_process);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->repaints_process);
    Evas_Coord x, y, width, height;

    smartData->changed.any = false;

    if (!smartData->main_frame || !priv->mainFrame)
        return;

    evas_object_geometry_get(ewkView, &x, &y, &width, &height);

    DBG("ewkView=%p geo=[%d, %d + %dx%d], changed: size=%hhu, "
        "scrolls=%zu, repaints=%zu",
        ewkView, x, y, width, height, smartData->changed.size,
        priv->scrolls.count, priv->repaints.count);

    if (smartData->changed.size && ((width != smartData->view.w) || (height != smartData->view.h))) {
        WebCore::FrameView* view = priv->mainFrame->view();
        if (view) {
            view->resize(width, height);
            view->forceLayout();
            view->adjustViewSize();
        }
        evas_object_resize(smartData->main_frame, width, height);
        evas_object_resize(smartData->events_rect, width, height);
        smartData->changed.frame_rect = true;
        smartData->view.w = width;
        smartData->view.h = height;

        _ewk_view_repaint_add(priv, 0, 0, width, height);

        // This callback is a good place e.g. to change fixed layout size (ewk_view_fixed_layout_size_set).
        evas_object_smart_callback_call(ewkView, "view,resized", 0);
    }
    smartData->changed.size = false;

    if (smartData->changed.position && ((x != smartData->view.x) || (y != smartData->view.y))) {
        evas_object_move(smartData->main_frame, x, y);
        evas_object_move(smartData->backing_store, x, y);
        evas_object_move(smartData->events_rect, x, y);
        smartData->changed.frame_rect = true;
        smartData->view.x = x;
        smartData->view.y = y;
    }
    smartData->changed.position = false;

    if (!smartData->api->scrolls_process(smartData))
        ERR("failed to process scrolls.");
    _ewk_view_scrolls_flush(priv);

    if (!smartData->api->repaints_process(smartData))
        ERR("failed to process repaints.");
    _ewk_view_repaints_flush(priv);

    if (smartData->changed.frame_rect) {
        WebCore::FrameView* view = priv->mainFrame->view();
        view->frameRectsChanged(); /* force tree to get position from root */
        smartData->changed.frame_rect = false;
    }
}

static void _ewk_view_smart_show(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);

    if (evas_object_clipees_get(smartData->base.clipper))
        evas_object_show(smartData->base.clipper);
    evas_object_show(smartData->backing_store);
}

static void _ewk_view_smart_hide(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);

    evas_object_hide(smartData->base.clipper);
    evas_object_hide(smartData->backing_store);
}

static Eina_Bool _ewk_view_smart_contents_resize(Ewk_View_Smart_Data* smartData, int width, int height)
{
    return true;
}

static Eina_Bool _ewk_view_smart_zoom_set(Ewk_View_Smart_Data* smartData, float zoom, Evas_Coord centerX, Evas_Coord centerY)
{
    double px, py;
    Evas_Coord x, y, width, height;
    Eina_Bool result;

    ewk_frame_scroll_size_get(smartData->main_frame, &width, &height);
    ewk_frame_scroll_pos_get(smartData->main_frame, &x, &y);

    if (width + smartData->view.w > 0)
        px = static_cast<double>(x + centerX) / (width + smartData->view.w);
    else
        px = 0.0;

    if (height + smartData->view.h > 0)
        py = static_cast<double>(y + centerY) / (height + smartData->view.h);
    else
        py = 0.0;

    result = ewk_frame_page_zoom_set(smartData->main_frame, zoom);

    ewk_frame_scroll_size_get(smartData->main_frame, &width, &height);
    x = (width + smartData->view.w) * px - centerX;
    y = (height + smartData->view.h) * py - centerY;
    ewk_frame_scroll_set(smartData->main_frame, x, y);
    return result;
}

static void _ewk_view_smart_flush(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    _ewk_view_repaints_flush(priv);
    _ewk_view_scrolls_flush(priv);
}

static Eina_Bool _ewk_view_smart_pre_render_region(Ewk_View_Smart_Data* smartData, Evas_Coord x, Evas_Coord y, Evas_Coord width, Evas_Coord height, float zoom)
{
    WRN("not supported by engine. smartDAta=%p area=%d,%d+%dx%d, zoom=%f",
        smartData, x, y, width, height, zoom);
    return false;
}

static Eina_Bool _ewk_view_smart_pre_render_relative_radius(Ewk_View_Smart_Data* smartData, unsigned int number, float zoom)
{
    WRN("not supported by engine. smartData=%p, n=%u zoom=%f",
        smartData, number, zoom);
    return false;
}

static void _ewk_view_smart_pre_render_cancel(Ewk_View_Smart_Data* smartData)
{
    WRN("not supported by engine. smartData=%p", smartData);
}

static void _ewk_view_zoom_animated_mark_stop(Ewk_View_Smart_Data* smartData)
{
    smartData->animated_zoom.zoom.start = 0.0;
    smartData->animated_zoom.zoom.end = 0.0;
    smartData->animated_zoom.zoom.current = 0.0;
}

static void _ewk_view_zoom_animated_finish(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_PRIV_GET(smartData, priv);
    ecore_animator_del(priv->animatedZoom.animator);
    priv->animatedZoom.animator = 0;
    _ewk_view_zoom_animated_mark_stop(smartData);
    evas_object_smart_callback_call(smartData->self, "zoom,animated,end", 0);
}

static float _ewk_view_zoom_animated_current(Ewk_View_Private_Data* priv)
{
    double now = ecore_loop_time_get();
    double delta = now - priv->animatedZoom.time.start;

    if (delta > priv->animatedZoom.time.duration)
        delta = priv->animatedZoom.time.duration;
    if (delta < 0.0) // time went back, clock adjusted?
        delta = 0.0;

    delta /= priv->animatedZoom.time.duration;

    return ((priv->animatedZoom.zoom.range * delta)
            + priv->animatedZoom.zoom.start);
}

static Eina_Bool _ewk_view_zoom_animator_cb(void* data)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    Evas_Coord centerX, centerY;
    EWK_VIEW_PRIV_GET(smartData, priv);
    double now = ecore_loop_time_get();

    centerX = priv->animatedZoom.center.x;
    centerY = priv->animatedZoom.center.y;

    // TODO: progressively center (cx, cy) -> (view.x + view.h/2, view.y + view.h/2)
    if (centerX >= smartData->view.w)
        centerX = smartData->view.w - 1;
    if (centerY >= smartData->view.h)
        centerY = smartData->view.h - 1;

    if ((now >= priv->animatedZoom.time.end)
        || (now < priv->animatedZoom.time.start)) {
        _ewk_view_zoom_animated_finish(smartData);
        ewk_view_zoom_set(smartData->self, priv->animatedZoom.zoom.end, centerX, centerY);
        smartData->api->sc.calculate(smartData->self);
        return false;
    }

    smartData->animated_zoom.zoom.current = _ewk_view_zoom_animated_current(priv);
    smartData->api->zoom_weak_set(smartData, smartData->animated_zoom.zoom.current, centerX, centerY);
    return true;
}

static void _ewk_view_zoom_animation_start(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    if (priv->animatedZoom.animator)
        return;
    priv->animatedZoom.animator = ecore_animator_add
                                       (_ewk_view_zoom_animator_cb, smartData);
}

static WebCore::ViewportAttributes _ewk_view_viewport_attributes_compute(const Ewk_View_Private_Data* priv)
{
    int desktopWidth = 980;
    int deviceDPI = ewk_util_dpi_get();

    WebCore::IntRect availableRect = enclosingIntRect(priv->page->chrome()->client()->pageRect());
    WebCore::IntRect deviceRect = enclosingIntRect(priv->page->chrome()->client()->windowRect());

    WebCore::ViewportAttributes attributes = WebCore::computeViewportAttributes(priv->viewportArguments, desktopWidth, deviceRect.width(), deviceRect.height(), deviceDPI, availableRect.size());
    WebCore::restrictMinimumScaleFactorToViewportSize(attributes, availableRect.size());
    WebCore::restrictScaleFactorToInitialScaleIfNotUserScalable(attributes);

    return attributes;
}

static Eina_Bool _ewk_view_smart_disable_render(Ewk_View_Smart_Data* smartData)
{
    WRN("not supported by engine. smartData=%p", smartData);
    return false;
}

static Eina_Bool _ewk_view_smart_enable_render(Ewk_View_Smart_Data* smartData)
{
    WRN("not supported by engine. smartData=%p", smartData);
    return false;
}

Eina_Bool ewk_view_base_smart_set(Ewk_View_Smart_Class* api)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(api, false);

    if (api->version != EWK_VIEW_SMART_CLASS_VERSION) {
        EINA_LOG_CRIT
            ("Ewk_View_Smart_Class %p is version %lu while %lu was expected.",
            api, api->version, EWK_VIEW_SMART_CLASS_VERSION);
        return false;
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
    api->sc.callbacks = _ewk_view_callback_names;

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

    return true;
}

void ewk_view_fixed_layout_size_set(Evas_Object* ewkView, Evas_Coord width, Evas_Coord height)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    WebCore::FrameView* view = priv->mainFrame->view();
    if (width <= 0 && height <= 0) {
        if (!view->useFixedLayout())
            return;
        view->setUseFixedLayout(false);
    } else {
        WebCore::IntSize size = view->fixedLayoutSize();
        if (size.width() == width && size.height() == height)
            return;
        if (view)
            view->setFixedLayoutSize(WebCore::IntSize(width, height));
    }

    if (!view)
        return;
    view->setUseFixedLayout(true);
    view->forceLayout();
}

void ewk_view_fixed_layout_size_get(const Evas_Object* ewkView, Evas_Coord* width, Evas_Coord* height)
{
    if (width)
        *width = 0;
    if (height)
        *height = 0;
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    WebCore::FrameView* view = priv->mainFrame->view();
    if (view->useFixedLayout()) {
        WebCore::IntSize size = view->fixedLayoutSize();
        if (width)
            *width = size.width();
        if (height)
            *height = size.height();
    }
}

void ewk_view_theme_set(Evas_Object* ewkView, const char* path)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    if (!eina_stringshare_replace(&priv->settings.theme, path))
        return;

    WebCore::FrameView* view = priv->mainFrame->view();
    if (view) {
        view->setEdjeTheme(WTF::String(path));
        priv->page->theme()->themeChanged();
    }

}

const char* ewk_view_theme_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.theme;
}

Evas_Object* ewk_view_frame_main_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    return smartData->main_frame;
}

Evas_Object* ewk_view_frame_focused_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    WebCore::Frame* core = priv->page->focusController()->focusedFrame();
    if (!core)
        return 0;

    WebCore::FrameLoaderClientEfl* client = static_cast<WebCore::FrameLoaderClientEfl*>(core->loader()->client());
    if (!client)
        return 0;
    return client->webFrame();
}

Eina_Bool ewk_view_uri_set(Evas_Object* ewkView, const char* uri)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_uri_set(smartData->main_frame, uri);
}

const char* ewk_view_uri_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    return ewk_frame_uri_get(smartData->main_frame);
}

const char* ewk_view_title_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    return ewk_frame_title_get(smartData->main_frame);
}

Eina_Bool ewk_view_editable_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_editable_get(smartData->main_frame);
}

void ewk_view_bg_color_set(Evas_Object* ewkView, int red, int green, int blue, int alpha)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->bg_color_set);

    if (alpha < 0) {
        WRN("Alpha less than zero (%d).", alpha);
        alpha = 0;
    } else if (alpha > 255) {
        WRN("Alpha is larger than 255 (%d).", alpha);
        alpha = 255;
    }

#define CHECK_PREMUL_COLOR(color, alpha)                                        \
    if (color < 0) {                                                        \
        WRN("Color component " #color " is less than zero (%d).", color);         \
        color = 0;                                                          \
    } else if (color > alpha) {                                                 \
        WRN("Color component " #color " is greater than alpha (%d, alpha=%d).", \
            color, alpha);                                                      \
        color = alpha;                                                          \
    }
    CHECK_PREMUL_COLOR(red, alpha);
    CHECK_PREMUL_COLOR(green, alpha);
    CHECK_PREMUL_COLOR(blue, alpha);
#undef CHECK_PREMUL_COLOR

    smartData->bg_color.r = red;
    smartData->bg_color.g = green;
    smartData->bg_color.b = blue;
    smartData->bg_color.a = alpha;

    smartData->api->bg_color_set(smartData, red, green, blue, alpha);

    WebCore::FrameView* view = smartData->_priv->mainFrame->view();
    if (view) {
        WebCore::Color color;

        if (!alpha)
            color = WebCore::Color(0, 0, 0, 0);
        else if (alpha == 255)
            color = WebCore::Color(red, green, blue, alpha);
        else
            color = WebCore::Color(red * 255 / alpha, green * 255 / alpha, blue * 255 / alpha, alpha);

        view->updateBackgroundRecursively(color, !alpha);
    }
}

void ewk_view_bg_color_get(const Evas_Object* ewkView, int* red, int* green, int* blue, int* alpha)
{
    if (red)
        *red = 0;
    if (green)
        *green = 0;
    if (blue)
        *blue = 0;
    if (alpha)
        *alpha = 0;
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    if (red)
        *red = smartData->bg_color.r;
    if (green)
        *green = smartData->bg_color.g;
    if (blue)
        *blue = smartData->bg_color.b;
    if (alpha)
        *alpha = smartData->bg_color.a;
}

Eina_Bool ewk_view_text_search(const Evas_Object* ewkView, const char* string, Eina_Bool caseSensitive, Eina_Bool forward, Eina_Bool wrap)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(string, false);
    WTF::TextCaseSensitivity sensitive;
    WebCore::FindDirection direction;

    if (caseSensitive)
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
 * @param ewkView view object where to search text.
 * @param string reference string to match.
 * @param caseSensitive if match should be case sensitive or not.
 * @param heightighlight if matches should be highlighted.
 * @param limit maximum amount of matches, or zero to unlimited.
 *
 * @return number of matches.
 */
unsigned int ewk_view_text_matches_mark(Evas_Object* ewkView, const char* string, Eina_Bool caseSensitive, Eina_Bool highlight, unsigned int limit)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(string, 0);
    WTF::TextCaseSensitivity sensitive;

    if (caseSensitive)
        sensitive = WTF::TextCaseSensitive;
    else
        sensitive = WTF::TextCaseInsensitive;

    return priv->page->markAllMatchesForText(String::fromUTF8(string), sensitive, highlight, limit);
}

Eina_Bool ewk_view_text_matches_unmark_all(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    priv->page->unmarkAllTextMatches();
    return true;
}

Eina_Bool ewk_view_text_matches_highlight_set(Evas_Object* ewkView, Eina_Bool highlight)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_text_matches_highlight_set(smartData->main_frame, highlight);
}

Eina_Bool ewk_view_text_matches_highlight_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_text_matches_highlight_get(smartData->main_frame);
}

Eina_Bool ewk_view_editable_set(Evas_Object* ewkView, Eina_Bool editable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_editable_set(smartData->main_frame, editable);
}

char* ewk_view_selection_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    CString selectedString = priv->page->focusController()->focusedOrMainFrame()->editor()->selectedText().utf8();
    if (selectedString.isNull())
        return 0;
    return strdup(selectedString.data());
}

static Eina_Bool _ewk_view_editor_command(Ewk_View_Private_Data* priv, const char* command, const char* value = 0)
{
    return priv->page->focusController()->focusedOrMainFrame()->editor()->command(WTF::String::fromUTF8(command)).execute(value);
}

Eina_Bool ewk_view_execute_editor_command(Evas_Object* ewkView, const Ewk_Editor_Command command, const char* value)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

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
        return false;
    }
}

Eina_Bool ewk_view_context_menu_forward_event(Evas_Object* ewkView, const Evas_Event_Mouse_Down* downEvent)
{
#if ENABLE(CONTEXT_MENUS)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    Eina_Bool mouse_press_handled = false;

    priv->page->contextMenuController()->clearContextMenu();
    WebCore::Frame* mainFrame = priv->page->mainFrame();
    Evas_Coord x, y;
    evas_object_geometry_get(smartData->self, &x, &y, 0, 0);

    WebCore::PlatformMouseEvent event(downEvent, WebCore::IntPoint(x, y));

    if (mainFrame->view()) {
        mouse_press_handled =
            mainFrame->eventHandler()->handleMousePressEvent(event);
    }

    if (mainFrame->eventHandler()->sendContextMenuEvent(event))
        return false;

    WebCore::ContextMenu* coreMenu =
        priv->page->contextMenuController()->contextMenu();
    if (!coreMenu) {
        // WebCore decided not to create a context menu, return true if event
        // was handled by handleMouseReleaseEvent
        return mouse_press_handled;
    }

    return true;
#else
    return false;
#endif
}

double ewk_view_load_progress_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1.0);
    return priv->page->progress()->estimatedProgress();
}

Eina_Bool ewk_view_stop(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_stop(smartData->main_frame);
}

Eina_Bool ewk_view_reload(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_reload(smartData->main_frame);
}

Eina_Bool ewk_view_reload_full(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_reload_full(smartData->main_frame);
}

Eina_Bool ewk_view_back(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_back(smartData->main_frame);
}

Eina_Bool ewk_view_forward(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_forward(smartData->main_frame);
}

Eina_Bool ewk_view_navigate(Evas_Object* ewkView, int steps)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_navigate(smartData->main_frame, steps);
}

Eina_Bool ewk_view_back_possible(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_back_possible(smartData->main_frame);
}

Eina_Bool ewk_view_forward_possible(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_forward_possible(smartData->main_frame);
}

Eina_Bool ewk_view_navigate_possible(Evas_Object* ewkView, int steps)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_navigate_possible(smartData->main_frame, steps);
}

Eina_Bool ewk_view_history_enable_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return static_cast<WebCore::BackForwardListImpl*>(priv->page->backForwardList())->enabled();
}

Eina_Bool ewk_view_history_enable_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    static_cast<WebCore::BackForwardListImpl*>(priv->page->backForwardList())->setEnabled(enable);
    return true;
}

Ewk_History* ewk_view_history_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    if (!static_cast<WebCore::BackForwardListImpl*>(priv->page->backForwardList())->enabled()) {
        ERR("asked history, but it's disabled! Returning 0!");
        return 0;
    }
    return priv->history;
}

float ewk_view_zoom_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    return ewk_frame_page_zoom_get(smartData->main_frame);
}

Eina_Bool ewk_view_zoom_set(Evas_Object* ewkView, float zoom, Evas_Coord centerX, Evas_Coord centerY)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET(smartData, priv);

    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api->zoom_set, false);

    if (!priv->settings.zoomRange.userScalable) {
        WRN("userScalable is false");
        return false;
    }

    if (zoom < priv->settings.zoomRange.minScale) {
        WRN("zoom level is < %f : %f", priv->settings.zoomRange.minScale, zoom);
        return false;
    }
    if (zoom > priv->settings.zoomRange.maxScale) {
        WRN("zoom level is > %f : %f", priv->settings.zoomRange.maxScale, zoom);
        return false;
    }

    _ewk_view_zoom_animated_mark_stop(smartData);
    return smartData->api->zoom_set(smartData, zoom, centerX, centerY);
}

float ewk_view_page_zoom_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    return ewk_frame_page_zoom_get(smartData->main_frame);
}

Eina_Bool ewk_view_page_zoom_set(Evas_Object* ewkView, float pageZoomFactor)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_page_zoom_set(smartData->main_frame, pageZoomFactor);
}

float ewk_view_scale_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1.0);
    return priv->page->pageScaleFactor();
}

Eina_Bool ewk_view_scale_set(Evas_Object* ewkView, float scaleFactor, Evas_Coord centerX, Evas_Coord centerY)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    float currentScaleFactor = ewk_view_scale_get(ewkView);
    if (currentScaleFactor == -1)
        return false;

    int x, y;
    ewk_frame_scroll_pos_get(smartData->main_frame, &x, &y);

    x = static_cast<int>(((x + centerX) / currentScaleFactor) * scaleFactor) - centerX;
    y = static_cast<int>(((y + centerY) / currentScaleFactor) * scaleFactor) - centerY;
    priv->page->setPageScaleFactor(scaleFactor, WebCore::LayoutPoint(x, y));
    return true;
}

float ewk_view_text_zoom_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    return ewk_frame_text_zoom_get(smartData->main_frame);
}

Eina_Bool ewk_view_text_zoom_set(Evas_Object* ewkView, float textZoomFactor)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_text_zoom_set(smartData->main_frame, textZoomFactor);
}

Eina_Bool ewk_view_zoom_weak_smooth_scale_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return smartData->zoom_weak_smooth_scale;
}

void ewk_view_zoom_weak_smooth_scale_set(Evas_Object* ewkView, Eina_Bool smoothScale)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    smoothScale = !!smoothScale;
    if (smartData->zoom_weak_smooth_scale == smoothScale)
        return;
    smartData->zoom_weak_smooth_scale = smoothScale;
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->zoom_weak_smooth_scale_set);
    smartData->api->zoom_weak_smooth_scale_set(smartData, smoothScale);
}

Eina_Bool ewk_view_zoom_weak_set(Evas_Object* ewkView, float zoom, Evas_Coord centerX, Evas_Coord centerY)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET(smartData, priv);

    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api->zoom_weak_set, false);

    if (!priv->settings.zoomRange.userScalable) {
        WRN("userScalable is false");
        return false;
    }

    if (zoom < priv->settings.zoomRange.minScale) {
        WRN("zoom level is < %f : %f", priv->settings.zoomRange.minScale, zoom);
        return false;
    }
    if (zoom > priv->settings.zoomRange.maxScale) {
        WRN("zoom level is > %f : %f", priv->settings.zoomRange.maxScale, zoom);
        return false;
    }

    smartData->animated_zoom.zoom.start = ewk_frame_page_zoom_get(smartData->main_frame);
    smartData->animated_zoom.zoom.end = zoom;
    smartData->animated_zoom.zoom.current = zoom;
    return smartData->api->zoom_weak_set(smartData, zoom, centerX, centerY);
}

Eina_Bool ewk_view_zoom_animated_mark_start(Evas_Object* ewkView, float zoom)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    smartData->animated_zoom.zoom.start = zoom;
    return true;
}

Eina_Bool ewk_view_zoom_animated_mark_end(Evas_Object* ewkView, float zoom)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    smartData->animated_zoom.zoom.end = zoom;
    return true;
}

Eina_Bool ewk_view_zoom_animated_mark_current(Evas_Object* ewkView, float zoom)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    smartData->animated_zoom.zoom.current = zoom;
    return true;
}

Eina_Bool ewk_view_zoom_animated_mark_stop(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    _ewk_view_zoom_animated_mark_stop(smartData);
    return true;
}

Eina_Bool ewk_view_zoom_animated_set(Evas_Object* ewkView, float zoom, float duration, Evas_Coord centerX, Evas_Coord centerY)
{
    double now;
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api->zoom_weak_set, false);

    if (!priv->settings.zoomRange.userScalable) {
        WRN("userScalable is false");
        return false;
    }

    if (zoom < priv->settings.zoomRange.minScale) {
        WRN("zoom level is < %f : %f", priv->settings.zoomRange.minScale, zoom);
        return false;
    }
    if (zoom > priv->settings.zoomRange.maxScale) {
        WRN("zoom level is > %f : %f", priv->settings.zoomRange.maxScale, zoom);
        return false;
    }

    if (priv->animatedZoom.animator)
        priv->animatedZoom.zoom.start = _ewk_view_zoom_animated_current(priv);
    else {
        priv->animatedZoom.zoom.start = ewk_frame_page_zoom_get(smartData->main_frame);
        _ewk_view_zoom_animation_start(smartData);
    }

    if (centerX < 0)
        centerX = 0;
    if (centerY < 0)
        centerY = 0;

    now = ecore_loop_time_get();
    priv->animatedZoom.time.start = now;
    priv->animatedZoom.time.end = now + duration;
    priv->animatedZoom.time.duration = duration;
    priv->animatedZoom.zoom.end = zoom;
    priv->animatedZoom.zoom.range = (priv->animatedZoom.zoom.end - priv->animatedZoom.zoom.start);
    priv->animatedZoom.center.x = centerX;
    priv->animatedZoom.center.y = centerY;
    smartData->animated_zoom.zoom.current = priv->animatedZoom.zoom.start;
    smartData->animated_zoom.zoom.start = priv->animatedZoom.zoom.start;
    smartData->animated_zoom.zoom.end = priv->animatedZoom.zoom.end;

    return true;
}

Eina_Bool ewk_view_pre_render_region(Evas_Object* ewkView, Evas_Coord x, Evas_Coord y, Evas_Coord width, Evas_Coord height, float zoom)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api->pre_render_region, false);
    float cur_zoom;
    Evas_Coord contentsWidth, contentsHeight;

    /* When doing animated zoom it's not possible to call pre-render since it
     * would screw up parameters that animation is currently using
     */
    if (priv->animatedZoom.animator)
        return false;

    cur_zoom = ewk_frame_page_zoom_get(smartData->main_frame);

    if (cur_zoom < 0.00001)
        return false;
    if (!ewk_frame_contents_size_get(smartData->main_frame, &contentsWidth, &contentsHeight))
        return false;

    contentsWidth *= zoom / cur_zoom;
    contentsHeight *= zoom / cur_zoom;
    DBG("region %d,%d+%dx%d @ %f contents=%dx%d", x, y, width, height, zoom, contentsWidth, contentsHeight);

    if (x + width > contentsWidth)
        width = contentsWidth - x;

    if (y + height > contentsHeight)
        height = contentsHeight - y;

    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }

    return smartData->api->pre_render_region(smartData, x, y, width, height, zoom);
}

Eina_Bool ewk_view_pre_render_relative_radius(Evas_Object* ewkView, unsigned int number)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api->pre_render_relative_radius, false);
    float currentZoom;

    if (priv->animatedZoom.animator)
        return false;

    currentZoom = ewk_frame_page_zoom_get(smartData->main_frame);
    return smartData->api->pre_render_relative_radius(smartData, number, currentZoom);
}

unsigned int ewk_view_imh_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->imh;
}

void ewk_view_pre_render_cancel(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->pre_render_cancel);
    smartData->api->pre_render_cancel(smartData);
}

Eina_Bool ewk_view_enable_render(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api->enable_render, false);
    return smartData->api->enable_render(smartData);
}

Eina_Bool ewk_view_disable_render(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api->disable_render, false);
    return smartData->api->disable_render(smartData);
}

const char* ewk_view_setting_user_agent_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.userAgent;
}

Eina_Bool ewk_view_setting_user_agent_set(Evas_Object* ewkView, const char* userAgent)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (eina_stringshare_replace(&priv->settings.userAgent, userAgent)) {
        WebCore::FrameLoaderClientEfl* client = static_cast<WebCore::FrameLoaderClientEfl*>(priv->mainFrame->loader()->client());
        client->setCustomUserAgent(String::fromUTF8(userAgent));
    }
    return true;
}

const char* ewk_view_setting_user_stylesheet_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.userStylesheet;
}

Eina_Bool ewk_view_setting_user_stylesheet_set(Evas_Object* ewkView, const char* uri)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (eina_stringshare_replace(&priv->settings.userStylesheet, uri)) {
        WebCore::KURL kurl(WebCore::KURL(), String::fromUTF8(uri));
        priv->pageSettings->setUserStyleSheetLocation(kurl);
    }
    return true;
}

Eina_Bool ewk_view_setting_auto_load_images_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.autoLoadImages;
}

Eina_Bool ewk_view_setting_auto_load_images_set(Evas_Object* ewkView, Eina_Bool automatic)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    automatic = !!automatic;
    if (priv->settings.autoLoadImages != automatic) {
        priv->pageSettings->setLoadsImagesAutomatically(automatic);
        priv->settings.autoLoadImages = automatic;
    }
    return true;
}

Eina_Bool ewk_view_setting_auto_shrink_images_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.autoShrinkImages;
}

Eina_Bool ewk_view_setting_auto_shrink_images_set(Evas_Object* ewkView, Eina_Bool automatic)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    automatic = !!automatic;
    if (priv->settings.autoShrinkImages != automatic) {
        priv->pageSettings->setShrinksStandaloneImagesToFit(automatic);
        priv->settings.autoShrinkImages = automatic;
    }
    return true;
}

Eina_Bool ewk_view_setting_enable_auto_resize_window_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.enableAutoResizeWindow;
}

Eina_Bool ewk_view_setting_enable_auto_resize_window_set(Evas_Object* ewkView, Eina_Bool resizable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    priv->settings.enableAutoResizeWindow = resizable;
    return true;
}

Eina_Bool ewk_view_setting_enable_scripts_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.enableScripts;
}

Eina_Bool ewk_view_setting_enable_scripts_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.enableScripts != enable) {
        priv->pageSettings->setScriptEnabled(enable);
        priv->settings.enableScripts = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_enable_plugins_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.enablePlugins;
}

Eina_Bool ewk_view_setting_enable_plugins_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.enablePlugins != enable) {
        priv->pageSettings->setPluginsEnabled(enable);
        priv->settings.enablePlugins = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_enable_frame_flattening_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.enableFrameFlattening;
}

Eina_Bool ewk_view_setting_enable_frame_flattening_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.enableFrameFlattening != enable) {
        priv->pageSettings->setFrameFlatteningEnabled(enable);
        priv->settings.enableFrameFlattening = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_scripts_can_open_windows_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.scriptsCanOpenWindows;
}

Eina_Bool ewk_view_setting_scripts_can_open_windows_set(Evas_Object* ewkView, Eina_Bool allow)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    allow = !!allow;
    if (priv->settings.scriptsCanOpenWindows != allow) {
        priv->pageSettings->setJavaScriptCanOpenWindowsAutomatically(allow);
        priv->settings.scriptsCanOpenWindows = allow;
    }
    return true;
}

Eina_Bool ewk_view_setting_scripts_can_close_windows_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.scriptsCanCloseWindows;
}

Eina_Bool ewk_view_setting_scripts_can_close_windows_set(Evas_Object* ewkView, Eina_Bool allow)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    allow = !!allow;
    if (priv->settings.scriptsCanCloseWindows != allow) {
        priv->pageSettings->setAllowScriptsToCloseWindows(allow);
        priv->settings.scriptsCanCloseWindows = allow;
    }
    return true;
}

Eina_Bool ewk_view_setting_resizable_textareas_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.resizableTextareas;
}

Eina_Bool ewk_view_setting_resizable_textareas_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.resizableTextareas != enable) {
        priv->pageSettings->setTextAreasAreResizable(enable);
        priv->settings.resizableTextareas = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_private_browsing_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.privateBrowsing;
}

Eina_Bool ewk_view_setting_private_browsing_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.privateBrowsing != enable) {
        priv->pageSettings->setPrivateBrowsingEnabled(enable);
        priv->settings.privateBrowsing = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_application_cache_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.offlineAppCache;
}

Eina_Bool ewk_view_setting_application_cache_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.offlineAppCache != enable) {
        priv->pageSettings->setOfflineWebApplicationCacheEnabled(enable);
        priv->settings.offlineAppCache = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_caret_browsing_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.caretBrowsing;
}

Eina_Bool ewk_view_setting_caret_browsing_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.caretBrowsing != enable) {
        priv->pageSettings->setCaretBrowsingEnabled(enable);
        priv->settings.caretBrowsing = enable;
    }
    return true;
}

const char* ewk_view_setting_encoding_custom_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    Evas_Object* main_frame = ewk_view_frame_main_get(ewkView);
    WebCore::Frame* core_frame = EWKPrivate::coreFrame(main_frame);

    String overrideEncoding = core_frame->loader()->documentLoader()->overrideEncoding();

    if (overrideEncoding.isEmpty())
        return 0;

    eina_stringshare_replace(&priv->settings.encodingCustom, overrideEncoding.utf8().data());
    return priv->settings.encodingCustom;
}

Eina_Bool ewk_view_setting_encoding_custom_set(Evas_Object* ewkView, const char* encoding)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    Evas_Object* main_frame = ewk_view_frame_main_get(ewkView);
    WebCore::Frame* coreFrame = EWKPrivate::coreFrame(main_frame);
    DBG("%s", encoding);
    if (eina_stringshare_replace(&priv->settings.encodingCustom, encoding))
        coreFrame->loader()->reloadWithOverrideEncoding(String::fromUTF8(encoding));
    return true;
}

const char* ewk_view_setting_encoding_default_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.encodingDefault;
}

Eina_Bool ewk_view_setting_encoding_default_set(Evas_Object* ewkView, const char* encoding)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (eina_stringshare_replace(&priv->settings.encodingDefault, encoding))
        priv->pageSettings->setDefaultTextEncodingName(String::fromUTF8(encoding));
    return true;
}

Eina_Bool ewk_view_setting_encoding_detector_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.encodingDetector != enable) {
        priv->pageSettings->setUsesEncodingDetector(enable);
        priv->settings.encodingDetector = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_encoding_detector_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.encodingDetector;
}

Eina_Bool ewk_view_setting_enable_developer_extras_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.enableDeveloperExtras;
}

Eina_Bool ewk_view_setting_enable_developer_extras_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.enableDeveloperExtras != enable) {
        priv->pageSettings->setDeveloperExtrasEnabled(enable);
        priv->settings.enableDeveloperExtras = enable;
    }
    return true;
}

int ewk_view_setting_font_minimum_size_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.fontMinimumSize;
}

Eina_Bool ewk_view_setting_font_minimum_size_set(Evas_Object* ewkView, int size)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (priv->settings.fontMinimumSize != size) {
        priv->pageSettings->setMinimumFontSize(size);
        priv->settings.fontMinimumSize = size;
    }
    return true;
}

int ewk_view_setting_font_minimum_logical_size_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.fontMinimumLogicalSize;
}

Eina_Bool ewk_view_setting_font_minimum_logical_size_set(Evas_Object* ewkView, int size)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (priv->settings.fontMinimumLogicalSize != size) {
        priv->pageSettings->setMinimumLogicalFontSize(size);
        priv->settings.fontMinimumLogicalSize = size;
    }
    return true;
}

int ewk_view_setting_font_default_size_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.fontDefaultSize;
}

Eina_Bool ewk_view_setting_font_default_size_set(Evas_Object* ewkView, int size)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (priv->settings.fontDefaultSize != size) {
        priv->pageSettings->setDefaultFontSize(size);
        priv->settings.fontDefaultSize = size;
    }
    return true;
}

int ewk_view_setting_font_monospace_size_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.fontMonospaceSize;
}

Eina_Bool ewk_view_setting_font_monospace_size_set(Evas_Object* ewkView, int size)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (priv->settings.fontMonospaceSize != size) {
        priv->pageSettings->setDefaultFixedFontSize(size);
        priv->settings.fontMonospaceSize = size;
    }
    return true;
}

const char* ewk_view_font_family_name_get(const Evas_Object* ewkView, Ewk_Font_Family fontFamily)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    switch (fontFamily) {
    case EWK_FONT_FAMILY_STANDARD:
        return priv->settings.fontStandard;
    case EWK_FONT_FAMILY_CURSIVE:
        return priv->settings.fontCursive;
    case EWK_FONT_FAMILY_FANTASY:
        return priv->settings.fontFantasy;
    case EWK_FONT_FAMILY_MONOSPACE:
        return priv->settings.fontMonospace;
    case EWK_FONT_FAMILY_SERIF:
        return priv->settings.fontSerif;
    case EWK_FONT_FAMILY_SANS_SERIF:
        return priv->settings.fontSansSerif;
    }
    return 0;
}

Eina_Bool ewk_view_font_family_name_set(Evas_Object* ewkView, Ewk_Font_Family fontFamily, const char* name)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    switch (fontFamily) {
    case EWK_FONT_FAMILY_STANDARD:
        if (eina_stringshare_replace(&priv->settings.fontStandard, name))
            priv->pageSettings->setStandardFontFamily(AtomicString::fromUTF8(name));
        break;
    case EWK_FONT_FAMILY_CURSIVE:
        if (eina_stringshare_replace(&priv->settings.fontCursive, name))
            priv->pageSettings->setCursiveFontFamily(AtomicString::fromUTF8(name));
        break;
    case EWK_FONT_FAMILY_FANTASY:
        if (eina_stringshare_replace(&priv->settings.fontFantasy, name))
            priv->pageSettings->setFantasyFontFamily(AtomicString::fromUTF8(name));
        break;
    case EWK_FONT_FAMILY_MONOSPACE:
        if (eina_stringshare_replace(&priv->settings.fontMonospace, name))
            priv->pageSettings->setFixedFontFamily(AtomicString::fromUTF8(name));
        break;
    case EWK_FONT_FAMILY_SERIF:
        if (eina_stringshare_replace(&priv->settings.fontSerif, name))
            priv->pageSettings->setSerifFontFamily(AtomicString::fromUTF8(name));
        break;
    case EWK_FONT_FAMILY_SANS_SERIF:
        if (eina_stringshare_replace(&priv->settings.fontSansSerif, name))
            priv->pageSettings->setSansSerifFontFamily(AtomicString::fromUTF8(name));
        break;
    default:
        return false;
    }

    return true;
}

Eina_Bool ewk_view_setting_spatial_navigation_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.spatialNavigation;
}

Eina_Bool ewk_view_setting_spatial_navigation_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.spatialNavigation != enable) {
        priv->pageSettings->setSpatialNavigationEnabled(enable);
        priv->settings.spatialNavigation = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_local_storage_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.localStorage;
}

Eina_Bool ewk_view_setting_local_storage_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.localStorage != enable) {
        priv->pageSettings->setLocalStorageEnabled(enable);
        priv->settings.localStorage = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_page_cache_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.pageCache;
}

Eina_Bool ewk_view_setting_page_cache_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.pageCache != enable) {
        priv->pageSettings->setUsesPageCache(enable);
        priv->settings.pageCache = enable;
    }
    return true;
}

const char* ewk_view_setting_local_storage_database_path_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.localStorageDatabasePath;
}

Eina_Bool ewk_view_setting_local_storage_database_path_set(Evas_Object* ewkView, const char* path)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (eina_stringshare_replace(&priv->settings.localStorageDatabasePath, path))
        priv->pageSettings->setLocalStorageDatabasePath(String::fromUTF8(path));
    return true;
}

Eina_Bool ewk_view_setting_minimum_timer_interval_set(Evas_Object* ewkView, double interval)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (fabs(priv->settings.domTimerInterval - interval) >= std::numeric_limits<double>::epsilon()) {
        priv->pageSettings->setMinDOMTimerInterval(interval);
        priv->settings.domTimerInterval = interval;
    }
    return true;
}

double ewk_view_setting_minimum_timer_interval_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.domTimerInterval;
}

Ewk_View_Smart_Data* ewk_view_smart_data_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    return smartData;
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
 * @param width width of area to be repainted
 * @param height height of area to be repainted
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
void ewk_view_repaint_add(Ewk_View_Private_Data* priv, Evas_Coord x, Evas_Coord y, Evas_Coord width, Evas_Coord height)
{
    EINA_SAFETY_ON_NULL_RETURN(priv);
    _ewk_view_repaint_add(priv, x, y, width, height);
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

    WebCore::FrameView* view = priv->mainFrame->view();
    if (!view) {
        ERR("no main frame view");
        return;
    }
    view->updateLayoutAndStyleIfNeededRecursive();
}

void ewk_view_scrolls_process(Ewk_View_Smart_Data* smartData)
{
    EINA_SAFETY_ON_NULL_RETURN(smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    if (!smartData->api->scrolls_process(smartData))
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
    WebCore::GraphicsContext* graphicContext;
    WebCore::FrameView* view;
    cairo_t* cr;
};

Ewk_View_Paint_Context* ewk_view_paint_context_new(Ewk_View_Private_Data* priv, cairo_t* cr)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(cr, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv->mainFrame, 0);
    WebCore::FrameView* view = priv->mainFrame->view();
    EINA_SAFETY_ON_NULL_RETURN_VAL(view, 0);
    Ewk_View_Paint_Context* context = static_cast<Ewk_View_Paint_Context*>(malloc(sizeof(*context)));
    EINA_SAFETY_ON_NULL_RETURN_VAL(context, 0);

    context->graphicContext = new WebCore::GraphicsContext(cr);
    if (!context->graphicContext) {
        free(context);
        return 0;
    }
    context->view = view;
    context->cr = cairo_reference(cr);
    return context;
}

void ewk_view_paint_context_free(Ewk_View_Paint_Context* context)
{
    EINA_SAFETY_ON_NULL_RETURN(context);
    delete context->graphicContext;
    cairo_destroy(context->cr);
    free(context);
}

void ewk_view_paint_context_save(Ewk_View_Paint_Context* context)
{
    EINA_SAFETY_ON_NULL_RETURN(context);
    cairo_save(context->cr);
    context->graphicContext->save();
}

void ewk_view_paint_context_restore(Ewk_View_Paint_Context* context)
{
    EINA_SAFETY_ON_NULL_RETURN(context);
    context->graphicContext->restore();
    cairo_restore(context->cr);
}

void ewk_view_paint_context_clip(Ewk_View_Paint_Context* context, const Eina_Rectangle* area)
{
    EINA_SAFETY_ON_NULL_RETURN(context);
    EINA_SAFETY_ON_NULL_RETURN(area);
    context->graphicContext->clip(WebCore::IntRect(area->x, area->y, area->w, area->h));
}

void ewk_view_paint_context_paint(Ewk_View_Paint_Context* context, const Eina_Rectangle* area)
{
    EINA_SAFETY_ON_NULL_RETURN(context);
    EINA_SAFETY_ON_NULL_RETURN(area);

    WebCore::IntRect rect(area->x, area->y, area->w, area->h);

    if (context->view->isTransparent())
        context->graphicContext->clearRect(rect);
    context->view->paint(context->graphicContext, rect);
}

void ewk_view_paint_context_paint_contents(Ewk_View_Paint_Context* context, const Eina_Rectangle* area)
{
    EINA_SAFETY_ON_NULL_RETURN(context);
    EINA_SAFETY_ON_NULL_RETURN(area);

    WebCore::IntRect rect(area->x, area->y, area->w, area->h);

    if (context->view->isTransparent())
        context->graphicContext->clearRect(rect);

    context->view->paintContents(context->graphicContext, rect);
}

void ewk_view_paint_context_scale(Ewk_View_Paint_Context* context, float scaleX, float scaleY)
{
    EINA_SAFETY_ON_NULL_RETURN(context);

    context->graphicContext->scale(WebCore::FloatSize(scaleX, scaleY));
}

void ewk_view_paint_context_translate(Ewk_View_Paint_Context* context, float x, float y)
{
    EINA_SAFETY_ON_NULL_RETURN(context);

    context->graphicContext->translate(x, y);
}

Eina_Bool ewk_view_paint(Ewk_View_Private_Data* priv, cairo_t* cr, const Eina_Rectangle* area)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(cr, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(area, false);
    WebCore::FrameView* view = priv->mainFrame->view();
    EINA_SAFETY_ON_NULL_RETURN_VAL(view, false);

    if (view->needsLayout())
        view->forceLayout();
    WebCore::GraphicsContext graphicsContext(cr);
    WebCore::IntRect rect(area->x, area->y, area->w, area->h);

    cairo_save(cr);
    graphicsContext.save();
    graphicsContext.clip(rect);
    if (view->isTransparent())
        graphicsContext.clearRect(rect);
    view->paint(&graphicsContext, rect);
    graphicsContext.restore();
    cairo_restore(cr);

    return true;
}

Eina_Bool ewk_view_paint_contents(Ewk_View_Private_Data* priv, cairo_t* cr, const Eina_Rectangle* area)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(cr, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(area, false);
    WebCore::FrameView* view = priv->mainFrame->view();
    EINA_SAFETY_ON_NULL_RETURN_VAL(view, false);

    WebCore::GraphicsContext graphicsContext(cr);
    WebCore::IntRect rect(area->x, area->y, area->w, area->h);

    cairo_save(cr);
    graphicsContext.save();
    graphicsContext.clip(rect);
    if (view->isTransparent())
        graphicsContext.clearRect(rect);
    view->paintContents(&graphicsContext,  rect);
    graphicsContext.restore();
    cairo_restore(cr);

    return true;
}


/* internal methods ****************************************************/
/**
 * @internal
 * Reports the view is ready to be displayed as all elements are aready.
 *
 * Emits signal: "ready" with no parameters.
 */
void ewk_view_ready(Evas_Object* ewkView)
{
    DBG("ewkView=%p", ewkView);
    evas_object_smart_callback_call(ewkView, "ready", 0);
}

/**
 * @internal
 * Reports the state of input method changed. This is triggered, for example
 * when a input field received/lost focus
 *
 * Emits signal: "inputmethod,changed" with a boolean indicating whether it's
 * enabled or not.
 */
void ewk_view_input_method_state_set(Evas_Object* ewkView, bool active)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET(smartData, priv);
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

    evas_object_smart_callback_call(ewkView, "inputmethod,changed", (void*)active);
}

/**
 * @internal
 * The view title was changed by the frame loader.
 *
 * Emits signal: "title,changed" with pointer to new title string.
 */
void ewk_view_title_set(Evas_Object* ewkView, const char* title)
{
    DBG("ewkView=%p, title=%s", ewkView, title ? title : "(null)");
    evas_object_smart_callback_call(ewkView, "title,changed", (void*)title);
}

/**
 * @internal
 * Reports that main frame's uri changed.
 *
 * Emits signal: "uri,changed" with pointer to the new uri string.
 */
void ewk_view_uri_changed(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    const char* uri = ewk_frame_uri_get(smartData->main_frame);
    DBG("ewkView=%p, uri=%s", ewkView, uri ? uri : "(null)");
    evas_object_smart_callback_call(ewkView, "uri,changed", (void*)uri);
}

/**
 * @internal
 * Reports that a DOM document object has finished loading for @p frame.
 *
 * @param ewkView View which contains the frame.
 * @param frame The frame whose load has triggered the event.
 *
 * Emits signal: "load,document,finished" with @p frame as the parameter.
 */
void ewk_view_load_document_finished(Evas_Object* ewkView, Evas_Object* frame)
{
    evas_object_smart_callback_call(ewkView, "load,document,finished", frame);
}

/**
 * @internal
 * Reports the view started loading something.
 *
 * @param ewkView View.
 *
 * Emits signal: "load,started" with no parameters.
 */
void ewk_view_load_started(Evas_Object* ewkView)
{
    DBG("ewkView=%p", ewkView);
    evas_object_smart_callback_call(ewkView, "load,started", 0);
}

/**
 * Reports the frame started loading something.
 *
 * @param ewkView View.
 *
 * Emits signal: "load,started" on main frame with no parameters.
 */
void ewk_view_frame_main_load_started(Evas_Object* ewkView)
{
    DBG("ewkView=%p", ewkView);
    Evas_Object* frame = ewk_view_frame_main_get(ewkView);
    evas_object_smart_callback_call(frame, "load,started", 0);
}

/**
 * @internal
 * Reports the main frame started provisional load.
 *
 * @param ewkView View.
 *
 * Emits signal: "load,provisional" on View with no parameters.
 */
void ewk_view_load_provisional(Evas_Object* ewkView)
{
    DBG("ewkView=%p", ewkView);
    evas_object_smart_callback_call(ewkView, "load,provisional", 0);
}

/**
 * @internal
 * Reports view can be shown after a new window is created.
 *
 * @param ewkView Frame.
 *
 * Emits signal: "load,newwindow,show" on view with no parameters.
 */
void ewk_view_load_show(Evas_Object* ewkView)
{
    DBG("ewkView=%p", ewkView);
    evas_object_smart_callback_call(ewkView, "load,newwindow,show", 0);
}


/**
 * @internal
 * Reports the main frame was cleared.
 *
 * @param ewkView View.
 */
void ewk_view_frame_main_cleared(Evas_Object* ewkView)
{
    DBG("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->flush);
    smartData->api->flush(smartData);
}

/**
 * @internal
 * Reports the main frame received an icon.
 *
 * @param ewkView View.
 *
 * Emits signal: "icon,received" with no parameters.
 */
void ewk_view_frame_main_icon_received(Evas_Object* ewkView)
{
    DBG("ewkView=%p", ewkView);
    Evas_Object* frame = ewk_view_frame_main_get(ewkView);
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
void ewk_view_load_finished(Evas_Object* ewkView, const Ewk_Frame_Load_Error* error)
{
    DBG("ewkView=%p, error=%p", ewkView, error);
    evas_object_smart_callback_call(ewkView, "load,finished", (void*)error);
}

/**
 * @internal
 * Reports load failed with error information.
 *
 * Emits signal: "load,error" with pointer to Ewk_Frame_Load_Error.
 */
void ewk_view_load_error(Evas_Object* ewkView, const Ewk_Frame_Load_Error* error)
{
    DBG("ewkView=%p, error=%p", ewkView, error);
    evas_object_smart_callback_call(ewkView, "load,error", (void*)error);
}

/**
 * @internal
 * Reports load progress changed.
 *
 * Emits signal: "load,progress" with pointer to a double from 0.0 to 1.0.
 */
void ewk_view_load_progress_changed(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    // Evas_Coord width, height;
    double progress = priv->page->progress()->estimatedProgress();

    DBG("ewkView=%p (p=%0.3f)", ewkView, progress);

    evas_object_smart_callback_call(ewkView, "load,progress", &progress);
}

/**
 * @internal
 * Reports view @param ewkView should be restored to default conditions
 *
 * @param ewkView View.
 * @param frame Frame that originated restore.
 *
 * Emits signal: "restore" with frame.
 */
void ewk_view_restore_state(Evas_Object* ewkView, Evas_Object* frame)
{
    evas_object_smart_callback_call(ewkView, "restore", frame);
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
 * @param ewkView Current view.
 * @param javascript @c true if the new window is originated from javascript,
 * @c false otherwise
 * @param widthindow_features Features of the new window being created. If it's @c
 * NULL, it will be created a window with default features.
 *
 * @return New view, in case smart class implements the creation of new windows;
 * else, current view @param ewkView or @c 0 on failure.
 *
 * @see ewk_window_features_ref().
 */
Evas_Object* ewk_view_window_create(Evas_Object* ewkView, bool javascript, const WebCore::WindowFeatures* coreFeatures)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);

    if (!smartData->api->window_create)
        return ewkView;

    Ewk_Window_Features* windowFeatures = ewk_window_features_new_from_core(coreFeatures);
    if (!windowFeatures)
        return 0;

    Evas_Object* view = smartData->api->window_create(smartData, javascript, windowFeatures);
    ewk_window_features_unref(windowFeatures);

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
 * @param ewkView View to be closed.
 */
void ewk_view_window_close(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);

    ewk_view_stop(ewkView);
    if (!smartData->api->window_close)
        return;
    smartData->api->window_close(smartData);
}

/**
 * @internal
 * Reports mouse has moved over a link.
 *
 * Emits signal: "link,hover,in"
 */
void ewk_view_mouse_link_hover_in(Evas_Object* ewkView, void* data)
{
    evas_object_smart_callback_call(ewkView, "link,hover,in", data);
}

/**
 * @internal
 * Reports mouse is not over a link anymore.
 *
 * Emits signal: "link,hover,out"
 */
void ewk_view_mouse_link_hover_out(Evas_Object* ewkView)
{
    evas_object_smart_callback_call(ewkView, "link,hover,out", 0);
}

/**
 * @internal
 * Set toolbar visible.
 *
 * Emits signal: "toolbars,visible,set" with a pointer to a boolean.
 */
void ewk_view_toolbars_visible_set(Evas_Object* ewkView, bool visible)
{
    DBG("ewkView=%p (visible=%d)", ewkView, !!visible);
    evas_object_smart_callback_call(ewkView, "toolbars,visible,set", &visible);
}

/**
 * @internal
 * Get toolbar visibility.
 *
 * @param ewkView View.
 * @param visible boolean pointer in which to save the result. It defaults
 * to @c FALSE, i.e. if browser does no listen to emitted signal, it means
 * there are no toolbars and therefore they are not visible.
 *
 * Emits signal: "toolbars,visible,get" with a pointer to a boolean.
 */
void ewk_view_toolbars_visible_get(Evas_Object* ewkView, bool* visible)
{
    DBG("%s, o=%p", __func__, ewkView);
    *visible = false;
    evas_object_smart_callback_call(ewkView, "toolbars,visible,get", visible);
}

/**
 * @internal
 * Set statusbar visible.
 *
 * @param ewkView View.
 * @param visible @c TRUE if statusbar are visible, @c FALSE otherwise.
 *
 * Emits signal: "statusbar,visible,set" with a pointer to a boolean.
 */
void ewk_view_statusbar_visible_set(Evas_Object* ewkView, bool  visible)
{
    DBG("ewkView=%p (visible=%d)", ewkView, !!visible);
    evas_object_smart_callback_call(ewkView, "statusbar,visible,set", &visible);
}

/**
 * @internal
 * Get statusbar visibility.
 *
 * @param ewkView View.
 * @param visible boolean pointer in which to save the result. It defaults
 * to @c FALSE, i.e. if browser does no listen to emitted signal, it means
 * there is no statusbar and therefore it is not visible.
 *
 * Emits signal: "statusbar,visible,get" with a pointer to a boolean.
 */
void ewk_view_statusbar_visible_get(Evas_Object* ewkView, bool* visible)
{
    DBG("%s, o=%p", __func__, ewkView);
    *visible = false;
    evas_object_smart_callback_call(ewkView, "statusbar,visible,get", visible);
}

/**
 * @internal
 * Set text of statusbar
 *
 * @param ewkView View.
 * @param text New text to put on statusbar.
 *
 * Emits signal: "statusbar,text,set" with a string.
 */
void ewk_view_statusbar_text_set(Evas_Object* ewkView, const char* text)
{
    DBG("ewkView=%p (text=%s)", ewkView, text);
    INF("status bar text set: %s", text);
    evas_object_smart_callback_call(ewkView, "statusbar,text,set", (void*)text);
}

/**
 * @internal
 * Set scrollbars visible.
 *
 * @param ewkView View.
 * @param visible @c TRUE if scrollbars are visible, @c FALSE otherwise.
 *
 * Emits signal: "scrollbars,visible,set" with a pointer to a boolean.
 */
void ewk_view_scrollbars_visible_set(Evas_Object* ewkView, bool visible)
{
    DBG("ewkView=%p (visible=%d)", ewkView, !!visible);
    evas_object_smart_callback_call(ewkView, "scrollbars,visible,set", &visible);
}

/**
 * @internal
 * Get scrollbars visibility.
 *
 * @param ewkView View.
 * @param visible boolean pointer in which to save the result. It defaults
 * to @c FALSE, i.e. if browser does no listen to emitted signal, it means
 * there are no scrollbars and therefore they are not visible.
 *
 * Emits signal: "scrollbars,visible,get" with a pointer to a boolean.
 */
void ewk_view_scrollbars_visible_get(Evas_Object* ewkView, bool* visible)
{
    DBG("%s, o=%p", __func__, ewkView);
    *visible = false;
    evas_object_smart_callback_call(ewkView, "scrollbars,visible,get", visible);
}

/**
 * @internal
 * Set menubar visible.
 *
 * @param ewkView View.
 * @param visible @c TRUE if menubar is visible, @c FALSE otherwise.
 *
 * Emits signal: "menubar,visible,set" with a pointer to a boolean.
 */
void ewk_view_menubar_visible_set(Evas_Object* ewkView, bool visible)
{
    DBG("ewkView=%p (visible=%d)", ewkView, !!visible);
    evas_object_smart_callback_call(ewkView, "menubar,visible,set", &visible);
}

/**
 * @internal
 * Get menubar visibility.
 *
 * @param ewkView View.
 * @param visible boolean pointer in which to save the result. It defaults
 * to @c FALSE, i.e. if browser does no listen to emitted signal, it means
 * there is no menubar and therefore it is not visible.
 *
 * Emits signal: "menubar,visible,get" with a pointer to a boolean.
 */
void ewk_view_menubar_visible_get(Evas_Object* ewkView, bool* visible)
{
    DBG("%s, o=%p", __func__, ewkView);
    *visible = false;
    evas_object_smart_callback_call(ewkView, "menubar,visible,get", visible);
}

/**
 * @internal
 * Set tooltip text and display if it is currently hidden.
 *
 * @param ewkView View.
 * @param text Text to set tooltip to.
 *
 * Emits signal: "tooltip,text,set" with a string. If tooltip must be actually
 * removed, text will be 0 or '\0'
 */
void ewk_view_tooltip_text_set(Evas_Object* ewkView, const char* text)
{
    DBG("ewkView=%p text=%s", ewkView, text);
    evas_object_smart_callback_call(ewkView, "tooltip,text,set", (void*)text);
}

/**
 * @internal
 *
 * @param ewkView View.
 * @param message String to show on console.
 * @param lineNumber Line number.
 * @sourceID Source id.
 *
 */
void ewk_view_add_console_message(Evas_Object* ewkView, const char* message, unsigned int lineNumber, const char* sourceID)
{
    DBG("ewkView=%p message=%s lineNumber=%u sourceID=%s", ewkView, message, lineNumber, sourceID);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->add_console_message);
    smartData->api->add_console_message(smartData, message, lineNumber, sourceID);
}

bool ewk_view_focus_can_cycle(Evas_Object* ewkView, Ewk_Focus_Direction direction)
{
    DBG("ewkView=%p direction=%d", ewkView, direction);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);

    if (!smartData->api->focus_can_cycle)
        return false;

    return smartData->api->focus_can_cycle(smartData, direction);
}

void ewk_view_run_javascript_alert(Evas_Object* ewkView, Evas_Object* frame, const char* message)
{
    DBG("ewkView=%p frame=%p message=%s", ewkView, frame, message);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);

    if (!smartData->api->run_javascript_alert)
        return;

    smartData->api->run_javascript_alert(smartData, frame, message);
}

bool ewk_view_run_javascript_confirm(Evas_Object* ewkView, Evas_Object* frame, const char* message)
{
    DBG("ewkView=%p frame=%p message=%s", ewkView, frame, message);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);

    if (!smartData->api->run_javascript_confirm)
        return false;

    return smartData->api->run_javascript_confirm(smartData, frame, message);
}

bool ewk_view_run_javascript_prompt(Evas_Object* ewkView, Evas_Object* frame, const char* message, const char* defaultValue, char** value)
{
    DBG("ewkView=%p frame=%p message=%s", ewkView, frame, message);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);

    if (!smartData->api->run_javascript_prompt)
        return false;

    return smartData->api->run_javascript_prompt(smartData, frame, message, defaultValue, value);
}

/**
 * @internal
 * Delegates to client to decide whether a script must be stopped because it's
 * running for too long. If client does not implement it, it goes to default
 * implementation, which logs and returns false. Client may remove log by
 * setting this function 0, which will just return false.
 *
 * @param ewkView View.
 *
 * @return @c true if script should be stopped; @c false otherwise
 */
bool ewk_view_should_interrupt_javascript(Evas_Object* ewkView)
{
    DBG("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);

    if (!smartData->api->should_interrupt_javascript)
        return false;

    return smartData->api->should_interrupt_javascript(smartData);
}

/**
 * @internal
 * This is called whenever the web site shown in @param frame is asking to store data
 * to the database @param databaseName and the quota allocated to that web site
 * is exceeded. Browser may use this to increase the size of quota before the
 * originating operationa fails.
 *
 * @param ewkView View.
 * @param frame The frame whose web page exceeded its database quota.
 * @param databaseName Database name.
 * @param currentSize Current size of this database
 * @param expectedSize The expected size of this database in order to fulfill
 * site's requirement.
 */
uint64_t ewk_view_exceeded_database_quota(Evas_Object* ewkView, Evas_Object* frame, const char* databaseName, uint64_t currentSize, uint64_t expectedSize)
{
    DBG("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, 0);
    if (!smartData->api->exceeded_database_quota)
        return 0;

    INF("currentSize=%" PRIu64 " expectedSize=%" PRIu64, currentSize, expectedSize);
    return smartData->api->exceeded_database_quota(smartData, frame, databaseName, currentSize, expectedSize);
}

/**
 * @internal
 * Open panel to choose a file.
 *
 * @param ewkView View.
 * @param frame Frame in which operation is required.
 * @param allowsMultipleFiles @c true when more than one file may be selected, @c false otherwise.
 * @param acceptMIMETypes List of accepted mime types. It is passed to child objects as an Eina_List of char pointers that is freed automatically.
 * @param selectedFilenames List of files selected.
 *
 * @return @false if user canceled file selection; @true if confirmed.
 */
bool ewk_view_run_open_panel(Evas_Object* ewkView, Evas_Object* frame, bool allowsMultipleFiles, const WTF::Vector<WTF::String>& acceptMIMETypes, Eina_List** selectedFilenames)
{
    DBG("ewkView=%p frame=%p allows_multiple_files=%d", ewkView, frame, allowsMultipleFiles);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);

    if (!smartData->api->run_open_panel)
        return false;

    *selectedFilenames = 0;

    Eina_List* cAcceptMIMETypes = 0;
    for (WTF::Vector<WTF::String>::const_iterator iterator = acceptMIMETypes.begin(); iterator != acceptMIMETypes.end(); ++iterator)
        cAcceptMIMETypes = eina_list_append(cAcceptMIMETypes, strdup((*iterator).utf8().data()));

    bool confirm = smartData->api->run_open_panel(smartData, frame, allowsMultipleFiles, cAcceptMIMETypes, selectedFilenames);
    if (!confirm && *selectedFilenames)
        ERR("Canceled file selection, but selected filenames != 0. Free names before return.");

    void* item = 0;
    EINA_LIST_FREE(cAcceptMIMETypes, item)
        free(item);

    return confirm;
}

void ewk_view_repaint(Evas_Object* ewkView, Evas_Coord x, Evas_Coord y, Evas_Coord width, Evas_Coord height)
{
    DBG("ewkView=%p, region=%d,%d + %dx%d", ewkView, x, y, width, height);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    _ewk_view_repaint_add(priv, x, y, width, height);
    _ewk_view_smart_changed(smartData);
}

void ewk_view_scroll(Evas_Object* ewkView, Evas_Coord dx, Evas_Coord dy, Evas_Coord scrollX, Evas_Coord scrollY, Evas_Coord scrollWidth, Evas_Coord scrollHeight, Evas_Coord centerX, Evas_Coord centerY, Evas_Coord centerWidth, Evas_Coord centerHeight, bool mainFrame)
{
    DBG("ewkView=%p, delta: %d,%d, scroll: %d,%d+%dx%d, clip: %d,%d+%dx%d",
        ewkView, dx, dy, scrollX, scrollY, scrollWidth, scrollHeight, centerX, centerY, centerWidth, centerHeight);

    if ((scrollX != centerX) || (scrollY != centerY) || (scrollWidth != centerWidth) || (scrollHeight != centerHeight))
        WRN("scroll region and clip are different! %d,%d+%dx%d and %d,%d+%dx%d",
            scrollX, scrollY, scrollWidth, scrollHeight, centerX, centerY, centerWidth, centerHeight);

    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    EINA_SAFETY_ON_TRUE_RETURN(!dx && !dy);

    _ewk_view_scroll_add(priv, dx, dy, scrollX, scrollY, scrollWidth, scrollHeight, mainFrame);

    _ewk_view_smart_changed(smartData);
}

WebCore::Page* ewk_view_core_page_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->page;
}

/**
 * Creates a new frame for given url and owner element.
 *
 * Emits "frame,created" with the new frame object on success.
 */
WTF::PassRefPtr<WebCore::Frame> ewk_view_frame_create(Evas_Object* ewkView, Evas_Object* frame, const WTF::String& name, WebCore::HTMLFrameOwnerElement* ownerElement, const WebCore::KURL& url, const WTF::String& referrer)
{
    DBG("ewkView=%p, frame=%p, name=%s, ownerElement=%p, url=%s, referrer=%s",
        ewkView, frame, name.utf8().data(), ownerElement,
        url.string().utf8().data(), referrer.utf8().data());

    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    WTF::RefPtr<WebCore::Frame> coreFrame = _ewk_view_core_frame_new
                                         (smartData, priv, ownerElement);
    if (!coreFrame) {
        ERR("Could not create child core frame '%s'", name.utf8().data());
        return 0;
    }

    if (!ewk_frame_child_add(frame, coreFrame, name, url, referrer)) {
        ERR("Could not create child frame object '%s'", name.utf8().data());
        return 0;
    }

    // The creation of the frame may have removed itself already.
    if (!coreFrame->page() || !coreFrame->tree() || !coreFrame->tree()->parent())
        return 0;

    smartData->changed.frame_rect = true;
    _ewk_view_smart_changed(smartData);

    return coreFrame.release();
}

WTF::PassRefPtr<WebCore::Widget> ewk_view_plugin_create(Evas_Object* ewkView, Evas_Object* frame, const WebCore::IntSize& pluginSize, WebCore::HTMLPlugInElement* element, const WebCore::KURL& url, const WTF::Vector<WTF::String>& paramNames, const WTF::Vector<WTF::String>& paramValues, const WTF::String& mimeType, bool loadManually)
{
    DBG("ewkView=%p, frame=%p, size=%dx%d, element=%p, url=%s, mimeType=%s",
        ewkView, frame, pluginSize.width(), pluginSize.height(), element,
        url.string().utf8().data(), mimeType.utf8().data());

    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    smartData->changed.frame_rect = true;
    _ewk_view_smart_changed(smartData);

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
void ewk_view_popup_new(Evas_Object* ewkView, WebCore::PopupMenuClient* client, int selected, const WebCore::IntRect& rect)
{
    INF("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    if (priv->popup.menuClient)
        ewk_view_popup_destroy(ewkView);

    priv->popup.menuClient = client;

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
    evas_object_smart_callback_call(ewkView, "popup,create", &priv->popup.menu);
}

Eina_Bool ewk_view_popup_destroy(Evas_Object* ewkView)
{
    INF("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    if (!priv->popup.menuClient)
        return false;

    evas_object_smart_callback_call(ewkView, "popup,willdelete", &priv->popup.menu);

    void* itemv;
    EINA_LIST_FREE(priv->popup.menu.items, itemv) {
        Ewk_Menu_Item* item = static_cast<Ewk_Menu_Item*>(itemv);
        eina_stringshare_del(item->text);
        free(item);
    }
    priv->popup.menuClient->popupDidHide();
    priv->popup.menuClient = 0;

    return true;
}

void ewk_view_popup_selected_set(Evas_Object* ewkView, int index)
{
    INF("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    EINA_SAFETY_ON_NULL_RETURN(priv->popup.menuClient);

    priv->popup.menuClient->valueChanged(index);
}

/**
 * @internal
 * Request a download to user.
 *
 * @param ewkView View.
 * @oaram download Ewk_Download struct to be sent.
 *
 * Emits: "download,request" with an Ewk_Download containing the details of the
 * requested download. The download per se must be handled outside of webkit.
 */
void ewk_view_download_request(Evas_Object* ewkView, Ewk_Download* download)
{
    DBG("ewkView=%p", ewkView);
    evas_object_smart_callback_call(ewkView, "download,request", download);
}

/**
 * @internal
 * Reports the JS window object was cleared.
 *
 * @param ewkView view.
 * @param frame the frame.
 */
void ewk_view_js_window_object_clear(Evas_Object* ewkView, Evas_Object* frame)
{
    evas_object_smart_callback_call(ewkView, "js,windowobject,clear", frame);
}

/**
 * @internal
 * Reports the viewport has changed.
 *
 * @param arguments viewport argument.
 *
 * Emits signal: "viewport,changed" with no parameters.
 */
void ewk_view_viewport_attributes_set(Evas_Object* ewkView, const WebCore::ViewportArguments& arguments)
{
    EWK_VIEW_SD_GET(ewkView, smartData);
    EWK_VIEW_PRIV_GET(smartData, priv);

    priv->viewportArguments = arguments;
    evas_object_smart_callback_call(ewkView, "viewport,changed", 0);
}

void ewk_view_viewport_attributes_get(const Evas_Object* ewkView, int* width, int* height, float* initScale, float* maxScale, float* minScale, float* devicePixelRatio, Eina_Bool* userScalable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    WebCore::ViewportAttributes attributes = _ewk_view_viewport_attributes_compute(priv);

    if (width)
        *width = attributes.layoutSize.width();
    if (height)
        *height = attributes.layoutSize.height();
    if (initScale)
        *initScale = attributes.initialScale;
    if (maxScale)
        *maxScale = attributes.maximumScale;
    if (minScale)
        *minScale = attributes.minimumScale;
    if (devicePixelRatio)
        *devicePixelRatio = attributes.devicePixelRatio;
    if (userScalable)
        *userScalable = static_cast<bool>(attributes.userScalable);
}

Eina_Bool ewk_view_zoom_range_set(Evas_Object* ewkView, float minScale, float maxScale)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    if (maxScale < minScale) {
        WRN("min_scale is larger than max_scale");
        return false;
    }

    priv->settings.zoomRange.minScale = minScale;
    priv->settings.zoomRange.maxScale = maxScale;

    return true;
}

float ewk_view_zoom_range_min_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1.0);

    return priv->settings.zoomRange.minScale;
}

float ewk_view_zoom_range_max_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1.0);

    return priv->settings.zoomRange.maxScale;
}

Eina_Bool ewk_view_user_scalable_set(Evas_Object* ewkView, Eina_Bool userScalable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->settings.zoomRange.userScalable = userScalable;

    return true;
}

Eina_Bool ewk_view_user_scalable_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    return priv->settings.zoomRange.userScalable;
}

float ewk_view_device_pixel_ratio_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1.0);

    return priv->settings.devicePixelRatio;
}

void ewk_view_did_first_visually_nonempty_layout(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    if (!priv->flags.viewCleared) {
        ewk_view_frame_main_cleared(ewkView);
        ewk_view_enable_render(ewkView);
        priv->flags.viewCleared = true;
    }
}

/**
 * @internal
 * Dispatch finished loading.
 *
 * @param ewkView view.
 */
void ewk_view_dispatch_did_finish_loading(Evas_Object* ewkView)
{
    /* If we reach this point and rendering is still disabled, WebCore will not
     * trigger the didFirstVisuallyNonEmptyLayout signal anymore. So, we
     * forcefully re-enable the rendering.
     */
    ewk_view_did_first_visually_nonempty_layout(ewkView);
}

void ewk_view_transition_to_commited_for_newpage(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    ewk_view_disable_render(ewkView);
    priv->flags.viewCleared = false;
}


/**
 * @internal
 * Reports a requeset will be loaded. It's client responsibility to decide if
 * request would be used. If @return is true, loader will try to load. Else,
 * Loader ignore action of request.
 *
 * @param ewkView View to load
 * @param request Request which contain url to navigate
 */
bool ewk_view_navigation_policy_decision(Evas_Object* ewkView, Ewk_Frame_Resource_Request* request)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, true);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, true);

    if (!smartData->api->navigation_policy_decision)
        return true;

    return smartData->api->navigation_policy_decision(smartData, request);
}

Eina_Bool ewk_view_js_object_add(Evas_Object* ewkView, Ewk_JS_Object* object, const char* objectName)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    if (object->view) // object has already been added to another ewk_view
        return false;
    object->name = eina_stringshare_add(objectName);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    JSC::JSLock lock(JSC::SilenceAssertionsOnly);
    WebCore::JSDOMWindow* window = toJSDOMWindow(priv->mainFrame, WebCore::mainThreadNormalWorld());
    JSC::Bindings::RootObject* root;
    root = priv->mainFrame->script()->bindingRootObject();

    if (!window) {
        ERR("Warning: couldn't get window object");
        return false;
    }

    JSC::ExecState* executeState = window->globalExec();

    object->view = ewkView;
    JSC::JSObject* runtimeObject = (JSC::JSObject*)JSC::Bindings::CInstance::create((NPObject*)object, root)->createRuntimeObject(executeState);
    JSC::Identifier id = JSC::Identifier(executeState, objectName);

    JSC::PutPropertySlot slot;
    window->methodTable()->put(window, executeState, id, runtimeObject, slot);
    return true;
#else
    return false;
#endif // ENABLE(NETSCAPE_PLUGIN_API)
}

/**
 * @internal
 * Reports that the contents have resized. The ewk_view calls contents_resize,
 * which can be reimplemented as needed.
 *
 * @param ewkView view.
 * @param width new content width.
 * @param height new content height.
 */
void ewk_view_contents_size_changed(Evas_Object* ewkView, int width, int height)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->contents_resize);

    if (!smartData->api->contents_resize(smartData, width, height))
        ERR("failed to resize contents to %dx%d", width, height);
}

/**
 * @internal
 * Gets page size from frameview.
 *
 * @param ewkView view.
 *
 * @return page size.
 */
WebCore::FloatRect ewk_view_page_rect_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET(ewkView, smartData);
    EWK_VIEW_PRIV_GET(smartData, priv);

    WebCore::Frame* main_frame = priv->page->mainFrame();
    return main_frame->view()->frameRect();
}

#if ENABLE(TOUCH_EVENTS)
void ewk_view_need_touch_events_set(Evas_Object* ewkView, bool needed)
{
    EWK_VIEW_SD_GET(ewkView, smartData);
    EWK_VIEW_PRIV_GET(smartData, priv);

    priv->flags.needTouchEvents = needed;
}

bool ewk_view_need_touch_events_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->flags.needTouchEvents;
}
#endif

Eina_Bool ewk_view_mode_set(Evas_Object* ewkView, Ewk_View_Mode viewMode)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    switch (viewMode) {
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
        return false;
    }

    return true;
}

Ewk_View_Mode ewk_view_mode_get(const Evas_Object* ewkView)
{
    Ewk_View_Mode mode = EWK_VIEW_MODE_WINDOWED;
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, mode);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, mode);

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
 * @param ewkView View.
 *
 * Emits signal: "editorclientselection,changed" with no parameters.
 */
void ewk_view_editor_client_selection_changed(Evas_Object* ewkView)
{
    evas_object_smart_callback_call(ewkView, "editorclient,selection,changed", 0);
}

/**
 * @internal
 * Reports to the view that editor client's contents were changed.
 *
 * @param ewkView View.
 *
 * Emits signal: "editorclient,contents,changed" with no parameters.
 */
void ewk_view_editor_client_contents_changed(Evas_Object* ewkView)
{
    evas_object_smart_callback_call(ewkView, "editorclient,contents,changed", 0);
}

Eina_Bool ewk_view_visibility_state_set(Evas_Object* ewkView, Ewk_Page_Visibility_State pageVisibleState, Eina_Bool initialState)
{
#if ENABLE(PAGE_VISIBILITY_API)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, EINA_FALSE);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, EINA_FALSE);

    switch (pageVisibleState) {
    case EWK_PAGE_VISIBILITY_STATE_VISIBLE:
        priv->page->setVisibilityState(WebCore::PageVisibilityStateVisible, initialState);
        break;
    case EWK_PAGE_VISIBILITY_STATE_HIDDEN:
        priv->page->setVisibilityState(WebCore::PageVisibilityStateHidden, initialState);
        break;
    case EWK_PAGE_VISIBILITY_STATE_PRERENDER:
        priv->page->setVisibilityState(WebCore::PageVisibilityStatePrerender, initialState);
        break;
    default:
        return EINA_FALSE;
    }

    return EINA_TRUE;
#else
    DBG("PAGE_VISIBILITY_API is disabled.");
    return EINA_FALSE;
#endif
}

Ewk_Page_Visibility_State ewk_view_visibility_state_get(const Evas_Object* ewkView)
{
#if ENABLE(PAGE_VISIBILITY_API)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, EWK_PAGE_VISIBILITY_STATE_VISIBLE);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, EWK_PAGE_VISIBILITY_STATE_VISIBLE);

    switch (priv->page->visibilityState()) {
    case WebCore::PageVisibilityStateVisible:
        return EWK_PAGE_VISIBILITY_STATE_VISIBLE;
    case WebCore::PageVisibilityStateHidden:
        return EWK_PAGE_VISIBILITY_STATE_HIDDEN;
    case WebCore::PageVisibilityStatePrerender:
        return EWK_PAGE_VISIBILITY_STATE_PRERENDER;
    default:
        return EWK_PAGE_VISIBILITY_STATE_VISIBLE;
    }
#else
    DBG("PAGE_VISIBILITY_API is disabled.");
    return EWK_PAGE_VISIBILITY_STATE_VISIBLE;
#endif
}

namespace EWKPrivate {

WebCore::Page *corePage(const Evas_Object *ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->page;
}

} // namespace EWKPrivate
