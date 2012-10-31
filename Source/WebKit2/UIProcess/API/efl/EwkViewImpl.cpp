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
#include "EwkViewImpl.h"

#include "EflScreenUtilities.h"
#include "FindClientEfl.h"
#include "FormClientEfl.h"
#include "InputMethodContextEfl.h"
#include "PageClientImpl.h"
#include "PageLoadClientEfl.h"
#include "PagePolicyClientEfl.h"
#include "PageUIClientEfl.h"
#include "PageViewportController.h"
#include "PageViewportControllerClientEfl.h"
#include "ResourceLoadClientEfl.h"
#include "WKString.h"
#include "WebContext.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPopupMenuProxyEfl.h"
#include "WebPreferences.h"
#include "ewk_back_forward_list_private.h"
#include "ewk_color_picker_private.h"
#include "ewk_context_private.h"
#include "ewk_favicon_database_private.h"
#include "ewk_popup_menu_item_private.h"
#include "ewk_popup_menu_private.h"
#include "ewk_private.h"
#include "ewk_settings_private.h"
#include "ewk_view.h"
#include "ewk_view_private.h"
#include <Ecore_Evas.h>
#include <Edje.h>
#include <WebCore/Cursor.h>


#if ENABLE(FULLSCREEN_API)
#include "WebFullScreenManagerProxy.h"
#endif

#if USE(ACCELERATED_COMPOSITING)
#include <Evas_GL.h>
#endif

using namespace WebCore;
using namespace WebKit;

static const int defaultCursorSize = 16;

typedef HashMap<WKPageRef, Evas_Object*> PageViewMap;

static inline PageViewMap& pageViewMap()
{
    DEFINE_STATIC_LOCAL(PageViewMap, map, ());
    return map;
}

void EwkViewImpl::addToPageViewMap(EwkViewImpl* viewImpl)
{
    PageViewMap::AddResult result = pageViewMap().add(viewImpl->wkPage(), viewImpl->view());
    ASSERT_UNUSED(result, result.isNewEntry);
}

void EwkViewImpl::removeFromPageViewMap(EwkViewImpl* viewImpl)
{
    ASSERT(pageViewMap().contains(viewImpl->wkPage()));
    pageViewMap().remove(viewImpl->wkPage());
}

const Evas_Object* EwkViewImpl::viewFromPageViewMap(const WKPageRef page)
{
    ASSERT(page);

    return pageViewMap().get(page);
}

EwkViewImpl::EwkViewImpl(Evas_Object* view, PassRefPtr<Ewk_Context> context, PassRefPtr<WebPageGroup> pageGroup)
    : m_view(view)
    , m_context(context)
    , m_pageClient(PageClientImpl::create(this))
    , m_pageProxy(toImpl(m_context->wkContext())->createWebPage(m_pageClient.get(), pageGroup.get()))
    , m_pageLoadClient(PageLoadClientEfl::create(this))
    , m_pagePolicyClient(PagePolicyClientEfl::create(this))
    , m_pageUIClient(PageUIClientEfl::create(this))
    , m_resourceLoadClient(ResourceLoadClientEfl::create(this))
    , m_findClient(FindClientEfl::create(this))
    , m_formClient(FormClientEfl::create(this))
    , m_backForwardList(Ewk_Back_Forward_List::create(toAPI(m_pageProxy->backForwardList())))
#if USE(TILED_BACKING_STORE)
    , m_pageViewportControllerClient(PageViewportControllerClientEfl::create(this))
    , m_pageViewportController(adoptPtr(new PageViewportController(m_pageProxy.get(), m_pageViewportControllerClient.get())))
#endif
    , m_settings(Ewk_Settings::create(this))
    , m_cursorGroup(0)
    , m_mouseEventsEnabled(false)
#if ENABLE(TOUCH_EVENTS)
    , m_touchEventsEnabled(false)
