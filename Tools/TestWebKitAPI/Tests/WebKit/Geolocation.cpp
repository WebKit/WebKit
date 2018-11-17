/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#if WK_HAVE_C_SPI

#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <WebKit/WKContextPrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <string.h>
#include <vector>

using namespace std;

namespace TestWebKitAPI {

enum class GeolocationEvent {
    StartUpdating,
    StopUpdating,
    EnableHighAccuracy,
    DisableHighAccuracy
};

ostream& operator<<(ostream& outputStream, const GeolocationEvent& geolocationEvent)
{
    switch (geolocationEvent) {
    case GeolocationEvent::StartUpdating:
        outputStream << "GeolocationEvent::StartUpdating";
        break;
    case GeolocationEvent::StopUpdating:
        outputStream << "GeolocationEvent::StopUpdating";
        break;
    case GeolocationEvent::EnableHighAccuracy:
        outputStream << "GeolocationEvent::EnableHighAccuracy";
        break;
    case GeolocationEvent::DisableHighAccuracy:
        outputStream << "GeolocationEvent::DisableHighAccuracy";
        break;
    }
    return outputStream;
}

struct GeolocationStateTracker {
    vector<GeolocationEvent> events;

    virtual ~GeolocationStateTracker() { }
    virtual void eventsChanged() { }

    static void startUpdatingCallback(WKGeolocationManagerRef manager, const void* clientInfo)
    {
        GeolocationStateTracker* stateTracker = static_cast<GeolocationStateTracker*>(const_cast<void*>(clientInfo));
        stateTracker->events.push_back(GeolocationEvent::StartUpdating);
        stateTracker->eventsChanged();

        WKRetainPtr<WKGeolocationPositionRef> position = adoptWK(WKGeolocationPositionCreate(0, 50.644358, 3.345453, 2.53));
        WKGeolocationManagerProviderDidChangePosition(manager, position.get());
    }

    static void stopUpdatingCallback(WKGeolocationManagerRef, const void* clientInfo)
    {
        GeolocationStateTracker* stateTracker = static_cast<GeolocationStateTracker*>(const_cast<void*>(clientInfo));
        stateTracker->events.push_back(GeolocationEvent::StopUpdating);
        stateTracker->eventsChanged();
    }

    static void setEnableHighAccuracyCallback(WKGeolocationManagerRef, bool enable, const void* clientInfo)
    {
        GeolocationStateTracker* stateTracker = static_cast<GeolocationStateTracker*>(const_cast<void*>(clientInfo));
        if (enable)
            stateTracker->events.push_back(GeolocationEvent::EnableHighAccuracy);
        else
            stateTracker->events.push_back(GeolocationEvent::DisableHighAccuracy);
        stateTracker->eventsChanged();
    }
};

void decidePolicyForGeolocationPermissionRequestCallBack(WKPageRef page, WKFrameRef frame, WKSecurityOriginRef origin, WKGeolocationPermissionRequestRef permissionRequest, const void* clientInfo)
{
    WKGeolocationPermissionRequestAllow(permissionRequest);
}

void setupGeolocationProvider(WKContextRef context, void* clientInfo)
{
    WKGeolocationProviderV1 providerCallback;
    memset(&providerCallback, 0, sizeof(WKGeolocationProviderV1));

    providerCallback.base.version = 1;
    providerCallback.base.clientInfo = clientInfo;
    providerCallback.startUpdating = GeolocationStateTracker::startUpdatingCallback;
    providerCallback.stopUpdating = GeolocationStateTracker::stopUpdatingCallback;
    providerCallback.setEnableHighAccuracy = GeolocationStateTracker::setEnableHighAccuracyCallback;

    WKGeolocationManagerSetProvider(WKContextGetGeolocationManager(context), &providerCallback.base);
}
    
void clearGeolocationProvider(WKContextRef context)
{
    WKGeolocationManagerSetProvider(WKContextGetGeolocationManager(context), nullptr);
}

void setupView(PlatformWebView& webView)
{
    WKPageUIClientV2 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));

    uiClient.base.version = 2;
    uiClient.decidePolicyForGeolocationPermissionRequest = decidePolicyForGeolocationPermissionRequestCallBack;

    WKPageSetPageUIClient(webView.page(), &uiClient.base);
}

