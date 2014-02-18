/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef APIUIClient_h
#define APIUIClient_h

#include "WKPage.h"
#include "WebEvent.h"
#include "WebHitTestResult.h"
#include <WebCore/FloatRect.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {
class ResourceRequest;
struct WindowFeatures;
}

namespace WebKit {
class GeolocationPermissionRequestProxy;
class ImmutableDictionary;
class NativeWebKeyboardEvent;
class NativeWebWheelEvent;
class NotificationPermissionRequest;
class WebColorPickerResultListenerProxy;
class WebFrameProxy;
class WebOpenPanelParameters;
class WebOpenPanelResultListenerProxy;
class WebPageProxy;
class WebSecurityOrigin;
}

namespace API {

class Data;
class Object;

class UIClient {
public:
    virtual ~UIClient() { }

    virtual PassRefPtr<WebKit::WebPageProxy> createNewPage(WebKit::WebPageProxy*, const WebCore::ResourceRequest&, const WebCore::WindowFeatures&, WebKit::WebEvent::Modifiers, WebKit::WebMouseEvent::Button) { return nullptr; }
    virtual void showPage(WebKit::WebPageProxy*) { }
    virtual void close(WebKit::WebPageProxy*) { }

    virtual void takeFocus(WebKit::WebPageProxy*, WKFocusDirection) { }
    virtual void focus(WebKit::WebPageProxy*) { }
    virtual void unfocus(WebKit::WebPageProxy*) { }

    virtual void runJavaScriptAlert(WebKit::WebPageProxy*, const WTF::String&, WebKit::WebFrameProxy*) { }
    virtual bool runJavaScriptConfirm(WebKit::WebPageProxy*, const WTF::String&, WebKit::WebFrameProxy*) { return false; }
    virtual WTF::String runJavaScriptPrompt(WebKit::WebPageProxy*, const WTF::String&, const WTF::String&, WebKit::WebFrameProxy*) { return WTF::String(); }

    virtual void setStatusText(WebKit::WebPageProxy*, const WTF::String&) { }
    virtual void mouseDidMoveOverElement(WebKit::WebPageProxy*, const WebKit::WebHitTestResult::Data&, WebKit::WebEvent::Modifiers, API::Object*) { }
#if ENABLE(NETSCAPE_PLUGIN_API)
    virtual void unavailablePluginButtonClicked(WebKit::WebPageProxy*, WKPluginUnavailabilityReason, WebKit::ImmutableDictionary*) { }
#endif // ENABLE(NETSCAPE_PLUGIN_API)

    virtual bool implementsDidNotHandleKeyEvent() const { return false; }
    virtual void didNotHandleKeyEvent(WebKit::WebPageProxy*, const WebKit::NativeWebKeyboardEvent&) { }

    virtual bool implementsDidNotHandleWheelEvent() const { return false; }
    virtual void didNotHandleWheelEvent(WebKit::WebPageProxy*, const WebKit::NativeWebWheelEvent&) { }

    virtual bool toolbarsAreVisible(WebKit::WebPageProxy*) { return true; }
    virtual void setToolbarsAreVisible(WebKit::WebPageProxy*, bool) { }
    virtual bool menuBarIsVisible(WebKit::WebPageProxy*) { return true; }
    virtual void setMenuBarIsVisible(WebKit::WebPageProxy*, bool) { }
    virtual bool statusBarIsVisible(WebKit::WebPageProxy*) { return true; }
    virtual void setStatusBarIsVisible(WebKit::WebPageProxy*, bool) { }
    virtual bool isResizable(WebKit::WebPageProxy*) { return true; }
    virtual void setIsResizable(WebKit::WebPageProxy*, bool) { }

    virtual void setWindowFrame(WebKit::WebPageProxy*, const WebCore::FloatRect&) { }
    virtual WebCore::FloatRect windowFrame(WebKit::WebPageProxy*) { return WebCore::FloatRect(); }

    virtual bool canRunBeforeUnloadConfirmPanel() const { return false; }
    virtual bool runBeforeUnloadConfirmPanel(WebKit::WebPageProxy*, const WTF::String&, WebKit::WebFrameProxy*) { return true; }

    virtual void didDraw(WebKit::WebPageProxy*) { }
    virtual void pageDidScroll(WebKit::WebPageProxy*) { }

    virtual unsigned long long exceededDatabaseQuota(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, WebKit::WebSecurityOrigin*, const WTF::String& databaseName, const WTF::String& databaseDisplayName, unsigned long long currentQuota, unsigned long long currentOriginUsage, unsigned long long currentDatabaseUsage, unsigned long long expectedUsage) { return currentQuota; }

    virtual bool runOpenPanel(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, WebKit::WebOpenPanelParameters*, WebKit::WebOpenPanelResultListenerProxy*) { return false; }
    virtual bool decidePolicyForGeolocationPermissionRequest(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, WebKit::WebSecurityOrigin*, WebKit::GeolocationPermissionRequestProxy*) { return false; }
    virtual bool decidePolicyForNotificationPermissionRequest(WebKit::WebPageProxy*, WebKit::WebSecurityOrigin*, WebKit::NotificationPermissionRequest*) { return false; }

    // Printing.
    virtual float headerHeight(WebKit::WebPageProxy*, WebKit::WebFrameProxy*) { return 0; }
    virtual float footerHeight(WebKit::WebPageProxy*, WebKit::WebFrameProxy*) { return 0; }
    virtual void drawHeader(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, const WebCore::FloatRect&) { }
    virtual void drawFooter(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, const WebCore::FloatRect&) { }
    virtual void printFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*) { }

    virtual bool canRunModal() const { return false; }
    virtual void runModal(WebKit::WebPageProxy*) { }

    virtual void saveDataToFileInDownloadsFolder(WebKit::WebPageProxy*, const WTF::String& suggestedFilename, const WTF::String& mimeType, const WTF::String& originatingURLString, API::Data*) { }

    virtual bool shouldInterruptJavaScript(WebKit::WebPageProxy*) { return false; }
};

} // namespace API

#endif // APIUIClient_h
