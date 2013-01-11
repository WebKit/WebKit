/*
   Copyright (C) 2011 Samsung Electronics
   Copyright (C) 2012 Intel Corporation. All rights reserved.

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

#include "config.h"
#include "ewk_view.h"

#include "EwkViewImpl.h"
#include "FindClientEfl.h"
#include "FormClientEfl.h"
#include "InputMethodContextEfl.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "PageClientBase.h"
#include "PageLoadClientEfl.h"
#include "PagePolicyClientEfl.h"
#include "PageUIClientEfl.h"
#include "ResourceLoadClientEfl.h"
#include "WKAPICast.h"
#include "WKEinaSharedString.h"
#include "WKFindOptions.h"
#include "WKRetainPtr.h"
#include "WKString.h"
#include "WebContext.h"
#include "WebFullScreenManagerProxy.h"
#include "WebPageGroup.h"
#include "WebPreferences.h"
#include "ewk_back_forward_list_private.h"
#include "ewk_context.h"
#include "ewk_context_private.h"
#include "ewk_favicon_database_private.h"
#include "ewk_intent_private.h"
#include "ewk_private.h"
#include "ewk_settings_private.h"
#include "ewk_view_private.h"
#include <Ecore_Evas.h>
#include <WebKit2/WKPageGroup.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>

#if ENABLE(INSPECTOR)
#include "WebInspectorProxy.h"
#endif

#if USE(TILED_BACKING_STORE)
#include "PageViewportController.h"
#include "PageViewportControllerClientEfl.h"
#endif

using namespace WebKit;
using namespace WebCore;

static const char EWK_VIEW_TYPE_STR[] = "EWK2_View";

#define EWK_VIEW_TYPE_CHECK(ewkView, result)                                   \
    bool result = true;                                                        \
    do {                                                                       \
        if (!ewkView) {                                                        \
            EINA_LOG_CRIT("null is not a ewk_view");                           \
            result = false;                                                    \
            break;                                                             \
        }                                                                      \
        const char* _tmp_otype = evas_object_type_get(ewkView);                \
        const Evas_Smart* _tmp_s = evas_object_smart_smart_get(ewkView);       \
        if (EINA_UNLIKELY(!_tmp_s)) {                                          \
            EINA_LOG_CRIT                                                      \
                ("%p (%s) is not a smart object!",                             \
                 ewkView, _tmp_otype ? _tmp_otype : "(null)");                 \
            result = false;                                                    \
            break;                                                             \
        }                                                                      \
        const Evas_Smart_Class* _tmp_sc = evas_smart_class_get(_tmp_s);        \
        if (EINA_UNLIKELY(!_tmp_sc)) {                                         \
            EINA_LOG_CRIT                                                      \
                ("%p (%s) is not a smart object!",                             \
                 ewkView, _tmp_otype ? _tmp_otype : "(null)");                 \
            result = false;                                                    \
            break;                                                             \
        }                                                                      \
        if (EINA_UNLIKELY(_tmp_sc->data != EWK_VIEW_TYPE_STR)) {               \
            EINA_LOG_CRIT                                                      \
                ("%p (%s) is not of an ewk_view (need %p, got %p)!",           \
                 ewkView, _tmp_otype ? _tmp_otype : "(null)",                  \
                 EWK_VIEW_TYPE_STR, _tmp_sc->data);                            \
            result = false;                                                    \
        }                                                                      \
    } while (0)

#define EWK_VIEW_SD_GET(ewkView, smartData)                                    \
    EWK_VIEW_TYPE_CHECK(ewkView, _tmp_result);                                 \
    Ewk_View_Smart_Data* smartData = 0;                                        \
    if (_tmp_result)                                                           \
        smartData = (Ewk_View_Smart_Data*)evas_object_smart_data_get(ewkView)

#define EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, ...)                     \
    EWK_VIEW_SD_GET(ewkView, smartData);                                       \
    do {                                                                       \
        if (!smartData) {                                                      \
            EINA_LOG_CRIT("no smart data for object %p (%s)",                  \
                     ewkView, evas_object_type_get(ewkView));                  \
            return __VA_ARGS__;                                                \
        }                                                                      \
    } while (0)

#define EWK_VIEW_IMPL_GET_BY_SD_OR_RETURN(smartData, impl, ...)                \
    if (!smartData) {                                                          \
        EINA_LOG_CRIT("smart data is null");                                   \
        return __VA_ARGS__;                                                    \
    }                                                                          \
    EwkViewImpl* impl = smartData->priv;                                       \
    do {                                                                       \
        if (!impl) {                                                           \
            EINA_LOG_CRIT("no private data for object %p (%s)",                \
                smartData->self, evas_object_type_get(smartData->self));       \
            return __VA_ARGS__;                                                \
        }                                                                      \
    } while (0)

#define EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, ...)                        \
    EwkViewImpl* impl = 0;                                                     \
    do {                                                                       \
        EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, __VA_ARGS__);            \
        impl = smartData->priv;                                                \
        if (!impl) {                                                           \
            EINA_LOG_CRIT("no private data for object %p (%s)",                \
                smartData->self, evas_object_type_get(smartData->self));       \
            return __VA_ARGS__;                                                \
        }                                                                      \
    } while (0)

static void _ewk_view_smart_changed(Ewk_View_Smart_Data* smartData)
{
    if (smartData->changed.any)
        return;
    smartData->changed.any = true;
    evas_object_smart_changed(smartData->self);
}

// Default Event Handling.
static Eina_Bool _ewk_view_smart_focus_in(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_IMPL_GET_BY_SD_OR_RETURN(smartData, impl, false);

    impl->page()->viewStateDidChange(WebPageProxy::ViewIsFocused | WebPageProxy::ViewWindowIsActive);
    return true;
}

static Eina_Bool _ewk_view_smart_focus_out(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_IMPL_GET_BY_SD_OR_RETURN(smartData, impl, false);

    impl->page()->viewStateDidChange(WebPageProxy::ViewIsFocused | WebPageProxy::ViewWindowIsActive);
    return true;
}

static Eina_Bool _ewk_view_smart_mouse_wheel(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Wheel* wheelEvent)
{
    EWK_VIEW_IMPL_GET_BY_SD_OR_RETURN(smartData, impl, false);

    impl->page()->handleWheelEvent(NativeWebWheelEvent(wheelEvent, impl->transformFromScene(), impl->transformToScreen()));
    return true;
}

static Eina_Bool _ewk_view_smart_mouse_down(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Down* downEvent)
{
    EWK_VIEW_IMPL_GET_BY_SD_OR_RETURN(smartData, impl, false);

    impl->page()->handleMouseEvent(NativeWebMouseEvent(downEvent, impl->transformFromScene(), impl->transformToScreen()));
    return true;
}

static Eina_Bool _ewk_view_smart_mouse_up(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Up* upEvent)
{
    EWK_VIEW_IMPL_GET_BY_SD_OR_RETURN(smartData, impl, false);

    impl->page()->handleMouseEvent(NativeWebMouseEvent(upEvent, impl->transformFromScene(), impl->transformToScreen()));

    InputMethodContextEfl* inputMethodContext = impl->inputMethodContext();
    if (inputMethodContext)
        inputMethodContext->handleMouseUpEvent(upEvent);

    return true;
}

static Eina_Bool _ewk_view_smart_mouse_move(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Move* moveEvent)
{
    EWK_VIEW_IMPL_GET_BY_SD_OR_RETURN(smartData, impl, false);

    impl->page()->handleMouseEvent(NativeWebMouseEvent(moveEvent, impl->transformFromScene(), impl->transformToScreen()));
    return true;
}

static Eina_Bool _ewk_view_smart_key_down(Ewk_View_Smart_Data* smartData, const Evas_Event_Key_Down* downEvent)
{
    EWK_VIEW_IMPL_GET_BY_SD_OR_RETURN(smartData, impl, false);

    bool isFiltered = false;
    InputMethodContextEfl* inputMethodContext = impl->inputMethodContext();
    if (inputMethodContext)
        inputMethodContext->handleKeyDownEvent(downEvent, &isFiltered);

    impl->page()->handleKeyboardEvent(NativeWebKeyboardEvent(downEvent, isFiltered));
    return true;
}

static Eina_Bool _ewk_view_smart_key_up(Ewk_View_Smart_Data* smartData, const Evas_Event_Key_Up* upEvent)
{
    EWK_VIEW_IMPL_GET_BY_SD_OR_RETURN(smartData, impl, false);

    impl->page()->handleKeyboardEvent(NativeWebKeyboardEvent(upEvent));
    return true;
}

// Event Handling.
static void _ewk_view_on_focus_in(void* data, Evas*, Evas_Object*, void* /*eventInfo*/)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->focus_in);
    smartData->api->focus_in(smartData);
}

