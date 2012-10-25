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
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "PageClientImpl.h"
#include "PageLoadClientEfl.h"
#include "PagePolicyClientEfl.h"
#include "PageUIClientEfl.h"
#include "ResourceLoadClientEfl.h"
#include "WKAPICast.h"
#include "WKColorPickerResultListener.h"
#include "WKEinaSharedString.h"
#include "WKFindOptions.h"
#include "WKRetainPtr.h"
#include "WKString.h"
#include "WebContext.h"
#include "WebPageGroup.h"
#include "WebPopupItem.h"
#include "WebPopupMenuProxyEfl.h"
#include "WebPreferences.h"
#include "ewk_back_forward_list_private.h"
#include "ewk_context.h"
#include "ewk_context_private.h"
#include "ewk_favicon_database_private.h"
#include "ewk_intent_private.h"
#include "ewk_popup_menu_item_private.h"
#include "ewk_private.h"
#include "ewk_settings_private.h"
#include "ewk_view_private.h"
#include <Ecore_Evas.h>
#include <WebKit2/WKPageGroup.h>
#include <wtf/text/CString.h>

#if ENABLE(FULLSCREEN_API)
#include "WebFullScreenManagerProxy.h"
#endif

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

static void _ewk_view_smart_changed(Ewk_View_Smart_Data* smartData)
{
    if (smartData->changed.any)
        return;
    smartData->changed.any = true;
    evas_object_smart_changed(smartData->self);
}

static void _ewk_view_on_favicon_changed(const char* pageURL, void* eventInfo)
{
    Evas_Object* ewkView = static_cast<Evas_Object*>(eventInfo);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl);

    const char* viewURL = ewk_view_url_get(ewkView);
    if (!viewURL || strcasecmp(viewURL, pageURL))
        return;

    impl->informIconChange();
}

// Default Event Handling.
static Eina_Bool _ewk_view_smart_focus_in(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    impl->pageProxy->viewStateDidChange(WebPageProxy::ViewIsFocused | WebPageProxy::ViewWindowIsActive);
    return true;
}

static Eina_Bool _ewk_view_smart_focus_out(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    impl->pageProxy->viewStateDidChange(WebPageProxy::ViewIsFocused | WebPageProxy::ViewWindowIsActive);
    return true;
}

#if USE(TILED_BACKING_STORE)
static Evas_Coord_Point mapToWebContent(Ewk_View_Smart_Data* smartData, Evas_Coord_Point point)
{
    Evas_Coord_Point result;
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, result);

    result.x = (point.x  - smartData->view.x) / impl->pageViewportControllerClient->scaleFactor() + smartData->view.x + impl->pageViewportControllerClient->scrollPosition().x();
    result.y = (point.y - smartData->view.y) / impl->pageViewportControllerClient->scaleFactor() + smartData->view.y + impl->pageViewportControllerClient->scrollPosition().y();
    return result;
}
#endif

static Eina_Bool _ewk_view_smart_mouse_wheel(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Wheel* wheelEvent)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    Evas_Point position = {smartData->view.x, smartData->view.y};
#if USE(TILED_BACKING_STORE)
    Evas_Event_Mouse_Wheel event(*wheelEvent);
    event.canvas = mapToWebContent(smartData, event.canvas);
    impl->pageProxy->handleWheelEvent(NativeWebWheelEvent(&event, &position));
#else
    impl->pageProxy->handleWheelEvent(NativeWebWheelEvent(wheelEvent, &position));
#endif
    return true;
}

static Eina_Bool _ewk_view_smart_mouse_down(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Down* downEvent)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    Evas_Point position = {smartData->view.x, smartData->view.y};
#if USE(TILED_BACKING_STORE)
    Evas_Event_Mouse_Down event(*downEvent);
    event.canvas = mapToWebContent(smartData, event.canvas);
    impl->pageProxy->handleMouseEvent(NativeWebMouseEvent(&event, &position));
#else
    impl->pageProxy->handleMouseEvent(NativeWebMouseEvent(downEvent, &position));
#endif
    return true;
}

