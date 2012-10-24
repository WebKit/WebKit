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
#include "RefPtrEfl.h"
#include "ResourceLoadClientEfl.h"
#include "WKAPICast.h"
#include "WKColorPickerResultListener.h"
#include "WKEinaSharedString.h"
#include "WKFindOptions.h"
#include "WKRetainPtr.h"
#include "WKString.h"
#include "WKURL.h"
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
#include "ewk_popup_menu_item.h"
#include "ewk_popup_menu_item_private.h"
#include "ewk_private.h"
#include "ewk_resource.h"
#include "ewk_resource_private.h"
#include "ewk_settings_private.h"
#include "ewk_view_private.h"
#include <Ecore_Evas.h>
#include <Ecore_IMF.h>
#include <Ecore_IMF_Evas.h>
#include <Edje.h>
#include <WebCore/Cursor.h>
#include <WebCore/Editor.h>
#include <WebCore/EflScreenUtilities.h>
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

static const int defaultCursorSize = 16;

typedef HashMap<const WebPageProxy*, const Evas_Object*> PageViewMap;

static inline PageViewMap& pageViewMap()
{
    DEFINE_STATIC_LOCAL(PageViewMap, map, ());
    return map;
}

static inline void addToPageViewMap(const Evas_Object* ewkView)
{
    ASSERT(ewkView);
    ASSERT(ewk_view_page_get(ewkView));
    PageViewMap::AddResult result = pageViewMap().add(ewk_view_page_get(ewkView), ewkView);
    ASSERT_UNUSED(result, result.isNewEntry);
}

static inline void removeFromPageViewMap(const Evas_Object* ewkView)
{
    ASSERT(ewkView);
    ASSERT(ewk_view_page_get(ewkView));
    ASSERT(pageViewMap().contains(ewk_view_page_get(ewkView)));
    pageViewMap().remove(ewk_view_page_get(ewkView));
}

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

    const char* view_url = ewk_view_url_get(ewkView);
    if (!view_url || strcasecmp(view_url, pageURL))
        return;

    ewk_view_update_icon(ewkView);
}

// Default Event Handling.
static Eina_Bool _ewk_view_smart_focus_in(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->pageProxy->viewStateDidChange(WebPageProxy::ViewIsFocused | WebPageProxy::ViewWindowIsActive);
    return true;
}

static Eina_Bool _ewk_view_smart_focus_out(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->pageProxy->viewStateDidChange(WebPageProxy::ViewIsFocused | WebPageProxy::ViewWindowIsActive);
    return true;
}

#if USE(TILED_BACKING_STORE)
static Evas_Coord_Point mapToWebContent(Ewk_View_Smart_Data* smartData, Evas_Coord_Point point)
{
    Evas_Coord_Point result;
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, result);

    result.x = (point.x  - smartData->view.x) / priv->pageViewportControllerClient->scaleFactor() + smartData->view.x + priv->pageViewportControllerClient->scrollPosition().x();
    result.y = (point.y - smartData->view.y) / priv->pageViewportControllerClient->scaleFactor() + smartData->view.y + priv->pageViewportControllerClient->scrollPosition().y();
    return result;
}
#endif

static Eina_Bool _ewk_view_smart_mouse_wheel(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Wheel* wheelEvent)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    Evas_Point position = {smartData->view.x, smartData->view.y};
#if USE(TILED_BACKING_STORE)
    Evas_Event_Mouse_Wheel event(*wheelEvent);
    event.canvas = mapToWebContent(smartData, event.canvas);
    priv->pageProxy->handleWheelEvent(NativeWebWheelEvent(&event, &position));
#else
    priv->pageProxy->handleWheelEvent(NativeWebWheelEvent(wheelEvent, &position));
#endif
    return true;
}

static Eina_Bool _ewk_view_smart_mouse_down(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Down* downEvent)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    Evas_Point position = {smartData->view.x, smartData->view.y};
#if USE(TILED_BACKING_STORE)
    Evas_Event_Mouse_Down event(*downEvent);
    event.canvas = mapToWebContent(smartData, event.canvas);
    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(&event, &position));
#else
    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(downEvent, &position));
#endif
    return true;
}

static Eina_Bool _ewk_view_smart_mouse_up(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Up* upEvent)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    Evas_Point position = {smartData->view.x, smartData->view.y};
#if USE(TILED_BACKING_STORE)
    Evas_Event_Mouse_Up event(*upEvent);
    event.canvas = mapToWebContent(smartData, event.canvas);
    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(&event, &position));
#else
    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(upEvent, &position));
#endif

    if (priv->imfContext)
        ecore_imf_context_reset(priv->imfContext);

    return true;
}

static Eina_Bool _ewk_view_smart_mouse_move(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Move* moveEvent)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    Evas_Point position = {smartData->view.x, smartData->view.y};
#if USE(TILED_BACKING_STORE)
    Evas_Event_Mouse_Move event(*moveEvent);
    event.cur.canvas = mapToWebContent(smartData, event.cur.canvas);
    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(&event, &position));
#else
    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(moveEvent, &position));
