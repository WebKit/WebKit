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
#include "EwkView.h"

#include "ContextMenuClientEfl.h"
#include "CoordinatedLayerTreeHostProxy.h"
#include "EflScreenUtilities.h"
#include "FindClientEfl.h"
#include "FormClientEfl.h"
#include "InputMethodContextEfl.h"
#include "PageClientBase.h"
#include "PageClientDefaultImpl.h"
#include "PageClientLegacyImpl.h"
#include "PageLoadClientEfl.h"
#include "PagePolicyClientEfl.h"
#include "PageUIClientEfl.h"
#include "SnapshotImageGL.h"
#include "WKDictionary.h"
#include "WKGeometry.h"
#include "WKNumber.h"
#include "WKString.h"
#include "WebContext.h"
#include "WebImage.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPopupMenuProxyEfl.h"
#include "WebPreferences.h"
#include "ewk_back_forward_list_private.h"
#include "ewk_color_picker_private.h"
#include "ewk_context_menu_private.h"
#include "ewk_context_private.h"
#include "ewk_favicon_database_private.h"
#include "ewk_popup_menu_item_private.h"
#include "ewk_popup_menu_private.h"
#include "ewk_private.h"
#include "ewk_security_origin_private.h"
#include "ewk_settings_private.h"
#include "ewk_view.h"
#include "ewk_view_private.h"
#include "ewk_window_features_private.h"
#include <Ecore_Evas.h>
#include <Ecore_X.h>
#include <Edje.h>
#include <WebCore/CairoUtilitiesEfl.h>
#include <WebCore/CoordinatedGraphicsScene.h>
#include <WebCore/Cursor.h>
#include <WebKit2/WKImageCairo.h>

#if ENABLE(VIBRATION)
#include "VibrationClientEfl.h"
#endif

#if ENABLE(FULLSCREEN_API)
#include "WebFullScreenManagerProxy.h"
#endif

#if USE(ACCELERATED_COMPOSITING)
#include <Evas_GL.h>
#endif

using namespace EwkViewCallbacks;
using namespace WebCore;
using namespace WebKit;

static const int defaultCursorSize = 16;

typedef HashMap<WKPageRef, Evas_Object*> PageViewMap;

static inline PageViewMap& pageViewMap()
{
    DEFINE_STATIC_LOCAL(PageViewMap, map, ());
    return map;
}

void EwkView::addToPageViewMap(EwkView* view)
{
    PageViewMap::AddResult result = pageViewMap().add(view->wkPage(), view->view());
    ASSERT_UNUSED(result, result.isNewEntry);
}

void EwkView::removeFromPageViewMap(EwkView* view)
{
    ASSERT(pageViewMap().contains(view->wkPage()));
    pageViewMap().remove(view->wkPage());
}

const Evas_Object* EwkView::viewFromPageViewMap(const WKPageRef page)
{
    ASSERT(page);

    return pageViewMap().get(page);
}

EwkView::EwkView(Evas_Object* evasObject, PassRefPtr<EwkContext> context, PassRefPtr<WebPageGroup> pageGroup, ViewBehavior behavior)
    : m_evasObject(evasObject)
    , m_context(context)
#if USE(ACCELERATED_COMPOSITING)
    , m_pendingSurfaceResize(false)
#endif
    , m_pageClient(behavior == DefaultBehavior ? PageClientDefaultImpl::create(this) : PageClientLegacyImpl::create(this))
    , m_pageProxy(toImpl(m_context->wkContext())->createWebPage(m_pageClient.get(), pageGroup.get()))
    , m_pageLoadClient(PageLoadClientEfl::create(this))
    , m_pagePolicyClient(PagePolicyClientEfl::create(this))
    , m_pageUIClient(PageUIClientEfl::create(this))
    , m_contextMenuClient(ContextMenuClientEfl::create(this))
    , m_findClient(FindClientEfl::create(this))
    , m_formClient(FormClientEfl::create(this))
#if ENABLE(VIBRATION)
    , m_vibrationClient(VibrationClientEfl::create(this))
#endif
    , m_backForwardList(EwkBackForwardList::create(toAPI(m_pageProxy->backForwardList())))
#if USE(TILED_BACKING_STORE)
    , m_pageScaleFactor(1)
#endif
    , m_settings(EwkSettings::create(this))
    , m_cursorIdentifier(0)
    , m_mouseEventsEnabled(false)
#if ENABLE(TOUCH_EVENTS)
    , m_touchEventsEnabled(false)
