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

#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "PageClientImpl.h"
#include "WKAPICast.h"
#include "WKEinaSharedString.h"
#include "WKFindOptions.h"
#include "WKRetainPtr.h"
#include "WKString.h"
#include "WKURL.h"
#include "ewk_back_forward_list_private.h"
#include "ewk_context.h"
#include "ewk_context_private.h"
#include "ewk_intent_private.h"
#include "ewk_private.h"
#include "ewk_view_find_client_private.h"
#include "ewk_view_form_client_private.h"
#include "ewk_view_loader_client_private.h"
#include "ewk_view_policy_client_private.h"
#include "ewk_view_private.h"
#include "ewk_view_resource_load_client_private.h"
#include "ewk_view_ui_client_private.h"
#include "ewk_web_resource.h"
#include <Ecore_Evas.h>
#include <Edje.h>
#include <WebCore/Cursor.h>
#include <WebCore/EflScreenUtilities.h>
#include <wtf/text/CString.h>

#if USE(ACCELERATED_COMPOSITING)
#include <Evas_GL.h>
#endif

#if USE(COORDINATED_GRAPHICS)
#include "EflViewportHandler.h"
#endif

using namespace WebKit;
using namespace WebCore;

static const char EWK_VIEW_TYPE_STR[] = "EWK2_View";

static const int defaultCursorSize = 16;

typedef HashMap<uint64_t, Ewk_Web_Resource*> LoadingResourcesMap;
static void _ewk_view_priv_loading_resources_clear(LoadingResourcesMap& loadingResourcesMap);

struct _Ewk_View_Private_Data {
    OwnPtr<PageClientImpl> pageClient;
#if USE(COORDINATED_GRAPHICS)
    OwnPtr<EflViewportHandler> viewportHandler;
#endif

    WKEinaSharedString uri;
    WKEinaSharedString title;
    WKEinaSharedString theme;
    WKEinaSharedString customEncoding;
    WKEinaSharedString cursorGroup;
    Evas_Object* cursorObject;
    LoadingResourcesMap loadingResourcesMap;
    Ewk_Back_Forward_List* backForwardList;

#ifdef HAVE_ECORE_X
    bool isUsingEcoreX;
#endif

#if USE(ACCELERATED_COMPOSITING)
    Evas_GL* evasGl;
    Evas_GL_Context* evasGlContext;
    Evas_GL_Surface* evasGlSurface;
#endif

    _Ewk_View_Private_Data()
        : cursorObject(0)
        , backForwardList(0)
#ifdef HAVE_ECORE_X
        , isUsingEcoreX(false)
#endif
#if USE(ACCELERATED_COMPOSITING)
        , evasGl(0)
        , evasGlContext(0)
        , evasGlSurface(0)
#endif
    { }

    ~_Ewk_View_Private_Data()
    {
        _ewk_view_priv_loading_resources_clear(loadingResourcesMap);

        if (cursorObject)
            evas_object_del(cursorObject);

        ewk_back_forward_list_free(backForwardList);
    }
};

#define EWK_VIEW_TYPE_CHECK(ewkView, result)                                   \
    bool result = true;                                                        \
    do {                                                                       \
        const char* _tmp_otype = evas_object_type_get(ewkView);                \
        const Evas_Smart* _tmp_s = evas_object_smart_smart_get(ewkView);       \
        if (EINA_UNLIKELY(!_tmp_s)) {                                          \
            EINA_LOG_CRIT                                                      \
                ("%p (%s) is not a smart object!",                             \
                 ewkView, _tmp_otype ? _tmp_otype : "(null)");                 \
            result = false;                                                    \
        }                                                                      \
        const Evas_Smart_Class* _tmp_sc = evas_smart_class_get(_tmp_s);        \
        if (EINA_UNLIKELY(!_tmp_sc)) {                                         \
            EINA_LOG_CRIT                                                      \
                ("%p (%s) is not a smart object!",                             \
                 ewkView, _tmp_otype ? _tmp_otype : "(null)");                 \
            result = false;                                                    \
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
        smartData = (Ewk_View_Smart_Data*)evas_object_smart_data_get(ewkView);

#define EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, ...)                     \
    EWK_VIEW_SD_GET(ewkView, smartData);                                       \
    if (!smartData) {                                                          \
        EINA_LOG_CRIT("no smart data for object %p (%s)",                      \
                 ewkView, evas_object_type_get(ewkView));                      \
        return __VA_ARGS__;                                                    \
    }

#define EWK_VIEW_PRIV_GET(smartData, priv)                                     \
    Ewk_View_Private_Data* priv = smartData->priv

#define EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, ...)                      \
    if (!smartData) {                                                          \
        EINA_LOG_CRIT("smart data is null");                                   \
        return __VA_ARGS__;                                                    \
    }                                                                          \
    EWK_VIEW_PRIV_GET(smartData, priv);                                        \
    if (!priv) {                                                               \
        EINA_LOG_CRIT("no private data for object %p (%s)",                    \
                 smartData->self, evas_object_type_get(smartData->self));      \
        return __VA_ARGS__;                                                    \
    }

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
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false)

    priv->pageClient->page()->viewStateDidChange(WebPageProxy::ViewIsFocused | WebPageProxy::ViewWindowIsActive);
    return true;
}