// GeolocationBasic.
struct GeolocationBasicStateTracker : GeolocationStateTracker {
    bool finished;

    GeolocationBasicStateTracker() : finished(false) { }
    virtual void eventsChanged()
    {
        switch (events.size()) {
        case 1:
            EXPECT_EQ(GeolocationEvent::DisableHighAccuracy, events[0]);
            break;
        case 2:
            EXPECT_EQ(GeolocationEvent::StartUpdating, events[1]);
            break;
        case 3:
            EXPECT_EQ(GeolocationEvent::StopUpdating, events[2]);
            finished = true;
            break;
        default:
            EXPECT_TRUE(false);
            finished = true;
        }
    }
};

TEST(WebKit, GeolocationBasic)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreate());

    GeolocationBasicStateTracker stateTracker;
    setupGeolocationProvider(context.get(), &stateTracker);

    PlatformWebView webView(context.get());
    setupView(webView);

    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("geolocationGetCurrentPosition", "html"));
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&stateTracker.finished);
    clearGeolocationProvider(context.get());
}

// Geolocation requested with High Accuracy.
struct GeolocationBasicWithHighAccuracyStateTracker : GeolocationStateTracker {
    bool finished;

    GeolocationBasicWithHighAccuracyStateTracker() : finished(false) { }
    virtual void eventsChanged()
    {
        switch (events.size()) {
        case 1:
            EXPECT_EQ(GeolocationEvent::EnableHighAccuracy, events[0]);
            break;
        case 2:
            EXPECT_EQ(GeolocationEvent::StartUpdating, events[1]);
            break;
        case 3:
            EXPECT_EQ(GeolocationEvent::StopUpdating, events[2]);
            finished = true;
            break;
        default:
            EXPECT_TRUE(false);
            finished = true;
        }
    }
};

TEST(WebKit, GeolocationBasicWithHighAccuracy)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreate());

    GeolocationBasicWithHighAccuracyStateTracker stateTracker;
    setupGeolocationProvider(context.get(), &stateTracker);

    PlatformWebView webView(context.get());
    setupView(webView);

    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("geolocationGetCurrentPositionWithHighAccuracy", "html"));
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&stateTracker.finished);
    clearGeolocationProvider(context.get());
}

// Geolocation start without High Accuracy, then requires High Accuracy.
struct GeolocationTransitionToHighAccuracyStateTracker : GeolocationStateTracker {
    bool finishedFirstStep { false };
    bool enabledHighAccuracy { false };
    bool finished { false };

    virtual void eventsChanged()
    {
        switch (events.size()) {
        case 1:
            EXPECT_EQ(GeolocationEvent::DisableHighAccuracy, events[0]);
            break;
        case 2:
            EXPECT_EQ(GeolocationEvent::StartUpdating, events[1]);
            finishedFirstStep = true;
            break;
        case 3:
            EXPECT_EQ(GeolocationEvent::EnableHighAccuracy, events[2]);
            enabledHighAccuracy = true;
            break;
        case 4:
            EXPECT_EQ(GeolocationEvent::DisableHighAccuracy, events[3]);
            break;
        case 5:
            EXPECT_EQ(GeolocationEvent::StopUpdating, events[4]);
            finished = true;
            break;
        default:
            EXPECT_TRUE(false);
            finishedFirstStep = true;
            enabledHighAccuracy = true;
            finished = true;
        }
    }
};