static Eina_Bool _ewk_view_smart_mouse_up(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Up* upEvent)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    Evas_Point position = {smartData->view.x, smartData->view.y};
#if USE(TILED_BACKING_STORE)
    Evas_Event_Mouse_Up event(*upEvent);
    event.canvas = mapToWebContent(smartData, event.canvas);
    impl->pageProxy->handleMouseEvent(NativeWebMouseEvent(&event, &position));
#else
    impl->pageProxy->handleMouseEvent(NativeWebMouseEvent(upEvent, &position));
#endif

    if (impl->imfContext)
        ecore_imf_context_reset(impl->imfContext);

    return true;
}

static Eina_Bool _ewk_view_smart_mouse_move(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Move* moveEvent)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    Evas_Point position = {smartData->view.x, smartData->view.y};
#if USE(TILED_BACKING_STORE)
    Evas_Event_Mouse_Move event(*moveEvent);
    event.cur.canvas = mapToWebContent(smartData, event.cur.canvas);
    impl->pageProxy->handleMouseEvent(NativeWebMouseEvent(&event, &position));
#else
    impl->pageProxy->handleMouseEvent(NativeWebMouseEvent(moveEvent, &position));
#endif
    return true;
}

static Eina_Bool _ewk_view_smart_key_down(Ewk_View_Smart_Data* smartData, const Evas_Event_Key_Down* downEvent)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    bool isFiltered = false;
    if (impl->imfContext) {
        Ecore_IMF_Event imfEvent;
        ecore_imf_evas_event_key_down_wrap(const_cast<Evas_Event_Key_Down*>(downEvent), &imfEvent.key_down);

        isFiltered = ecore_imf_context_filter_event(impl->imfContext, ECORE_IMF_EVENT_KEY_DOWN, &imfEvent);
    }

    impl->pageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(downEvent, isFiltered));
    return true;
}

static Eina_Bool _ewk_view_smart_key_up(Ewk_View_Smart_Data* smartData, const Evas_Event_Key_Up* upEvent)
{
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    impl->pageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(upEvent));
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

static void _ewk_view_on_mouse_down(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Down* downEvent = static_cast<Evas_Event_Mouse_Down*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_down);
    smartData->api->mouse_down(smartData, downEvent);
}

static void _ewk_view_on_mouse_up(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Up* upEvent = static_cast<Evas_Event_Mouse_Up*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_up);
    smartData->api->mouse_up(smartData, upEvent);
}

static void _ewk_view_on_mouse_move(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Move* moveEvent = static_cast<Evas_Event_Mouse_Move*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_move);
    smartData->api->mouse_move(smartData, moveEvent);
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
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl);
    impl->pageProxy->viewStateDidChange(WebPageProxy::ViewIsVisible);
}

static void _ewk_view_on_hide(void* data, Evas*, Evas_Object*, void* /*eventInfo*/)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl);

    // This call may look wrong, but we really need to pass ViewIsVisible here.
    // viewStateDidChange() itself is responsible for actually setting the visibility to Visible or Hidden
    // depending on what WebPageProxy::isViewVisible() returns, this simply triggers the process.
    impl->pageProxy->viewStateDidChange(WebPageProxy::ViewIsVisible);
}

#if ENABLE(TOUCH_EVENTS)
static inline void _ewk_view_feed_touch_event_using_touch_point_list_of_evas(Evas_Object* ewkView, Ewk_Touch_Event_Type type)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);

    unsigned count = evas_touch_point_list_count(smartData->base.evas);
    if (!count)
        return;

    Eina_List* points = 0;
    for (unsigned i = 0; i < count; ++i) {
        Ewk_Touch_Point* point = new Ewk_Touch_Point;
        point->id = evas_touch_point_list_nth_id_get(smartData->base.evas, i);
        evas_touch_point_list_nth_xy_get(smartData->base.evas, i, &point->x, &point->y);
        point->state = evas_touch_point_list_nth_state_get(smartData->base.evas, i);
        points = eina_list_append(points, point);
    }

    ewk_view_feed_touch_event(ewkView, type, points, evas_key_modifier_get(smartData->base.evas));

    void* data;
    EINA_LIST_FREE(points, data)
        delete static_cast<Ewk_Touch_Point*>(data);
}

