/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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

#if !USE(NETWORK_SESSION)

#include "CookieStorageShimLibrary.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcessConnection.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/CookiesStrategy.h>
#include <WebCore/URL.h>
#include <dlfcn.h>
#include <pal/SessionID.h>
#include <pal/spi/cf/CFNetworkSPI.h>
#include <wtf/MainThread.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/SoftLinking.h>
#include <wtf/text/WTFString.h>

typedef const struct _CFURLRequest* CFURLRequestRef;
@class NSURLSessionTask;

using namespace WebCore;

@interface WKNSURLSessionLocal : NSObject
- (CFDictionaryRef) _copyCookiesForRequestUsingAllAppropriateStorageSemantics:(CFURLRequestRef) request;
- (void)_getCookieHeadersForTask:(NSURLSessionTask*)task completionHandler:(void (^)(CFDictionaryRef))completionHandler;
@end

namespace WebKit {

static CFDictionaryRef webKitCookieStorageCopyRequestHeaderFieldsForURL(CFHTTPCookieStorageRef inCookieStorage, CFURLRef inRequestURL)
{
    IncludeSecureCookies includeSecureCookies = URL(inRequestURL).protocolIs("https") ? IncludeSecureCookies::Yes : IncludeSecureCookies::No;

    String cookies;
    bool secureCookiesAccessed = false;
    URL firstPartyForCookiesURL;
    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::CookieRequestHeaderFieldValue(PAL::SessionID::defaultSessionID(), firstPartyForCookiesURL, inRequestURL, std::nullopt, std::nullopt, includeSecureCookies), Messages::NetworkConnectionToWebProcess::CookieRequestHeaderFieldValue::Reply(cookies, secureCookiesAccessed), 0))
        return 0;

    if (cookies.isNull())
        return 0;

    RetainPtr<CFStringRef> cfCookies = cookies.createCFString();
    static const void* cookieKeys[] = { CFSTR("Cookie") };
    const void* cookieValues[] = { cfCookies.get() };
    return CFDictionaryCreate(0, cookieKeys, cookieValues, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

CookieStorageShim& CookieStorageShim::singleton()
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

    if (Method original = class_getInstanceMethod(__NSURLSessionLocalClass, @selector(_copyCookiesForRequestUsingAllAppropriateStorageSemantics:))) {
        Method replacement = class_getInstanceMethod([WKNSURLSessionLocal class], @selector(_copyCookiesForRequestUsingAllAppropriateStorageSemantics:));
        ASSERT(replacement);
        method_exchangeImplementations(original, replacement);
    }

    if (Method original = class_getInstanceMethod(__NSURLSessionLocalClass, @selector(_getCookieHeadersForTask:completionHandler:))) {
        Method replacement = class_getInstanceMethod([WKNSURLSessionLocal class], @selector(_getCookieHeadersForTask:completionHandler:));
        ASSERT(replacement);
        method_exchangeImplementations(original, replacement);
    }
}

}

@implementation WKNSURLSessionLocal
- (CFDictionaryRef)_copyCookiesForRequestUsingAllAppropriateStorageSemantics:(CFURLRequestRef) request
{
    if (!CFURLRequestShouldHandleHTTPCookies(request))
        return nullptr;
    return WebKit::webKitCookieStorageCopyRequestHeaderFieldsForURL(nullptr, CFURLRequestGetURL(request));
}

using CompletionHandlerBlock = void(^)(CFDictionaryRef);
- (void)_getCookieHeadersForTask:(NSURLSessionTask*)task completionHandler:(CompletionHandlerBlock)completionHandler
{
    if (!completionHandler)
        return;

    if (![[task currentRequest] HTTPShouldHandleCookies]) {
        completionHandler(nullptr);
        return;
    }

    CompletionHandlerBlock completionHandlerCopy = [completionHandler copy];
    RunLoop::main().dispatch([task = RetainPtr<NSURLSessionTask>(task), completionHandlerCopy] {
        RetainPtr<CFDictionaryRef> headers = adoptCF(WebKit::webKitCookieStorageCopyRequestHeaderFieldsForURL(nullptr, (CFURLRef)[[task currentRequest] URL]));
        completionHandlerCopy(headers.get());
        [completionHandlerCopy release];
    });
}

@end

#endif