static Eina_Bool _ewk_view_smart_focus_out(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false)

    priv->pageClient->page()->viewStateDidChange(WebPageProxy::ViewIsFocused | WebPageProxy::ViewWindowIsActive);
    return true;
}

static Eina_Bool _ewk_view_smart_mouse_wheel(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Wheel* wheelEvent)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false)

    Evas_Point position = {smartData->view.x, smartData->view.y};
    priv->pageClient->page()->handleWheelEvent(NativeWebWheelEvent(wheelEvent, &position));
    return true;
}

static Eina_Bool _ewk_view_smart_mouse_down(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Down* downEvent)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false)

    Evas_Point position = {smartData->view.x, smartData->view.y};
    priv->pageClient->page()->handleMouseEvent(NativeWebMouseEvent(downEvent, &position));
    return true;
}

static Eina_Bool _ewk_view_smart_mouse_up(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Up* upEvent)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false)

    Evas_Point position = {smartData->view.x, smartData->view.y};
    priv->pageClient->page()->handleMouseEvent(NativeWebMouseEvent(upEvent, &position));
    return true;
}

static Eina_Bool _ewk_view_smart_mouse_move(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Move* moveEvent)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false)

    Evas_Point position = {smartData->view.x, smartData->view.y};
    priv->pageClient->page()->handleMouseEvent(NativeWebMouseEvent(moveEvent, &position));
    return true;
}

static Eina_Bool _ewk_view_smart_key_down(Ewk_View_Smart_Data* smartData, const Evas_Event_Key_Down* downEvent)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false)

    priv->pageClient->page()->handleKeyboardEvent(NativeWebKeyboardEvent(downEvent));
    return true;
}

static Eina_Bool _ewk_view_smart_key_up(Ewk_View_Smart_Data* smartData, const Evas_Event_Key_Up* upEvent)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false)

    priv->pageClient->page()->handleKeyboardEvent(NativeWebKeyboardEvent(upEvent));
    return true;
}

// Event Handling.
static void _ewk_view_on_focus_in(void* data, Evas* canvas, Evas_Object* ewkView, void* eventInfo)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->focus_in);
    smartData->api->focus_in(smartData);
}

static void _ewk_view_on_focus_out(void* data, Evas* canvas, Evas_Object* ewkView, void* eventInfo)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->focus_out);
    smartData->api->focus_out(smartData);
}

static void _ewk_view_on_mouse_wheel(void* data, Evas* canvas, Evas_Object* ewkView, void* eventInfo)
{
    Evas_Event_Mouse_Wheel* wheelEvent = static_cast<Evas_Event_Mouse_Wheel*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_wheel);
    smartData->api->mouse_wheel(smartData, wheelEvent);
}

static void _ewk_view_on_mouse_down(void* data, Evas* canvas, Evas_Object* ewkView, void* eventInfo)
{
    Evas_Event_Mouse_Down* downEvent = static_cast<Evas_Event_Mouse_Down*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_down);
    smartData->api->mouse_down(smartData, downEvent);
}