#endif
    , m_displayTimer(this, &EwkViewImpl::displayTimerFired)
    , m_inputMethodContext(InputMethodContextEfl::create(this, smartData()->base.evas))
{
    ASSERT(m_view);
    ASSERT(m_context);
    ASSERT(m_pageProxy);

#if USE(COORDINATED_GRAPHICS)
    m_pageProxy->pageGroup()->preferences()->setAcceleratedCompositingEnabled(true);
    m_pageProxy->pageGroup()->preferences()->setForceCompositingMode(true);
#if ENABLE(WEBGL)
    m_pageProxy->pageGroup()->preferences()->setWebGLEnabled(true);
#endif
    m_pageProxy->setUseFixedLayout(true);
#endif

    m_pageProxy->initializeWebPage();

#if USE(TILED_BACKING_STORE)
    m_pageClient->setPageViewportController(m_pageViewportController.get());
#endif

#if ENABLE(FULLSCREEN_API)
    m_pageProxy->fullScreenManager()->setWebView(m_view);
    m_pageProxy->pageGroup()->preferences()->setFullScreenEnabled(true);
#endif

    // Enable mouse events by default
    setMouseEventsEnabled(true);

    // Listen for favicon changes.
    Ewk_Favicon_Database* iconDatabase = m_context->faviconDatabase();
    ASSERT(iconDatabase);

    iconDatabase->watchChanges(IconChangeCallbackData(EwkViewImpl::onFaviconChanged, this));

    EwkViewImpl::addToPageViewMap(this);
}

EwkViewImpl::~EwkViewImpl()
{
    // Unregister icon change callback.
    Ewk_Favicon_Database* iconDatabase = m_context->faviconDatabase();
    ASSERT(iconDatabase);

    iconDatabase->unwatchChanges(EwkViewImpl::onFaviconChanged);

    EwkViewImpl::removeFromPageViewMap(this);
}

Ewk_View_Smart_Data* EwkViewImpl::smartData()
{
    return static_cast<Ewk_View_Smart_Data*>(evas_object_smart_data_get(m_view));
}

EwkViewImpl* EwkViewImpl::fromEvasObject(const Evas_Object* view)
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
WKPageRef EwkViewImpl::wkPage()
{
    return toAPI(m_pageProxy.get());
}