#endif
    return true;
}

static Eina_Bool _ewk_view_smart_key_down(Ewk_View_Smart_Data* smartData, const Evas_Event_Key_Down* downEvent)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    bool isFiltered = false;
    if (priv->imfContext) {
        Ecore_IMF_Event imfEvent;
        ecore_imf_evas_event_key_down_wrap(const_cast<Evas_Event_Key_Down*>(downEvent), &imfEvent.key_down);

        isFiltered = ecore_imf_context_filter_event(priv->imfContext, ECORE_IMF_EVENT_KEY_DOWN, &imfEvent);
    }

    priv->pageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(downEvent, isFiltered));
    return true;
}

static Eina_Bool _ewk_view_smart_key_up(Ewk_View_Smart_Data* smartData, const Evas_Event_Key_Up* upEvent)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->pageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(upEvent));
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
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    priv->pageProxy->viewStateDidChange(WebPageProxy::ViewIsVisible);
}

static void _ewk_view_on_hide(void* data, Evas*, Evas_Object*, void* /*eventInfo*/)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    // This call may look wrong, but we really need to pass ViewIsVisible here.
    // viewStateDidChange() itself is responsible for actually setting the visibility to Visible or Hidden
    // depending on what WebPageProxy::isViewVisible() returns, this simply triggers the process.
    priv->pageProxy->viewStateDidChange(WebPageProxy::ViewIsVisible);
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

static void _ewk_view_priv_del(EwkViewImpl* priv)
{
    /* Unregister icon change callback */
    Ewk_Favicon_Database* iconDatabase = priv->context->faviconDatabase();
    iconDatabase->unwatchChanges(_ewk_view_on_favicon_changed);

    delete priv;
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
    removeFromPageViewMap(ewkView);
    EWK_VIEW_SD_GET(ewkView, smartData);
    if (smartData && smartData->priv)
        _ewk_view_priv_del(smartData->priv);

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

IntSize ewk_view_size_get(const Evas_Object* ewkView)
{
    int width, height;
    evas_object_geometry_get(ewkView, 0, 0, &width, &height);
    return IntSize(width, height);
}

#if USE(ACCELERATED_COMPOSITING)
static bool ewk_view_create_gl_surface(const Evas_Object* ewkView, const IntSize& viewSize)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    Evas_GL_Config evasGlConfig = {
        EVAS_GL_RGBA_8888,
        EVAS_GL_DEPTH_BIT_8,
        EVAS_GL_STENCIL_NONE,
        EVAS_GL_OPTIONS_NONE,
        EVAS_GL_MULTISAMPLE_NONE
    };

    ASSERT(!priv->evasGlSurface);
    priv->evasGlSurface = evas_gl_surface_create(priv->evasGl, &evasGlConfig, viewSize.width(), viewSize.height());
    if (!priv->evasGlSurface)
        return false;

    Evas_Native_Surface nativeSurface;
    evas_gl_native_surface_get(priv->evasGl, priv->evasGlSurface, &nativeSurface);
    evas_object_image_native_surface_set(smartData->image, &nativeSurface);

    return true;
}

bool ewk_view_accelerated_compositing_mode_enter(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    if (priv->evasGl) {
        EINA_LOG_DOM_WARN(_ewk_log_dom, "Accelerated compositing mode already entered.");
        return false;
    }

    Evas* evas = evas_object_evas_get(ewkView);
    priv->evasGl = evas_gl_new(evas);
    if (!priv->evasGl)
        return false;

    priv->evasGlContext = evas_gl_context_create(priv->evasGl, 0);
    if (!priv->evasGlContext) {
        evas_gl_free(priv->evasGl);
        priv->evasGl = 0;
        return false;
    }

    if (!ewk_view_create_gl_surface(ewkView, ewk_view_size_get(ewkView))) {
        evas_gl_context_destroy(priv->evasGl, priv->evasGlContext);
        priv->evasGlContext = 0;

        evas_gl_free(priv->evasGl);
        priv->evasGl = 0;
        return false;
    }

    priv->pageViewportControllerClient->setRendererActive(true);
    return true;
}

bool ewk_view_accelerated_compositing_mode_exit(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    EINA_SAFETY_ON_NULL_RETURN_VAL(priv->evasGl, false);

    if (priv->evasGlSurface) {
        evas_gl_surface_destroy(priv->evasGl, priv->evasGlSurface);
        priv->evasGlSurface = 0;
    }

    if (priv->evasGlContext) {
        evas_gl_context_destroy(priv->evasGl, priv->evasGlContext);
        priv->evasGlContext = 0;
    }

    evas_gl_free(priv->evasGl);
    priv->evasGl = 0;

    return true;
}
#endif

