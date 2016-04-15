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
#include "WebKitFileChooserRequestPrivate.h"
#include "WebKitGeolocationPermissionRequestPrivate.h"
#include "WebKitNavigationActionPrivate.h"
#include "WebKitNotificationPermissionRequestPrivate.h"
#include "WebKitPrivate.h"
#include "WebKitURIRequestPrivate.h"
#include "WebKitUserMediaPermissionRequestPrivate.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebKitWindowPropertiesPrivate.h"
#include "WebPageProxy.h"
#include <WebCore/GtkUtilities.h>
#include <wtf/glib/GRefPtr.h>

using namespace WebKit;

class UIClient : public API::UIClient {
public:
    explicit UIClient(WebKitWebView* webView)
        : m_webView(webView)
    {
    }

private:
    PassRefPtr<WebPageProxy> createNewPage(WebPageProxy*, WebFrameProxy*, const WebCore::SecurityOriginData&, const WebCore::ResourceRequest& resourceRequest, const WebCore::WindowFeatures& windowFeatures, const NavigationActionData& navigationActionData) override
    {
        GRefPtr<WebKitURIRequest> request = adoptGRef(webkitURIRequestCreateForResourceRequest(resourceRequest));
        WebKitNavigationAction navigationAction(request.get(), navigationActionData);
        return webkitWebViewCreateNewPage(m_webView, windowFeatures, &navigationAction);
    }

    void showPage(WebPageProxy*) override
    {
        webkitWebViewReadyToShowPage(m_webView);
    }

    void close(WebPageProxy*) override
    {
        webkitWebViewClosePage(m_webView);
    }

    void runJavaScriptAlert(WebPageProxy*, const String& message, WebFrameProxy*, const WebCore::SecurityOriginData&, std::function<void ()> completionHandler) override
    {
        webkitWebViewRunJavaScriptAlert(m_webView, message.utf8());
        completionHandler();
    }

    void runJavaScriptConfirm(WebPageProxy*, const String& message, WebFrameProxy*, const WebCore::SecurityOriginData&, std::function<void (bool)> completionHandler) override
    {
        completionHandler(webkitWebViewRunJavaScriptConfirm(m_webView, message.utf8()));
    }

    void runJavaScriptPrompt(WebPageProxy*, const String& message, const String& defaultValue, WebFrameProxy*, const WebCore::SecurityOriginData&, std::function<void (const String&)> completionHandler) override
    {
        CString result = webkitWebViewRunJavaScriptPrompt(m_webView, message.utf8(), defaultValue.utf8());
        if (result.isNull()) {
            completionHandler(String());
            return;
        }

        completionHandler(String::fromUTF8(result.data()));
    }

    bool canRunBeforeUnloadConfirmPanel() const override { return true; }

    void runBeforeUnloadConfirmPanel(WebPageProxy*, const String& message, WebFrameProxy*, std::function<void (bool)> completionHandler) override
    {
        completionHandler(webkitWebViewRunJavaScriptBeforeUnloadConfirm(m_webView, message.utf8()));
    }

    void mouseDidMoveOverElement(WebPageProxy*, const WebHitTestResultData& data, WebEvent::Modifiers modifiers, API::Object*) override
    {
        webkitWebViewMouseTargetChanged(m_webView, data, toGdkModifiers(modifiers));
    }

    bool toolbarsAreVisible(WebPageProxy*) override
    {
        return webkit_window_properties_get_toolbar_visible(webkit_web_view_get_window_properties(m_webView));
    }

    void setToolbarsAreVisible(WebPageProxy*, bool visible) override
    {
        webkitWindowPropertiesSetToolbarVisible(webkit_web_view_get_window_properties(m_webView), visible);
    }

    bool menuBarIsVisible(WebPageProxy*) override
    {
        return webkit_window_properties_get_menubar_visible(webkit_web_view_get_window_properties(m_webView));
    }

    void setMenuBarIsVisible(WebPageProxy*, bool visible) override
    {
        webkitWindowPropertiesSetToolbarVisible(webkit_web_view_get_window_properties(m_webView), visible);
    }

    bool statusBarIsVisible(WebPageProxy*) override
    {
        return webkit_window_properties_get_statusbar_visible(webkit_web_view_get_window_properties(m_webView));
    }

    void setStatusBarIsVisible(WebPageProxy*, bool visible) override
    {
        webkitWindowPropertiesSetStatusbarVisible(webkit_web_view_get_window_properties(m_webView), visible);
    }

    bool isResizable(WebPageProxy*) override
    {
        return webkit_window_properties_get_resizable(webkit_web_view_get_window_properties(m_webView));
    }

