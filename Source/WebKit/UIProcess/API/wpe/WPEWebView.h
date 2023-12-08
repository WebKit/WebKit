/*
 * Copyright (C) 2014 Igalia S.L.
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

#pragma once

#include "APIObject.h"
#include "GRefPtrWPE.h"
#include "InputMethodFilter.h"
#include "KeyAutoRepeatHandler.h"
#include "PageClientImpl.h"
#include "WebFullScreenManagerProxy.h"
#include "WebPageProxy.h"
#include <WebCore/ActivityState.h>
#include <memory>
#include <wtf/OptionSet.h>
#include <wtf/RefPtr.h>
#include <wtf/glib/GRefPtr.h>

#if ENABLE(ACCESSIBILITY)
#include "WebKitWebViewAccessible.h"
#endif

typedef struct OpaqueJSContext* JSGlobalContextRef;
typedef struct _WebKitInputMethodContext WebKitInputMethodContext;
typedef struct _WPEDisplay WPEDisplay;
typedef struct _WPEView WPEView;
struct wpe_input_keyboard_event;
struct wpe_view_backend;

namespace API {
class ViewClient;
}

namespace WebCore {
class Cursor;
struct CompositionUnderline;
}

namespace WebKit {
class AcceleratedBackingStoreDMABuf;
class TouchGestureController;
class WebKitWebResourceLoadManager;
class WebPageGroup;
class WebPlatformTouchPoint;
class WebProcessPool;
struct EditingRange;
struct UserMessage;
}

namespace WKWPE {

class View : public API::ObjectImpl<API::Object::Type::View> {
public:
#if ENABLE(WPE_PLATFORM)
    static View* create(struct wpe_view_backend* backend, WPEDisplay* display, const API::PageConfiguration& configuration)
    {
        return new View(backend, display, configuration);
    }
#else
    static View* create(struct wpe_view_backend* backend, const API::PageConfiguration& configuration)
    {
        return new View(backend, configuration);
    }
#endif

    ~View();

    // Client methods
    void setClient(std::unique_ptr<API::ViewClient>&&);
    void frameDisplayed();
    void willStartLoad();
    void didChangePageID();
    void didReceiveUserMessage(WebKit::UserMessage&&, CompletionHandler<void(WebKit::UserMessage&&)>&&);
    WebKit::WebKitWebResourceLoadManager* webResourceLoadManager();

    void setInputMethodContext(WebKitInputMethodContext*);
    WebKitInputMethodContext* inputMethodContext() const;
    void setInputMethodState(std::optional<WebKit::InputMethodState>&&);
    void synthesizeCompositionKeyPress(const String&, std::optional<Vector<WebCore::CompositionUnderline>>&&, std::optional<WebKit::EditingRange>&&);

    void selectionDidChange();

    WebKit::WebPageProxy& page() { return *m_pageProxy; }

    API::ViewClient& client() const { return *m_client; }
    struct wpe_view_backend* backend() { return m_backend; }
#if ENABLE(WPE_PLATFORM)
    WPEView* wpeView() const { return m_wpeView.get(); }
#endif

    const WebCore::IntSize& size() const { return m_size; }

    OptionSet<WebCore::ActivityState> viewState() const { return m_viewStateFlags; }

    void close();

#if ENABLE(FULLSCREEN_API)
    bool isFullScreen() const;
    void willEnterFullScreen();
#if ENABLE(WPE_PLATFORM)
    void enterFullScreen();
    void didEnterFullScreen();
#endif
    void willExitFullScreen();
#if ENABLE(WPE_PLATFORM)
    void exitFullScreen();
    void didExitFullScreen();
    void requestExitFullScreen();
#endif
    bool setFullScreen(bool);
#endif

#if ENABLE(ACCESSIBILITY)
    WebKitWebViewAccessible* accessible() const;
#endif

#if ENABLE(TOUCH_EVENTS)
    WebKit::TouchGestureController& touchGestureController() const { return *m_touchGestureController; }
#endif
#if ENABLE(GAMEPAD)
    static WebKit::WebPageProxy* platformWebPageProxyForGamepadInput();
#endif

#if ENABLE(WPE_PLATFORM)
    void updateAcceleratedSurface(uint64_t);
#endif

    void setCursor(const WebCore::Cursor&);

private:
#if ENABLE(WPE_PLATFORM)
    View(struct wpe_view_backend*, WPEDisplay*, const API::PageConfiguration&);
#else
    View(struct wpe_view_backend*, const API::PageConfiguration&);
#endif

    void setSize(const WebCore::IntSize&);
    void setViewState(OptionSet<WebCore::ActivityState>);
    void handleKeyboardEvent(struct wpe_input_keyboard_event*);

#if ENABLE(TOUCH_EVENTS) && ENABLE(WPE_PLATFORM)
    Vector<WebKit::WebPlatformTouchPoint> touchPointsForEvent(WPEEvent*);
#endif

    std::unique_ptr<API::ViewClient> m_client;

#if ENABLE(TOUCH_EVENTS)
    std::unique_ptr<WebKit::TouchGestureController> m_touchGestureController;
#if ENABLE(WPE_PLATFORM)
    HashMap<uint32_t, GRefPtr<WPEEvent>> m_touchEvents;
#endif
#endif
    std::unique_ptr<WebKit::PageClientImpl> m_pageClient;
    RefPtr<WebKit::WebPageProxy> m_pageProxy;
    WebCore::IntSize m_size;
    OptionSet<WebCore::ActivityState> m_viewStateFlags;

    struct wpe_view_backend* m_backend;
#if ENABLE(WPE_PLATFORM)
    GRefPtr<WPEView> m_wpeView;
    std::unique_ptr<WebKit::AcceleratedBackingStoreDMABuf> m_backingStore;
#endif

#if ENABLE(FULLSCREEN_API)
    WebKit::WebFullScreenManagerProxy::FullscreenState m_fullscreenState { WebKit::WebFullScreenManagerProxy::FullscreenState::NotInFullscreen };
    bool m_viewWasAlreadyInFullScreen { false };
#endif

#if ENABLE(ACCESSIBILITY)
    mutable GRefPtr<WebKitWebViewAccessible> m_accessible;
#endif

    bool m_horizontalScrollActive { false };
    bool m_verticalScrollActive { false };

    WebKit::InputMethodFilter m_inputMethodFilter;
    WebKit::KeyAutoRepeatHandler m_keyAutoRepeatHandler;
};

} // namespace WKWPE