void EwkViewImpl::setCursor(const Cursor& cursor)
{
    const char* group = cursor.platformCursor();
    if (!group || group == m_cursorGroup)
        return;

    m_cursorGroup = group;
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

void EwkViewImpl::displayTimerFired(WebCore::Timer<EwkViewImpl>*)
{
    Ewk_View_Smart_Data* sd = smartData();

    if (!sd->image)
        return;

    Region dirtyRegion;
    for (Vector<IntRect>::iterator it = m_dirtyRects.begin(); it != m_dirtyRects.end(); ++it)
        dirtyRegion.unite(*it);

    m_dirtyRects.clear();

    Vector<IntRect> rects = dirtyRegion.rects();
    Vector<IntRect>::iterator end = rects.end();

    for (Vector<IntRect>::iterator it = rects.begin(); it != end; ++it) {
        IntRect rect = *it;
#if USE(COORDINATED_GRAPHICS)
        evas_gl_make_current(evasGL(), evasGLSurface(), evasGLContext());
        m_pageViewportControllerClient->display(rect, IntPoint(sd->view.x, sd->view.y));
#endif

        evas_object_image_data_update_add(sd->image, rect.x(), rect.y(), rect.width(), rect.height());
    }
}

void EwkViewImpl::redrawRegion(const IntRect& rect)
{
    if (!m_displayTimer.isActive())
        m_displayTimer.startOneShot(0);
    m_dirtyRects.append(rect);
}

#if ENABLE(FULLSCREEN_API)
/**
 * @internal
 * Calls fullscreen_enter callback or falls back to default behavior and enables fullscreen mode.
 */
void EwkViewImpl::enterFullScreen()
{
    Ewk_View_Smart_Data* sd = smartData();

    if (!sd->api->fullscreen_enter || !sd->api->fullscreen_enter(sd)) {
        Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(sd->base.evas);
        ecore_evas_fullscreen_set(ecoreEvas, true);
    }
}

/**
 * @internal
 * Calls fullscreen_exit callback or falls back to default behavior and disables fullscreen mode.
 */
void EwkViewImpl::exitFullScreen()
{
    Ewk_View_Smart_Data* sd = smartData();

    if (!sd->api->fullscreen_exit || !sd->api->fullscreen_exit(sd)) {
        Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(sd->base.evas);
        ecore_evas_fullscreen_set(ecoreEvas, false);
    }
}
#endif

void EwkViewImpl::setImageData(void* imageData, const IntSize& size)
{
    Ewk_View_Smart_Data* sd = smartData();
    if (!imageData || !sd->image)
        return;

    evas_object_resize(sd->image, size.width(), size.height());
    evas_object_image_size_set(sd->image, size.width(), size.height());
    evas_object_image_data_copy_set(sd->image, imageData);
}

#if USE(TILED_BACKING_STORE)
void EwkViewImpl::informLoadCommitted()
{
    m_pageViewportController->didCommitLoad();
}
#endif

IntSize EwkViewImpl::size() const
{
    int width, height;
    evas_object_geometry_get(m_view, 0, 0, &width, &height);
    return IntSize(width, height);
}

bool EwkViewImpl::isFocused() const
{
    return evas_object_focus_get(m_view);
}

bool EwkViewImpl::isVisible() const
{
    return evas_object_visible_get(m_view);
}

const char* EwkViewImpl::title() const
{
    m_title = m_pageProxy->pageTitle().utf8().data();

    return m_title;
}

/**
 * @internal
 * This function may return @c NULL.
 */
InputMethodContextEfl* EwkViewImpl::inputMethodContext()
{
    return m_inputMethodContext.get();
}

const char* EwkViewImpl::themePath() const
{
    return m_theme;
}

void EwkViewImpl::setThemePath(const char* theme)
{
    if (m_theme != theme) {
        m_theme = theme;
        m_pageProxy->setThemePath(theme);
    }
}

const char* EwkViewImpl::customTextEncodingName() const
{
    String customEncoding = m_pageProxy->customTextEncodingName();
    if (customEncoding.isEmpty())
        return 0;

    m_customEncoding = customEncoding.utf8().data();

    return m_customEncoding;
}

void EwkViewImpl::setCustomTextEncodingName(const String& encoding)
{
    m_pageProxy->setCustomTextEncodingName(encoding);
}

void EwkViewImpl::setMouseEventsEnabled(bool enabled)
{
    if (m_mouseEventsEnabled == enabled)
        return;

    m_mouseEventsEnabled = enabled;
    if (enabled) {
        Ewk_View_Smart_Data* sd = smartData();
        evas_object_event_callback_add(m_view, EVAS_CALLBACK_MOUSE_DOWN, onMouseDown, sd);
        evas_object_event_callback_add(m_view, EVAS_CALLBACK_MOUSE_UP, onMouseUp, sd);
        evas_object_event_callback_add(m_view, EVAS_CALLBACK_MOUSE_MOVE, onMouseMove, sd);
    } else {
        evas_object_event_callback_del(m_view, EVAS_CALLBACK_MOUSE_DOWN, onMouseDown);
        evas_object_event_callback_del(m_view, EVAS_CALLBACK_MOUSE_UP, onMouseUp);
        evas_object_event_callback_del(m_view, EVAS_CALLBACK_MOUSE_MOVE, onMouseMove);
    }
}

#if ENABLE(TOUCH_EVENTS)
void EwkViewImpl::setTouchEventsEnabled(bool enabled)
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
        evas_object_event_callback_add(m_view, EVAS_CALLBACK_MOUSE_DOWN, onTouchDown, sd);
        evas_object_event_callback_add(m_view, EVAS_CALLBACK_MOUSE_UP, onTouchUp, sd);
        evas_object_event_callback_add(m_view, EVAS_CALLBACK_MOUSE_MOVE, onTouchMove, sd);
        evas_object_event_callback_add(m_view, EVAS_CALLBACK_MULTI_DOWN, onTouchDown, sd);
        evas_object_event_callback_add(m_view, EVAS_CALLBACK_MULTI_UP, onTouchUp, sd);
        evas_object_event_callback_add(m_view, EVAS_CALLBACK_MULTI_MOVE, onTouchMove, sd);
    } else {
        evas_object_event_callback_del(m_view, EVAS_CALLBACK_MOUSE_DOWN, onTouchDown);
        evas_object_event_callback_del(m_view, EVAS_CALLBACK_MOUSE_UP, onTouchUp);
        evas_object_event_callback_del(m_view, EVAS_CALLBACK_MOUSE_MOVE, onTouchMove);
        evas_object_event_callback_del(m_view, EVAS_CALLBACK_MULTI_DOWN, onTouchDown);
        evas_object_event_callback_del(m_view, EVAS_CALLBACK_MULTI_UP, onTouchUp);
        evas_object_event_callback_del(m_view, EVAS_CALLBACK_MULTI_MOVE, onTouchMove);
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
void EwkViewImpl::informIconChange()
{
    Ewk_Favicon_Database* iconDatabase = m_context->faviconDatabase();
    ASSERT(iconDatabase);

    m_faviconURL = ewk_favicon_database_icon_url_get(iconDatabase, m_url);
    evas_object_smart_callback_call(m_view, "icon,changed", 0);
}

#if USE(ACCELERATED_COMPOSITING)
bool EwkViewImpl::createGLSurface(const IntSize& viewSize)
{
    Ewk_View_Smart_Data* sd = smartData();

    Evas_GL_Config evasGLConfig = {
        EVAS_GL_RGBA_8888,
        EVAS_GL_DEPTH_BIT_8,
        EVAS_GL_STENCIL_NONE,
        EVAS_GL_OPTIONS_NONE,
        EVAS_GL_MULTISAMPLE_NONE
    };

    ASSERT(!m_evasGLSurface);
    m_evasGLSurface = EvasGLSurface::create(evasGL(), &evasGLConfig, viewSize);
    if (!m_evasGLSurface)
        return false;

    Evas_Native_Surface nativeSurface;
    evas_gl_native_surface_get(evasGL(), evasGLSurface(), &nativeSurface);
    evas_object_image_native_surface_set(sd->image, &nativeSurface);

    evas_gl_make_current(evasGL(), evasGLSurface(), evasGLContext());

    Evas_GL_API* gl = evas_gl_api_get(evasGL());
    gl->glViewport(0, 0, viewSize.width() + sd->view.x, viewSize.height() + sd->view.y);

    return true;
}

bool EwkViewImpl::enterAcceleratedCompositingMode()
{
    if (m_evasGL) {
        EINA_LOG_DOM_WARN(_ewk_log_dom, "Accelerated compositing mode already entered.");
        return false;
    }

    Evas* evas = evas_object_evas_get(m_view);
    m_evasGL = adoptPtr(evas_gl_new(evas));
    if (!m_evasGL)
        return false;

    m_evasGLContext = EvasGLContext::create(evasGL());
    if (!m_evasGLContext) {
        EINA_LOG_DOM_WARN(_ewk_log_dom, "Failed to create GLContext.");
        m_evasGL.clear();
        return false;
    }

    if (!createGLSurface(size())) {
        EINA_LOG_DOM_WARN(_ewk_log_dom, "Failed to create GLSurface.");
        m_evasGLContext.clear();
        m_evasGL.clear();
        return false;
    }

    m_pageViewportControllerClient->setRendererActive(true);
    return true;
}

bool EwkViewImpl::exitAcceleratedCompositingMode()
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(m_evasGL, false);

    m_evasGLSurface.clear();
    m_evasGLContext.clear();
    m_evasGL.clear();

    return true;
}
#endif