    void setIsResizable(WebPageProxy*, bool resizable) override
    {
        webkitWindowPropertiesSetResizable(webkit_web_view_get_window_properties(m_webView), resizable);
    }

    void setWindowFrame(WebPageProxy*, const WebCore::FloatRect& frame) override
    {
        GdkRectangle geometry = WebCore::IntRect(frame);
        webkitWindowPropertiesSetGeometry(webkit_web_view_get_window_properties(m_webView), &geometry);
    }

    WebCore::FloatRect windowFrame(WebPageProxy*) override
    {
        GdkRectangle geometry = { 0, 0, 0, 0 };
        GtkWidget* window = gtk_widget_get_toplevel(GTK_WIDGET(m_webView));
        if (WebCore::widgetIsOnscreenToplevelWindow(window) && gtk_widget_get_visible(window)) {
            gtk_window_get_position(GTK_WINDOW(window), &geometry.x, &geometry.y);
            gtk_window_get_size(GTK_WINDOW(window), &geometry.width, &geometry.height);
        }
        return WebCore::FloatRect(geometry);
    }

    void exceededDatabaseQuota(WebPageProxy*, WebFrameProxy*, API::SecurityOrigin*, const String&, const String&, unsigned long long /*currentQuota*/, unsigned long long /*currentOriginUsage*/, unsigned long long /*currentDatabaseUsage*/, unsigned long long /*expectedUsage*/, std::function<void (unsigned long long)> completionHandler) override
    {
        static const unsigned long long defaultQuota = 5 * 1024 * 1204; // 5 MB
        // FIXME: Provide API for this.
        completionHandler(defaultQuota);
    }

    bool runOpenPanel(WebPageProxy*, WebFrameProxy*, const WebCore::SecurityOriginData&, API::OpenPanelParameters* parameters, WebOpenPanelResultListenerProxy* listener) override
    {
        GRefPtr<WebKitFileChooserRequest> request = adoptGRef(webkitFileChooserRequestCreate(parameters, listener));
        webkitWebViewRunFileChooserRequest(m_webView, request.get());
        return true;
    }

    bool decidePolicyForGeolocationPermissionRequest(WebPageProxy*, WebFrameProxy*, API::SecurityOrigin*, GeolocationPermissionRequestProxy* permissionRequest) override
    {
        GRefPtr<WebKitGeolocationPermissionRequest> geolocationPermissionRequest = adoptGRef(webkitGeolocationPermissionRequestCreate(permissionRequest));
        webkitWebViewMakePermissionRequest(m_webView, WEBKIT_PERMISSION_REQUEST(geolocationPermissionRequest.get()));
        return true;
    }

    bool decidePolicyForUserMediaPermissionRequest(WebPageProxy&, WebFrameProxy&, API::SecurityOrigin& userMediaDocumentOrigin, API::SecurityOrigin& topLevelDocumentOrigin, UserMediaPermissionRequestProxy& permissionRequest) override
    {
        GRefPtr<WebKitUserMediaPermissionRequest> userMediaPermissionRequest = adoptGRef(webkitUserMediaPermissionRequestCreate(permissionRequest, userMediaDocumentOrigin, topLevelDocumentOrigin));
        webkitWebViewMakePermissionRequest(m_webView, WEBKIT_PERMISSION_REQUEST(userMediaPermissionRequest.get()));
        return true;
    }

    bool decidePolicyForNotificationPermissionRequest(WebPageProxy*, API::SecurityOrigin*, NotificationPermissionRequest* permissionRequest) override
    {
        GRefPtr<WebKitNotificationPermissionRequest> notificationPermissionRequest = adoptGRef(webkitNotificationPermissionRequestCreate(permissionRequest));
        webkitWebViewMakePermissionRequest(m_webView, WEBKIT_PERMISSION_REQUEST(notificationPermissionRequest.get()));
        return true;
    }

    void printFrame(WebPageProxy*, WebFrameProxy* frame) override
    {
        webkitWebViewPrintFrame(m_webView, frame);
    }

    bool canRunModal() const override { return true; }

    void runModal(WebPageProxy*) override
    {
        webkitWebViewRunAsModal(m_webView);
    }

    void isPlayingAudioDidChange(WebPageProxy&) override
    {
        webkitWebViewIsPlayingAudioChanged(m_webView);
    }

    WebKitWebView* m_webView;
};

void attachUIClientToView(WebKitWebView* webView)
{
    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    page->setUIClient(std::make_unique<UIClient>(webView));
}

