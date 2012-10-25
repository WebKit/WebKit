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

#ifndef EwkViewImpl_h
#define EwkViewImpl_h

#include "RefPtrEfl.h"
#include "WKEinaSharedString.h"
#include "WKRetainPtr.h"
#include <Ecore_IMF.h>
#include <Ecore_IMF_Evas.h>
#include <Evas.h>
#include <WebCore/TextDirection.h>
#include <WebCore/Timer.h>
#include <WebKit2/WKBase.h>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

#if USE(ACCELERATED_COMPOSITING)
#include <Evas_GL.h>
#endif

#define EWK_VIEW_IMPL_GET(smartData, impl)                                     \
    EwkViewImpl* impl = smartData->priv

#define EWK_VIEW_IMPL_GET_OR_RETURN(smartData, impl, ...)                      \
    if (!smartData) {                                                          \
        EINA_LOG_CRIT("smart data is null");                                   \
        return __VA_ARGS__;                                                    \
    }                                                                          \
    EWK_VIEW_IMPL_GET(smartData, impl);                                        \
    do {                                                                       \
        if (!impl) {                                                           \
            EINA_LOG_CRIT("no private data for object %p (%s)",                \
                smartData->self, evas_object_type_get(smartData->self));       \
            return __VA_ARGS__;                                                \
        }                                                                      \
    } while (0)

namespace WebKit {
class FindClientEfl;
class FormClientEfl;
class PageClientImpl;
class PageLoadClientEfl;
class PagePolicyClientEfl;
class PageUIClientEfl;
class PageViewportControllerClientEfl;
class PageViewportController;
class ResourceLoadClientEfl;
class WebPageProxy;
class WebPopupItem;
class WebPopupMenuProxyEfl;
}

namespace WebCore {
class Cursor;
class IntRect;
class IntSize;
}

class Ewk_Back_Forward_List;
class Ewk_Context;
class Ewk_Download_Job;
class Ewk_Error;
class Ewk_Form_Submission_Request;
class Ewk_Intent;
class Ewk_Intent_Service;
class Ewk_Navigation_Policy_Decision;
class Ewk_Resource;
class Ewk_Settings;
class Ewk_Url_Request;
class Ewk_Url_Response;

typedef struct Ewk_View_Smart_Data Ewk_View_Smart_Data;

class EwkViewImpl {
public:
    explicit EwkViewImpl(Evas_Object* view);
    ~EwkViewImpl();

    static EwkViewImpl* fromEvasObject(const Evas_Object* view);

    inline Evas_Object* view() { return m_view; }
    WKPageRef wkPage();
    inline WebKit::WebPageProxy* page() { return pageProxy.get(); }

    WebCore::IntSize size() const;

    void setCursor(const WebCore::Cursor& cursor);
    void redrawRegion(const WebCore::IntRect& rect);
    void setImageData(void* imageData, const WebCore::IntSize& size);

    static void addToPageViewMap(const Evas_Object* ewkView);
    static void removeFromPageViewMap(const Evas_Object* ewkView);
    static const Evas_Object* viewFromPageViewMap(const WKPageRef);

#if ENABLE(FULLSCREEN_API)
    void enterFullScreen();
    void exitFullScreen();
#endif

#if USE(ACCELERATED_COMPOSITING)
    bool createGLSurface(const WebCore::IntSize& viewSize);
    bool enterAcceleratedCompositingMode();
    bool exitAcceleratedCompositingMode();
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    void requestColorPicker(int r, int g, int b, int a, WKColorPickerResultListenerRef listener);
    void dismissColorPicker();
#endif

    WKPageRef createNewPage();
    void closePage();

    void requestPopupMenu(WebKit::WebPopupMenuProxyEfl*, const WebCore::IntRect&, WebCore::TextDirection, double pageScaleFactor, const Vector<WebKit::WebPopupItem>& items, int32_t selectedIndex);
    void updateTextInputState();

    void requestJSAlertPopup(const WKEinaSharedString& message);
    bool requestJSConfirmPopup(const WKEinaSharedString& message);
    WKEinaSharedString requestJSPromptPopup(const WKEinaSharedString& message, const WKEinaSharedString& defaultValue);

    void informDownloadJobCancelled(Ewk_Download_Job* download);
    void informDownloadJobFailed(Ewk_Download_Job* download, Ewk_Error* error);
    void informDownloadJobFinished(Ewk_Download_Job* download);
    void informDownloadJobRequested(Ewk_Download_Job* download);