#if ENABLE(INPUT_TYPE_COLOR)
/**
 * @internal
 * Requests to show external color picker.
 */
void EwkViewImpl::requestColorPicker(WKColorPickerResultListenerRef listener, const WebCore::Color& color)
{
    Ewk_View_Smart_Data* sd = smartData();
    EINA_SAFETY_ON_NULL_RETURN(sd->api->input_picker_color_request);

    if (!sd->api->input_picker_color_request)
        return;

    if (m_colorPicker)
        dismissColorPicker();

    m_colorPicker = Ewk_Color_Picker::create(listener, color);

    sd->api->input_picker_color_request(sd, m_colorPicker.get());
}

/**
 * @internal
 * Requests to hide external color picker.
 */
void EwkViewImpl::dismissColorPicker()
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

void EwkViewImpl::informContentsSizeChange(const IntSize& size)
{
#if USE(COORDINATED_GRAPHICS)
    m_pageViewportControllerClient->didChangeContentsSize(size);
#else
    UNUSED_PARAM(size);
#endif
}

COMPILE_ASSERT_MATCHING_ENUM(EWK_TEXT_DIRECTION_RIGHT_TO_LEFT, RTL);
COMPILE_ASSERT_MATCHING_ENUM(EWK_TEXT_DIRECTION_LEFT_TO_RIGHT, LTR);

void EwkViewImpl::requestPopupMenu(WebPopupMenuProxyEfl* popupMenuProxy, const IntRect& rect, TextDirection textDirection, double pageScaleFactor, const Vector<WebPopupItem>& items, int32_t selectedIndex)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    ASSERT(popupMenuProxy);

    if (!sd->api->popup_menu_show)
        return;

    if (m_popupMenu)
        closePopupMenu();

    m_popupMenu = Ewk_Popup_Menu::create(this, popupMenuProxy, items, selectedIndex);

    sd->api->popup_menu_show(sd, rect, static_cast<Ewk_Text_Direction>(textDirection), pageScaleFactor, m_popupMenu.get());
}

