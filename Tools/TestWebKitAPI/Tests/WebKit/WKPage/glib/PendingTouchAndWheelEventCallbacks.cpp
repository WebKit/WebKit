/*
 * Copyright (C) 2026 Igalia S.L.
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

#include "config.h"

#if WK_HAVE_C_SPI && ENABLE(TOUCH_EVENTS) && (PLATFORM(GTK) || ENABLE(WPE_PLATFORM))

#include "Helpers/PlatformUtilities.h"
#include "Helpers/PlatformWebView.h"
#include <WebKit/WKPagePrivate.h>

#if PLATFORM(GTK)
#include <webkit/WebKitWebViewBaseInternal.h>
#else
#include <wpe/wpe-platform.h>
#endif

namespace TestWebKitAPI {

// Once enabled, the event handlers keep the web process busy so that the events are still pending when the page
// closes or its web process exits.
static constexpr const char* pageWithBusyEventHandlers = R"HTML(
<body style='margin: 0; width: 100vw; height: 200vh'>
<script>
let busy = false;
function busyWait() {
    if (!busy)
        return;
    const start = Date.now();
    while (Date.now() - start < 1000) { }
}
document.addEventListener('touchstart', busyWait, { passive: false });
document.addEventListener('wheel', busyWait, { passive: false });
</script>
</body>
)HTML";

static bool didFinishLoad;
// Static so that a callback that is not run before the test returns doesn't write to a dead stack frame later.
static bool didRunCallback;

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

static void setDidRunCallback(void*)
{
    didRunCallback = true;
}

static bool canSendEvents()
{
#if PLATFORM(GTK)
    return true;
#else
    // Sending events is only implemented for WPEPlatform, not for the legacy API.
    return g_type_class_peek(WPE_TYPE_DISPLAY);
#endif
}

#if PLATFORM(GTK)
static WebKitWebViewBase* webViewBase(PlatformWebView& webView)
{
    return const_cast<WebKitWebViewBase*>(reinterpret_cast<const WebKitWebViewBase*>(webView.platformView()));
}
#endif

enum class TouchType : bool { Down, Up };

static void sendTouch(PlatformWebView& webView, TouchType type)
{
#if PLATFORM(GTK)
    auto* view = webViewBase(webView);
    if (type == TouchType::Down)
        webkitWebViewBaseSynthesizeTouchEvent(view, TouchEventType::Start, { { 0, SyntheticTouchPoint::State::Pressed, 10, 10 } }, 0);
    else
        webkitWebViewBaseSynthesizeTouchEvent(view, TouchEventType::End, { { 0, SyntheticTouchPoint::State::Released, 10, 10 } }, 0);
#else
    auto* view = WKViewGetView(webView.platformView());
    auto* event = wpe_event_touch_new(type == TouchType::Down ? WPE_EVENT_TOUCH_DOWN : WPE_EVENT_TOUCH_UP, view, WPE_INPUT_SOURCE_TOUCHSCREEN, 0, static_cast<WPEModifiers>(0), 1, 10, 10);
    wpe_view_event(view, event);
    wpe_event_unref(event);
#endif
}

static void sendWheel(PlatformWebView& webView)
{
#if PLATFORM(GTK)
    webkitWebViewBaseSynthesizeWheelEvent(webViewBase(webView), 0, -1, 10, 10, WheelEventPhase::NoPhase, WheelEventPhase::NoPhase, false);
#else
    auto* view = WKViewGetView(webView.platformView());
    auto* event = wpe_event_scroll_new(view, WPE_INPUT_SOURCE_MOUSE, 0, static_cast<WPEModifiers>(0), 0, -10, FALSE, FALSE, 10, 10);
    wpe_view_event(view, event);
    wpe_event_unref(event);
#endif
}

// Sends a touch event and waits until it is handled. Returns whether it was sent to the web process.
static bool sendTouchAndWait(PlatformWebView& webView, TouchType type)
{
    sendTouch(webView, type);
    didRunCallback = false;
    WKPageDoAfterProcessingAllPendingTouchEvents(webView.page(), nullptr, setDidRunCallback);
    bool wasSent = !didRunCallback;
    Util::run(&didRunCallback);
    return wasSent;
}

// The UI process may send touch events to the web process only after a rendering update tells it about the touch
// event handlers, so send touches until one is sent.
static bool waitUntilTouchEventsAreSent(PlatformWebView& webView)
{
    for (unsigned i = 0; i < 100; ++i) {
        bool wasSent = sendTouchAndWait(webView, TouchType::Down);
        sendTouchAndWait(webView, TouchType::Up);
        if (wasSent)
            return true;
        Util::runFor(100_ms);
    }
    return false;
}

static void enableBusyEventHandlers(PlatformWebView& webView)
{
    static bool done;
    done = false;
    WKPageEvaluateJavaScriptInMainFrame(webView.page(), Util::toWK("busy = true").get(), nullptr, [](WKTypeRef, WKErrorRef, void*) {
        done = true;
    });
    Util::run(&done);
}

static void loadPageWithBusyEventHandlers(PlatformWebView& webView)
{
    WKPageNavigationClientV0 navigationClient { };
    navigationClient.base.version = 0;
    navigationClient.didFinishNavigation = didFinishNavigation;
    WKPageSetPageNavigationClient(webView.page(), &navigationClient.base);

    didFinishLoad = false;
    WKPageLoadHTMLString(webView.page(), Util::toWK(pageWithBusyEventHandlers).get(), nullptr);
    Util::run(&didFinishLoad);
}

TEST(WebKit, PendingTouchEventCallbacksRunOnClose)
{
    if (!canSendEvents())
        return;

    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());
    loadPageWithBusyEventHandlers(webView);
    ASSERT_TRUE(waitUntilTouchEventsAreSent(webView));
    enableBusyEventHandlers(webView);

    sendTouch(webView, TouchType::Down);
    didRunCallback = false;
    WKPageDoAfterProcessingAllPendingTouchEvents(webView.page(), nullptr, setDidRunCallback);
    ASSERT_FALSE(didRunCallback);

    WKPageClose(webView.page());
    EXPECT_TRUE(didRunCallback);
}

TEST(WebKit, PendingTouchEventCallbacksRunOnWebProcessTermination)
{
    if (!canSendEvents())
        return;

    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());
    loadPageWithBusyEventHandlers(webView);
    ASSERT_TRUE(waitUntilTouchEventsAreSent(webView));
    enableBusyEventHandlers(webView);

    sendTouch(webView, TouchType::Down);
    didRunCallback = false;
    WKPageDoAfterProcessingAllPendingTouchEvents(webView.page(), nullptr, setDidRunCallback);
    ASSERT_FALSE(didRunCallback);

    WKPageTerminate(webView.page());
    EXPECT_TRUE(didRunCallback);
}

TEST(WebKit, PendingWheelEventCallbacksRunOnClose)
{
    if (!canSendEvents())
        return;

    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());
    loadPageWithBusyEventHandlers(webView);
    enableBusyEventHandlers(webView);

    sendWheel(webView);
    didRunCallback = false;
    WKPageDoAfterProcessingAllPendingWheelEvents(webView.page(), nullptr, setDidRunCallback);
    ASSERT_FALSE(didRunCallback);

    WKPageClose(webView.page());
    EXPECT_TRUE(didRunCallback);
}

TEST(WebKit, PendingWheelEventCallbacksRunOnWebProcessTermination)
{
    if (!canSendEvents())
        return;

    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());
    loadPageWithBusyEventHandlers(webView);
    enableBusyEventHandlers(webView);

    sendWheel(webView);
    didRunCallback = false;
    WKPageDoAfterProcessingAllPendingWheelEvents(webView.page(), nullptr, setDidRunCallback);
    ASSERT_FALSE(didRunCallback);

    WKPageTerminate(webView.page());
    EXPECT_TRUE(didRunCallback);
}

} // namespace TestWebKitAPI

#endif // WK_HAVE_C_SPI && ENABLE(TOUCH_EVENTS) && (PLATFORM(GTK) || ENABLE(WPE_PLATFORM))