#endif
    , m_displayTimer(this, &EwkView::displayTimerFired)
    , m_inputMethodContext(InputMethodContextEfl::create(this, smartData()->base.evas))
    , m_isHardwareAccelerated(true)
    , m_setDrawsBackground(false)
{
    ASSERT(m_evasObject);
    ASSERT(m_context);
    ASSERT(m_pageProxy);

#if USE(COORDINATED_GRAPHICS)
    m_pageProxy->pageGroup()->preferences()->setAcceleratedCompositingEnabled(true);
    m_pageProxy->pageGroup()->preferences()->setForceCompositingMode(true);
    char* debugVisualsEnvironment = getenv("WEBKIT_SHOW_COMPOSITING_DEBUG_VISUALS");
    bool showDebugVisuals = debugVisualsEnvironment && !strcmp(debugVisualsEnvironment, "1");
    m_pageProxy->pageGroup()->preferences()->setCompositingBordersVisible(showDebugVisuals);
    m_pageProxy->pageGroup()->preferences()->setCompositingRepaintCountersVisible(showDebugVisuals);
#if ENABLE(WEBGL)
    m_pageProxy->pageGroup()->preferences()->setWebGLEnabled(true);
#endif
    if (behavior == DefaultBehavior)
        m_pageProxy->setUseFixedLayout(true);
#endif

    m_pageProxy->initializeWebPage();

#if ENABLE(FULLSCREEN_API)
    m_pageProxy->fullScreenManager()->setWebView(m_evasObject);
    m_pageProxy->pageGroup()->preferences()->setFullScreenEnabled(true);
#endif
#if ENABLE(WEB_AUDIO)
    m_pageProxy->pageGroup()->preferences()->setWebAudioEnabled(true);
#endif

    m_pageProxy->pageGroup()->preferences()->setOfflineWebApplicationCacheEnabled(true);

    // Enable mouse events by default
    setMouseEventsEnabled(true);

    // Listen for favicon changes.
    EwkFaviconDatabase* iconDatabase = m_context->faviconDatabase();
    ASSERT(iconDatabase);

    iconDatabase->watchChanges(IconChangeCallbackData(EwkView::onFaviconChanged, this));

    EwkView::addToPageViewMap(this);
}

EwkView::~EwkView()
{
    m_pageProxy->close();

    // Unregister icon change callback.
    EwkFaviconDatabase* iconDatabase = m_context->faviconDatabase();
    ASSERT(iconDatabase);

    iconDatabase->unwatchChanges(EwkView::onFaviconChanged);

    EwkView::removeFromPageViewMap(this);
}

Ewk_View_Smart_Data* EwkView::smartData() const
{
    return static_cast<Ewk_View_Smart_Data*>(evas_object_smart_data_get(m_evasObject));
}

EwkView* EwkView::fromEvasObject(const Evas_Object* view)
{
    ASSERT(view);
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(evas_object_smart_data_get(view));
    ASSERT(sd);
    ASSERT(sd->priv);
    return sd->priv;
}

/**
 * @internal
 * Retrieves the internal WKPage for this view.
 */
WKPageRef EwkView::wkPage()
{
    return toAPI(m_pageProxy.get());
}