static void _ewk_view_on_focus_out(void* data, Evas*, Evas_Object*, void* /*eventInfo*/)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->focus_out);
    smartData->api->focus_out(smartData);
}

static void _ewk_view_on_mouse_wheel(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Wheel* wheelEvent = static_cast<Evas_Event_Mouse_Wheel*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_wheel);
    smartData->api->mouse_wheel(smartData, wheelEvent);
}

static void _ewk_view_on_key_down(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Key_Down* downEvent = static_cast<Evas_Event_Key_Down*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->key_down);
    smartData->api->key_down(smartData, downEvent);
}

static void _ewk_view_on_key_up(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Key_Up* upEvent = static_cast<Evas_Event_Key_Up*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->key_up);
    smartData->api->key_up(smartData, upEvent);
}

static void _ewk_view_on_show(void* data, Evas*, Evas_Object*, void* /*eventInfo*/)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EWK_VIEW_IMPL_GET_BY_SD_OR_RETURN(smartData, impl);
    impl->page()->viewStateDidChange(WebPageProxy::ViewIsVisible);
}

static void _ewk_view_on_hide(void* data, Evas*, Evas_Object*, void* /*eventInfo*/)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EWK_VIEW_IMPL_GET_BY_SD_OR_RETURN(smartData, impl);

    // This call may look wrong, but we really need to pass ViewIsVisible here.
    // viewStateDidChange() itself is responsible for actually setting the visibility to Visible or Hidden
    // depending on what WebPageProxy::isViewVisible() returns, this simply triggers the process.
    impl->page()->viewStateDidChange(WebPageProxy::ViewIsVisible);
}

