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
#include "PageClientImpl.h"
#include "PageLoadClientEfl.h"
#include "PagePolicyClientEfl.h"
#include "PageUIClientEfl.h"
#include "PageViewportController.h"
#include "PageViewportControllerClientEfl.h"
#include "ResourceLoadClientEfl.h"
#include "WebPageProxy.h"
#include "WebPopupMenuProxyEfl.h"
#include "ewk_back_forward_list_private.h"
#include "ewk_context_private.h"
#include "ewk_favicon_database_private.h"
#include "ewk_popup_menu_item_private.h"
#include "ewk_private.h"
#include "ewk_settings_private.h"
#include "ewk_view.h"
#include "ewk_view_private.h"
#include <Ecore_Evas.h>
#include <Ecore_IMF.h>
#include <Ecore_IMF_Evas.h>
#include <Edje.h>
#include <WebCore/Cursor.h>

using namespace WebCore;
using namespace WebKit;

static const int defaultCursorSize = 16;

EwkViewImpl::PageViewMap EwkViewImpl::pageViewMap;

void EwkViewImpl::addToPageViewMap(const Evas_Object* ewkView)
{
    EwkViewImpl* viewImpl = EwkViewImpl::fromEvasObject(ewkView);

    PageViewMap::AddResult result = pageViewMap.add(viewImpl->wkPage(), ewkView);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void EwkViewImpl::removeFromPageViewMap(const Evas_Object* ewkView)
{
    EwkViewImpl* viewImpl = EwkViewImpl::fromEvasObject(ewkView);

    ASSERT(pageViewMap.contains(viewImpl->wkPage()));
    pageViewMap.remove(viewImpl->wkPage());
}

const Evas_Object* EwkViewImpl::viewFromPageViewMap(const WKPageRef page)
{
    ASSERT(page);

    return pageViewMap.get(page);
}

static void _ewk_view_commit(void* data, Ecore_IMF_Context*, void* eventInfo)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, viewImpl);
    if (!eventInfo || !viewImpl->isImfFocused)
        return;

    viewImpl->pageProxy->confirmComposition(String::fromUTF8(static_cast<char*>(eventInfo)));
}

static void _ewk_view_preedit_changed(void* data, Ecore_IMF_Context* context, void*)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EWK_VIEW_IMPL_GET_OR_RETURN(smartData, viewImpl);

    if (!viewImpl->pageProxy->focusedFrame() || !viewImpl->isImfFocused)
        return;

    char* buffer = 0;
    ecore_imf_context_preedit_string_get(context, &buffer, 0);
    if (!buffer)
        return;

    String preeditString = String::fromUTF8(buffer);
    free(buffer);
    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(0, preeditString.length(), Color(0, 0, 0), false));
    viewImpl->pageProxy->setComposition(preeditString, underlines, 0);
}

static void _ewk_view_imf_context_destroy(Ecore_IMF_Context* imfContext)
{
    if (!imfContext)
        return;

    ecore_imf_context_event_callback_del(imfContext, ECORE_IMF_CALLBACK_PREEDIT_CHANGED, _ewk_view_preedit_changed);
    ecore_imf_context_event_callback_del(imfContext, ECORE_IMF_CALLBACK_COMMIT, _ewk_view_commit);
    ecore_imf_context_del(imfContext);
}

static Ecore_IMF_Context* _ewk_view_imf_context_create(Ewk_View_Smart_Data* smartData)
{
    const char* defaultContextID = ecore_imf_context_default_id_get();
    if (!defaultContextID)
        return 0;

    Ecore_IMF_Context* imfContext = ecore_imf_context_add(defaultContextID);
    if (!imfContext)
        return 0;

    ecore_imf_context_event_callback_add(imfContext, ECORE_IMF_CALLBACK_PREEDIT_CHANGED, _ewk_view_preedit_changed, smartData);
    ecore_imf_context_event_callback_add(imfContext, ECORE_IMF_CALLBACK_COMMIT, _ewk_view_commit, smartData);

    Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(smartData->base.evas);
    ecore_imf_context_client_window_set(imfContext, (void*)ecore_evas_window_get(ecoreEvas));
    ecore_imf_context_client_canvas_set(imfContext, smartData->base.evas);

    return imfContext;
}