void EwkViewImpl::closePopupMenu()
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
void EwkViewImpl::requestJSAlertPopup(const WKEinaSharedString& message)
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
bool EwkViewImpl::requestJSConfirmPopup(const WKEinaSharedString& message)
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
WKEinaSharedString EwkViewImpl::requestJSPromptPopup(const WKEinaSharedString& message, const WKEinaSharedString& defaultValue)
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
unsigned long long EwkViewImpl::informDatabaseQuotaReached(const String& databaseName, const String& displayName, unsigned long long currentQuota, unsigned long long currentOriginUsage, unsigned long long currentDatabaseUsage, unsigned long long expectedUsage)
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
void EwkViewImpl::informURLChange()
{
    String activeURL = m_pageProxy->activeURL();
    if (activeURL.isEmpty())
        return;

    CString rawActiveURL = activeURL.utf8();
    if (m_url == rawActiveURL.data())
        return;

    m_url = rawActiveURL.data();
    const char* callbackArgument = static_cast<const char*>(m_url);
    evas_object_smart_callback_call(m_view, "url,changed", const_cast<char*>(callbackArgument));

    // Update the view's favicon.
    informIconChange();
}

WKPageRef EwkViewImpl::createNewPage()
{
    Evas_Object* newEwkView = 0;
    evas_object_smart_callback_call(m_view, "create,window", &newEwkView);

    if (!newEwkView)
        return 0;

    EwkViewImpl* newViewImpl = EwkViewImpl::fromEvasObject(newEwkView);
    ASSERT(newViewImpl);

    return static_cast<WKPageRef>(WKRetain(newViewImpl->page()));
}

void EwkViewImpl::closePage()
{
    evas_object_smart_callback_call(m_view, "close,window", 0);
}

void EwkViewImpl::onMouseDown(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Down* downEvent = static_cast<Evas_Event_Mouse_Down*>(eventInfo);
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->mouse_down);
    sd->api->mouse_down(sd, downEvent);
}

void EwkViewImpl::onMouseUp(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Up* upEvent = static_cast<Evas_Event_Mouse_Up*>(eventInfo);
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->mouse_up);
    sd->api->mouse_up(sd, upEvent);
}

void EwkViewImpl::onMouseMove(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Move* moveEvent = static_cast<Evas_Event_Mouse_Move*>(eventInfo);
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(sd->api);
    EINA_SAFETY_ON_NULL_RETURN(sd->api->mouse_move);
    sd->api->mouse_move(sd, moveEvent);
}

#if ENABLE(TOUCH_EVENTS)
void EwkViewImpl::feedTouchEvents(Ewk_Touch_Event_Type type)
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

    ewk_view_feed_touch_event(m_view, type, points, evas_key_modifier_get(sd->base.evas));

    void* data;
    EINA_LIST_FREE(points, data)
        delete static_cast<Ewk_Touch_Point*>(data);
}

void EwkViewImpl::onTouchDown(void* /* data */, Evas* /* canvas */, Evas_Object* ewkView, void* /* eventInfo */)
{
    EwkViewImpl* viewImpl = EwkViewImpl::fromEvasObject(ewkView);
    viewImpl->feedTouchEvents(EWK_TOUCH_START);
}

void EwkViewImpl::onTouchUp(void* /* data */, Evas* /* canvas */, Evas_Object* ewkView, void* /* eventInfo */)
{
    EwkViewImpl* viewImpl = EwkViewImpl::fromEvasObject(ewkView);
    viewImpl->feedTouchEvents(EWK_TOUCH_END);
}

void EwkViewImpl::onTouchMove(void* /* data */, Evas* /* canvas */, Evas_Object* ewkView, void* /* eventInfo */)
{
    EwkViewImpl* viewImpl = EwkViewImpl::fromEvasObject(ewkView);
    viewImpl->feedTouchEvents(EWK_TOUCH_MOVE);
}
#endif

void EwkViewImpl::onFaviconChanged(const char* pageURL, void* eventInfo)
{
    EwkViewImpl* viewImpl = static_cast<EwkViewImpl*>(eventInfo);

    if (!viewImpl->url() || strcasecmp(viewImpl->url(), pageURL))
        return;

    viewImpl->informIconChange();
}