static void _ewk_view_on_mouse_up(void* data, Evas* canvas, Evas_Object* ewkView, void* eventInfo)
{
    Evas_Event_Mouse_Up* upEvent = static_cast<Evas_Event_Mouse_Up*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_up);
    smartData->api->mouse_up(smartData, upEvent);
}

static void _ewk_view_on_mouse_move(void* data, Evas* canvas, Evas_Object* ewkView, void* eventInfo)
{
    Evas_Event_Mouse_Move* moveEvent = static_cast<Evas_Event_Mouse_Move*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_move);
    smartData->api->mouse_move(smartData, moveEvent);
}

static void _ewk_view_on_key_down(void* data, Evas* canvas, Evas_Object* ewkView, void* eventInfo)
{
    Evas_Event_Key_Down* downEvent = static_cast<Evas_Event_Key_Down*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->key_down);
    smartData->api->key_down(smartData, downEvent);
}

static void _ewk_view_on_key_up(void* data, Evas* canvas, Evas_Object* ewkView, void* eventInfo)
{
    Evas_Event_Key_Up* upEvent = static_cast<Evas_Event_Key_Up*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->key_up);
    smartData->api->key_up(smartData, upEvent);
}

static Evas_Smart_Class g_parentSmartClass = EVAS_SMART_CLASS_INIT_NULL;

static Ewk_View_Private_Data* _ewk_view_priv_new(Ewk_View_Smart_Data* smartData)
{
    Ewk_View_Private_Data* priv = new Ewk_View_Private_Data;
    if (!priv) {
        EINA_LOG_CRIT("could not allocate Ewk_View_Private_Data");
        return 0;
    }

#ifdef HAVE_ECORE_X
    priv->isUsingEcoreX = WebCore::isUsingEcoreX(smartData->base.evas);
#endif

    return priv;
}

static void _ewk_view_priv_loading_resources_clear(LoadingResourcesMap& loadingResourcesMap)
{
    // Clear the loadingResources HashMap.
    LoadingResourcesMap::iterator it = loadingResourcesMap.begin();
    LoadingResourcesMap::iterator end = loadingResourcesMap.end();
    for ( ; it != end; ++it)
        ewk_web_resource_unref(it->second);

    loadingResourcesMap.clear();
}

static void _ewk_view_priv_del(Ewk_View_Private_Data* priv)
{
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

    smartData->priv = _ewk_view_priv_new(smartData);
    if (!smartData->priv) {
        EINA_LOG_CRIT("could not allocate _Ewk_View_Private_Data");
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

static void _ewk_view_smart_move(Evas_Object* ewkView, Evas_Coord x, Evas_Coord y)
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

    EINA_SAFETY_ON_NULL_RETURN_VAL(!priv->evasGl, false);

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
    Evas_Coord x, y, width, height;

    smartData->changed.any = false;

    evas_object_geometry_get(ewkView, &x, &y, &width, &height);

    if (smartData->changed.size) {
#if USE(COORDINATED_GRAPHICS)
        priv->viewportHandler->updateViewportSize(IntSize(width, height));
#endif

        if (priv->pageClient->page()->drawingArea())
            priv->pageClient->page()->drawingArea()->setSize(IntSize(width, height), IntSize());

#if USE(ACCELERATED_COMPOSITING)
        if (!priv->evasGlSurface)
            return;
        evas_gl_surface_destroy(priv->evasGl, priv->evasGlSurface);
        priv->evasGlSurface = 0;
        ewk_view_create_gl_surface(ewkView, IntSize(width, height));
        ewk_view_display(ewkView, IntRect(IntPoint(), IntSize(width, height)));
#endif

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
    priv->pageClient->page()->setDrawsBackground(red || green || blue);
    priv->pageClient->page()->setDrawsTransparentBackground(alpha < 255);

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

static void _ewk_view_initialize(Evas_Object* ewkView, Ewk_Context* context, WKPageGroupRef pageGroupRef)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv)
    EINA_SAFETY_ON_NULL_RETURN(context);

    if (priv->pageClient)
        return;

    priv->pageClient = PageClientImpl::create(toImpl(ewk_context_WKContext_get(context)), toImpl(pageGroupRef), ewkView);
    priv->backForwardList = ewk_back_forward_list_new(toAPI(priv->pageClient->page()->backForwardList()));

#if USE(COORDINATED_GRAPHICS)
    priv->viewportHandler = EflViewportHandler::create(priv->pageClient.get());
#endif

    WKPageRef wkPage = toAPI(priv->pageClient->page());
    ewk_view_find_client_attach(wkPage, ewkView);
    ewk_view_form_client_attach(wkPage, ewkView);
    ewk_view_loader_client_attach(wkPage, ewkView);
    ewk_view_policy_client_attach(wkPage, ewkView);
    ewk_view_resource_load_client_attach(wkPage, ewkView);
    ewk_view_ui_client_attach(wkPage, ewkView);

    ewk_view_theme_set(ewkView, DEFAULT_THEME_PATH"/default.edj");
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

    _ewk_view_initialize(ewkView, ewk_context_new_from_WKContext(contextRef), pageGroupRef);

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
    return ewk_view_smart_add(canvas, _ewk_view_smart_class_new(), context);
}

Evas_Object* ewk_view_add(Evas* canvas)
{
    return ewk_view_add_with_context(canvas, ewk_context_default_get());
}

/**
 * @internal
 * The uri of view was changed by the frame loader.
 *
 * Emits signal: "uri,changed" with pointer to new uri string.
 */
void ewk_view_uri_update(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    String activeURL = priv->pageClient->page()->activeURL();
    if (activeURL.isEmpty())
        return;

    if (priv->uri == activeURL.utf8().data())
        return;

    priv->uri = activeURL.utf8().data();
    const char* callbackArgument = static_cast<const char*>(priv->uri);
    evas_object_smart_callback_call(ewkView, "uri,changed", const_cast<char*>(callbackArgument));
}

Eina_Bool ewk_view_uri_set(Evas_Object* ewkView, const char* uri)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(uri, false);

    priv->pageClient->page()->loadURL(uri);
    ewk_view_uri_update(ewkView);

    return true;
}