static void _ewk_view_on_touch_down(void* /* data */, Evas* /* canvas */, Evas_Object* ewkView, void* /* eventInfo */)
{
    _ewk_view_feed_touch_event_using_touch_point_list_of_evas(ewkView, EWK_TOUCH_START);
}

static void _ewk_view_on_touch_up(void* /* data */, Evas* /* canvas */, Evas_Object* ewkView, void* /* eventInfo */)
{
    _ewk_view_feed_touch_event_using_touch_point_list_of_evas(ewkView, EWK_TOUCH_END);
}

static void _ewk_view_on_touch_move(void* /* data */, Evas* /* canvas */, Evas_Object* ewkView, void* /* eventInfo */)
{
    _ewk_view_feed_touch_event_using_touch_point_list_of_evas(ewkView, EWK_TOUCH_MOVE);
}
#endif

static Evas_Smart_Class g_parentSmartClass = EVAS_SMART_CLASS_INIT_NULL;

static void _ewk_view_impl_del(EwkViewImpl* impl)
{
    /* Unregister icon change callback */
    Ewk_Favicon_Database* iconDatabase = impl->context->faviconDatabase();
    iconDatabase->unwatchChanges(_ewk_view_on_favicon_changed);

    delete impl;
}

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

    smartData->priv = new EwkViewImpl(ewkView);
    if (!smartData->priv) {
        EINA_LOG_CRIT("could not allocate EwkViewImpl");
        evas_object_smart_data_set(ewkView, 0);
        free(smartData);
        return;
    }

    // Create evas_object_image to draw web contents.
    smartData->image = evas_object_image_add(smartData->base.evas);
    evas_object_image_alpha_set(smartData->image, false);
    evas_object_image_filled_set(smartData->image, true);
    evas_object_smart_member_add(smartData->image, ewkView);
    evas_object_show(smartData->image);

    ewk_view_mouse_events_enabled_set(ewkView, true);

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
    EwkViewImpl::removeFromPageViewMap(ewkView);
    EWK_VIEW_SD_GET(ewkView, smartData);
    if (smartData && smartData->priv)
        _ewk_view_impl_del(smartData->priv);

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
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl);

#if USE(ACCELERATED_COMPOSITING)
    bool needsNewSurface = false;
#endif

    smartData->changed.any = false;

    Evas_Coord x, y, width, height;
    evas_object_geometry_get(ewkView, &x, &y, &width, &height);

    if (smartData->changed.size) {
#if USE(COORDINATED_GRAPHICS)
        impl->pageViewportControllerClient->updateViewportSize(IntSize(width, height));
#endif
#if USE(ACCELERATED_COMPOSITING)
        needsNewSurface = impl->evasGlSurface;
#endif

        if (impl->pageProxy->drawingArea())
            impl->pageProxy->drawingArea()->setSize(IntSize(width, height), IntSize());

        smartData->view.w = width;
        smartData->view.h = height;
        smartData->changed.size = false;
    }

    if (smartData->changed.position) {
        evas_object_move(smartData->image, x, y);
        smartData->view.x = x;
        smartData->view.y = y;
        smartData->changed.position = false;
    }

#if USE(ACCELERATED_COMPOSITING)
    if (needsNewSurface) {
        evas_gl_surface_destroy(impl->evasGl, impl->evasGlSurface);
        impl->evasGlSurface = 0;
        impl->createGLSurface(IntSize(width, height));
        impl->redrawRegion(IntRect(IntPoint(), IntSize(width, height)));
    }
#endif
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
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl);

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
    impl->pageProxy->setDrawsBackground(red || green || blue);
    impl->pageProxy->setDrawsTransparentBackground(alpha < 255);

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

static inline Evas_Smart* _ewk_view_smart_class_new(void)
{
    static Ewk_View_Smart_Class api = EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION("Ewk_View");
    static Evas_Smart* smart = 0;

    if (EINA_UNLIKELY(!smart)) {
        ewk_view_smart_class_set(&api);
        smart = evas_smart_class_new(&api.sc);
    }

    return smart;
}