void EwkView::setCursor(const Cursor& cursor)
{
    if (cursor.image()) {
        // Custom cursor.
        if (cursor.image() == m_cursorIdentifier)
            return;

        m_cursorIdentifier = cursor.image();

        Ewk_View_Smart_Data* sd = smartData();
        RefPtr<Evas_Object> cursorObject = adoptRef(cursor.image()->getEvasObject(sd->base.evas));
        if (!cursorObject)
            return;

        // Resize cursor.
        evas_object_resize(cursorObject.get(), cursor.image()->size().width(), cursor.image()->size().height());

        // Get cursor hot spot.
        IntPoint hotSpot;
        cursor.image()->getHotSpot(hotSpot);

        Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(sd->base.evas);
        // ecore_evas takes care of freeing the cursor object.
        ecore_evas_object_cursor_set(ecoreEvas, cursorObject.release().leakRef(), EVAS_LAYER_MAX, hotSpot.x(), hotSpot.y());

        return;
    }

    // Standard cursor.
    const char* group = cursor.platformCursor();
    if (!group || group == m_cursorIdentifier)
        return;

    m_cursorIdentifier = group;
    Ewk_View_Smart_Data* sd = smartData();
    RefPtr<Evas_Object> cursorObject = adoptRef(edje_object_add(sd->base.evas));

    Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(sd->base.evas);
    if (!m_theme || !edje_object_file_set(cursorObject.get(), m_theme, group)) {
        ecore_evas_object_cursor_set(ecoreEvas, 0, 0, 0, 0);
#ifdef HAVE_ECORE_X
        if (WebCore::isUsingEcoreX(sd->base.evas))
            WebCore::applyFallbackCursor(ecoreEvas, group);
#endif
        return;
    }

    // Set cursor size.
    Evas_Coord width, height;
    edje_object_size_min_get(cursorObject.get(), &width, &height);
    if (width <= 0 || height <= 0)
        edje_object_size_min_calc(cursorObject.get(), &width, &height);
    if (width <= 0 || height <= 0) {
        width = defaultCursorSize;
        height = defaultCursorSize;
    }
    evas_object_resize(cursorObject.get(), width, height);

    // Get cursor hot spot.
    const char* data;
    int hotspotX = 0;
    data = edje_object_data_get(cursorObject.get(), "hot.x");
    if (data)
        hotspotX = atoi(data);

    int hotspotY = 0;
    data = edje_object_data_get(cursorObject.get(), "hot.y");
    if (data)
        hotspotY = atoi(data);

    // ecore_evas takes care of freeing the cursor object.
    ecore_evas_object_cursor_set(ecoreEvas, cursorObject.release().leakRef(), EVAS_LAYER_MAX, hotspotX, hotspotY);
}

void EwkView::setDeviceScaleFactor(float scale)
{
    page()->setIntrinsicDeviceScaleFactor(scale);
}

float EwkView::deviceScaleFactor() const
{
    return m_pageProxy->deviceScaleFactor();
}

AffineTransform EwkView::transformFromScene() const
{
    AffineTransform transform;

#if USE(TILED_BACKING_STORE)
    // Note that we apply both page and device scale factors.
    transform.scale(1 / pageScaleFactor());
    transform.scale(1 / deviceScaleFactor());
    transform.translate(pagePosition().x(), pagePosition().y());
#endif

    Ewk_View_Smart_Data* sd = smartData();
    transform.translate(-sd->view.x, -sd->view.y);

    return transform;
}

AffineTransform EwkView::transformToScene() const
{
    return transformFromScene().inverse();
}

AffineTransform EwkView::transformToScreen() const
{
    AffineTransform transform;

    int windowGlobalX = 0;
    int windowGlobalY = 0;

    Ewk_View_Smart_Data* sd = smartData();

#ifdef HAVE_ECORE_X
    Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(sd->base.evas);

    Ecore_X_Window window;
#if USE(ACCELERATED_COMPOSITING)
    window = ecore_evas_gl_x11_window_get(ecoreEvas);
    // Fallback to software mode if necessary.
    if (!window)
#endif
    window = ecore_evas_software_x11_window_get(ecoreEvas); // Returns 0 if none.

    int x, y; // x, y are relative to parent (in a reparenting window manager).
    while (window) {
        ecore_x_window_geometry_get(window, &x, &y, 0, 0);
        windowGlobalX += x;
        windowGlobalY += y;
        window = ecore_x_window_parent_get(window);
    }
#endif

    transform.translate(-sd->view.x, -sd->view.y);
    transform.translate(windowGlobalX, windowGlobalY);

    return transform;
}

#if USE(COORDINATED_GRAPHICS)
CoordinatedGraphicsScene* EwkView::coordinatedGraphicsScene()
{
    DrawingAreaProxy* drawingArea = page()->drawingArea();
    if (!drawingArea)
        return 0;

    WebKit::CoordinatedLayerTreeHostProxy* coordinatedLayerTreeHostProxy = drawingArea->coordinatedLayerTreeHostProxy();
    if (!coordinatedLayerTreeHostProxy)
        return 0;

    return coordinatedLayerTreeHostProxy->coordinatedGraphicsScene();
}
#endif