static Evas_Smart_Class g_parentSmartClass = EVAS_SMART_CLASS_INIT_NULL;

static void _ewk_view_smart_add(Evas_Object* ewkView)
{
    const Evas_Smart* smart = evas_object_smart_smart_get(ewkView);
    const Evas_Smart_Class* smartClass = evas_smart_class_get(smart);
    const Ewk_View_Smart_Class* api = reinterpret_cast<const Ewk_View_Smart_Class*>(smartClass);
    EWK_VIEW_SD_GET(ewkView, smartData);

    if (!smartData) {
        smartData = static_cast<Ewk_View_Smart_Data*>(calloc(1, sizeof(Ewk_View_Smart_Data)));
        if (!smartData) {
            EINA_LOG_CRIT("could not allocate Ewk_View_Smart_Data");
            return;
        }
        evas_object_smart_data_set(ewkView, smartData);
    }

    smartData->self = ewkView;
    smartData->api = api;

    g_parentSmartClass.add(ewkView);

    smartData->priv = 0;

    // Create evas_object_image to draw web contents.
    smartData->image = evas_object_image_add(smartData->base.evas);
    evas_object_image_alpha_set(smartData->image, false);
    evas_object_image_filled_set(smartData->image, true);
    evas_object_smart_member_add(smartData->image, ewkView);
    evas_object_show(smartData->image);

#define CONNECT(s, c) evas_object_event_callback_add(ewkView, s, c, smartData)
    CONNECT(EVAS_CALLBACK_FOCUS_IN, _ewk_view_on_focus_in);
    CONNECT(EVAS_CALLBACK_FOCUS_OUT, _ewk_view_on_focus_out);
    CONNECT(EVAS_CALLBACK_MOUSE_WHEEL, _ewk_view_on_mouse_wheel);
    CONNECT(EVAS_CALLBACK_KEY_DOWN, _ewk_view_on_key_down);
    CONNECT(EVAS_CALLBACK_KEY_UP, _ewk_view_on_key_up);
    CONNECT(EVAS_CALLBACK_SHOW, _ewk_view_on_show);
    CONNECT(EVAS_CALLBACK_HIDE, _ewk_view_on_hide);
#undef CONNECT
}

static void _ewk_view_smart_del(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET(ewkView, smartData);
    if (smartData)
        delete smartData->priv;

    g_parentSmartClass.del(ewkView);
}