static void _ewk_view_smart_calculate(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

#if USE(ACCELERATED_COMPOSITING)
    bool needsNewSurface = false;
#endif

    smartData->changed.any = false;

    Evas_Coord x, y, width, height;
    evas_object_geometry_get(ewkView, &x, &y, &width, &height);

    if (smartData->changed.size) {
#if USE(COORDINATED_GRAPHICS)
        priv->pageViewportControllerClient->updateViewportSize(IntSize(width, height));
#endif
#if USE(ACCELERATED_COMPOSITING)
        needsNewSurface = priv->evasGlSurface;
#endif

        if (priv->pageProxy->drawingArea())
            priv->pageProxy->drawingArea()->setSize(IntSize(width, height), IntSize());

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
        evas_gl_surface_destroy(priv->evasGl, priv->evasGlSurface);
        priv->evasGlSurface = 0;
        ewk_view_create_gl_surface(ewkView, IntSize(width, height));
        ewk_view_display(ewkView, IntRect(IntPoint(), IntSize(width, height)));
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
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

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
    priv->pageProxy->setDrawsBackground(red || green || blue);
    priv->pageProxy->setDrawsTransparentBackground(alpha < 255);

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
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    EINA_SAFETY_ON_NULL_RETURN(context);

    if (priv->pageClient)
        return;

    priv->pageClient = PageClientImpl::create(ewkView);

    if (pageGroupRef)
        priv->pageProxy = toImpl(context->wkContext())->createWebPage(priv->pageClient.get(), toImpl(pageGroupRef));
    else
        priv->pageProxy = toImpl(context->wkContext())->createWebPage(priv->pageClient.get(), WebPageGroup::create().get());

    addToPageViewMap(ewkView);

#if USE(COORDINATED_GRAPHICS)
    priv->pageProxy->pageGroup()->preferences()->setAcceleratedCompositingEnabled(true);
    priv->pageProxy->pageGroup()->preferences()->setForceCompositingMode(true);
    priv->pageProxy->setUseFixedLayout(true);
#endif
    priv->pageProxy->initializeWebPage();

    priv->backForwardList = Ewk_Back_Forward_List::create(toAPI(priv->pageProxy->backForwardList()));

    priv->context = context;

#if USE(TILED_BACKING_STORE)
    priv->pageViewportControllerClient = PageViewportControllerClientEfl::create(ewkView);
    priv->pageViewportController = adoptPtr(new PageViewportController(priv->pageProxy.get(), priv->pageViewportControllerClient.get()));
    priv->pageClient->setPageViewportController(priv->pageViewportController.get());
#endif

#if ENABLE(FULLSCREEN_API)
    priv->pageProxy->fullScreenManager()->setWebView(ewkView);
    priv->pageProxy->pageGroup()->preferences()->setFullScreenEnabled(true);
#endif

    // Initialize page clients.
    priv->pageLoadClient = PageLoadClientEfl::create(ewkView);
    priv->pagePolicyClient = PagePolicyClientEfl::create(ewkView);
    priv->pageUIClient = PageUIClientEfl::create(ewkView);
    priv->resourceLoadClient = ResourceLoadClientEfl::create(ewkView);
    priv->findClient = FindClientEfl::create(ewkView);
    priv->formClient = FormClientEfl::create(ewkView);

    /* Listen for favicon changes */
    Ewk_Favicon_Database* iconDatabase = priv->context->faviconDatabase();
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

    EWK_VIEW_PRIV_GET(smartData, priv);
    if (!priv) {
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
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    return priv->context.get();
}

/**
 * @internal
 * The url of view was changed by the frame loader.
 *
 * Emits signal: "url,changed" with pointer to new url string.
 */
void ewk_view_url_update(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    String activeURL = priv->pageProxy->activeURL();
    if (activeURL.isEmpty())
        return;

    if (priv->url == activeURL.utf8().data())
        return;

    priv->url = activeURL.utf8().data();
    const char* callbackArgument = static_cast<const char*>(priv->url);
    evas_object_smart_callback_call(ewkView, "url,changed", const_cast<char*>(callbackArgument));

    // Update the view's favicon.
    ewk_view_update_icon(ewkView);
}

Eina_Bool ewk_view_url_set(Evas_Object* ewkView, const char* url)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(url, false);

    priv->pageProxy->loadURL(url);
    ewk_view_url_update(ewkView);

    return true;
}

const char* ewk_view_url_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    return priv->url;
}

const char *ewk_view_icon_url_get(const Evas_Object *ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    return priv->faviconURL;
}

Eina_Bool ewk_view_reload(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->pageProxy->reload(/*reloadFromOrigin*/ false);
    ewk_view_url_update(ewkView);

    return true;
}

Eina_Bool ewk_view_reload_bypass_cache(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->pageProxy->reload(/*reloadFromOrigin*/ true);
    ewk_view_url_update(ewkView);

    return true;
}

Eina_Bool ewk_view_stop(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->pageProxy->stopLoading();

    return true;
}

Ewk_Settings* ewk_view_settings_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    return priv->settings.get();
}

/**
 * @internal
 * Retrieves the internal WKPage for this view.
 */
WKPageRef ewk_view_wkpage_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    return toAPI(priv->pageProxy.get());
}

/**
 * @internal
 * Load was initiated for a resource in the view.
 *
 * Emits signal: "resource,request,new" with pointer to resource request.
 */
