/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import "InjectedBundle.h"
#import "WKConnectionInternal.h"
#import "WKBundle.h"
#import "WKBundleAPICast.h"
#import "WKRetainPtr.h"
#import "WKWebProcessPlugInBrowserContextControllerInternal.h"
#import <wtf/RetainPtr.h>

using namespace WebKit;

@interface WKWebProcessPlugInController () {
    RetainPtr<id <WKWebProcessPlugIn>> _principalClassInstance;
    RefPtr<InjectedBundle> _bundle;
    RetainPtr<WKConnection *> _connectionWrapper;
}
@end

@implementation WKWebProcessPlugInController

static void didCreatePage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    WKWebProcessPlugInController *plugInController = (WKWebProcessPlugInController *)clientInfo;
    id<WKWebProcessPlugIn> principalClassInstance = plugInController->_principalClassInstance.get();

    if ([principalClassInstance respondsToSelector:@selector(webProcessPlugIn:didCreateBrowserContextController:)])
        [principalClassInstance webProcessPlugIn:plugInController didCreateBrowserContextController:wrapper(*toImpl(page))];
}

static void willDestroyPage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    WKWebProcessPlugInController *plugInController = (WKWebProcessPlugInController *)clientInfo;
    id<WKWebProcessPlugIn> principalClassInstance = plugInController->_principalClassInstance.get();

    if ([principalClassInstance respondsToSelector:@selector(webProcessPlugIn:willDestroyBrowserContextController:)])
        [principalClassInstance webProcessPlugIn:plugInController willDestroyBrowserContextController:wrapper(*toImpl(page))];
}

static void setUpBundleClient(WKWebProcessPlugInController *plugInController, InjectedBundle& bundle)
{
    WKBundleClientV1 bundleClient;
    memset(&bundleClient, 0, sizeof(bundleClient));

    bundleClient.base.version = 1;
    bundleClient.base.clientInfo = plugInController;
    bundleClient.didCreatePage = didCreatePage;
    bundleClient.willDestroyPage = willDestroyPage;

    bundle.initializeClient(&bundleClient.base);
}

static WKWebProcessPlugInController *sharedInstance;

+ (WKWebProcessPlugInController *)_shared
{
    ASSERT_WITH_MESSAGE(sharedInstance, "+[WKWebProcessPlugIn _shared] called without first initializing it.");
    return sharedInstance;
}

- (id)_initWithPrincipalClassInstance:(id<WKWebProcessPlugIn>)principalClassInstance bundle:(InjectedBundle&)bundle
{
    self = [super init];
    if (!self)
        return nil;

    _principalClassInstance = principalClassInstance;
    _bundle = &bundle;

    ASSERT_WITH_MESSAGE(!sharedInstance, "WKWebProcessPlugInController initialized multiple times.");
    sharedInstance = self;

    setUpBundleClient(self, *_bundle);

    return self;
}

- (WKConnection *)connection
{
    return wrapper(*_bundle->webConnectionToUIProcess());
}

@end

@implementation WKWebProcessPlugInController (Private)

- (WKBundleRef)_bundleRef
{
    return toAPI(_bundle.get());
}

@end

#endif // WK_API_ENABLED
