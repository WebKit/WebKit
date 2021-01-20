/*
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
#include <WebKit/WKPreferencesRef.h>
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKUserMediaPermissionCheck.h>
#include <string.h>
#include <vector>

namespace TestWebKitAPI {

static bool loadedFirstTime;
static bool loadedSecondTime;

void checkUserMediaPermissionCallback(WKPageRef, WKFrameRef, WKSecurityOriginRef, WKSecurityOriginRef, WKUserMediaPermissionCheckRef permissionRequest, const void*)
{
    WKUserMediaPermissionCheckSetUserMediaAccessInfo(permissionRequest, WKStringCreateWithUTF8CString("0x123456789"), true);
    if (!loadedFirstTime) {
        loadedFirstTime = true;
        return;
    }

    loadedSecondTime = true;
}

TEST(WebKit, EnumerateDevices)
{
    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));

    WKRetainPtr<WKPageGroupRef> pageGroup = adoptWK(WKPageGroupCreateWithIdentifier(Util::toWK("EnumerateDevices").get()));
    WKPreferencesRef preferences = WKPageGroupGetPreferences(pageGroup.get());
    WKPreferencesSetMediaDevicesEnabled(preferences, true);
    WKPreferencesSetFileAccessFromFileURLsAllowed(preferences, true);
    WKPreferencesSetMediaCaptureRequiresSecureConnection(preferences, false);

    WKPageUIClientV6 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));
    uiClient.base.version = 6;
    uiClient.checkUserMediaPermissionForOrigin = checkUserMediaPermissionCallback;

    PlatformWebView webView(context.get(), pageGroup.get());
    WKPageSetPageUIClient(webView.page(), &uiClient.base);

    auto url = adoptWK(Util::createURLForResource("enumerateMediaDevices", "html"));

    // Load and kill the page.
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&loadedFirstTime);
    WKPageTerminate(webView.page());

    // Load it again to make sure the user media process manager doesn't assert.
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&loadedSecondTime);
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)

#endif // WK_HAVE_C_SPI
