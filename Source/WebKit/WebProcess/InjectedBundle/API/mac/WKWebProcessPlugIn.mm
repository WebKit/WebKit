/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKWebProcessPlugInInternal.h"

#if WK_API_ENABLED

#import "APIArray.h"
#import "WKConnectionInternal.h"
#import "WKBundle.h"
#import "WKBundleAPICast.h"
#import "WKRetainPtr.h"
#import "WKStringCF.h"
#import "WKWebProcessPlugInBrowserContextControllerInternal.h"
#import <wtf/RetainPtr.h>

@interface WKWebProcessPlugInController () {
    API::ObjectStorage<WebKit::InjectedBundle> _bundle;
    RetainPtr<id <WKWebProcessPlugIn>> _principalClassInstance;
}
@end

@implementation WKWebProcessPlugInController

- (void)dealloc
{
    _bundle->~InjectedBundle();

    [super dealloc];
}

static void didCreatePage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    auto plugInController = (__bridge WKWebProcessPlugInController *)clientInfo;
    id <WKWebProcessPlugIn> principalClassInstance = plugInController->_principalClassInstance.get();

    if ([principalClassInstance respondsToSelector:@selector(webProcessPlugIn:didCreateBrowserContextController:)])
        [principalClassInstance webProcessPlugIn:plugInController didCreateBrowserContextController:wrapper(*WebKit::toImpl(page))];
}

static void willDestroyPage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    auto plugInController = (__bridge WKWebProcessPlugInController *)clientInfo;
    id <WKWebProcessPlugIn> principalClassInstance = plugInController->_principalClassInstance.get();

    if ([principalClassInstance respondsToSelector:@selector(webProcessPlugIn:willDestroyBrowserContextController:)])
        [principalClassInstance webProcessPlugIn:plugInController willDestroyBrowserContextController:wrapper(*WebKit::toImpl(page))];
}

static void setUpBundleClient(WKWebProcessPlugInController *plugInController, WebKit::InjectedBundle& bundle)
{
    WKBundleClientV1 bundleClient;
    memset(&bundleClient, 0, sizeof(bundleClient));

    bundleClient.base.version = 1;
    bundleClient.base.clientInfo = (__bridge void*)plugInController;
    bundleClient.didCreatePage = didCreatePage;
    bundleClient.willDestroyPage = willDestroyPage;

    WKBundleSetClient(toAPI(&bundle), &bundleClient.base);
}

- (void)_setPrincipalClassInstance:(id <WKWebProcessPlugIn>)principalClassInstance
{
    ASSERT(!_principalClassInstance);
    _principalClassInstance = principalClassInstance;

    setUpBundleClient(self, *_bundle);
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (WKConnection *)connection
{
    return wrapper(*_bundle->webConnectionToUIProcess());
}
ALLOW_DEPRECATED_DECLARATIONS_END

- (id)parameters
{
    return _bundle->bundleParameters();
}

static Ref<API::Array> createWKArray(NSArray *array)
{
    NSUInteger count = [array count];
    Vector<RefPtr<API::Object>> strings;
    strings.reserveInitialCapacity(count);
    
    for (id entry in array) {
        if ([entry isKindOfClass:[NSString class]])
            strings.uncheckedAppend(adoptRef(WebKit::toImpl(WKStringCreateWithCFString((__bridge CFStringRef)entry))));
    }
    
    return API::Array::create(WTFMove(strings));
}

- (void)extendClassesForParameterCoder:(NSArray *)classes
{
    auto classList = createWKArray(classes);
    _bundle->extendClassesForParameterCoder(classList.get());
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_bundle;
}

@end

@implementation WKWebProcessPlugInController (Private)

- (WKBundleRef)_bundleRef
{
    return toAPI(_bundle.get());
}

@end

#endif // WK_API_ENABLED