void EwkView::displayTimerFired(Timer<EwkView>*)
{
#if USE(COORDINATED_GRAPHICS)
    Ewk_View_Smart_Data* sd = smartData();

    if (m_pendingSurfaceResize) {
        // Create a GL surface here so that Evas has no chance of painting to an empty GL surface.
        if (!createGLSurface(IntSize(sd->view.w, sd->view.h)))
            return;

        m_pendingSurfaceResize = false;
    }

    evas_gl_make_current(m_evasGL.get(), m_evasGLSurface->surface(), m_evasGLContext->context());

    // We are supposed to clip to the actual viewport, nothing less.
    IntRect viewport(sd->view.x, sd->view.y, sd->view.w, sd->view.h);

    CoordinatedGraphicsScene* scene = coordinatedGraphicsScene();
    if (!scene)
        return;

    scene->setActive(true);
    scene->setDrawsBackground(m_setDrawsBackground);
    if (m_isHardwareAccelerated) {
        scene->paintToCurrentGLContext(transformToScene().toTransformationMatrix(), /* opacity */ 1, viewport);
        // sd->image is tied to a native surface. The native surface is in the parent's coordinates,
        // so we need to account for the viewport position when calling evas_object_image_data_update_add.
        evas_object_image_data_update_add(sd->image, viewport.x(), viewport.y(), viewport.width(), viewport.height());
    } else {
        RefPtr<cairo_surface_t> surface = createSurfaceForImage(sd->image);
        if (!surface)
            return;

        RefPtr<cairo_t> graphicsContext = adoptRef(cairo_create(surface.get()));
        cairo_translate(graphicsContext.get(), - pagePosition().x(), - pagePosition().y());
        cairo_scale(graphicsContext.get(), pageScaleFactor(), pageScaleFactor());
        cairo_scale(graphicsContext.get(), deviceScaleFactor(), deviceScaleFactor());
        scene->paintToGraphicsContext(graphicsContext.get());
        evas_object_image_data_update_add(sd->image, 0, 0, viewport.width(), viewport.height());
    }
#endif
}

void EwkView::update(const IntRect& rect)
{
    Ewk_View_Smart_Data* sd = smartData();
#if USE(COORDINATED_GRAPHICS)
    // Coordinated graphices needs to schedule an full update, not
    // repainting of a region. Update in the event loop.
    UNUSED_PARAM(rect);

    // Guard for zero sized viewport.
    if (!(sd->view.w && sd->view.h))
        return;

    if (!m_displayTimer.isActive())
        m_displayTimer.startOneShot(0);
#else
    if (!sd->image)
        return;

    evas_object_image_data_update_add(sd->image, rect.x(), rect.y(), rect.width(), rect.height());
#endif
}

#if ENABLE(FULLSCREEN_API)
/**
 * @internal
 * Calls fullscreen_enter callback or falls back to default behavior and enables fullscreen mode.
 */
void EwkView::enterFullScreen()
{
    Ewk_View_Smart_Data* sd = smartData();

    RefPtr<EwkSecurityOrigin> origin = EwkSecurityOrigin::create(KURL(ParsedURLString, String::fromUTF8(m_url)));

    if (!sd->api->fullscreen_enter || !sd->api->fullscreen_enter(sd, origin.get())) {
        Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(sd->base.evas);
        ecore_evas_fullscreen_set(ecoreEvas, true);
    }
}

/**
 * @internal
 * Calls fullscreen_exit callback or falls back to default behavior and disables fullscreen mode.
 */
void EwkView::exitFullScreen()
{
    Ewk_View_Smart_Data* sd = smartData();

    if (!sd->api->fullscreen_exit || !sd->api->fullscreen_exit(sd)) {
        Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(sd->base.evas);
        ecore_evas_fullscreen_set(ecoreEvas, false);
    }
}
#endif

WKRect EwkView::windowGeometry() const
{
    Evas_Coord x, y, width, height;
    Ewk_View_Smart_Data* sd = smartData();

    if (!sd->api->window_geometry_get || !sd->api->window_geometry_get(sd, &x, &y, &width, &height)) {
        Ecore_Evas* ee = ecore_evas_ecore_evas_get(sd->base.evas);
        ecore_evas_request_geometry_get(ee, &x, &y, &width, &height);
    }

    return WKRectMake(x, y, width, height);
}