EwkViewImpl::EwkViewImpl(Evas_Object* view)
    : settings(adoptPtr(new Ewk_Settings(this)))
    , areMouseEventsEnabled(false)
#if ENABLE(TOUCH_EVENTS)
    , areTouchEventsEnabled(false)
#endif
    , popupMenuProxy(0)
    , popupMenuItems(0)
    , imfContext(0)
    , isImfFocused(false)
#ifdef HAVE_ECORE_X
    , isUsingEcoreX(false)
#endif
#if USE(ACCELERATED_COMPOSITING)
    , evasGl(0)
    , evasGlContext(0)
    , evasGlSurface(0)
#endif
    , m_view(view)
    , m_displayTimer(this, &EwkViewImpl::displayTimerFired)
{
    ASSERT(view);
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd);

    imfContext = _ewk_view_imf_context_create(sd);

#ifdef HAVE_ECORE_X
    isUsingEcoreX = WebCore::isUsingEcoreX(sd->base.evas);
#endif
}

EwkViewImpl::~EwkViewImpl()
{
    _ewk_view_imf_context_destroy(imfContext);

    void* item;
    EINA_LIST_FREE(popupMenuItems, item)
        delete static_cast<Ewk_Popup_Menu_Item*>(item);
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
    return toAPI(pageProxy.get());
}

void EwkViewImpl::setCursor(const Cursor& cursor)
{
    Ewk_View_Smart_Data* sd = smartData();

    const char* group = cursor.platformCursor();
    if (!group || group == cursorGroup)
        return;

    cursorGroup = group;
    cursorObject = adoptRef(edje_object_add(sd->base.evas));

    Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(sd->base.evas);
    if (!theme || !edje_object_file_set(cursorObject.get(), theme, group)) {
        cursorObject.clear();

        ecore_evas_object_cursor_set(ecoreEvas, 0, 0, 0, 0);
#ifdef HAVE_ECORE_X
        if (isUsingEcoreX)
            WebCore::applyFallbackCursor(ecoreEvas, group);
#endif
        return;
    }

    Evas_Coord width, height;
    edje_object_size_min_get(cursorObject.get(), &width, &height);
    if (width <= 0 || height <= 0)
        edje_object_size_min_calc(cursorObject.get(), &width, &height);
    if (width <= 0 || height <= 0) {
        width = defaultCursorSize;
        height = defaultCursorSize;
    }
    evas_object_resize(cursorObject.get(), width, height);

    const char* data;
    int hotspotX = 0;
    data = edje_object_data_get(cursorObject.get(), "hot.x");
    if (data)
        hotspotX = atoi(data);

    int hotspotY = 0;
    data = edje_object_data_get(cursorObject.get(), "hot.y");
    if (data)
        hotspotY = atoi(data);

    ecore_evas_object_cursor_set(ecoreEvas, cursorObject.get(), EVAS_LAYER_MAX, hotspotX, hotspotY);
}

