/*
 * Copyright (C) 2014 Igalia S.L
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if WK_HAVE_C_SPI

#if ENABLE(MEDIA_STREAM)
#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKPreferencesRef.h>
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <string.h>
#include <vector>

namespace TestWebKitAPI {

static bool wasPrompted;
static bool isPlayingChanged;
static bool didFinishLoad;

static void nullJavaScriptCallback(WKSerializedScriptValueRef, WKErrorRef error, void*)
{
}

static void decidePolicyForUserMediaPermissionRequestCallBack(WKPageRef, WKFrameRef, WKSecurityOriginRef, WKSecurityOriginRef, WKUserMediaPermissionRequestRef permissionRequest, const void* /* clientInfo */)
{
    WKRetainPtr<WKArrayRef> audioDeviceUIDs = WKUserMediaPermissionRequestAudioDeviceUIDs(permissionRequest);
    WKRetainPtr<WKArrayRef> videoDeviceUIDs = WKUserMediaPermissionRequestVideoDeviceUIDs(permissionRequest);

    if (WKArrayGetSize(videoDeviceUIDs.get()) || WKArrayGetSize(audioDeviceUIDs.get())) {
        WKRetainPtr<WKStringRef> videoDeviceUID;
        if (WKArrayGetSize(videoDeviceUIDs.get()))
            videoDeviceUID = reinterpret_cast<WKStringRef>(WKArrayGetItemAtIndex(videoDeviceUIDs.get(), 0));
        else
            videoDeviceUID = WKStringCreateWithUTF8CString("");

        WKRetainPtr<WKStringRef> audioDeviceUID;
        if (WKArrayGetSize(audioDeviceUIDs.get()))
            audioDeviceUID = reinterpret_cast<WKStringRef>(WKArrayGetItemAtIndex(audioDeviceUIDs.get(), 0));
        else
            audioDeviceUID = WKStringCreateWithUTF8CString("");

        WKUserMediaPermissionRequestAllow(permissionRequest, audioDeviceUID.get(), videoDeviceUID.get());
    }

    wasPrompted = true;
}

static void isPlayingDidChangeCallback(WKPageRef page, const void*)
{
    isPlayingChanged = true;
}

static void didFinishLoadForFrame(WKPageRef page, WKFrameRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

TEST(WebKit2, UserMediaBasic)
{
    auto context = adoptWK(WKContextCreate());

    WKRetainPtr<WKPageGroupRef> pageGroup(AdoptWK, WKPageGroupCreateWithIdentifier(Util::toWK("GetUserMedia").get()));
    WKPreferencesRef preferences = WKPageGroupGetPreferences(pageGroup.get());
    WKPreferencesSetMediaStreamEnabled(preferences, true);
    WKPreferencesSetFileAccessFromFileURLsAllowed(preferences, true);
    WKPreferencesSetMediaCaptureRequiresSecureConnection(preferences, false);
    WKPreferencesSetMockCaptureDevicesEnabled(preferences, true);

    WKPageLoaderClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));
    loaderClient.base.version = 0;
    loaderClient.didFinishLoadForFrame = didFinishLoadForFrame;

    WKPageUIClientV6 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));
    uiClient.base.version = 6;
    uiClient.isPlayingAudioDidChange = isPlayingDidChangeCallback;
    uiClient.decidePolicyForUserMediaPermissionRequest = decidePolicyForUserMediaPermissionRequestCallBack;

    PlatformWebView webView(context.get(), pageGroup.get());
    WKPageSetPageUIClient(webView.page(), &uiClient.base);
    WKPageSetPageLoaderClient(webView.page(), &loaderClient.base);

    auto url = adoptWK(Util::createURLForResource("getUserMedia", "html"));
    ASSERT(url.get());
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&didFinishLoad);

    WKMediaState captureState = WKPageGetMediaState(webView.page());
    EXPECT_FALSE(captureState & kWKMediaHasActiveVideoCaptureDevice);
    EXPECT_FALSE(captureState & kWKMediaHasActiveAudioCaptureDevice);

    WKPageRunJavaScriptInMainFrame(webView.page(), Util::toWK("requestAccess()").get(), 0, nullJavaScriptCallback);
    Util::run(&wasPrompted);

    captureState = WKPageGetMediaState(webView.page());
    if (!(captureState & kWKMediaHasActiveVideoCaptureDevice)) {
        Util::run(&isPlayingChanged);
        captureState = WKPageGetMediaState(webView.page());
    }

    EXPECT_TRUE(captureState & kWKMediaHasActiveVideoCaptureDevice);
    EXPECT_FALSE(captureState & kWKMediaHasActiveAudioCaptureDevice);

    isPlayingChanged = false;
    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple", "html")).get());
    Util::run(&isPlayingChanged);

    captureState = WKPageGetMediaState(webView.page());
    EXPECT_FALSE(captureState & kWKMediaHasActiveVideoCaptureDevice);
    EXPECT_FALSE(captureState & kWKMediaHasActiveAudioCaptureDevice);
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)

#endif // WK_HAVE_C_SPI