void EwkView::setWindowGeometry(const WKRect& rect)
{
    Ewk_View_Smart_Data* sd = smartData();

    if (!sd->api->window_geometry_set || !sd->api->window_geometry_set(sd, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height)) {
        Ecore_Evas* ee = ecore_evas_ecore_evas_get(sd->base.evas);
        ecore_evas_move_resize(ee, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
    }
}

void EwkView::setImageData(void* imageData, const IntSize& size)
{
    Ewk_View_Smart_Data* sd = smartData();
    if (!imageData || !sd->image)
        return;

    evas_object_resize(sd->image, size.width(), size.height());
    evas_object_image_size_set(sd->image, size.width(), size.height());
    evas_object_image_data_copy_set(sd->image, imageData);
}

IntSize EwkView::size() const
{
    int width, height;
    evas_object_geometry_get(m_evasObject, 0, 0, &width, &height);
    return IntSize(width, height);
}

bool EwkView::isFocused() const
{
    return evas_object_focus_get(m_evasObject);
}

bool EwkView::isVisible() const
{
    return evas_object_visible_get(m_evasObject);
}

const char* EwkView::title() const
{
    m_title = m_pageProxy->pageTitle().utf8().data();

    return m_title;
}

/**
 * @internal
 * This function may return @c NULL.
 */
InputMethodContextEfl* EwkView::inputMethodContext()
{
    return m_inputMethodContext.get();
}

const char* EwkView::themePath() const
{
    return m_theme;
}

void EwkView::setThemePath(const char* theme)
{
    if (m_theme != theme) {
        m_theme = theme;
        m_pageProxy->setThemePath(theme);
    }
}

const char* EwkView::customTextEncodingName() const
{
    String customEncoding = m_pageProxy->customTextEncodingName();
    if (customEncoding.isEmpty())
        return 0;

    m_customEncoding = customEncoding.utf8().data();

    return m_customEncoding;
}

void EwkView::setCustomTextEncodingName(const String& encoding)
{
    m_pageProxy->setCustomTextEncodingName(encoding);
}

void EwkView::setMouseEventsEnabled(bool enabled)
{
    if (m_mouseEventsEnabled == enabled)
        return;

    m_mouseEventsEnabled = enabled;
    if (enabled) {
        Ewk_View_Smart_Data* sd = smartData();
        evas_object_event_callback_add(m_evasObject, EVAS_CALLBACK_MOUSE_DOWN, onMouseDown, sd);
        evas_object_event_callback_add(m_evasObject, EVAS_CALLBACK_MOUSE_UP, onMouseUp, sd);
        evas_object_event_callback_add(m_evasObject, EVAS_CALLBACK_MOUSE_MOVE, onMouseMove, sd);
    } else {
        evas_object_event_callback_del(m_evasObject, EVAS_CALLBACK_MOUSE_DOWN, onMouseDown);
        evas_object_event_callback_del(m_evasObject, EVAS_CALLBACK_MOUSE_UP, onMouseUp);
        evas_object_event_callback_del(m_evasObject, EVAS_CALLBACK_MOUSE_MOVE, onMouseMove);
    }
}

#if ENABLE(TOUCH_EVENTS)
void EwkView::setTouchEventsEnabled(bool enabled)
{
    if (m_touchEventsEnabled == enabled)
        return;

    m_touchEventsEnabled = enabled;

    if (enabled) {
        // FIXME: We have to connect touch callbacks with mouse and multi events
        // because the Evas creates mouse events for first touch and multi events
        // for second and third touches. Below codes should be fixed when the Evas
        // supports the touch events.
        // See https://bugs.webkit.org/show_bug.cgi?id=97785 for details.
        Ewk_View_Smart_Data* sd = smartData();
        evas_object_event_callback_add(m_evasObject, EVAS_CALLBACK_MOUSE_DOWN, onTouchDown, sd);
        evas_object_event_callback_add(m_evasObject, EVAS_CALLBACK_MOUSE_UP, onTouchUp, sd);
        evas_object_event_callback_add(m_evasObject, EVAS_CALLBACK_MOUSE_MOVE, onTouchMove, sd);
        evas_object_event_callback_add(m_evasObject, EVAS_CALLBACK_MULTI_DOWN, onTouchDown, sd);
        evas_object_event_callback_add(m_evasObject, EVAS_CALLBACK_MULTI_UP, onTouchUp, sd);
        evas_object_event_callback_add(m_evasObject, EVAS_CALLBACK_MULTI_MOVE, onTouchMove, sd);
    } else {
        evas_object_event_callback_del(m_evasObject, EVAS_CALLBACK_MOUSE_DOWN, onTouchDown);
        evas_object_event_callback_del(m_evasObject, EVAS_CALLBACK_MOUSE_UP, onTouchUp);
        evas_object_event_callback_del(m_evasObject, EVAS_CALLBACK_MOUSE_MOVE, onTouchMove);
        evas_object_event_callback_del(m_evasObject, EVAS_CALLBACK_MULTI_DOWN, onTouchDown);
        evas_object_event_callback_del(m_evasObject, EVAS_CALLBACK_MULTI_UP, onTouchUp);
        evas_object_event_callback_del(m_evasObject, EVAS_CALLBACK_MULTI_MOVE, onTouchMove);
    }
}
#endif

/**
 * @internal
 * Update the view's favicon and emits a "icon,changed" signal if it has
 * changed.
 *
 * This function is called whenever the URL has changed or when the icon for
 * the current page URL has changed.
 */
void EwkView::informIconChange()
{
    EwkFaviconDatabase* iconDatabase = m_context->faviconDatabase();
    ASSERT(iconDatabase);

    m_faviconURL = ewk_favicon_database_icon_url_get(iconDatabase, m_url);
    smartCallback<IconChanged>().call();
}

#if USE(ACCELERATED_COMPOSITING)
bool EwkView::createGLSurface(const IntSize& viewSize)
{
    if (!m_isHardwareAccelerated)
        return true;

    if (!m_evasGL) {
        Evas* evas = evas_object_evas_get(m_evasObject);
        m_evasGL = adoptPtr(evas_gl_new(evas));
        if (!m_evasGL) {
            WARN("Failed to create Evas_GL, falling back to software mode.");
            m_isHardwareAccelerated = false;
#if ENABLE(WEBGL)
            m_pageProxy->pageGroup()->preferences()->setWebGLEnabled(false);
#endif
            return false;
        }
    }

    if (!m_evasGLContext) {
        m_evasGLContext = EvasGLContext::create(m_evasGL.get());
        if (!m_evasGLContext) {
            WARN("Failed to create GLContext.");
            return false;
        }
    }

    Ewk_View_Smart_Data* sd = smartData();

    Evas_GL_Config evasGLConfig = {
        EVAS_GL_RGBA_8888,
        EVAS_GL_DEPTH_BIT_8,
        EVAS_GL_STENCIL_NONE,
        EVAS_GL_OPTIONS_NONE,
        EVAS_GL_MULTISAMPLE_NONE
    };

    // Replaces if non-null, and frees existing surface after (OwnPtr).
    m_evasGLSurface = EvasGLSurface::create(m_evasGL.get(), &evasGLConfig, viewSize);
    if (!m_evasGLSurface)
        return false;

    Evas_Native_Surface nativeSurface;
    evas_gl_native_surface_get(m_evasGL.get(), m_evasGLSurface->surface(), &nativeSurface);
    evas_object_image_native_surface_set(sd->image, &nativeSurface);

    evas_gl_make_current(m_evasGL.get(), m_evasGLSurface->surface(), m_evasGLContext->context());

    Evas_GL_API* gl = evas_gl_api_get(m_evasGL.get());
    gl->glViewport(0, 0, viewSize.width() + sd->view.x, viewSize.height() + sd->view.y);
    gl->glClearColor(1.0, 1.0, 1.0, 0);
    gl->glClear(GL_COLOR_BUFFER_BIT);

    return true;
}

bool EwkView::enterAcceleratedCompositingMode()
{
    coordinatedGraphicsScene()->setActive(true);

    if (!m_isHardwareAccelerated)
        return true;

    if (!m_evasGLSurface) {
        if (!createGLSurface(size())) {
            WARN("Failed to create GLSurface.");
            return false;
        }
    }

    return true;
}

bool EwkView::exitAcceleratedCompositingMode()
{
    return true;
}
#endif

#if ENABLE(INPUT_TYPE_COLOR)
/**
 * @internal
 * Requests to show external color picker.
 */
void EwkView::requestColorPicker(WKColorPickerResultListenerRef listener, const WebCore::Color& color)
{
    Ewk_View_Smart_Data* sd = smartData();
    EINA_SAFETY_ON_NULL_RETURN(sd->api->input_picker_color_request);

    if (!sd->api->input_picker_color_request)
        return;

    if (m_colorPicker)
        dismissColorPicker();

    m_colorPicker = EwkColorPicker::create(listener, color);

    sd->api->input_picker_color_request(sd, m_colorPicker.get());
}

/**
 * @internal
 * Requests to hide external color picker.
 */
void EwkView::dismissColorPicker()
{
    if (!m_colorPicker)
        return;

    Ewk_View_Smart_Data* sd = smartData();
    EINA_SAFETY_ON_NULL_RETURN(sd->api->input_picker_color_dismiss);

    if (sd->api->input_picker_color_dismiss)
        sd->api->input_picker_color_dismiss(sd);

    m_colorPicker.clear();
}
#endif

COMPILE_ASSERT_MATCHING_ENUM(EWK_TEXT_DIRECTION_RIGHT_TO_LEFT, RTL);
COMPILE_ASSERT_MATCHING_ENUM(EWK_TEXT_DIRECTION_LEFT_TO_RIGHT, LTR);

void EwkView::showContextMenu(WebContextMenuProxyEfl* contextMenuProxy, const WebCore::IntPoint& position, const Vector<WebContextMenuItemData>& items)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    ASSERT(contextMenuProxy);

    if (!sd->api->context_menu_show)
        return;

    if (m_contextMenu)
        hideContextMenu();

    m_contextMenu = Ewk_Context_Menu::create(this, contextMenuProxy, items);

    sd->api->context_menu_show(sd, position.x(), position.y(), m_contextMenu.get());
}