void ewk_view_resource_load_initiated(Evas_Object* ewkView, Ewk_Resource* resource, Ewk_Url_Request* request)
{
    Ewk_Resource_Request resourceRequest = {resource, request, 0};

    evas_object_smart_callback_call(ewkView, "resource,request,new", &resourceRequest);
}

/**
 * @internal
 * Received a response to a resource load request in the view.
 *
 * Emits signal: "resource,request,response" with pointer to resource response.
 */
void ewk_view_resource_load_response(Evas_Object* ewkView, Ewk_Resource* resource, Ewk_Url_Response* response)
{
    Ewk_Resource_Load_Response resourceLoadResponse = {resource, response};
    evas_object_smart_callback_call(ewkView, "resource,request,response", &resourceLoadResponse);
}

/**
 * @internal
 * Failed loading a resource in the view.
 *
 * Emits signal: "resource,request,finished" with pointer to the resource load error.
 */
void ewk_view_resource_load_failed(Evas_Object* ewkView, Ewk_Resource* resource, Ewk_Error* error)
{
    Ewk_Resource_Load_Error resourceLoadError = {resource, error};
    evas_object_smart_callback_call(ewkView, "resource,request,failed", &resourceLoadError);
}

/**
 * @internal
 * Finished loading a resource in the view.
 *
 * Emits signal: "resource,request,finished" with pointer to the resource.
 */
void ewk_view_resource_load_finished(Evas_Object* ewkView, Ewk_Resource* resource)
{
    evas_object_smart_callback_call(ewkView, "resource,request,finished", resource);
}

/**
 * @internal
 * Request was sent for a resource in the view.
 *
 * Emits signal: "resource,request,sent" with pointer to resource request and possible redirect response.
 */
void ewk_view_resource_request_sent(Evas_Object* ewkView, Ewk_Resource* resource, Ewk_Url_Request* request, Ewk_Url_Response* redirectResponse)
{
    Ewk_Resource_Request resourceRequest = {resource, request, redirectResponse};
    evas_object_smart_callback_call(ewkView, "resource,request,sent", &resourceRequest);
}

const char* ewk_view_title_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    CString title = priv->pageProxy->pageTitle().utf8();
    priv->title = title.data();

    return priv->title;
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

/**
 * @internal
 * The view was requested to update text input state
 */
void ewk_view_text_input_state_update(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    if (!priv->imfContext)
        return;

    const EditorState& editor = priv->pageProxy->editorState();

    if (editor.isContentEditable) {
        if (priv->isImfFocused)
            return;

        ecore_imf_context_reset(priv->imfContext);
        ecore_imf_context_focus_in(priv->imfContext);
        priv->isImfFocused = true;
    } else {
        if (!priv->isImfFocused)
            return;

        if (editor.hasComposition)
            priv->pageProxy->cancelComposition();

        priv->isImfFocused = false;
        ecore_imf_context_reset(priv->imfContext);
        ecore_imf_context_focus_out(priv->imfContext);
    }
}

/**
 * @internal
 * The view title was changed by the frame loader.
 *
 * Emits signal: "title,changed" with pointer to new title string.
 */
void ewk_view_title_changed(Evas_Object* ewkView, const char* title)
{
    evas_object_smart_callback_call(ewkView, "title,changed", const_cast<char*>(title));
}

/**
 * @internal
 */
void ewk_view_tooltip_text_set(Evas_Object* ewkView, const char* text)
{
    if (text && *text)
        evas_object_smart_callback_call(ewkView, "tooltip,text,set", const_cast<char*>(text));
    else
        evas_object_smart_callback_call(ewkView, "tooltip,text,unset", 0);
}

double ewk_view_load_progress_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1.0);

    return priv->pageProxy->estimatedProgress();
}

Eina_Bool ewk_view_scale_set(Evas_Object* ewkView, double scaleFactor, int x, int y)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->pageProxy->scalePage(scaleFactor, IntPoint(x, y));
    return true;
}

double ewk_view_scale_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1);

    return priv->pageProxy->pageScaleFactor();
}

Eina_Bool ewk_view_device_pixel_ratio_set(Evas_Object* ewkView, float ratio)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->pageProxy->setCustomDeviceScaleFactor(ratio);

    return true;
}

float ewk_view_device_pixel_ratio_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1.0);

    return priv->pageProxy->deviceScaleFactor();
}

/**
 * @internal
 * Reports load progress changed.
 *
 * Emits signal: "load,progress" with pointer to a double from 0.0 to 1.0.
 */
void ewk_view_load_progress_changed(Evas_Object* ewkView, double progress)
{
    evas_object_smart_callback_call(ewkView, "load,progress", &progress);
}

/**
 * @internal
 * The view received a new intent request.
 *
 * Emits signal: "intent,request,new" with pointer to a Ewk_Intent.
 */
void ewk_view_intent_request_new(Evas_Object* ewkView, const Ewk_Intent* ewkIntent)
{
#if ENABLE(WEB_INTENTS)
    evas_object_smart_callback_call(ewkView, "intent,request,new", const_cast<Ewk_Intent*>(ewkIntent));
#endif
}