void EwkViewImpl::displayTimerFired(WebCore::Timer<EwkViewImpl>*)
{
    Ewk_View_Smart_Data* sd = smartData();

    if (!sd->image)
        return;

#if USE(COORDINATED_GRAPHICS)
    EWK_VIEW_IMPL_GET_OR_RETURN(sd, viewImpl);
#endif

    Region dirtyRegion;
    for (Vector<IntRect>::iterator it = m_dirtyRects.begin(); it != m_dirtyRects.end(); ++it)
        dirtyRegion.unite(*it);

    m_dirtyRects.clear();

    Vector<IntRect> rects = dirtyRegion.rects();
    Vector<IntRect>::iterator end = rects.end();

    for (Vector<IntRect>::iterator it = rects.begin(); it != end; ++it) {
        IntRect rect = *it;
#if USE(COORDINATED_GRAPHICS)
        evas_gl_make_current(viewImpl->evasGl, viewImpl->evasGlSurface, viewImpl->evasGlContext);
        viewImpl->pageViewportControllerClient->display(rect, IntPoint(sd->view.x, sd->view.y));
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

/**
 * @internal
 * A download for that view was cancelled.
 *
 * Emits signal: "download,cancelled" with pointer to a Ewk_Download_Job.
 */
void EwkViewImpl::informDownloadJobCancelled(Ewk_Download_Job* download)
{
    evas_object_smart_callback_call(m_view, "download,cancelled", download);
}

/**
 * @internal
 * A download for that view has failed.
 *
 * Emits signal: "download,failed" with pointer to a Ewk_Download_Job_Error.
 */
void EwkViewImpl::informDownloadJobFailed(Ewk_Download_Job* download, Ewk_Error* error)
{
    Ewk_Download_Job_Error downloadError = { download, error };
    evas_object_smart_callback_call(m_view, "download,failed", &downloadError);
}

/**
 * @internal
 * A download for that view finished successfully.
 *
 * Emits signal: "download,finished" with pointer to a Ewk_Download_Job.
 */
void EwkViewImpl::informDownloadJobFinished(Ewk_Download_Job* download)
{
    evas_object_smart_callback_call(m_view, "download,finished", download);
}

/**
 * @internal
 * A new download has been requested for that view.
 *
 * Emits signal: "download,request" with pointer to a Ewk_Download_Job.
 */
void EwkViewImpl::informDownloadJobRequested(Ewk_Download_Job* download)
{
    evas_object_smart_callback_call(m_view, "download,request", download);
}

/**
 * @internal
 * informs that a form request is about to be submitted.
 *
 * Emits signal: "form,submission,request" with pointer to Ewk_Form_Submission_Request.
 */
void EwkViewImpl::informNewFormSubmissionRequest(Ewk_Form_Submission_Request* request)
{
    evas_object_smart_callback_call(m_view, "form,submission,request", request);
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

/**
 * @internal
 * informs load failed with error information.
 *
 * Emits signal: "load,error" with pointer to Ewk_Error.
 */
void EwkViewImpl::informLoadError(Ewk_Error* error)
{
    evas_object_smart_callback_call(m_view, "load,error", error);
}

/**
 * @internal
 * informs load finished.
 *
 * Emits signal: "load,finished".
 */
void EwkViewImpl::informLoadFinished()
{
    informURLChange();
    evas_object_smart_callback_call(m_view, "load,finished", 0);
}

/**
 * @internal
 * informs load progress changed.
 *
 * Emits signal: "load,progress" with pointer to a double from 0.0 to 1.0.
 */
void EwkViewImpl::informLoadProgress(double progress)
{
    evas_object_smart_callback_call(m_view, "load,progress", &progress);
}

/**
 * @internal
 * informs view provisional load failed with error information.
 *
 * Emits signal: "load,provisional,failed" with pointer to Ewk_Error.
 */
void EwkViewImpl::informProvisionalLoadFailed(Ewk_Error* error)
{
    evas_object_smart_callback_call(m_view, "load,provisional,failed", error);
}

#if USE(TILED_BACKING_STORE)
void EwkViewImpl::informLoadCommitted()
{
    pageViewportController->didCommitLoad();
}
#endif

/**
 * @internal
 * informs view received redirect for provisional load.
 *
 * Emits signal: "load,provisional,redirect".
 */
void EwkViewImpl::informProvisionalLoadRedirect()
{
    informURLChange();
    evas_object_smart_callback_call(m_view, "load,provisional,redirect", 0);
}

/**
 * @internal
 * informs view provisional load started.
 *
 * Emits signal: "load,provisional,started".
 */
void EwkViewImpl::informProvisionalLoadStarted()
{
    informURLChange();
    evas_object_smart_callback_call(m_view, "load,provisional,started", 0);
}

/**
 * @internal
 * informs that a navigation policy decision should be taken.
 *
 * Emits signal: "policy,decision,navigation".
 */
void EwkViewImpl::informNavigationPolicyDecision(Ewk_Navigation_Policy_Decision* decision)
{
    evas_object_smart_callback_call(m_view, "policy,decision,navigation", decision);
}

/**
 * @internal
 * informs that a new window policy decision should be taken.
 *
 * Emits signal: "policy,decision,new,window".
 */
void EwkViewImpl::informNewWindowPolicyDecision(Ewk_Navigation_Policy_Decision* decision)
{
    evas_object_smart_callback_call(m_view, "policy,decision,new,window", decision);
}

/**
 * @internal
 * Load was initiated for a resource in the view.
 *
 * Emits signal: "resource,request,new" with pointer to resource request.
 */
void EwkViewImpl::informResourceLoadStarted(Ewk_Resource* resource, Ewk_Url_Request* request)
{
    Ewk_Resource_Request resourceRequest = {resource, request, 0};

    evas_object_smart_callback_call(m_view, "resource,request,new", &resourceRequest);
}

/**
 * @internal
 * Received a response to a resource load request in the view.
 *
 * Emits signal: "resource,request,response" with pointer to resource response.
 */
void EwkViewImpl::informResourceLoadResponse(Ewk_Resource* resource, Ewk_Url_Response* response)
{
    Ewk_Resource_Load_Response resourceLoadResponse = {resource, response};
    evas_object_smart_callback_call(m_view, "resource,request,response", &resourceLoadResponse);
}

/**
 * @internal
 * Failed loading a resource in the view.
 *
 * Emits signal: "resource,request,finished" with pointer to the resource load error.
 */
void EwkViewImpl::informResourceLoadFailed(Ewk_Resource* resource, Ewk_Error* error)
{
    Ewk_Resource_Load_Error resourceLoadError = {resource, error};
    evas_object_smart_callback_call(m_view, "resource,request,failed", &resourceLoadError);
}

/**
 * @internal
 * Finished loading a resource in the view.
 *
 * Emits signal: "resource,request,finished" with pointer to the resource.
 */
void EwkViewImpl::informResourceLoadFinished(Ewk_Resource* resource)
{
    evas_object_smart_callback_call(m_view, "resource,request,finished", resource);
}

/**
 * @internal
 * Request was sent for a resource in the view.
 *
 * Emits signal: "resource,request,sent" with pointer to resource request and possible redirect response.
 */
void EwkViewImpl::informResourceRequestSent(Ewk_Resource* resource, Ewk_Url_Request* request, Ewk_Url_Response* redirectResponse)
{
    Ewk_Resource_Request resourceRequest = {resource, request, redirectResponse};
    evas_object_smart_callback_call(m_view, "resource,request,sent", &resourceRequest);
}

/**
 * @internal
 * The view title was changed by the frame loader.
 *
 * Emits signal: "title,changed" with pointer to new title string.
 */
void EwkViewImpl::informTitleChange(const String& title)
{
    evas_object_smart_callback_call(m_view, "title,changed", const_cast<char*>(title.utf8().data()));
}

/**
 * @internal
 */
void EwkViewImpl::informTooltipTextChange(const String& text)
{
    if (text.isEmpty())
        evas_object_smart_callback_call(m_view, "tooltip,text,unset", 0);
    else
        evas_object_smart_callback_call(m_view, "tooltip,text,set", const_cast<char*>(text.utf8().data()));

}

/**
 * @internal
 * informs that the requested text was found.
 *
 * Emits signal: "text,found" with the number of matches.
 */
void EwkViewImpl::informTextFound(unsigned matchCount)
{
    evas_object_smart_callback_call(m_view, "text,found", &matchCount);
}

IntSize EwkViewImpl::size() const
{
    int width, height;
    evas_object_geometry_get(m_view, 0, 0, &width, &height);
    return IntSize(width, height);
}

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
    Ewk_Favicon_Database* iconDatabase = context->faviconDatabase();
    ASSERT(iconDatabase);

    faviconURL = ewk_favicon_database_icon_url_get(iconDatabase, url);
    evas_object_smart_callback_call(m_view, "icon,changed", 0);
}

#if ENABLE(WEB_INTENTS)
/**
 * @internal
 * The view received a new intent request.
 *
 * Emits signal: "intent,request,new" with pointer to a Ewk_Intent.
 */
void EwkViewImpl::informIntentRequest(Ewk_Intent* ewkIntent)
{
    evas_object_smart_callback_call(m_view, "intent,request,new", ewkIntent);
}
#endif

#if ENABLE(WEB_INTENTS_TAG)
/**
 * @internal
 * The view received a new intent service registration.
 *
 * Emits signal: "intent,service,register" with pointer to a Ewk_Intent_Service.
 */
void EwkViewImpl::informIntentServiceRegistration(Ewk_Intent_Service* ewkIntentService)
{
    evas_object_smart_callback_call(m_view, "intent,service,register", ewkIntentService);
}
#endif // ENABLE(WEB_INTENTS_TAG)

#if USE(ACCELERATED_COMPOSITING)
bool EwkViewImpl::createGLSurface(const IntSize& viewSize)
{
    Ewk_View_Smart_Data* sd = smartData();

    Evas_GL_Config evasGlConfig = {
        EVAS_GL_RGBA_8888,
        EVAS_GL_DEPTH_BIT_8,
        EVAS_GL_STENCIL_NONE,
        EVAS_GL_OPTIONS_NONE,
        EVAS_GL_MULTISAMPLE_NONE
    };

    ASSERT(!evasGlSurface);
    evasGlSurface = evas_gl_surface_create(evasGl, &evasGlConfig, viewSize.width(), viewSize.height());
    if (!evasGlSurface)
        return false;

    Evas_Native_Surface nativeSurface;
    evas_gl_native_surface_get(evasGl, evasGlSurface, &nativeSurface);
    evas_object_image_native_surface_set(sd->image, &nativeSurface);

    return true;
}

bool EwkViewImpl::enterAcceleratedCompositingMode()
{
    if (evasGl) {
        EINA_LOG_DOM_WARN(_ewk_log_dom, "Accelerated compositing mode already entered.");
        return false;
    }

    Evas* evas = evas_object_evas_get(m_view);
    evasGl = evas_gl_new(evas);
    if (!evasGl)
        return false;

    evasGlContext = evas_gl_context_create(evasGl, 0);
    if (!evasGlContext) {
        evas_gl_free(evasGl);
        evasGl = 0;
        return false;
    }

    if (!createGLSurface(size())) {
        evas_gl_context_destroy(evasGl, evasGlContext);
        evasGlContext = 0;

        evas_gl_free(evasGl);
        evasGl = 0;
        return false;
    }

    pageViewportControllerClient->setRendererActive(true);
    return true;
}

bool EwkViewImpl::exitAcceleratedCompositingMode()
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(evasGl, false);

    if (evasGlSurface) {
        evas_gl_surface_destroy(evasGl, evasGlSurface);
        evasGlSurface = 0;
    }

    if (evasGlContext) {
        evas_gl_context_destroy(evasGl, evasGlContext);
        evasGlContext = 0;
    }

    evas_gl_free(evasGl);
    evasGl = 0;

    return true;
}
#endif

#if ENABLE(INPUT_TYPE_COLOR)
/**
 * @internal
 * Requests to show external color picker.
 */
void EwkViewImpl::requestColorPicker(int r, int g, int b, int a, WKColorPickerResultListenerRef listener)
{
    Ewk_View_Smart_Data* sd = smartData();
    EINA_SAFETY_ON_NULL_RETURN(sd->api->input_picker_color_request);

    colorPickerResultListener = listener;

    sd->api->input_picker_color_request(sd, r, g, b, a);
}

/**
 * @internal
 * Requests to hide external color picker.
 */
void EwkViewImpl::dismissColorPicker()
{
    Ewk_View_Smart_Data* sd = smartData();
    EINA_SAFETY_ON_NULL_RETURN(sd->api->input_picker_color_dismiss);

    colorPickerResultListener.clear();

    sd->api->input_picker_color_dismiss(sd);
}
#endif

/**
 * @internal
 * informs that the view's back / forward list has changed.
 *
 * Emits signal: "back,forward,list,changed".
 */
void EwkViewImpl::informBackForwardListChange()
{
    evas_object_smart_callback_call(m_view, "back,forward,list,changed", 0);
}

/**
 * @internal
 * Web process has crashed.
 *
 * Emits signal: "webprocess,crashed" with pointer to crash handling boolean.
 */
void EwkViewImpl::informWebProcessCrashed()
{
    bool handled = false;
    evas_object_smart_callback_call(m_view, "webprocess,crashed", &handled);

    if (!handled) {
        CString url = pageProxy->urlAtProcessExit().utf8();
        WARN("WARNING: The web process experienced a crash on '%s'.\n", url.data());

        // Display an error page
        ewk_view_html_string_load(m_view, "The web process has crashed.", 0, url.data());
    }
}

void EwkViewImpl::informContentsSizeChange(const IntSize& size)
{
#if USE(COORDINATED_GRAPHICS)
    pageViewportControllerClient->didChangeContentsSize(size);
#else
    UNUSED_PARAM(size);
#endif
}

COMPILE_ASSERT_MATCHING_ENUM(EWK_TEXT_DIRECTION_RIGHT_TO_LEFT, RTL);
COMPILE_ASSERT_MATCHING_ENUM(EWK_TEXT_DIRECTION_LEFT_TO_RIGHT, LTR);

void EwkViewImpl::requestPopupMenu(WebPopupMenuProxyEfl* popupMenu, const IntRect& rect, TextDirection textDirection, double pageScaleFactor, const Vector<WebPopupItem>& items, int32_t selectedIndex)
{
    Ewk_View_Smart_Data* sd = smartData();
    ASSERT(sd->api);

    ASSERT(popupMenu);

    if (!sd->api->popup_menu_show)
        return;

    if (popupMenuProxy)
        ewk_view_popup_menu_close(m_view);
    popupMenuProxy = popupMenu;

    Eina_List* popupItems = 0;
    const size_t size = items.size();
    for (size_t i = 0; i < size; ++i)
        popupItems = eina_list_append(popupItems, Ewk_Popup_Menu_Item::create(items[i]).leakPtr());
    popupMenuItems = popupItems;

    sd->api->popup_menu_show(sd, rect, static_cast<Ewk_Text_Direction>(textDirection), pageScaleFactor, popupItems, selectedIndex);
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
 * The view was requested to update text input state
 */
void EwkViewImpl::updateTextInputState()
{
    if (!imfContext)
        return;

    const EditorState& editor = pageProxy->editorState();

    if (editor.isContentEditable) {
        if (isImfFocused)
            return;

        ecore_imf_context_reset(imfContext);
        ecore_imf_context_focus_in(imfContext);
        isImfFocused = true;
    } else {
        if (!isImfFocused)
            return;

        if (editor.hasComposition)
            pageProxy->cancelComposition();

        isImfFocused = false;
        ecore_imf_context_reset(imfContext);
        ecore_imf_context_focus_out(imfContext);
    }
}

/**
 * @internal
 * The url of view was changed by the frame loader.
 *
 * Emits signal: "url,changed" with pointer to new url string.
 */
void EwkViewImpl::informURLChange()
{
    String activeURL = pageProxy->activeURL();
    if (activeURL.isEmpty())
        return;

    CString rawActiveURL = activeURL.utf8();
    if (url == rawActiveURL.data())
        return;

    url = rawActiveURL.data();
    const char* callbackArgument = static_cast<const char*>(url);
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