void EwkView::hideContextMenu()
{
    if (!m_contextMenu)
        return;

    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (sd->api->context_menu_hide)
        sd->api->context_menu_hide(sd);

    m_contextMenu.clear();
}

void EwkView::requestPopupMenu(WebPopupMenuProxyEfl* popupMenuProxy, const IntRect& rect, TextDirection textDirection, double pageScaleFactor, const Vector<WebPopupItem>& items, int32_t selectedIndex)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    ASSERT(popupMenuProxy);

    if (!sd->api->popup_menu_show)
        return;

    if (m_popupMenu)
        closePopupMenu();

    m_popupMenu = EwkPopupMenu::create(this, popupMenuProxy, items, selectedIndex);

    sd->api->popup_menu_show(sd, rect, static_cast<Ewk_Text_Direction>(textDirection), pageScaleFactor, m_popupMenu.get());
}

void EwkView::closePopupMenu()
{
    if (!m_popupMenu)
        return;

    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (sd->api->popup_menu_hide)
        sd->api->popup_menu_hide(sd);

    m_popupMenu.clear();
}

/**
 * @internal
 * Calls a smart member function for javascript alert().
 */
void EwkView::requestJSAlertPopup(const WKEinaSharedString& message)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (!sd->api->run_javascript_alert)
        return;

    sd->api->run_javascript_alert(sd, message);
}