void ewk_view_theme_set(Evas_Object* ewkView, const char* path)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    if (priv->theme != path) {
        priv->theme = path;
        priv->pageProxy->setThemePath(path);
    }
}

const char* ewk_view_theme_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    return priv->theme;
}

void ewk_view_cursor_set(Evas_Object* ewkView, const Cursor& cursor)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    const char* group = cursor.platformCursor();
    if (!group || group == priv->cursorGroup)
        return;

    priv->cursorGroup = group;
    priv->cursorObject = adoptRef(edje_object_add(smartData->base.evas));

    Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(smartData->base.evas);
    if (!priv->theme || !edje_object_file_set(priv->cursorObject.get(), priv->theme, group)) {
        priv->cursorObject.clear();

        ecore_evas_object_cursor_set(ecoreEvas, 0, 0, 0, 0);
#ifdef HAVE_ECORE_X
        if (priv->isUsingEcoreX)
            WebCore::applyFallbackCursor(ecoreEvas, group);
#endif
        return;
    }

    Evas_Coord width, height;
    edje_object_size_min_get(priv->cursorObject.get(), &width, &height);
    if (width <= 0 || height <= 0)
        edje_object_size_min_calc(priv->cursorObject.get(), &width, &height);
    if (width <= 0 || height <= 0) {
        width = defaultCursorSize;
        height = defaultCursorSize;
    }
    evas_object_resize(priv->cursorObject.get(), width, height);

    const char* data;
    int hotspotX = 0;
    data = edje_object_data_get(priv->cursorObject.get(), "hot.x");
    if (data)
        hotspotX = atoi(data);

    int hotspotY = 0;
    data = edje_object_data_get(priv->cursorObject.get(), "hot.y");
    if (data)
        hotspotY = atoi(data);

    ecore_evas_object_cursor_set(ecoreEvas, priv->cursorObject.get(), EVAS_LAYER_MAX, hotspotX, hotspotY);
}

void ewk_view_display(Evas_Object* ewkView, const IntRect& rect)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    if (!smartData->image)
        return;

#if USE(COORDINATED_GRAPHICS)
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    evas_gl_make_current(priv->evasGl, priv->evasGlSurface, priv->evasGlContext);
    priv->pageViewportControllerClient->display(rect, IntPoint(smartData->view.x, smartData->view.y));
#endif

    evas_object_image_data_update_add(smartData->image, rect.x(), rect.y(), rect.width(), rect.height());
}

#if ENABLE(FULLSCREEN_API)
/**
 * @internal
 * Calls fullscreen_enter callback or falls back to default behavior and enables fullscreen mode.
 */
void ewk_view_full_screen_enter(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);

    if (!smartData->api->fullscreen_enter || !smartData->api->fullscreen_enter(smartData)) {
        Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(smartData->base.evas);
        ecore_evas_fullscreen_set(ecoreEvas, true);
    }
}

/**
 * @internal
 * Calls fullscreen_exit callback or falls back to default behavior and disables fullscreen mode.
 */
void ewk_view_full_screen_exit(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);

    if (!smartData->api->fullscreen_exit || !smartData->api->fullscreen_exit(smartData)) {
        Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(smartData->base.evas);
        ecore_evas_fullscreen_set(ecoreEvas, false);
    }
}
#endif

#if ENABLE(SQL_DATABASE)
/**
 * @internal
 * Calls exceeded_database_quota callback or falls back to default behavior returns default database quota.
 */
unsigned long long ewk_view_database_quota_exceeded(Evas_Object* ewkView, const char* databaseName, const char* displayName, unsigned long long currentQuota, unsigned long long currentOriginUsage, unsigned long long currentDatabaseUsage, unsigned long long expectedUsage)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);

    static const unsigned long long defaultQuota = 5 * 1024 * 1204; // 5 MB
    if (smartData->api->exceeded_database_quota)
        return smartData->api->exceeded_database_quota(smartData, databaseName, displayName, currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage);

    return defaultQuota;
}
#endif

/**
 * @internal
 * A download for that view was cancelled.
 *
 * Emits signal: "download,cancelled" with pointer to a Ewk_Download_Job.
 */
void ewk_view_download_job_cancelled(Evas_Object* ewkView, Ewk_Download_Job* download)
{
    evas_object_smart_callback_call(ewkView, "download,cancelled", download);
}

/**
 * @internal
 * A new download has been requested for that view.
 *
 * Emits signal: "download,request" with pointer to a Ewk_Download_Job.
 */
void ewk_view_download_job_requested(Evas_Object* ewkView, Ewk_Download_Job* download)
{
     evas_object_smart_callback_call(ewkView, "download,request", download);
}

/**
 * @internal
 * A download for that view has failed.
 *
 * Emits signal: "download,failed" with pointer to a Ewk_Download_Job_Error.
 */
void ewk_view_download_job_failed(Evas_Object* ewkView, Ewk_Download_Job* download, Ewk_Error* error)
{
    Ewk_Download_Job_Error downloadError = { download, error };
    evas_object_smart_callback_call(ewkView, "download,failed", &downloadError);
}