static void _ewk_view_initialize(Evas_Object* ewkView, PassRefPtr<Ewk_Context> context, WKPageGroupRef pageGroupRef)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl);
    EINA_SAFETY_ON_NULL_RETURN(context);

    if (impl->pageClient)
        return;

    impl->pageClient = PageClientImpl::create(impl);

    if (pageGroupRef)
        impl->pageProxy = toImpl(context->wkContext())->createWebPage(impl->pageClient.get(), toImpl(pageGroupRef));
    else
        impl->pageProxy = toImpl(context->wkContext())->createWebPage(impl->pageClient.get(), WebPageGroup::create().get());

    EwkViewImpl::addToPageViewMap(ewkView);

#if USE(COORDINATED_GRAPHICS)
    impl->pageProxy->pageGroup()->preferences()->setAcceleratedCompositingEnabled(true);
    impl->pageProxy->pageGroup()->preferences()->setForceCompositingMode(true);
    impl->pageProxy->setUseFixedLayout(true);
#endif
    impl->pageProxy->initializeWebPage();

    impl->backForwardList = Ewk_Back_Forward_List::create(toAPI(impl->pageProxy->backForwardList()));

    impl->context = context;

#if USE(TILED_BACKING_STORE)
    impl->pageViewportControllerClient = PageViewportControllerClientEfl::create(impl);
    impl->pageViewportController = adoptPtr(new PageViewportController(impl->pageProxy.get(), impl->pageViewportControllerClient.get()));
    impl->pageClient->setPageViewportController(impl->pageViewportController.get());
#endif

#if ENABLE(FULLSCREEN_API)
    impl->pageProxy->fullScreenManager()->setWebView(ewkView);
    impl->pageProxy->pageGroup()->preferences()->setFullScreenEnabled(true);
#endif

    // Initialize page clients.
    impl->pageLoadClient = PageLoadClientEfl::create(impl);
    impl->pagePolicyClient = PagePolicyClientEfl::create(impl);
    impl->pageUIClient = PageUIClientEfl::create(impl);
    impl->resourceLoadClient = ResourceLoadClientEfl::create(impl);
    impl->findClient = FindClientEfl::create(impl);
    impl->formClient = FormClientEfl::create(impl);

    /* Listen for favicon changes */
    Ewk_Favicon_Database* iconDatabase = impl->context->faviconDatabase();
    iconDatabase->watchChanges(IconChangeCallbackData(_ewk_view_on_favicon_changed, ewkView));
}

static Evas_Object* _ewk_view_add_with_smart(Evas* canvas, Evas_Smart* smart)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(canvas, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smart, 0);

    Evas_Object* ewkView = evas_object_smart_add(canvas, smart);
    if (!ewkView)
        return 0;

    EWK_VIEW_SD_GET(ewkView, smartData);
    if (!smartData) {
        evas_object_del(ewkView);
        return 0;
    }

    EWK_VIEW_IMPL_GET(smartData, impl);
    if (!impl) {
        evas_object_del(ewkView);
        return 0;
    }

    return ewkView;
}

/**
 * @internal
 * Constructs a ewk_view Evas_Object with WKType parameters.
 */
Evas_Object* ewk_view_base_add(Evas* canvas, WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(canvas, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(contextRef, 0);
    Evas_Object* ewkView = _ewk_view_add_with_smart(canvas, _ewk_view_smart_class_new());
    if (!ewkView)
        return 0;

    _ewk_view_initialize(ewkView, Ewk_Context::create(contextRef), pageGroupRef);

    return ewkView;
}

Evas_Object* ewk_view_smart_add(Evas* canvas, Evas_Smart* smart, Ewk_Context* context)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(canvas, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smart, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(context, 0);

    Evas_Object* ewkView = _ewk_view_add_with_smart(canvas, smart);
    if (!ewkView)
        return 0;

    _ewk_view_initialize(ewkView, context, 0);

    return ewkView;
}

Evas_Object* ewk_view_add_with_context(Evas* canvas, Ewk_Context* context)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(context, 0);
    return ewk_view_smart_add(canvas, _ewk_view_smart_class_new(), context);
}

Evas_Object* ewk_view_add(Evas* canvas)
{
    return ewk_view_add_with_context(canvas, ewk_context_default_get());
}