/**
 * @internal
 * Calls a smart member function for javascript confirm() and returns a value from the function. Returns false by default.
 */
bool EwkView::requestJSConfirmPopup(const WKEinaSharedString& message)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (!sd->api->run_javascript_confirm)
        return false;

    return sd->api->run_javascript_confirm(sd, message);
}

/**
 * @internal
 * Calls a smart member function for javascript prompt() and returns a value from the function. Returns null string by default.
 */
WKEinaSharedString EwkView::requestJSPromptPopup(const WKEinaSharedString& message, const WKEinaSharedString& defaultValue)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (!sd->api->run_javascript_prompt)
        return WKEinaSharedString();

    return WKEinaSharedString::adopt(sd->api->run_javascript_prompt(sd, message, defaultValue));
}

#if ENABLE(SQL_DATABASE)
/**
 * @internal
 * Calls exceeded_database_quota callback or falls back to default behavior returns default database quota.
 */
unsigned long long EwkView::informDatabaseQuotaReached(const String& databaseName, const String& displayName, unsigned long long currentQuota, unsigned long long currentOriginUsage, unsigned long long currentDatabaseUsage, unsigned long long expectedUsage)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    static const unsigned long long defaultQuota = 5 * 1024 * 1204; // 5 MB
    if (sd->api->exceeded_database_quota)
        return sd->api->exceeded_database_quota(sd, databaseName.utf8().data(), displayName.utf8().data(), currentQuota, currentOriginUsage, currentDatabaseUsage, expectedUsage);

    return defaultQuota;
}
#endif

/**
 * @internal
 * The url of view was changed by the frame loader.
 *
 * Emits signal: "url,changed" with pointer to new url string.
 */
void EwkView::informURLChange()
{
    String activeURL = m_pageProxy->activeURL();
    if (activeURL.isEmpty())
        return;

    CString rawActiveURL = activeURL.utf8();
    if (m_url == rawActiveURL.data())
        return;

    m_url = rawActiveURL.data();
    smartCallback<URLChanged>().call(m_url);

    // Update the view's favicon.
    informIconChange();
}

EwkWindowFeatures* EwkView::windowFeatures()
{
    if (!m_windowFeatures)
        m_windowFeatures = EwkWindowFeatures::create(0, this);

    return m_windowFeatures.get();
}