/**
 * @internal
 * A download for that view finished successfully.
 *
 * Emits signal: "download,finished" with pointer to a Ewk_Download_Job.
 */
void ewk_view_download_job_finished(Evas_Object* ewkView, Ewk_Download_Job* download)
{
     evas_object_smart_callback_call(ewkView, "download,finished", download);
}

Eina_Bool ewk_view_back(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    WebPageProxy* page = priv->pageProxy.get();
    if (page->canGoBack()) {
        page->goBack();
        return true;
    }

    return false;
}

Eina_Bool ewk_view_forward(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    WebPageProxy* page = priv->pageProxy.get();
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
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(intent, false);

    WebPageProxy* page = priv->pageProxy.get();
    page->deliverIntentToFrame(page->mainFrame(), intent->webIntentData());

    return true;
#else
    return false;
#endif
}

Eina_Bool ewk_view_back_possible(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    return priv->pageProxy->canGoBack();
}

Eina_Bool ewk_view_forward_possible(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    return priv->pageProxy->canGoForward();
}

Ewk_Back_Forward_List* ewk_view_back_forward_list_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    return priv->backForwardList.get();
}

void ewk_view_image_data_set(Evas_Object* ewkView, void* imageData, const IntSize& size)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    if (!imageData || !smartData->image)
        return;

    evas_object_resize(smartData->image, size.width(), size.height());
    evas_object_image_size_set(smartData->image, size.width(), size.height());
    evas_object_image_data_copy_set(smartData->image, imageData);
}

/**
 * @internal
 * Reports that a form request is about to be submitted.
 *
 * Emits signal: "form,submission,request" with pointer to Ewk_Form_Submission_Request.
 */
void ewk_view_form_submission_request_new(Evas_Object* ewkView, Ewk_Form_Submission_Request* request)
{
    evas_object_smart_callback_call(ewkView, "form,submission,request", request);
}

/**
 * @internal
 * Reports load failed with error information.
 *
 * Emits signal: "load,error" with pointer to Ewk_Error.
 */
void ewk_view_load_error(Evas_Object* ewkView, const Ewk_Error* error)
{
    evas_object_smart_callback_call(ewkView, "load,error", const_cast<Ewk_Error*>(error));
}

/**
 * @internal
 * Reports load finished.
 *
 * Emits signal: "load,finished".
 */
void ewk_view_load_finished(Evas_Object* ewkView)
{
    ewk_view_url_update(ewkView);
    evas_object_smart_callback_call(ewkView, "load,finished", 0);
}

/**
 * @internal
 * Reports view provisional load failed with error information.
 *
 * Emits signal: "load,provisional,failed" with pointer to Ewk_Error.
 */
void ewk_view_load_provisional_failed(Evas_Object* ewkView, const Ewk_Error* error)
{
    evas_object_smart_callback_call(ewkView, "load,provisional,failed", const_cast<Ewk_Error*>(error));
}

#if USE(TILED_BACKING_STORE)
void ewk_view_load_committed(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    priv->pageViewportController->didCommitLoad();
}
#endif

/**
 * @internal
 * Reports view received redirect for provisional load.
 *
 * Emits signal: "load,provisional,redirect".
 */
void ewk_view_load_provisional_redirect(Evas_Object* ewkView)
{
    ewk_view_url_update(ewkView);
    evas_object_smart_callback_call(ewkView, "load,provisional,redirect", 0);
}

/**
 * @internal
 * Reports view provisional load started.
 *
 * Emits signal: "load,provisional,started".
 */
void ewk_view_load_provisional_started(Evas_Object* ewkView)
{
    ewk_view_url_update(ewkView);
    evas_object_smart_callback_call(ewkView, "load,provisional,started", 0);
}

/**
 * @internal
 * Reports that the view's back / forward list has changed.
 *
 * Emits signal: "back,forward,list,changed".
 */
void ewk_view_back_forward_list_changed(Evas_Object* ewkView)
{
    evas_object_smart_callback_call(ewkView, "back,forward,list,changed", 0);
}

/**
 * @internal
 * Update the view's favicon and emits a "icon,changed" signal if it has
 * changed.
 *
 * This function is called whenever the URL has changed or when the icon for
 * the current page URL has changed.
 */
void ewk_view_update_icon(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    Ewk_Favicon_Database* iconDatabase = priv->context->faviconDatabase();
    ASSERT(iconDatabase);

    priv->faviconURL = ewk_favicon_database_icon_url_get(iconDatabase, priv->url);
    evas_object_smart_callback_call(ewkView, "icon,changed", 0);
}

/**
 * @internal
 * Reports that a navigation policy decision should be taken.
 *
 * Emits signal: "policy,decision,navigation".
 */
void ewk_view_navigation_policy_decision(Evas_Object* ewkView, Ewk_Navigation_Policy_Decision* decision)
{
    evas_object_smart_callback_call(ewkView, "policy,decision,navigation", decision);
}

/**
 * @internal
 * Reports that a new window policy decision should be taken.
 *
 * Emits signal: "policy,decision,new,window".
 */