static void _ewk_view_smart_resize(Evas_Object* ewkView, Evas_Coord width, Evas_Coord height)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);

    evas_object_resize(smartData->image, width, height);
    evas_object_image_size_set(smartData->image, width, height);
    evas_object_image_fill_set(smartData->image, 0, 0, width, height);

    smartData->changed.size = true;
    _ewk_view_smart_changed(smartData);
}

static void _ewk_view_smart_move(Evas_Object* ewkView, Evas_Coord /*x*/, Evas_Coord /*y*/)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);

    smartData->changed.position = true;
    _ewk_view_smart_changed(smartData);
}

static void _ewk_view_smart_calculate(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_IMPL_GET_BY_SD_OR_RETURN(smartData, impl);

    smartData->changed.any = false;

    Evas_Coord x, y, width, height;
    evas_object_geometry_get(ewkView, &x, &y, &width, &height);

    if (smartData->changed.position) {
        smartData->changed.position = false;
        smartData->view.x = x;
        smartData->view.y = y;
        evas_object_move(smartData->image, x, y);
    }

    if (smartData->changed.size) {
        smartData->changed.size = false;
        smartData->view.w = width;
        smartData->view.h = height;

        if (impl->page()->drawingArea())
            impl->page()->drawingArea()->setSize(IntSize(width, height), IntSize());

#if USE(ACCELERATED_COMPOSITING)
        impl->setNeedsSurfaceResize();
#endif
#if USE(TILED_BACKING_STORE)
        impl->pageClient()->updateViewportSize();
#endif
    }
}

static void _ewk_view_smart_show(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);

    if (evas_object_clipees_get(smartData->base.clipper))
        evas_object_show(smartData->base.clipper);
    evas_object_show(smartData->image);
}

static void _ewk_view_smart_hide(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);

    evas_object_hide(smartData->base.clipper);
    evas_object_hide(smartData->image);
}

static void _ewk_view_smart_color_set(Evas_Object* ewkView, int red, int green, int blue, int alpha)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_IMPL_GET_BY_SD_OR_RETURN(smartData, impl);

    if (alpha < 0)
        alpha = 0;
    else if (alpha > 255)
        alpha = 255;

#define CHECK_COLOR(color, alpha) \
    if (color < 0)                \
        color = 0;                \
    else if (color > alpha)       \
        color = alpha;
    CHECK_COLOR(red, alpha);
    CHECK_COLOR(green, alpha);
    CHECK_COLOR(blue, alpha);
#undef CHECK_COLOR

    evas_object_image_alpha_set(smartData->image, alpha < 255);
    impl->page()->setDrawsBackground(red || green || blue);
    impl->page()->setDrawsTransparentBackground(alpha < 255);

    g_parentSmartClass.color_set(ewkView, red, green, blue, alpha);
}

Eina_Bool ewk_view_smart_class_set(Ewk_View_Smart_Class* api)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(api, false);

    if (api->version != EWK_VIEW_SMART_CLASS_VERSION) {
        EINA_LOG_CRIT("Ewk_View_Smart_Class %p is version %lu while %lu was expected.",
             api, api->version, EWK_VIEW_SMART_CLASS_VERSION);
        return false;
    }

    if (EINA_UNLIKELY(!g_parentSmartClass.add))
        evas_object_smart_clipped_smart_set(&g_parentSmartClass);

    evas_object_smart_clipped_smart_set(&api->sc);

    // Set Evas_Smart_Class functions.
    api->sc.add = _ewk_view_smart_add;
    api->sc.del = _ewk_view_smart_del;
    api->sc.move = _ewk_view_smart_move;
    api->sc.resize = _ewk_view_smart_resize;
    api->sc.show = _ewk_view_smart_show;
    api->sc.hide = _ewk_view_smart_hide;
    api->sc.color_set = _ewk_view_smart_color_set;
    api->sc.calculate = _ewk_view_smart_calculate;
    api->sc.data = EWK_VIEW_TYPE_STR; // It is used by type checking.

    // Set Ewk_View_Smart_Class functions.
    api->focus_in = _ewk_view_smart_focus_in;
    api->focus_out = _ewk_view_smart_focus_out;
    api->mouse_wheel = _ewk_view_smart_mouse_wheel;
    api->mouse_down = _ewk_view_smart_mouse_down;
    api->mouse_up = _ewk_view_smart_mouse_up;
    api->mouse_move = _ewk_view_smart_mouse_move;
    api->key_down = _ewk_view_smart_key_down;
    api->key_up = _ewk_view_smart_key_up;

    return true;
}