WKPageRef EwkView::createNewPage(PassRefPtr<EwkUrlRequest> request, WKDictionaryRef windowFeatures)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (!sd->api->window_create)
        return 0;

    RefPtr<EwkWindowFeatures> ewkWindowFeatures = EwkWindowFeatures::create(windowFeatures, this);

    Evas_Object* newEwkView = sd->api->window_create(sd, request->url(), ewkWindowFeatures.get());
    if (!newEwkView)
        return 0;

    EwkView* newViewImpl = EwkView::fromEvasObject(newEwkView);
    ASSERT(newViewImpl);

    newViewImpl->m_windowFeatures = ewkWindowFeatures;

    return static_cast<WKPageRef>(WKRetain(newViewImpl->page()));
}

void EwkView::close()
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    if (!sd->api->window_close)
        return;

    sd->api->window_close(sd);
}

void EwkView::onMouseDown(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Down* downEvent = static_cast<Evas_Event_Mouse_Down*>(eventInfo);
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->mouse_down);
    sd->api->mouse_down(sd, downEvent);
}

void EwkView::onMouseUp(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Up* upEvent = static_cast<Evas_Event_Mouse_Up*>(eventInfo);
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->mouse_up);
    sd->api->mouse_up(sd, upEvent);
}

void EwkView::onMouseMove(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Move* moveEvent = static_cast<Evas_Event_Mouse_Move*>(eventInfo);
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->mouse_move);
    sd->api->mouse_move(sd, moveEvent);
}

#if ENABLE(TOUCH_EVENTS)
void EwkView::feedTouchEvents(Ewk_Touch_Event_Type type)
{
    Ewk_View_Smart_Data* sd = smartData();

    unsigned count = evas_touch_point_list_count(sd->base.evas);
    if (!count)
        return;

    Eina_List* points = 0;
    for (unsigned i = 0; i < count; ++i) {
        Ewk_Touch_Point* point = new Ewk_Touch_Point;
        point->id = evas_touch_point_list_nth_id_get(sd->base.evas, i);
        evas_touch_point_list_nth_xy_get(sd->base.evas, i, &point->x, &point->y);
        point->state = evas_touch_point_list_nth_state_get(sd->base.evas, i);
        points = eina_list_append(points, point);
    }

    ewk_view_feed_touch_event(m_evasObject, type, points, evas_key_modifier_get(sd->base.evas));

    void* data;
    EINA_LIST_FREE(points, data)
        delete static_cast<Ewk_Touch_Point*>(data);
}

void EwkView::onTouchDown(void* /* data */, Evas* /* canvas */, Evas_Object* ewkView, void* /* eventInfo */)
{
    EwkView* view = EwkView::fromEvasObject(ewkView);
    view->feedTouchEvents(EWK_TOUCH_START);
}

void EwkView::onTouchUp(void* /* data */, Evas* /* canvas */, Evas_Object* ewkView, void* /* eventInfo */)
{
    EwkView* view = EwkView::fromEvasObject(ewkView);
    view->feedTouchEvents(EWK_TOUCH_END);
}

void EwkView::onTouchMove(void* /* data */, Evas* /* canvas */, Evas_Object* ewkView, void* /* eventInfo */)
{
    EwkView* view = EwkView::fromEvasObject(ewkView);
    view->feedTouchEvents(EWK_TOUCH_MOVE);
}
#endif

void EwkView::onFaviconChanged(const char* pageURL, void* eventInfo)
{
    EwkView* view = static_cast<EwkView*>(eventInfo);

    if (!view->url() || strcasecmp(view->url(), pageURL))
        return;

    view->informIconChange();
}

PassRefPtr<cairo_surface_t> EwkView::takeSnapshot()
{
    // Suspend all animations before taking the snapshot.
    m_pageProxy->suspendActiveDOMObjectsAndAnimations();

    // Wait for the pending repaint events to be processed.
    while (m_displayTimer.isActive())
        ecore_main_loop_iterate();

    Ewk_View_Smart_Data* sd = smartData();
#if USE(ACCELERATED_COMPOSITING)
    if (!m_isHardwareAccelerated) {
#endif
        RefPtr<cairo_surface_t> snapshot = createSurfaceForImage(sd->image);
        // Resume all animations.
        m_pageProxy->resumeActiveDOMObjectsAndAnimations();

        return snapshot.release();
#if USE(ACCELERATED_COMPOSITING)
    }

    RefPtr<cairo_surface_t> snapshot = getImageSurfaceFromFrameBuffer(0, 0, sd->view.w, sd->view.h);
    // Resume all animations.
    m_pageProxy->resumeActiveDOMObjectsAndAnimations();

    return snapshot.release();
#endif
}