void ewk_view_new_window_policy_decision(Evas_Object* ewkView, Ewk_Navigation_Policy_Decision* decision)
{
    evas_object_smart_callback_call(ewkView, "policy,decision,new,window", decision);
}

Eina_Bool ewk_view_html_string_load(Evas_Object* ewkView, const char* html, const char* baseUrl, const char* unreachableUrl)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(html, false);

    if (unreachableUrl && *unreachableUrl)
        priv->pageProxy->loadAlternateHTMLString(String::fromUTF8(html), baseUrl ? String::fromUTF8(baseUrl) : "", String::fromUTF8(unreachableUrl));
    else
        priv->pageProxy->loadHTMLString(String::fromUTF8(html), baseUrl ? String::fromUTF8(baseUrl) : "");
    ewk_view_url_update(ewkView);

    return true;
}

#if ENABLE(WEB_INTENTS_TAG)
/**
 * @internal
 * The view received a new intent service registration.
 *
 * Emits signal: "intent,service,register" with pointer to a Ewk_Intent_Service.
 */
void ewk_view_intent_service_register(Evas_Object* ewkView, const Ewk_Intent_Service* ewkIntentService)
{
    evas_object_smart_callback_call(ewkView, "intent,service,register", const_cast<Ewk_Intent_Service*>(ewkIntentService));
}
#endif // ENABLE(WEB_INTENTS_TAG)

const Evas_Object* ewk_view_from_page_get(const WebKit::WebPageProxy* page)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(page, 0);

    return pageViewMap().get(page);
}

WebPageProxy* ewk_view_page_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    return priv->pageProxy.get();
}

const char* ewk_view_setting_encoding_custom_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    String customEncoding = priv->pageProxy->customTextEncodingName();
    if (customEncoding.isEmpty())
        return 0;

    priv->customEncoding = customEncoding.utf8().data();

    return priv->customEncoding;
}

Eina_Bool ewk_view_setting_encoding_custom_set(Evas_Object* ewkView, const char* encoding)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->customEncoding = encoding;
    priv->pageProxy->setCustomTextEncodingName(encoding ? encoding : String());

    return true;
}

void ewk_view_page_close(Evas_Object* ewkView)
{
    evas_object_smart_callback_call(ewkView, "close,window", 0);
}

WKPageRef ewk_view_page_create(Evas_Object* ewkView)
{
    Evas_Object* newEwkView = 0;
    evas_object_smart_callback_call(ewkView, "create,window", &newEwkView);

    if (!newEwkView)
        return 0;

    return static_cast<WKPageRef>(WKRetain(ewk_view_page_get(newEwkView)));
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
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(text, false);

    priv->pageProxy->findString(String::fromUTF8(text), static_cast<WebKit::FindOptions>(options), maxMatchCount);

    return true;
}

Eina_Bool ewk_view_text_find_highlight_clear(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->pageProxy->hideFindUI();

    return true;
}

Eina_Bool ewk_view_text_matches_count(Evas_Object* ewkView, const char* text, Ewk_Find_Options options, unsigned maxMatchCount)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(text, false);

    priv->pageProxy->countStringMatches(String::fromUTF8(text), static_cast<WebKit::FindOptions>(options), maxMatchCount);

    return true;
}

void ewk_view_contents_size_changed(const Evas_Object* ewkView, const IntSize& size)
{
#if USE(COORDINATED_GRAPHICS)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    priv->pageViewportControllerClient->didChangeContentsSize(size);
#else
    UNUSED_PARAM(ewkView);
    UNUSED_PARAM(size);
#endif
}

COMPILE_ASSERT_MATCHING_ENUM(EWK_TEXT_DIRECTION_RIGHT_TO_LEFT, RTL);
COMPILE_ASSERT_MATCHING_ENUM(EWK_TEXT_DIRECTION_LEFT_TO_RIGHT, LTR);

void ewk_view_popup_menu_request(Evas_Object* ewkView, WebPopupMenuProxyEfl* popupMenu, const IntRect& rect, TextDirection textDirection, double pageScaleFactor, const Vector<WebPopupItem>& items, int32_t selectedIndex)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);

    ASSERT(popupMenu);

    if (!smartData->api->popup_menu_show)
        return;

    if (priv->popupMenuProxy)
        ewk_view_popup_menu_close(ewkView);
    priv->popupMenuProxy = popupMenu;

    Eina_List* popupItems = 0;
    size_t size = items.size();
    for (size_t i = 0; i < size; ++i)
        popupItems = eina_list_append(popupItems, Ewk_Popup_Menu_Item::create(items[i]).leakPtr());
    priv->popupMenuItems = popupItems;

    smartData->api->popup_menu_show(smartData, rect, static_cast<Ewk_Text_Direction>(textDirection), pageScaleFactor, popupItems, selectedIndex);
}

Eina_Bool ewk_view_popup_menu_close(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);

    if (!priv->popupMenuProxy)
        return false;

    priv->popupMenuProxy = 0;

    if (smartData->api->popup_menu_hide)
        smartData->api->popup_menu_hide(smartData);

    void* item;
    EINA_LIST_FREE(priv->popupMenuItems, item)
        delete static_cast<Ewk_Popup_Menu_Item*>(item);

    return true;
}