const char* ewk_view_uri_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    return priv->uri;
}

Eina_Bool ewk_view_reload(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->pageClient->page()->reload(/*reloadFromOrigin*/ false);
    ewk_view_uri_update(ewkView);

    return true;
}

Eina_Bool ewk_view_reload_bypass_cache(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->pageClient->page()->reload(/*reloadFromOrigin*/ true);
    ewk_view_uri_update(ewkView);

    return true;
}

Eina_Bool ewk_view_stop(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->pageClient->page()->stopLoading();

    return true;
}

/**
 * @internal
 * Load was initiated for a resource in the view.
 *
 * Emits signal: "resource,request,new" with pointer to resource request.
 */
void ewk_view_resource_load_initiated(Evas_Object* ewkView, uint64_t resourceIdentifier, Ewk_Web_Resource* resource, Ewk_Url_Request* request)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    Ewk_Web_Resource_Request resourceRequest = {resource, request, 0};

    // Keep the resource internally to reuse it later.
    ewk_web_resource_ref(resource);
    priv->loadingResourcesMap.add(resourceIdentifier, resource);

    evas_object_smart_callback_call(ewkView, "resource,request,new", &resourceRequest);
}

/**
 * @internal
 * Received a response to a resource load request in the view.
 *
 * Emits signal: "resource,request,response" with pointer to resource response.
 */
void ewk_view_resource_load_response(Evas_Object* ewkView, uint64_t resourceIdentifier, Ewk_Url_Response* response)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    if (!priv->loadingResourcesMap.contains(resourceIdentifier))
        return;

    Ewk_Web_Resource* resource = priv->loadingResourcesMap.get(resourceIdentifier);
    Ewk_Web_Resource_Load_Response resourceLoadResponse = {resource, response};
    evas_object_smart_callback_call(ewkView, "resource,request,response", &resourceLoadResponse);
}

/**
 * @internal
 * Failed loading a resource in the view.
 *
 * Emits signal: "resource,request,finished" with pointer to the resource load error.
 */
void ewk_view_resource_load_failed(Evas_Object* ewkView, uint64_t resourceIdentifier, Ewk_Web_Error* error)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    if (!priv->loadingResourcesMap.contains(resourceIdentifier))
        return;

    Ewk_Web_Resource* resource = priv->loadingResourcesMap.get(resourceIdentifier);
    Ewk_Web_Resource_Load_Error resourceLoadError = {resource, error};
    evas_object_smart_callback_call(ewkView, "resource,request,failed", &resourceLoadError);
}