TEST(WebKit, GeolocationTransitionToHighAccuracy)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreate());

    GeolocationTransitionToHighAccuracyStateTracker stateTracker;
    setupGeolocationProvider(context.get(), &stateTracker);

    PlatformWebView lowAccuracyWebView(context.get());
    setupView(lowAccuracyWebView);
    WKRetainPtr<WKURLRef> lowAccuracyURL(AdoptWK, Util::createURLForResource("geolocationWatchPosition", "html"));
    WKPageLoadURL(lowAccuracyWebView.page(), lowAccuracyURL.get());
    Util::run(&stateTracker.finishedFirstStep);

    PlatformWebView highAccuracyWebView(lowAccuracyWebView.page());
    setupView(highAccuracyWebView);
    WKRetainPtr<WKURLRef> highAccuracyURL(AdoptWK, Util::createURLForResource("geolocationWatchPositionWithHighAccuracy", "html"));
    WKPageLoadURL(highAccuracyWebView.page(), highAccuracyURL.get());
    Util::run(&stateTracker.enabledHighAccuracy);
    
    WKRetainPtr<WKURLRef> resetUrl = adoptWK(WKURLCreateWithUTF8CString("about:blank"));
    WKPageLoadURL(highAccuracyWebView.page(), resetUrl.get());
    Util::run(&stateTracker.enabledHighAccuracy);
    WKPageLoadURL(lowAccuracyWebView.page(), resetUrl.get());
    Util::run(&stateTracker.finished);

    clearGeolocationProvider(context.get());
}

// Geolocation start with High Accuracy, then should fall back to low accuracy.
struct GeolocationTransitionToLowAccuracyStateTracker : GeolocationStateTracker {
    bool finishedFirstStep { false };
    bool disabledHighAccuracy { false };
    bool finished { false };

    virtual void eventsChanged()
    {
        switch (events.size()) {
        case 1:
            EXPECT_EQ(GeolocationEvent::EnableHighAccuracy, events[0]);
            break;
        case 2:
            EXPECT_EQ(GeolocationEvent::StartUpdating, events[1]);
            finishedFirstStep = true;
            break;
        case 3:
            EXPECT_EQ(GeolocationEvent::DisableHighAccuracy, events[2]);
            disabledHighAccuracy = true;
            break;
        case 4:
            EXPECT_EQ(GeolocationEvent::StopUpdating, events[3]);
            finished = true;
            break;
        default:
            EXPECT_TRUE(false);
            finishedFirstStep = true;
            disabledHighAccuracy = true;
            finished = true;
        }
    }
};

static void runJavaScriptAlert(WKPageRef page, WKStringRef alertText, WKFrameRef frame, const void* clientInfo)
{
    *static_cast<bool*>(const_cast<void*>(clientInfo)) = true;
}

TEST(WebKit, GeolocationTransitionToLowAccuracy)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreate());

    GeolocationTransitionToLowAccuracyStateTracker stateTracker;
    setupGeolocationProvider(context.get(), &stateTracker);

    PlatformWebView highAccuracyWebView(context.get());
    setupView(highAccuracyWebView);
    WKRetainPtr<WKURLRef> highAccuracyURL(AdoptWK, Util::createURLForResource("geolocationWatchPositionWithHighAccuracy", "html"));
    WKPageLoadURL(highAccuracyWebView.page(), highAccuracyURL.get());
    Util::run(&stateTracker.finishedFirstStep);

    PlatformWebView lowAccuracyWebView(context.get());
    setupView(lowAccuracyWebView);

    bool finishedSecondStep = false;

    WKPageUIClientV2 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));
    uiClient.base.version = 2;
    uiClient.base.clientInfo = &finishedSecondStep;
    uiClient.decidePolicyForGeolocationPermissionRequest = decidePolicyForGeolocationPermissionRequestCallBack;
    uiClient.runJavaScriptAlert = runJavaScriptAlert;
    WKPageSetPageUIClient(lowAccuracyWebView.page(), &uiClient.base);

    WKRetainPtr<WKURLRef> lowAccuracyURL(AdoptWK, Util::createURLForResource("geolocationWatchPosition", "html"));
    WKPageLoadURL(lowAccuracyWebView.page(), lowAccuracyURL.get());
    Util::run(&finishedSecondStep);

    WKRetainPtr<WKURLRef> resetUrl = adoptWK(WKURLCreateWithUTF8CString("about:blank"));
    WKPageLoadURL(highAccuracyWebView.page(), resetUrl.get());

    Util::run(&stateTracker.disabledHighAccuracy);

    WKPageLoadURL(lowAccuracyWebView.page(), resetUrl.get());
    Util::run(&stateTracker.finished);

    clearGeolocationProvider(context.get());
}

} // namespace TestWebKitAPI

#endif
