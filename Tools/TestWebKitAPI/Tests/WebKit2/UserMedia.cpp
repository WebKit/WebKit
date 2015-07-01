/*
 * Copyright (C) 2014 Igalia S.L
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
#include <WebKit/WKRetainPtr.h>
#include <string.h>
#include <vector>

namespace TestWebKitAPI {

static bool done;

void decidePolicyForUserMediaPermissionRequestCallBack(WKPageRef, WKFrameRef, WKSecurityOriginRef, WKUserMediaPermissionRequestRef permissionRequest, const void* /* clientInfo */)
{
    WKUserMediaPermissionRequestAllow(permissionRequest);
    done = true;
}

TEST(WebKit2, UserMediaBasic)
{
    auto context = adoptWK(WKContextCreate());
    PlatformWebView webView(context.get());
    WKPageUIClientV5 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));


    uiClient.base.version = 5;
    uiClient.decidePolicyForUserMediaPermissionRequest = decidePolicyForUserMediaPermissionRequestCallBack;

    WKPageSetPageUIClient(webView.page(), &uiClient.base);

    done = false;
    auto url = adoptWK(Util::createURLForResource("getUserMedia", "html"));
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&done);
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)

#endif // WK_HAVE_C_SPI

