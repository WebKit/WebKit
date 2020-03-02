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
#include "WebKitPointerLockPermissionRequestPrivate.h"
#include "WebKitURIRequestPrivate.h"
#include "WebKitUserMediaPermissionRequestPrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebKitWindowPropertiesPrivate.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/PlatformDisplay.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>

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

    ~UIClient()
    {
#if ENABLE(POINTER_LOCK)
        if (m_pointerLockPermissionRequest)
            g_object_remove_weak_pointer(G_OBJECT(m_pointerLockPermissionRequest), reinterpret_cast<void**>(&m_pointerLockPermissionRequest));
#endif
    }

private:
    void createNewPage(WebPageProxy& page, WebCore::WindowFeatures&& windowFeatures, Ref<API::NavigationAction>&& apiNavigationAction, CompletionHandler<void(RefPtr<WebPageProxy>&&)>&& completionHandler) final
    {
        WebKitNavigationAction navigationAction(WTFMove(apiNavigationAction));
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

    void runJavaScriptAlert(WebPageProxy&, const String& message, WebFrameProxy*, WebCore::SecurityOriginData&&, Function<void()>&& completionHandler) final
    {
        webkitWebViewRunJavaScriptAlert(m_webView, message.utf8(), WTFMove(completionHandler));
    }

    void runJavaScriptConfirm(WebPageProxy&, const String& message, WebFrameProxy*, WebCore::SecurityOriginData&&, Function<void(bool)>&& completionHandler) final
    {
        webkitWebViewRunJavaScriptConfirm(m_webView, message.utf8(), WTFMove(completionHandler));
    }

    void runJavaScriptPrompt(WebPageProxy&, const String& message, const String& defaultValue, WebFrameProxy*, WebCore::SecurityOriginData&&, Function<void(const String&)>&& completionHandler) final
    {
        webkitWebViewRunJavaScriptPrompt(m_webView, message.utf8(), defaultValue.utf8(), WTFMove(completionHandler));
    }

    bool canRunBeforeUnloadConfirmPanel() const final { return true; }

    void runBeforeUnloadConfirmPanel(WebPageProxy&, const String& message, WebFrameProxy*, WebCore::SecurityOriginData&&, Function<void(bool)>&& completionHandler) final
    {
        webkitWebViewRunJavaScriptBeforeUnloadConfirm(m_webView, message.utf8(), WTFMove(completionHandler));
    }

    void mouseDidMoveOverElement(WebPageProxy&, const WebHitTestResultData& data, OptionSet<WebEvent::Modifier> modifiers, API::Object*) final
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

#if PLATFORM(GTK)
    static gboolean windowConfigureEventCallback(GtkWindow* window, GdkEventConfigure*, GdkRectangle* targetGeometry)
    {
        GdkRectangle geometry = { 0, 0, 0, 0 };
        // Position a toplevel window is not supported under wayland.
#if PLATFORM(WAYLAND)
        if (WebCore::PlatformDisplay::sharedDisplay().type() != WebCore::PlatformDisplay::Type::Wayland)
#endif
        {
            gtk_window_get_position(window, &geometry.x, &geometry.y);
            if (geometry.x != targetGeometry->x || geometry.y != targetGeometry->y)
                return FALSE;
        }

        gtk_window_get_size(window, &geometry.width, &geometry.height);
        if (geometry.width != targetGeometry->width || geometry.height != targetGeometry->height)
            return FALSE;

        RunLoop::current().stop();
        return FALSE;
    }

    void setWindowFrameTimerFired()
    {
        RunLoop::current().stop();
    }
#endif

    void setWindowFrame(WebPageProxy&, const WebCore::FloatRect& frame) final
    {
#if PLATFORM(GTK)
        GdkRectangle geometry = WebCore::IntRect(frame);
        GtkWidget* window = gtk_widget_get_toplevel(GTK_WIDGET(m_webView));
        if (webkit_web_view_is_controlled_by_automation(m_webView) && WebCore::widgetIsOnscreenToplevelWindow(window) && gtk_widget_get_visible(window)) {
            bool needsMove = false;
            // Position a toplevel window is not supported under wayland.
#if PLATFORM(WAYLAND)
            if (WebCore::PlatformDisplay::sharedDisplay().type() != WebCore::PlatformDisplay::Type::Wayland)
#endif
            {
                if (geometry.x >= 0 && geometry.y >= 0) {
                    int x, y;
                    gtk_window_get_position(GTK_WINDOW(window), &x, &y);
                    needsMove = x != geometry.x || y != geometry.y;
                }
            }

            bool needsResize = false;
            if (geometry.width > 0 && geometry.height > 0) {
                int width, height;
                gtk_window_get_size(GTK_WINDOW(window), &width, &height);
                needsResize = width != geometry.width || height != geometry.height;
            }

            if (!needsMove && !needsResize)
                return;

            auto signalID = g_signal_connect(window, "configure-event", G_CALLBACK(windowConfigureEventCallback), &geometry);
            if (needsMove)
                gtk_window_move(GTK_WINDOW(window), geometry.x, geometry.y);
            if (needsResize)
                gtk_window_resize(GTK_WINDOW(window), geometry.width, geometry.height);

            // We need the move/resize to happen synchronously in automation mode, so we use a nested RunLoop
            // to wait, up top 200 milliseconds, for the configure events.
            auto timer = makeUnique<RunLoop::Timer<UIClient>>(RunLoop::main(), this, &UIClient::setWindowFrameTimerFired);
            timer->setPriority(RunLoopSourcePriority::RunLoopTimer);
            timer->startOneShot(200_ms);
            RunLoop::run();
            timer = nullptr;
            g_signal_handler_disconnect(window, signalID);
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

    bool runOpenPanel(WebPageProxy&, WebFrameProxy*, WebCore::SecurityOriginData&&, API::OpenPanelParameters* parameters, WebOpenPanelResultListenerProxy* listener) final
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

    void decidePolicyForUserMediaPermissionRequest(WebPageProxy&, WebFrameProxy&, API::SecurityOrigin& userMediaDocumentOrigin, API::SecurityOrigin& topLevelDocumentOrigin, UserMediaPermissionRequestProxy& permissionRequest) final
    {
        GRefPtr<WebKitUserMediaPermissionRequest> userMediaPermissionRequest = adoptGRef(webkitUserMediaPermissionRequestCreate(permissionRequest, userMediaDocumentOrigin, topLevelDocumentOrigin));
        webkitWebViewMakePermissionRequest(m_webView, WEBKIT_PERMISSION_REQUEST(userMediaPermissionRequest.get()));
    }

    void checkUserMediaPermissionForOrigin(WebPageProxy& page, WebFrameProxy&, API::SecurityOrigin& userMediaDocumentOrigin, API::SecurityOrigin& topLevelDocumentOrigin, UserMediaPermissionCheckProxy& permissionRequest) override
    {
        auto deviceInfoPermissionRequest = adoptGRef(webkitDeviceInfoPermissionRequestCreate(permissionRequest, &page.websiteDataStore().deviceIdHashSaltStorage()));
        webkitWebViewMakePermissionRequest(m_webView, WEBKIT_PERMISSION_REQUEST(deviceInfoPermissionRequest.get()));
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

#if ENABLE(POINTER_LOCK)
    void requestPointerLock(WebPageProxy* page) final
    {
        GRefPtr<WebKitPointerLockPermissionRequest> permissionRequest = adoptGRef(webkitPointerLockPermissionRequestCreate(m_webView));
        RELEASE_ASSERT(!m_pointerLockPermissionRequest);
        m_pointerLockPermissionRequest = permissionRequest.get();
        g_object_add_weak_pointer(G_OBJECT(m_pointerLockPermissionRequest), reinterpret_cast<void**>(&m_pointerLockPermissionRequest));
        webkitWebViewMakePermissionRequest(m_webView, WEBKIT_PERMISSION_REQUEST(permissionRequest.get()));
    }

    void didLosePointerLock(WebPageProxy*) final
    {
        if (m_pointerLockPermissionRequest) {
            webkitPointerLockPermissionRequestDidLosePointerLock(m_pointerLockPermissionRequest);
            g_object_remove_weak_pointer(G_OBJECT(m_pointerLockPermissionRequest), reinterpret_cast<void**>(&m_pointerLockPermissionRequest));
            m_pointerLockPermissionRequest = nullptr;
        }
        webkitWebViewDidLosePointerLock(m_webView);
    }
#endif

    WebKitWebView* m_webView;
#if ENABLE(POINTER_LOCK)
    WebKitPointerLockPermissionRequest* m_pointerLockPermissionRequest { nullptr };
#endif
};

void attachUIClientToView(WebKitWebView* webView)
{
    webkitWebViewGetPage(webView).setUIClient(makeUnique<UIClient>(webView));
}

