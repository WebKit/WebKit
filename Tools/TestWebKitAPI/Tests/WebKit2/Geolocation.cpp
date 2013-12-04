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
#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <WebKit2/WKRetainPtr.h>
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

void setupGeolocationProvider(WKContextRef context, void *clientInfo)
{
    WKGeolocationProviderV1 providerCallback;
    memset(&providerCallback, 0, sizeof(WKGeolocationProvider));

    providerCallback.base.version = 1;
    providerCallback.base.clientInfo = clientInfo;
    providerCallback.startUpdating = GeolocationStateTracker::startUpdatingCallback;
    providerCallback.stopUpdating = GeolocationStateTracker::stopUpdatingCallback;
    providerCallback.setEnableHighAccuracy = GeolocationStateTracker::setEnableHighAccuracyCallback;

    WKGeolocationManagerSetProvider(WKContextGetGeolocationManager(context), &providerCallback.base);
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

TEST(WebKit2, GeolocationBasic)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreate());

    GeolocationBasicStateTracker stateTracker;
    setupGeolocationProvider(context.get(), &stateTracker);

    PlatformWebView webView(context.get());
    setupView(webView);

    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("geolocationGetCurrentPosition", "html"));
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&stateTracker.finished);
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

TEST(WebKit2, GeolocationBasicWithHighAccuracy)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreate());

    GeolocationBasicWithHighAccuracyStateTracker stateTracker;
    setupGeolocationProvider(context.get(), &stateTracker);

    PlatformWebView webView(context.get());
    setupView(webView);

    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("geolocationGetCurrentPositionWithHighAccuracy", "html"));
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&stateTracker.finished);
}

// Geolocation start without High Accuracy, then requires High Accuracy.
struct GeolocationTransitionToHighAccuracyStateTracker : GeolocationStateTracker {
    bool finishedFirstStep;
    bool finished;

    GeolocationTransitionToHighAccuracyStateTracker()
        : finishedFirstStep(false)
        , finished(false)
    {
    }

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
            finished = true;
            break;
        default:
            EXPECT_TRUE(false);
            finishedFirstStep = true;
            finished = true;
        }
    }
};

TEST(WebKit2, GeolocationTransitionToHighAccuracy)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreate());

    GeolocationTransitionToHighAccuracyStateTracker stateTracker;
    setupGeolocationProvider(context.get(), &stateTracker);

    PlatformWebView lowAccuracyWebView(context.get());
    setupView(lowAccuracyWebView);
    WKRetainPtr<WKURLRef> lowAccuracyURL(AdoptWK, Util::createURLForResource("geolocationWatchPosition", "html"));
    WKPageLoadURL(lowAccuracyWebView.page(), lowAccuracyURL.get());
    Util::run(&stateTracker.finishedFirstStep);

    PlatformWebView highAccuracyWebView(context.get());
    setupView(highAccuracyWebView);
    WKRetainPtr<WKURLRef> highAccuracyURL(AdoptWK, Util::createURLForResource("geolocationWatchPositionWithHighAccuracy", "html"));
    WKPageLoadURL(highAccuracyWebView.page(), highAccuracyURL.get());
    Util::run(&stateTracker.finished);
}

// Geolocation start with High Accuracy, then should fall back to low accuracy.
struct GeolocationTransitionToLowAccuracyStateTracker : GeolocationStateTracker {
    bool finishedFirstStep;
    bool finished;

    GeolocationTransitionToLowAccuracyStateTracker()
        : finishedFirstStep(false)
        , finished(false)
    {
    }

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
            finished = true;
            break;
        default:
            EXPECT_TRUE(false);
            finishedFirstStep = true;
            finished = true;
        }
    }
};

static void didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    *static_cast<bool*>(const_cast<void*>(clientInfo)) = true;
}

TEST(WebKit2, GeolocationTransitionToLowAccuracy)
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

    WKPageLoaderClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.base.clientInfo = &finishedSecondStep;
    loaderClient.didFinishLoadForFrame = didFinishLoadForFrame;

    WKPageSetPageLoaderClient(lowAccuracyWebView.page(), &loaderClient.base);

    WKRetainPtr<WKURLRef> lowAccuracyURL(AdoptWK, Util::createURLForResource("geolocationWatchPosition", "html"));
    WKPageLoadURL(lowAccuracyWebView.page(), lowAccuracyURL.get());
    Util::run(&finishedSecondStep);

    WKRetainPtr<WKURLRef> resetUrl = adoptWK(WKURLCreateWithUTF8CString("about:blank"));
    WKPageLoadURL(highAccuracyWebView.page(), resetUrl.get());
    Util::run(&stateTracker.finished);
}

} // namespace TestWebKitAPI