Ewk_Context* ewk_view_context_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, 0);

    return impl->context.get();
}

Eina_Bool ewk_view_url_set(Evas_Object* ewkView, const char* url)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(url, false);

    impl->pageProxy->loadURL(url);
    impl->informURLChange();

    return true;
}

const char* ewk_view_url_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, 0);

    return impl->url;
}

const char *ewk_view_icon_url_get(const Evas_Object *ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, 0);

    return impl->faviconURL;
}

Eina_Bool ewk_view_reload(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    impl->pageProxy->reload(/*reloadFromOrigin*/ false);
    impl->informURLChange();

    return true;
}

Eina_Bool ewk_view_reload_bypass_cache(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    impl->pageProxy->reload(/*reloadFromOrigin*/ true);
    impl->informURLChange();

    return true;
}

Eina_Bool ewk_view_stop(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    impl->pageProxy->stopLoading();

    return true;
}

Ewk_Settings* ewk_view_settings_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, 0);

    return impl->settings.get();
}

const char* ewk_view_title_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, 0);

    CString title = impl->pageProxy->pageTitle().utf8();
    impl->title = title.data();

    return impl->title;
}

/**
 * @internal
 * Reports that the requested text was found.
 *
 * Emits signal: "text,found" with the number of matches.
 */
void ewk_view_text_found(Evas_Object* ewkView, unsigned int matchCount)
{
    evas_object_smart_callback_call(ewkView, "text,found", &matchCount);
}

double ewk_view_load_progress_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, -1.0);

    return impl->pageProxy->estimatedProgress();
}

Eina_Bool ewk_view_scale_set(Evas_Object* ewkView, double scaleFactor, int x, int y)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    impl->pageProxy->scalePage(scaleFactor, IntPoint(x, y));
    return true;
}

double ewk_view_scale_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, -1);

    return impl->pageProxy->pageScaleFactor();
}

Eina_Bool ewk_view_device_pixel_ratio_set(Evas_Object* ewkView, float ratio)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    impl->pageProxy->setCustomDeviceScaleFactor(ratio);

    return true;
}

float ewk_view_device_pixel_ratio_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, -1.0);

    return impl->pageProxy->deviceScaleFactor();
}

void ewk_view_theme_set(Evas_Object* ewkView, const char* path)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl);

    if (impl->theme != path) {
        impl->theme = path;
        impl->pageProxy->setThemePath(path);
    }
}

const char* ewk_view_theme_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, 0);

    return impl->theme;
}

Eina_Bool ewk_view_back(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    WebPageProxy* page = impl->pageProxy.get();
    if (page->canGoBack()) {
        page->goBack();
        return true;
    }

    return false;
}

Eina_Bool ewk_view_forward(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    WebPageProxy* page = impl->pageProxy.get();
    if (page->canGoForward()) {
        page->goForward();
        return true;
    }

    return false;
}

Eina_Bool ewk_view_intent_deliver(Evas_Object* ewkView, Ewk_Intent* intent)
{
#if ENABLE(WEB_INTENTS)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, false);

    WebPageProxy* page = impl->pageProxy.get();
    page->deliverIntentToFrame(page->mainFrame(), intent->webIntentData());

    return true;
#else
    return false;
#endif
}

Eina_Bool ewk_view_back_possible(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    return impl->pageProxy->canGoBack();
}

Eina_Bool ewk_view_forward_possible(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    return impl->pageProxy->canGoForward();
}

Ewk_Back_Forward_List* ewk_view_back_forward_list_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, 0);

    return impl->backForwardList.get();
}

Eina_Bool ewk_view_html_string_load(Evas_Object* ewkView, const char* html, const char* baseUrl, const char* unreachableUrl)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(html, false);

    if (unreachableUrl && *unreachableUrl)
        impl->pageProxy->loadAlternateHTMLString(String::fromUTF8(html), baseUrl ? String::fromUTF8(baseUrl) : "", String::fromUTF8(unreachableUrl));
    else
        impl->pageProxy->loadHTMLString(String::fromUTF8(html), baseUrl ? String::fromUTF8(baseUrl) : "");

    impl->informURLChange();

    return true;
}