/**
 * @internal
 * Finished loading a resource in the view.
 *
 * Emits signal: "resource,request,finished" with pointer to the resource.
 */
void ewk_view_resource_load_finished(Evas_Object* ewkView, uint64_t resourceIdentifier)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    if (!priv->loadingResourcesMap.contains(resourceIdentifier))
        return;

    Ewk_Web_Resource* resource = priv->loadingResourcesMap.take(resourceIdentifier);
    evas_object_smart_callback_call(ewkView, "resource,request,finished", resource);

    ewk_web_resource_unref(resource);
}

/**
 * @internal
 * Request was sent for a resource in the view.
 *
 * Emits signal: "resource,request,sent" with pointer to resource request and possible redirect response.
 */
void ewk_view_resource_request_sent(Evas_Object* ewkView, uint64_t resourceIdentifier, Ewk_Url_Request* request, Ewk_Url_Response* redirectResponse)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    if (!priv->loadingResourcesMap.contains(resourceIdentifier))
        return;

    Ewk_Web_Resource* resource = priv->loadingResourcesMap.get(resourceIdentifier);
    Ewk_Web_Resource_Request resourceRequest = {resource, request, redirectResponse};

    evas_object_smart_callback_call(ewkView, "resource,request,sent", &resourceRequest);
}

const char* ewk_view_title_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    CString title = priv->pageClient->page()->pageTitle().utf8();
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
 * The view title was changed by the frame loader.
 *
 * Emits signal: "title,changed" with pointer to new title string.
 */
void ewk_view_title_changed(Evas_Object* ewkView, const char* title)
{
    evas_object_smart_callback_call(ewkView, "title,changed", const_cast<char*>(title));
}

double ewk_view_load_progress_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1.0);

    return priv->pageClient->page()->estimatedProgress();
}

Eina_Bool ewk_view_scale_set(Evas_Object* ewkView, double scaleFactor, int x, int y)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->pageClient->page()->scalePage(scaleFactor, IntPoint(x, y));
    return true;
}

double ewk_view_scale_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1);

    return priv->pageClient->page()->pageScaleFactor();
}

Eina_Bool ewk_view_device_pixel_ratio_set(Evas_Object* ewkView, float ratio)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->pageClient->page()->setCustomDeviceScaleFactor(ratio);

    return true;
}

float ewk_view_device_pixel_ratio_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1.0);

    return priv->pageClient->page()->deviceScaleFactor();
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
        priv->pageClient->page()->setThemePath(path);
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

    if (priv->cursorObject)
        evas_object_del(priv->cursorObject);
    priv->cursorObject = edje_object_add(smartData->base.evas);

    Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(smartData->base.evas);
    if (!priv->theme || !edje_object_file_set(priv->cursorObject, priv->theme, group)) {
        evas_object_del(priv->cursorObject);
        priv->cursorObject = 0;

        ecore_evas_object_cursor_set(ecoreEvas, 0, 0, 0, 0);
#ifdef HAVE_ECORE_X
        if (priv->isUsingEcoreX)
            WebCore::applyFallbackCursor(ecoreEvas, group);
#endif
        return;
    }

    Evas_Coord width, height;
    edje_object_size_min_get(priv->cursorObject, &width, &height);
    if (width <= 0 || height <= 0)
        edje_object_size_min_calc(priv->cursorObject, &width, &height);
    if (width <= 0 || height <= 0) {
        width = defaultCursorSize;
        height = defaultCursorSize;
    }
    evas_object_resize(priv->cursorObject, width, height);

    const char* data;
    int hotspotX = 0;
    data = edje_object_data_get(priv->cursorObject, "hot.x");
    if (data)
        hotspotX = atoi(data);

    int hotspotY = 0;
    data = edje_object_data_get(priv->cursorObject, "hot.y");
    if (data)
        hotspotY = atoi(data);

    ecore_evas_object_cursor_set(ecoreEvas, priv->cursorObject, EVAS_LAYER_MAX, hotspotX, hotspotY);
}

void ewk_view_display(Evas_Object* ewkView, const IntRect& rect)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    if (!smartData->image)
        return;