    void informNewFormSubmissionRequest(Ewk_Form_Submission_Request* request);
    void informLoadError(Ewk_Error* error);
    void informLoadFinished();
    void informLoadProgress(double progress);
    void informProvisionalLoadFailed(Ewk_Error* error);
#if USE(TILED_BACKING_STORE)
    void informLoadCommitted();
#endif
    void informProvisionalLoadRedirect();
    void informProvisionalLoadStarted();

    void informResourceLoadStarted(Ewk_Resource* resource, Ewk_Url_Request* request);
    void informResourceLoadResponse(Ewk_Resource* resource, Ewk_Url_Response* response);
    void informResourceLoadFailed(Ewk_Resource* resource, Ewk_Error* error);
    void informResourceLoadFinished(Ewk_Resource* resource);
    void informResourceRequestSent(Ewk_Resource* resource, Ewk_Url_Request* request, Ewk_Url_Response* redirectResponse);

    void informNavigationPolicyDecision(Ewk_Navigation_Policy_Decision* decision);
    void informNewWindowPolicyDecision(Ewk_Navigation_Policy_Decision* decision);
    void informBackForwardListChange();

    void informTitleChange(const String& title);
    void informTooltipTextChange(const String& text);
    void informTextFound(unsigned matchCount);
    void informIconChange();
    void informWebProcessCrashed();
    void informContentsSizeChange(const WebCore::IntSize& size);
    unsigned long long informDatabaseQuotaReached(const String& databaseName, const String& displayName, unsigned long long currentQuota, unsigned long long currentOriginUsage, unsigned long long currentDatabaseUsage, unsigned long long expectedUsage);
    void informURLChange();

#if ENABLE(WEB_INTENTS)
    void informIntentRequest(Ewk_Intent* ewkIntent);
#endif
#if ENABLE(WEB_INTENTS_TAG)
    void informIntentServiceRegistration(Ewk_Intent_Service* ewkIntentService);
#endif

    // FIXME: Make members private for encapsulation.
    OwnPtr<WebKit::PageClientImpl> pageClient;
#if USE(TILED_BACKING_STORE)
    OwnPtr<WebKit::PageViewportControllerClientEfl> pageViewportControllerClient;
    OwnPtr<WebKit::PageViewportController> pageViewportController;
#endif
    RefPtr<WebKit::WebPageProxy> pageProxy;
    OwnPtr<WebKit::PageLoadClientEfl> pageLoadClient;
    OwnPtr<WebKit::PagePolicyClientEfl> pagePolicyClient;
    OwnPtr<WebKit::PageUIClientEfl> pageUIClient;
    OwnPtr<WebKit::ResourceLoadClientEfl> resourceLoadClient;
    OwnPtr<WebKit::FindClientEfl> findClient;
    OwnPtr<WebKit::FormClientEfl> formClient;

    WKEinaSharedString url;
    WKEinaSharedString title;
    WKEinaSharedString theme;
    WKEinaSharedString customEncoding;
    WKEinaSharedString cursorGroup;
    WKEinaSharedString faviconURL;
    RefPtr<Evas_Object> cursorObject;
    OwnPtr<Ewk_Back_Forward_List> backForwardList;
    OwnPtr<Ewk_Settings> settings;
    bool areMouseEventsEnabled;
    WKRetainPtr<WKColorPickerResultListenerRef> colorPickerResultListener;
    RefPtr<Ewk_Context> context;
#if ENABLE(TOUCH_EVENTS)
    bool areTouchEventsEnabled;
#endif

    WebKit::WebPopupMenuProxyEfl* popupMenuProxy;
    Eina_List* popupMenuItems;

    Ecore_IMF_Context* imfContext;
    bool isImfFocused;

#ifdef HAVE_ECORE_X
    bool isUsingEcoreX;
#endif

#if USE(ACCELERATED_COMPOSITING)
    Evas_GL* evasGl;
    Evas_GL_Context* evasGlContext;
    Evas_GL_Surface* evasGlSurface;
#endif

private:
    Ewk_View_Smart_Data* smartData();
    void displayTimerFired(WebCore::Timer<EwkViewImpl>*);

    Evas_Object* m_view;
    typedef HashMap<WKPageRef, const Evas_Object*> PageViewMap;
    static PageViewMap pageViewMap;
    WebCore::Timer<EwkViewImpl> m_displayTimer;
    WTF::Vector <WebCore::IntRect> m_dirtyRects;
};

#endif // EwkViewImpl_h
