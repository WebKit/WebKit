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
#include "WebKitPrivate.h"
#include "WebKitURIRequestPrivate.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebKitWindowPropertiesPrivate.h"
#include "WebPageProxy.h"
#include <WebCore/GtkUtilities.h>
#include <wtf/gobject/GRefPtr.h>

using namespace WebKit;

class UIClient : public API::UIClient {
public:
    explicit UIClient(WebKitWebView* webView)
        : m_webView(webView)
    {
    }

private:
    virtual PassRefPtr<WebPageProxy> createNewPage(WebPageProxy*, WebFrameProxy*, const WebCore::ResourceRequest& resourceRequest, const WebCore::WindowFeatures& windowFeatures, const NavigationActionData& navigationActionData) override
    {
        GRefPtr<WebKitURIRequest> request = adoptGRef(webkitURIRequestCreateForResourceRequest(resourceRequest));
        WebKitNavigationAction navigationAction(request.get(), navigationActionData);
        return webkitWebViewCreateNewPage(m_webView, windowFeatures, &navigationAction);
    }

    virtual void showPage(WebPageProxy*) override
    {
        webkitWebViewReadyToShowPage(m_webView);
    }

    virtual void close(WebPageProxy*) override
    {
        webkitWebViewClosePage(m_webView);
    }

    virtual void runJavaScriptAlert(WebPageProxy*, const String& message, WebFrameProxy*, std::function<void ()> completionHandler) override
    {
        webkitWebViewRunJavaScriptAlert(m_webView, message.utf8());
        completionHandler();
    }

    virtual void runJavaScriptConfirm(WebPageProxy*, const String& message, WebFrameProxy*, std::function<void (bool)> completionHandler) override
    {
        completionHandler(webkitWebViewRunJavaScriptConfirm(m_webView, message.utf8()));
    }

    virtual void runJavaScriptPrompt(WebPageProxy*, const String& message, const String& defaultValue, WebFrameProxy*, std::function<void (const String&)> completionHandler) override
    {
        CString result = webkitWebViewRunJavaScriptPrompt(m_webView, message.utf8(), defaultValue.utf8());
        if (result.isNull()) {
            completionHandler(String());
            return;
        }

        completionHandler(String::fromUTF8(result.data()));
    }

    virtual void mouseDidMoveOverElement(WebPageProxy*, const WebHitTestResult::Data& data, WebEvent::Modifiers modifiers, API::Object*) override
    {
        webkitWebViewMouseTargetChanged(m_webView, data, toGdkModifiers(modifiers));
    }

    virtual bool toolbarsAreVisible(WebPageProxy*) override
    {
        return webkit_window_properties_get_toolbar_visible(webkit_web_view_get_window_properties(m_webView));
    }

    virtual void setToolbarsAreVisible(WebPageProxy*, bool visible) override
    {
        webkitWindowPropertiesSetToolbarVisible(webkit_web_view_get_window_properties(m_webView), visible);
    }

    virtual bool menuBarIsVisible(WebPageProxy*) override
    {
        return webkit_window_properties_get_menubar_visible(webkit_web_view_get_window_properties(m_webView));
    }

    virtual void setMenuBarIsVisible(WebPageProxy*, bool visible) override
    {
        webkitWindowPropertiesSetToolbarVisible(webkit_web_view_get_window_properties(m_webView), visible);
    }

    virtual bool statusBarIsVisible(WebPageProxy*) override
    {
        return webkit_window_properties_get_statusbar_visible(webkit_web_view_get_window_properties(m_webView));
    }

    virtual void setStatusBarIsVisible(WebPageProxy*, bool visible) override
    {
        webkitWindowPropertiesSetStatusbarVisible(webkit_web_view_get_window_properties(m_webView), visible);
    }

    virtual bool isResizable(WebPageProxy*) override
    {
        return webkit_window_properties_get_resizable(webkit_web_view_get_window_properties(m_webView));
    }

    virtual void setIsResizable(WebPageProxy*, bool resizable) override
    {
        webkitWindowPropertiesSetResizable(webkit_web_view_get_window_properties(m_webView), resizable);
    }

    virtual void setWindowFrame(WebPageProxy*, const WebCore::FloatRect& frame) override
    {
        GdkRectangle geometry = WebCore::IntRect(frame);
        webkitWindowPropertiesSetGeometry(webkit_web_view_get_window_properties(m_webView), &geometry);
    }

    virtual WebCore::FloatRect windowFrame(WebPageProxy*) override
    {
        GdkRectangle geometry = { 0, 0, 0, 0 };
        GtkWidget* window = gtk_widget_get_toplevel(GTK_WIDGET(m_webView));
        if (WebCore::widgetIsOnscreenToplevelWindow(window) && gtk_widget_get_visible(window)) {
            gtk_window_get_position(GTK_WINDOW(window), &geometry.x, &geometry.y);
            gtk_window_get_size(GTK_WINDOW(window), &geometry.width, &geometry.height);
        }
        return WebCore::FloatRect(geometry);
    }

    virtual bool runOpenPanel(WebPageProxy*, WebFrameProxy*, WebOpenPanelParameters* parameters, WebOpenPanelResultListenerProxy* listener) override
    {
        GRefPtr<WebKitFileChooserRequest> request = adoptGRef(webkitFileChooserRequestCreate(parameters, listener));
        webkitWebViewRunFileChooserRequest(m_webView, request.get());
        return true;
    }

    virtual bool decidePolicyForGeolocationPermissionRequest(WebPageProxy*, WebFrameProxy*, WebSecurityOrigin*, GeolocationPermissionRequestProxy* permissionRequest) override
    {
        GRefPtr<WebKitGeolocationPermissionRequest> geolocationPermissionRequest = adoptGRef(webkitGeolocationPermissionRequestCreate(permissionRequest));
        webkitWebViewMakePermissionRequest(m_webView, WEBKIT_PERMISSION_REQUEST(geolocationPermissionRequest.get()));
        return true;
    }

    virtual void printFrame(WebPageProxy*, WebFrameProxy* frame) override
    {
        webkitWebViewPrintFrame(m_webView, frame);
    }

    virtual bool canRunModal() const override { return true; }

    virtual void runModal(WebPageProxy*) override
    {
        webkitWebViewRunAsModal(m_webView);
    }

    WebKitWebView* m_webView;
};

void attachUIClientToView(WebKitWebView* webView)
{
    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    page->setUIClient(std::make_unique<UIClient>(webView));
}