static inline Evas_Smart* createEwkViewSmartClass(void)
{
    static Ewk_View_Smart_Class api = EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION("Ewk_View");
    static Evas_Smart* smart = 0;

    if (EINA_UNLIKELY(!smart)) {
        ewk_view_smart_class_set(&api);
        smart = evas_smart_class_new(&api.sc);
    }

    return smart;
}

static inline Evas_Object* createEwkView(Evas* canvas, Evas_Smart* smart, PassRefPtr<EwkContext> context, WKPageGroupRef pageGroupRef = 0, EwkViewImpl::ViewBehavior behavior = EwkViewImpl::DefaultBehavior)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(canvas, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smart, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(context, 0);

    Evas_Object* ewkView = evas_object_smart_add(canvas, smart);
    EINA_SAFETY_ON_NULL_RETURN_VAL(ewkView, 0);

    EWK_VIEW_SD_GET(ewkView, smartData);
    if (!smartData) {
        evas_object_del(ewkView);
        return 0;
    }

    ASSERT(!smartData->priv);

    // Default WebPageGroup is created in WebContext constructor if the pageGroupRef is 0,
    // so we do not need to create it here.
    smartData->priv = new EwkViewImpl(ewkView, context, toImpl(pageGroupRef), behavior);
    return ewkView;
}

/**
 * @internal
 * Constructs a ewk_view Evas_Object with WKType parameters.
 */
Evas_Object* ewk_view_base_add(Evas* canvas, WKContextRef contextRef, WKPageGroupRef pageGroupRef, EwkViewImpl::ViewBehavior behavior)
{
    return createEwkView(canvas, createEwkViewSmartClass(), contextRef ? EwkContext::create(toImpl(contextRef)) : EwkContext::defaultContext(), pageGroupRef, behavior);
}

Evas_Object* ewk_view_smart_add(Evas* canvas, Evas_Smart* smart, Ewk_Context* context)
{
    return createEwkView(canvas, smart, ewk_object_cast<EwkContext*>(context));
}

Evas_Object* ewk_view_add_with_context(Evas* canvas, Ewk_Context* context)
{
    return ewk_view_smart_add(canvas, createEwkViewSmartClass(), ewk_object_cast<EwkContext*>(context));
}

Evas_Object* ewk_view_add(Evas* canvas)
{
    return ewk_view_add_with_context(canvas, ewk_context_default_get());
}

Ewk_Context* ewk_view_context_get(const Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, 0);

    return impl->ewkContext();
}

Eina_Bool ewk_view_url_set(Evas_Object* ewkView, const char* url)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(url, false);

    impl->page()->loadURL(url);
    impl->informURLChange();

    return true;
}

const char* ewk_view_url_get(const Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, 0);

    return impl->url();
}

const char *ewk_view_icon_url_get(const Evas_Object *ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, 0);

    return impl->faviconURL();
}

Eina_Bool ewk_view_reload(Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    impl->page()->reload(/*reloadFromOrigin*/ false);
    impl->informURLChange();

    return true;
}

Eina_Bool ewk_view_reload_bypass_cache(Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    impl->page()->reload(/*reloadFromOrigin*/ true);
    impl->informURLChange();

    return true;
}

Eina_Bool ewk_view_stop(Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    impl->page()->stopLoading();

    return true;
}

Ewk_Settings* ewk_view_settings_get(const Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, 0);

    return impl->settings();
}

const char* ewk_view_title_get(const Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, 0);

    return impl->title();
}

double ewk_view_load_progress_get(const Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, -1.0);

    return impl->page()->estimatedProgress();
}

Eina_Bool ewk_view_scale_set(Evas_Object* ewkView, double scaleFactor, int x, int y)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    impl->page()->scalePage(scaleFactor, IntPoint(x, y));
    return true;
}

double ewk_view_scale_get(const Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, -1);

    return impl->page()->pageScaleFactor();
}

Eina_Bool ewk_view_device_pixel_ratio_set(Evas_Object* ewkView, float ratio)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    impl->setDeviceScaleFactor(ratio);

    return true;
}

float ewk_view_device_pixel_ratio_get(const Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, -1.0);

    return impl->page()->deviceScaleFactor();
}

void ewk_view_theme_set(Evas_Object* ewkView, const char* path)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl);

    impl->setThemePath(path);
}

const char* ewk_view_theme_get(const Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, 0);

    return impl->themePath();
}