Eina_Bool ewk_view_popup_menu_select(Evas_Object* ewkView, unsigned int selectedIndex)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv->popupMenuProxy, false);

    if (selectedIndex >= eina_list_count(priv->popupMenuItems))
        return false;

    priv->popupMenuProxy->valueChanged(selectedIndex);

    return true;
}

Eina_Bool ewk_view_mouse_events_enabled_set(Evas_Object* ewkView, Eina_Bool enabled)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    enabled = !!enabled;
    if (priv->areMouseEventsEnabled == enabled)
        return true;

    priv->areMouseEventsEnabled = enabled;
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
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    return priv->areMouseEventsEnabled;
}

/**
 * @internal
 * Web process has crashed.
 *
 * Emits signal: "webprocess,crashed" with pointer to crash handling boolean.
 */
void ewk_view_webprocess_crashed(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    bool handled = false;
    evas_object_smart_callback_call(ewkView, "webprocess,crashed", &handled);

    if (!handled) {
        CString url = priv->pageProxy->urlAtProcessExit().utf8();
        WARN("WARNING: The web process experienced a crash on '%s'.\n", url.data());

        // Display an error page
        ewk_view_html_string_load(ewkView, "The web process has crashed.", 0, url.data());
    }
}

/**
 * @internal
 * Calls a smart member function for javascript alert().
 */
void ewk_view_run_javascript_alert(Evas_Object* ewkView, const WKEinaSharedString& message)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);

    if (!smartData->api->run_javascript_alert)
        return;

    smartData->api->run_javascript_alert(smartData, message);
}

/**
 * @internal
 * Calls a smart member function for javascript confirm() and returns a value from the function. Returns false by default.
 */
bool ewk_view_run_javascript_confirm(Evas_Object* ewkView, const WKEinaSharedString& message)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);

    if (!smartData->api->run_javascript_confirm)
        return false;

    return smartData->api->run_javascript_confirm(smartData, message);
}

/**
 * @internal
 * Calls a smart member function for javascript prompt() and returns a value from the function. Returns null string by default.
 */
WKEinaSharedString ewk_view_run_javascript_prompt(Evas_Object* ewkView, const WKEinaSharedString& message, const WKEinaSharedString& defaultValue)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, WKEinaSharedString());
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, WKEinaSharedString());

    if (!smartData->api->run_javascript_prompt)
        return WKEinaSharedString();

    return WKEinaSharedString::adopt(smartData->api->run_javascript_prompt(smartData, message, defaultValue));
}

#if ENABLE(INPUT_TYPE_COLOR)
/**
 * @internal
 * Requests to show external color picker.
 */
void ewk_view_color_picker_request(Evas_Object* ewkView, int r, int g, int b, int a, WKColorPickerResultListenerRef listener)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->input_picker_color_request);

    priv->colorPickerResultListener = listener;

    smartData->api->input_picker_color_request(smartData, r, g, b, a);
}

/**
 * @internal
 * Requests to hide external color picker.
 */
void ewk_view_color_picker_dismiss(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->input_picker_color_dismiss);

    priv->colorPickerResultListener.clear();

    smartData->api->input_picker_color_dismiss(smartData);
}
#endif

Eina_Bool ewk_view_color_picker_color_set(Evas_Object* ewkView, int r, int g, int b, int a)
{
#if ENABLE(INPUT_TYPE_COLOR)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv->colorPickerResultListener, false);

    WebCore::Color color = WebCore::Color(r, g, b, a);
    WKRetainPtr<WKStringRef> colorString(AdoptWK, WKStringCreateWithUTF8CString(color.serialized().utf8().data()));
    WKColorPickerResultListenerSetColor(priv->colorPickerResultListener.get(), colorString.get());
    priv->colorPickerResultListener.clear();

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
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    Evas_Point position = { smartData->view.x, smartData->view.y };
    // FIXME: Touch points do not take scroll position and scale into account when 
    // TILED_BACKING_STORE is enabled.
    priv->pageProxy->handleTouchEvent(NativeWebTouchEvent(type, points, modifiers, &position, ecore_time_get()));

    return true;
#else
    return false;
#endif
}

Eina_Bool ewk_view_touch_events_enabled_set(Evas_Object* ewkView, Eina_Bool enabled)
{
#if ENABLE(TOUCH_EVENTS)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    enabled = !!enabled;
    if (priv->areTouchEventsEnabled == enabled)
        return true;

    priv->areTouchEventsEnabled = enabled;
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
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    return priv->areTouchEventsEnabled;
#else
    return false;
#endif
}

Eina_Bool ewk_view_inspector_show(Evas_Object* ewkView)
{
#if ENABLE(INSPECTOR)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    WebInspectorProxy* inspector = priv->pageProxy->inspector();
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
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    WebInspectorProxy* inspector = priv->pageProxy->inspector();
    if (inspector)
        inspector->close();

    return true;
#else
    return false;
#endif
}