#if USE(COORDINATED_GRAPHICS)
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    evas_gl_make_current(priv->evasGl, priv->evasGlSurface, priv->evasGlContext);
    priv->viewportHandler->display(rect);
#endif

    evas_object_image_data_update_add(smartData->image, rect.x(), rect.y(), rect.width(), rect.height());
}

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
void ewk_view_download_job_failed(Evas_Object* ewkView, Ewk_Download_Job* download, Ewk_Web_Error* error)
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

    WebPageProxy* page = priv->pageClient->page();
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

    WebPageProxy* page = priv->pageClient->page();
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

    WebPageProxy* page = priv->pageClient->page();
    page->deliverIntentToFrame(page->mainFrame(), toImpl(ewk_intent_WKIntentDataRef_get(intent)));

    return true;
#else
    return false;
#endif
}

Eina_Bool ewk_view_back_possible(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    return priv->pageClient->page()->canGoBack();
}

Eina_Bool ewk_view_forward_possible(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    return priv->pageClient->page()->canGoForward();
}

Ewk_Back_Forward_List* ewk_view_back_forward_list_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    return priv->backForwardList;
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
 * Emits signal: "load,error" with pointer to Ewk_Web_Error.
 */
void ewk_view_load_error(Evas_Object* ewkView, const Ewk_Web_Error* error)
{
    evas_object_smart_callback_call(ewkView, "load,error", const_cast<Ewk_Web_Error*>(error));
}

/**
 * @internal
 * Reports load finished.
 *
 * Emits signal: "load,finished".
 */
void ewk_view_load_finished(Evas_Object* ewkView)
{
    ewk_view_uri_update(ewkView);
    evas_object_smart_callback_call(ewkView, "load,finished", 0);
}

/**
 * @internal
 * Reports view provisional load failed with error information.
 *
 * Emits signal: "load,provisional,failed" with pointer to Ewk_Web_Error.
 */
void ewk_view_load_provisional_failed(Evas_Object* ewkView, const Ewk_Web_Error* error)
{
    evas_object_smart_callback_call(ewkView, "load,provisional,failed", const_cast<Ewk_Web_Error*>(error));
}

/**
 * @internal
 * Reports view received redirect for provisional load.
 *
 * Emits signal: "load,provisional,redirect".
 */
void ewk_view_load_provisional_redirect(Evas_Object* ewkView)
{
    ewk_view_uri_update(ewkView);
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
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    // The main frame started provisional load, we should clear
    // the loadingResources HashMap to start clean.
    _ewk_view_priv_loading_resources_clear(priv->loadingResourcesMap);

    ewk_view_uri_update(ewkView);
    evas_object_smart_callback_call(ewkView, "load,provisional,started", 0);
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
        priv->pageClient->page()->loadAlternateHTMLString(String::fromUTF8(html), baseUrl ? String::fromUTF8(baseUrl) : "", String::fromUTF8(unreachableUrl));
    else
        priv->pageClient->page()->loadHTMLString(String::fromUTF8(html), baseUrl ? String::fromUTF8(baseUrl) : "");
    ewk_view_uri_update(ewkView);

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

WebPageProxy* ewk_view_page_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    return priv->pageClient->page();
}

const char* ewk_view_setting_encoding_custom_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    String customEncoding = priv->pageClient->page()->customTextEncodingName();
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
    priv->pageClient->page()->setCustomTextEncodingName(encoding ? encoding : String());

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

Eina_Bool ewk_view_text_find(Evas_Object* ewkView, const char* text, Ewk_Find_Options options, unsigned int maxMatchCount)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(text, false);

    WKRetainPtr<WKStringRef> findText(AdoptWK, WKStringCreateWithUTF8CString(text));
    WKPageFindString(toAPI(priv->pageClient->page()), findText.get(), static_cast<WKFindOptions>(options), maxMatchCount);

    return true;
}

Eina_Bool ewk_view_text_find_highlight_clear(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    WKPageHideFindUI(toAPI(priv->pageClient->page()));

    return true;
}

void ewk_view_contents_size_changed(const Evas_Object* ewkView, const IntSize& size)
{
#if USE(COORDINATED_GRAPHICS)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    priv->viewportHandler->didChangeContentsSize(size);
#endif
}