Eina_Bool ewk_view_back(Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    WebPageProxy* page = impl->page();
    if (page->canGoBack()) {
        page->goBack();
        return true;
    }

    return false;
}

Eina_Bool ewk_view_forward(Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    WebPageProxy* page = impl->page();
    if (page->canGoForward()) {
        page->goForward();
        return true;
    }

    return false;
}

Eina_Bool ewk_view_intent_deliver(Evas_Object* ewkView, Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);
    EwkIntent* intentImpl = ewk_object_cast<EwkIntent*>(intent);
    EINA_SAFETY_ON_NULL_RETURN_VAL(intentImpl, false);

    WebPageProxy* page = impl->page();
    page->deliverIntentToFrame(page->mainFrame(), intentImpl->webIntentData());

    return true;
#else
    UNUSED_PARAM(ewkView);
    UNUSED_PARAM(intent);
    return false;
#endif
}

Eina_Bool ewk_view_back_possible(Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    return impl->page()->canGoBack();
}

Eina_Bool ewk_view_forward_possible(Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    return impl->page()->canGoForward();
}

Ewk_Back_Forward_List* ewk_view_back_forward_list_get(const Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, 0);

    return impl->backForwardList();
}

Eina_Bool ewk_view_html_string_load(Evas_Object* ewkView, const char* html, const char* baseUrl, const char* unreachableUrl)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(html, false);

    if (unreachableUrl && *unreachableUrl)
        impl->page()->loadAlternateHTMLString(String::fromUTF8(html), baseUrl ? String::fromUTF8(baseUrl) : "", String::fromUTF8(unreachableUrl));
    else
        impl->page()->loadHTMLString(String::fromUTF8(html), baseUrl ? String::fromUTF8(baseUrl) : "");

    impl->informURLChange();

    return true;
}

const char* ewk_view_custom_encoding_get(const Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, 0);

    return impl->customTextEncodingName();
}

Eina_Bool ewk_view_custom_encoding_set(Evas_Object* ewkView, const char* encoding)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    impl->setCustomTextEncodingName(encoding ? encoding : String());

    return true;
}

// EwkFindOptions should be matched up orders with WkFindOptions.
COMPILE_ASSERT_MATCHING_ENUM(EWK_FIND_OPTIONS_CASE_INSENSITIVE, kWKFindOptionsCaseInsensitive);
COMPILE_ASSERT_MATCHING_ENUM(EWK_FIND_OPTIONS_AT_WORD_STARTS, kWKFindOptionsAtWordStarts);
COMPILE_ASSERT_MATCHING_ENUM(EWK_FIND_OPTIONS_TREAT_MEDIAL_CAPITAL_AS_WORD_START, kWKFindOptionsTreatMedialCapitalAsWordStart);
COMPILE_ASSERT_MATCHING_ENUM(EWK_FIND_OPTIONS_BACKWARDS, kWKFindOptionsBackwards);
COMPILE_ASSERT_MATCHING_ENUM(EWK_FIND_OPTIONS_WRAP_AROUND, kWKFindOptionsWrapAround);
COMPILE_ASSERT_MATCHING_ENUM(EWK_FIND_OPTIONS_SHOW_OVERLAY, kWKFindOptionsShowOverlay);
COMPILE_ASSERT_MATCHING_ENUM(EWK_FIND_OPTIONS_SHOW_FIND_INDICATOR, kWKFindOptionsShowFindIndicator);
COMPILE_ASSERT_MATCHING_ENUM(EWK_FIND_OPTIONS_SHOW_HIGHLIGHT, kWKFindOptionsShowHighlight);

Eina_Bool ewk_view_text_find(Evas_Object* ewkView, const char* text, Ewk_Find_Options options, unsigned maxMatchCount)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(text, false);

    impl->page()->findString(String::fromUTF8(text), static_cast<WebKit::FindOptions>(options), maxMatchCount);

    return true;
}

Eina_Bool ewk_view_text_find_highlight_clear(Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    impl->page()->hideFindUI();

    return true;
}

Eina_Bool ewk_view_text_matches_count(Evas_Object* ewkView, const char* text, Ewk_Find_Options options, unsigned maxMatchCount)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(text, false);

    impl->page()->countStringMatches(String::fromUTF8(text), static_cast<WebKit::FindOptions>(options), maxMatchCount);

    return true;
}