const char* ewk_view_setting_encoding_custom_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, 0);

    String customEncoding = impl->pageProxy->customTextEncodingName();
    if (customEncoding.isEmpty())
        return 0;

    impl->customEncoding = customEncoding.utf8().data();

    return impl->customEncoding;
}

Eina_Bool ewk_view_setting_encoding_custom_set(Evas_Object* ewkView, const char* encoding)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    impl->customEncoding = encoding;
    impl->pageProxy->setCustomTextEncodingName(encoding ? encoding : String());

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
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(text, false);

    impl->pageProxy->findString(String::fromUTF8(text), static_cast<WebKit::FindOptions>(options), maxMatchCount);

    return true;
}

Eina_Bool ewk_view_text_find_highlight_clear(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    impl->pageProxy->hideFindUI();

    return true;
}

Eina_Bool ewk_view_text_matches_count(Evas_Object* ewkView, const char* text, Ewk_Find_Options options, unsigned maxMatchCount)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(text, false);

    impl->pageProxy->countStringMatches(String::fromUTF8(text), static_cast<WebKit::FindOptions>(options), maxMatchCount);

    return true;
}

Eina_Bool ewk_view_popup_menu_close(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);

    if (!impl->popupMenuProxy)
        return false;

    impl->popupMenuProxy = 0;

    if (smartData->api->popup_menu_hide)
        smartData->api->popup_menu_hide(smartData);

    void* item;
    EINA_LIST_FREE(impl->popupMenuItems, item)
        delete static_cast<Ewk_Popup_Menu_Item*>(item);

    return true;
}

Eina_Bool ewk_view_popup_menu_select(Evas_Object* ewkView, unsigned int selectedIndex)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(impl->popupMenuProxy, false);

    if (selectedIndex >= eina_list_count(impl->popupMenuItems))
        return false;

    impl->popupMenuProxy->valueChanged(selectedIndex);

    return true;
}

Eina_Bool ewk_view_mouse_events_enabled_set(Evas_Object* ewkView, Eina_Bool enabled)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    enabled = !!enabled;
    if (impl->areMouseEventsEnabled == enabled)
        return true;

    impl->areMouseEventsEnabled = enabled;
    if (enabled) {
        evas_object_event_callback_add(ewkView, EVAS_CALLBACK_MOUSE_DOWN, _ewk_view_on_mouse_down, smartData);
        evas_object_event_callback_add(ewkView, EVAS_CALLBACK_MOUSE_UP, _ewk_view_on_mouse_up, smartData);
        evas_object_event_callback_add(ewkView, EVAS_CALLBACK_MOUSE_MOVE, _ewk_view_on_mouse_move, smartData);
    } else {
        evas_object_event_callback_del(ewkView, EVAS_CALLBACK_MOUSE_DOWN, _ewk_view_on_mouse_down);
        evas_object_event_callback_del(ewkView, EVAS_CALLBACK_MOUSE_UP, _ewk_view_on_mouse_up);
        evas_object_event_callback_del(ewkView, EVAS_CALLBACK_MOUSE_MOVE, _ewk_view_on_mouse_move);
    }

    return true;
}

Eina_Bool ewk_view_mouse_events_enabled_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    return impl->areMouseEventsEnabled;
}

Eina_Bool ewk_view_color_picker_color_set(Evas_Object* ewkView, int r, int g, int b, int a)
{
#if ENABLE(INPUT_TYPE_COLOR)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(impl->colorPickerResultListener, false);

    WebCore::Color color = WebCore::Color(r, g, b, a);
    WKRetainPtr<WKStringRef> colorString(AdoptWK, WKStringCreateWithUTF8CString(color.serialized().utf8().data()));
    WKColorPickerResultListenerSetColor(impl->colorPickerResultListener.get(), colorString.get());
    impl->colorPickerResultListener.clear();

    return true;
#else
    return false;
#endif
}

