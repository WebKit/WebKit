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
#include "CookieStorageShim.h"

#if ENABLE(NETWORK_PROCESS)

#include "CookieStorageShimLibrary.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcessConnection.h"
#include "SessionTracker.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/URL.h>
#include <WebCore/SoftLinking.h>
#include <dlfcn.h>
#include <wtf/MainThread.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

typedef const struct _CFURLRequest* CFURLRequestRef;

SOFT_LINK_FRAMEWORK(CFNetwork)
SOFT_LINK(CFNetwork, CFURLRequestGetURL, CFURLRef, (CFURLRequestRef request), (request))

using namespace WebCore;

@interface WKNSURLSessionLocal : NSObject
- (CFDictionaryRef) _copyCookiesForRequestUsingAllAppropriateStorageSemantics:(CFURLRequestRef) request;
@end

namespace WebKit {

static CFDictionaryRef webKitCookieStorageCopyRequestHeaderFieldsForURL(CFHTTPCookieStorageRef inCookieStorage, CFURLRef inRequestURL)
{
    String cookies;
    URL firstPartyForCookiesURL;
    if (!WebProcess::shared().networkConnection()->connection()->sendSync(Messages::NetworkConnectionToWebProcess::CookieRequestHeaderFieldValue(SessionTracker::defaultSessionID, firstPartyForCookiesURL, inRequestURL), Messages::NetworkConnectionToWebProcess::CookiesForDOM::Reply(cookies), 0))
        return 0;

    if (cookies.isNull())
        return 0;

    RetainPtr<CFStringRef> cfCookies = cookies.createCFString();
    static const void* cookieKeys[] = { CFSTR("Cookie") };
    const void* cookieValues[] = { cfCookies.get() };
    return CFDictionaryCreate(0, cookieKeys, cookieValues, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

CookieStorageShim& CookieStorageShim::shared()
{
    static NeverDestroyed<CookieStorageShim> storage;
    return storage;
}

void CookieStorageShim::initialize()
{
    CookieStorageShimCallbacks callbacks = { &webKitCookieStorageCopyRequestHeaderFieldsForURL };
    CookieStorageShimInitializeFunc func = reinterpret_cast<CookieStorageShimInitializeFunc>(dlsym(RTLD_DEFAULT, "WebKitCookieStorageShimInitialize"));
    if (func)
        func(callbacks);

    Class __NSURLSessionLocalClass = objc_getClass("__NSURLSessionLocal");
    if (!__NSURLSessionLocalClass)
        return;

    Method original = class_getInstanceMethod(__NSURLSessionLocalClass, @selector(_copyCookiesForRequestUsingAllAppropriateStorageSemantics:));
    if (!original)
        return;

    Method replacement = class_getInstanceMethod([WKNSURLSessionLocal class], @selector(_copyCookiesForRequestUsingAllAppropriateStorageSemantics:));
    ASSERT(replacement);

    method_exchangeImplementations(original, replacement);
}

}

@implementation WKNSURLSessionLocal
- (CFDictionaryRef)_copyCookiesForRequestUsingAllAppropriateStorageSemantics:(CFURLRequestRef) request
{
    return WebKit::webKitCookieStorageCopyRequestHeaderFieldsForURL(nullptr, CFURLRequestGetURL(request));
}
@end

#endif // ENABLE(NETWORK_PROCESS)
