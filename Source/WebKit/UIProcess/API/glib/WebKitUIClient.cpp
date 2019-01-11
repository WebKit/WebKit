/*
 * Copyright (C) 2011, 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitUIClient.h"

#include "APIUIClient.h"
#include "DrawingAreaProxy.h"
#include "WebKitDeviceInfoPermissionRequestPrivate.h"
#include "WebKitFileChooserRequestPrivate.h"
#include "WebKitGeolocationPermissionRequestPrivate.h"
#include "WebKitNavigationActionPrivate.h"
#include "WebKitNotificationPermissionRequestPrivate.h"
#include "WebKitURIRequestPrivate.h"
#include "WebKitUserMediaPermissionRequestPrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebKitWindowPropertiesPrivate.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <wtf/glib/GRefPtr.h>

#if PLATFORM(GTK)
#include <WebCore/GtkUtilities.h>
#endif

using namespace WebKit;

class UIClient : public API::UIClient {
public:
    explicit UIClient(WebKitWebView* webView)
        : m_webView(webView)
    {
    }

private:
    void createNewPage(WebPageProxy& page, Ref<API::FrameInfo>&& frameInfo, WebCore::ResourceRequest&& resourceRequest, WebCore::WindowFeatures&& windowFeatures, NavigationActionData&& navigationActionData, CompletionHandler<void(RefPtr<WebPageProxy>&&)>&& completionHandler) final
    {
        auto userInitiatedActivity = page.process().userInitiatedActivity(navigationActionData.userGestureTokenIdentifier);
        WebKitNavigationAction navigationAction(API::NavigationAction::create(WTFMove(navigationActionData), frameInfo.ptr(), nullptr, WTF::nullopt, WTFMove(resourceRequest), URL { }, false, WTFMove(userInitiatedActivity)));
        completionHandler(webkitWebViewCreateNewPage(m_webView, windowFeatures, &navigationAction));
    }

    void showPage(WebPageProxy*) final
    {
        webkitWebViewReadyToShowPage(m_webView);
    }

    void close(WebPageProxy*) final
    {
        webkitWebViewClosePage(m_webView);
    }

    void runJavaScriptAlert(WebPageProxy*, const String& message, WebFrameProxy*, const WebCore::SecurityOriginData&, Function<void()>&& completionHandler) final
    {
        webkitWebViewRunJavaScriptAlert(m_webView, message.utf8(), WTFMove(completionHandler));
    }

    void runJavaScriptConfirm(WebPageProxy*, const String& message, WebFrameProxy*, const WebCore::SecurityOriginData&, Function<void(bool)>&& completionHandler) final
    {
        webkitWebViewRunJavaScriptConfirm(m_webView, message.utf8(), WTFMove(completionHandler));
    }

    void runJavaScriptPrompt(WebPageProxy*, const String& message, const String& defaultValue, WebFrameProxy*, const WebCore::SecurityOriginData&, Function<void(const String&)>&& completionHandler) final
    {
        webkitWebViewRunJavaScriptPrompt(m_webView, message.utf8(), defaultValue.utf8(), WTFMove(completionHandler));
    }

    bool canRunBeforeUnloadConfirmPanel() const final { return true; }

    void runBeforeUnloadConfirmPanel(WebPageProxy*, const String& message, WebFrameProxy*, const WebCore::SecurityOriginData&, Function<void(bool)>&& completionHandler) final
    {
        webkitWebViewRunJavaScriptBeforeUnloadConfirm(m_webView, message.utf8(), WTFMove(completionHandler));
    }

    void mouseDidMoveOverElement(WebPageProxy&, const WebHitTestResultData& data, WebEvent::Modifiers modifiers, API::Object*) final
    {
        webkitWebViewMouseTargetChanged(m_webView, data, modifiers);
    }

    void toolbarsAreVisible(WebPageProxy&, Function<void(bool)>&& completionHandler) final
    {
        completionHandler(webkit_window_properties_get_toolbar_visible(webkit_web_view_get_window_properties(m_webView)));
    }

    void setToolbarsAreVisible(WebPageProxy&, bool visible) final
    {
        webkitWindowPropertiesSetToolbarVisible(webkit_web_view_get_window_properties(m_webView), visible);
    }

    void menuBarIsVisible(WebPageProxy&, Function<void(bool)>&& completionHandler) final
    {
        completionHandler(webkit_window_properties_get_menubar_visible(webkit_web_view_get_window_properties(m_webView)));
    }

    void setMenuBarIsVisible(WebPageProxy&, bool visible) final
    {
        webkitWindowPropertiesSetToolbarVisible(webkit_web_view_get_window_properties(m_webView), visible);
    }

    void statusBarIsVisible(WebPageProxy&, Function<void(bool)>&& completionHandler) final
    {
        completionHandler(webkit_window_properties_get_statusbar_visible(webkit_web_view_get_window_properties(m_webView)));
    }

    void setStatusBarIsVisible(WebPageProxy&, bool visible) final
    {
        webkitWindowPropertiesSetStatusbarVisible(webkit_web_view_get_window_properties(m_webView), visible);
    }

    void setIsResizable(WebPageProxy&, bool resizable) final
    {
        webkitWindowPropertiesSetResizable(webkit_web_view_get_window_properties(m_webView), resizable);
    }

    void setWindowFrame(WebPageProxy&, const WebCore::FloatRect& frame) final
    {
#if PLATFORM(GTK)
        GdkRectangle geometry = WebCore::IntRect(frame);
        GtkWidget* window = gtk_widget_get_toplevel(GTK_WIDGET(m_webView));
        if (webkit_web_view_is_controlled_by_automation(m_webView) && WebCore::widgetIsOnscreenToplevelWindow(window) && gtk_widget_get_visible(window)) {
            if (geometry.x >= 0 && geometry.y >= 0)
                gtk_window_move(GTK_WINDOW(window), geometry.x, geometry.y);
            if (geometry.width > 0 && geometry.height > 0)
                gtk_window_resize(GTK_WINDOW(window), geometry.width, geometry.height);
        } else
            webkitWindowPropertiesSetGeometry(webkit_web_view_get_window_properties(m_webView), &geometry);
#endif
    }

    void windowFrame(WebPageProxy&, Function<void(WebCore::FloatRect)>&& completionHandler) final
    {
#if PLATFORM(GTK)
        GdkRectangle geometry = { 0, 0, 0, 0 };
        GtkWidget* window = gtk_widget_get_toplevel(GTK_WIDGET(m_webView));
        if (WebCore::widgetIsOnscreenToplevelWindow(window) && gtk_widget_get_visible(window)) {
            gtk_window_get_position(GTK_WINDOW(window), &geometry.x, &geometry.y);
            gtk_window_get_size(GTK_WINDOW(window), &geometry.width, &geometry.height);
        }
        completionHandler(WebCore::FloatRect(geometry));
#elif PLATFORM(WPE)
        // FIXME: I guess this is actually the view size in WPE. We need more refactoring here.
        WebCore::FloatRect rect;
        auto& page = webkitWebViewGetPage(m_webView);
        if (page.drawingArea())
            rect.setSize(page.drawingArea()->size());
        completionHandler(WTFMove(rect));
#endif
    }

    void exceededDatabaseQuota(WebPageProxy*, WebFrameProxy*, API::SecurityOrigin*, const String&, const String&, unsigned long long /*currentQuota*/, unsigned long long /*currentOriginUsage*/, unsigned long long /*currentDatabaseUsage*/, unsigned long long /*expectedUsage*/, Function<void(unsigned long long)>&& completionHandler) final
    {
        static const unsigned long long defaultQuota = 5 * 1024 * 1204; // 5 MB
        // FIXME: Provide API for this.
        completionHandler(defaultQuota);
    }

    bool runOpenPanel(WebPageProxy*, WebFrameProxy*, const WebCore::SecurityOriginData&, API::OpenPanelParameters* parameters, WebOpenPanelResultListenerProxy* listener) final
    {
        GRefPtr<WebKitFileChooserRequest> request = adoptGRef(webkitFileChooserRequestCreate(parameters, listener));
        webkitWebViewRunFileChooserRequest(m_webView, request.get());
        return true;
    }

    void decidePolicyForGeolocationPermissionRequest(WebPageProxy&, WebFrameProxy&, API::SecurityOrigin&, Function<void(bool)>& completionHandler) final
    {
        GRefPtr<WebKitGeolocationPermissionRequest> geolocationPermissionRequest = adoptGRef(webkitGeolocationPermissionRequestCreate(GeolocationPermissionRequest::create(std::exchange(completionHandler, nullptr)).ptr()));
        webkitWebViewMakePermissionRequest(m_webView, WEBKIT_PERMISSION_REQUEST(geolocationPermissionRequest.get()));
    }

    bool decidePolicyForUserMediaPermissionRequest(WebPageProxy&, WebFrameProxy&, API::SecurityOrigin& userMediaDocumentOrigin, API::SecurityOrigin& topLevelDocumentOrigin, UserMediaPermissionRequestProxy& permissionRequest) final
    {
        GRefPtr<WebKitUserMediaPermissionRequest> userMediaPermissionRequest = adoptGRef(webkitUserMediaPermissionRequestCreate(permissionRequest, userMediaDocumentOrigin, topLevelDocumentOrigin));
        webkitWebViewMakePermissionRequest(m_webView, WEBKIT_PERMISSION_REQUEST(userMediaPermissionRequest.get()));
        return true;
    }

    bool checkUserMediaPermissionForOrigin(WebPageProxy& page, WebFrameProxy&, API::SecurityOrigin& userMediaDocumentOrigin, API::SecurityOrigin& topLevelDocumentOrigin, UserMediaPermissionCheckProxy& permissionRequest) override
    {
        auto deviceInfoPermissionRequest = adoptGRef(webkitDeviceInfoPermissionRequestCreate(permissionRequest, &page.websiteDataStore().deviceIdHashSaltStorage()));
        webkitWebViewMakePermissionRequest(m_webView, WEBKIT_PERMISSION_REQUEST(deviceInfoPermissionRequest.get()));
        return true;
    }

    void decidePolicyForNotificationPermissionRequest(WebPageProxy&, API::SecurityOrigin&, Function<void(bool)>&& completionHandler) final
    {
        GRefPtr<WebKitNotificationPermissionRequest> notificationPermissionRequest = adoptGRef(webkitNotificationPermissionRequestCreate(NotificationPermissionRequest::create(WTFMove(completionHandler)).ptr()));
        webkitWebViewMakePermissionRequest(m_webView, WEBKIT_PERMISSION_REQUEST(notificationPermissionRequest.get()));
    }

#if PLATFORM(GTK)
    void printFrame(WebPageProxy&, WebFrameProxy& frame) final
    {
        webkitWebViewPrintFrame(m_webView, &frame);
    }
#endif

    bool canRunModal() const final { return true; }

    void runModal(WebPageProxy&) final
    {
        webkitWebViewRunAsModal(m_webView);
    }

    void isPlayingMediaDidChange(WebPageProxy&) final
    {
        webkitWebViewIsPlayingAudioChanged(m_webView);
    }

    WebKitWebView* m_webView;
};

void attachUIClientToView(WebKitWebView* webView)
{
    webkitWebViewGetPage(webView).setUIClient(std::make_unique<UIClient>(webView));
}