Eina_Bool ewk_view_feed_touch_event(Evas_Object* ewkView, Ewk_Touch_Event_Type type, const Eina_List* points, const Evas_Modifier* modifiers)
{
#if ENABLE(TOUCH_EVENTS)
    EINA_SAFETY_ON_NULL_RETURN_VAL(points, false);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    Evas_Point position = { smartData->view.x, smartData->view.y };
    // FIXME: Touch points do not take scroll position and scale into account when 
    // TILED_BACKING_STORE is enabled.
    impl->pageProxy->handleTouchEvent(NativeWebTouchEvent(type, points, modifiers, &position, ecore_time_get()));

    return true;
#else
    return false;
#endif
}

Eina_Bool ewk_view_touch_events_enabled_set(Evas_Object* ewkView, Eina_Bool enabled)
{
#if ENABLE(TOUCH_EVENTS)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    enabled = !!enabled;
    if (impl->areTouchEventsEnabled == enabled)
        return true;

    impl->areTouchEventsEnabled = enabled;
    if (enabled) {
        // FIXME: We have to connect touch callbacks with mouse and multi events
        // because the Evas creates mouse events for first touch and multi events
        // for second and third touches. Below codes should be fixed when the Evas
        // supports the touch events.
        // See https://bugs.webkit.org/show_bug.cgi?id=97785 for details.
        evas_object_event_callback_add(ewkView, EVAS_CALLBACK_MOUSE_DOWN, _ewk_view_on_touch_down, smartData);
        evas_object_event_callback_add(ewkView, EVAS_CALLBACK_MOUSE_UP, _ewk_view_on_touch_up, smartData);
        evas_object_event_callback_add(ewkView, EVAS_CALLBACK_MOUSE_MOVE, _ewk_view_on_touch_move, smartData);
        evas_object_event_callback_add(ewkView, EVAS_CALLBACK_MULTI_DOWN, _ewk_view_on_touch_down, smartData);
        evas_object_event_callback_add(ewkView, EVAS_CALLBACK_MULTI_UP, _ewk_view_on_touch_up, smartData);
        evas_object_event_callback_add(ewkView, EVAS_CALLBACK_MULTI_MOVE, _ewk_view_on_touch_move, smartData);
    } else {
        evas_object_event_callback_del(ewkView, EVAS_CALLBACK_MOUSE_DOWN, _ewk_view_on_touch_down);
        evas_object_event_callback_del(ewkView, EVAS_CALLBACK_MOUSE_UP, _ewk_view_on_touch_up);
        evas_object_event_callback_del(ewkView, EVAS_CALLBACK_MOUSE_MOVE, _ewk_view_on_touch_move);
        evas_object_event_callback_del(ewkView, EVAS_CALLBACK_MULTI_DOWN, _ewk_view_on_touch_down);
        evas_object_event_callback_del(ewkView, EVAS_CALLBACK_MULTI_UP, _ewk_view_on_touch_up);
        evas_object_event_callback_del(ewkView, EVAS_CALLBACK_MULTI_MOVE, _ewk_view_on_touch_move);
    }

    return true;
#else
    return false;
#endif
}

Eina_Bool ewk_view_touch_events_enabled_get(const Evas_Object* ewkView)
{
#if ENABLE(TOUCH_EVENTS)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    return impl->areTouchEventsEnabled;
#else
    return false;
#endif
}

Eina_Bool ewk_view_inspector_show(Evas_Object* ewkView)
{
#if ENABLE(INSPECTOR)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    WebInspectorProxy* inspector = impl->pageProxy->inspector();
    if (inspector)
        inspector->show();

    return true;
#else
    return false;
#endif
}

Eina_Bool ewk_view_inspector_close(Evas_Object* ewkView)
{
#if ENABLE(INSPECTOR)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);

    WebInspectorProxy* inspector = impl->pageProxy->inspector();
    if (inspector)
        inspector->close();

    return true;
#else
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
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, false);
    
    impl->pageProxy->setPaginationMode(static_cast<WebCore::Pagination::Mode>(mode));

    return true;
}

Ewk_Pagination_Mode ewk_view_pagination_mode_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, EWK_PAGINATION_MODE_INVALID);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, EWK_PAGINATION_MODE_INVALID);

    return static_cast<Ewk_Pagination_Mode>(impl->pageProxy->paginationMode());
}