Eina_Bool ewk_view_mouse_events_enabled_set(Evas_Object* ewkView, Eina_Bool enabled)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    impl->setMouseEventsEnabled(!!enabled);

    return true;
}

Eina_Bool ewk_view_mouse_events_enabled_get(const Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    return impl->mouseEventsEnabled();
}

Eina_Bool ewk_view_feed_touch_event(Evas_Object* ewkView, Ewk_Touch_Event_Type type, const Eina_List* points, const Evas_Modifier* modifiers)
{
#if ENABLE(TOUCH_EVENTS)
    EINA_SAFETY_ON_NULL_RETURN_VAL(points, false);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_BY_SD_OR_RETURN(smartData, impl, false);

    impl->page()->handleTouchEvent(NativeWebTouchEvent(type, points, modifiers, impl->transformFromScene(), impl->transformToScreen(), ecore_time_get()));

    return true;
#else
    UNUSED_PARAM(ewkView);
    UNUSED_PARAM(type);
    UNUSED_PARAM(points);
    UNUSED_PARAM(modifiers);
    return false;
#endif
}

Eina_Bool ewk_view_touch_events_enabled_set(Evas_Object* ewkView, Eina_Bool enabled)
{
#if ENABLE(TOUCH_EVENTS)
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    impl->setTouchEventsEnabled(!!enabled);

    return true;
#else
    UNUSED_PARAM(ewkView);
    UNUSED_PARAM(enabled);
    return false;
#endif
}

Eina_Bool ewk_view_touch_events_enabled_get(const Evas_Object* ewkView)
{
#if ENABLE(TOUCH_EVENTS)
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    return impl->touchEventsEnabled();
#else
    UNUSED_PARAM(ewkView);
    return false;
#endif
}

Eina_Bool ewk_view_inspector_show(Evas_Object* ewkView)
{
#if ENABLE(INSPECTOR)
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    WebInspectorProxy* inspector = impl->page()->inspector();
    if (inspector)
        inspector->show();

    return true;
#else
    UNUSED_PARAM(ewkView);
    return false;
#endif
}

Eina_Bool ewk_view_inspector_close(Evas_Object* ewkView)
{
#if ENABLE(INSPECTOR)
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    WebInspectorProxy* inspector = impl->page()->inspector();
    if (inspector)
        inspector->close();

    return true;
#else
    UNUSED_PARAM(ewkView);
    return false;
#endif
}

// Ewk_Pagination_Mode should be matched up orders with WebCore::Pagination::Mode.
COMPILE_ASSERT_MATCHING_ENUM(EWK_PAGINATION_MODE_UNPAGINATED, WebCore::Pagination::Unpaginated);
COMPILE_ASSERT_MATCHING_ENUM(EWK_PAGINATION_MODE_LEFT_TO_RIGHT, WebCore::Pagination::LeftToRightPaginated);
COMPILE_ASSERT_MATCHING_ENUM(EWK_PAGINATION_MODE_RIGHT_TO_LEFT, WebCore::Pagination::RightToLeftPaginated);
COMPILE_ASSERT_MATCHING_ENUM(EWK_PAGINATION_MODE_TOP_TO_BOTTOM, WebCore::Pagination::TopToBottomPaginated);
COMPILE_ASSERT_MATCHING_ENUM(EWK_PAGINATION_MODE_BOTTOM_TO_TOP, WebCore::Pagination::BottomToTopPaginated);

Eina_Bool ewk_view_pagination_mode_set(Evas_Object* ewkView, Ewk_Pagination_Mode mode)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);
    
    impl->page()->setPaginationMode(static_cast<WebCore::Pagination::Mode>(mode));

    return true;
}

Ewk_Pagination_Mode ewk_view_pagination_mode_get(const Evas_Object* ewkView)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, EWK_PAGINATION_MODE_INVALID);

    return static_cast<Ewk_Pagination_Mode>(impl->page()->paginationMode());
}

Eina_Bool ewk_view_fullscreen_exit(Evas_Object* ewkView)
{
#if ENABLE(FULLSCREEN_API)
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl, false);

    impl->page()->fullScreenManager()->requestExitFullScreen();

    return true;
#else
    UNUSED_PARAM(ewkView);
    return false;
#endif
}

void ewk_view_draws_page_background_set(Evas_Object *ewkView, Eina_Bool enabled)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(ewkView, impl);

    impl->setDrawsBackground(enabled);
}
