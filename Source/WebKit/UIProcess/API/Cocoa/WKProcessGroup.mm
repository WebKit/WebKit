/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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
#import "WKProcessGroupPrivate.h"

#import "APINavigationData.h"
#import "APIProcessPoolConfiguration.h"
#import "ObjCObjectGraph.h"
#import "WKAPICast.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKBrowsingContextHistoryDelegate.h"
#import "WKConnectionInternal.h"
#import "WKNSString.h"
#import "WKNSURL.h"
#import "WKNavigationDataInternal.h"
#import "WKRetainPtr.h"
#import "WKStringCF.h"
#import "WebFrameProxy.h"
#import "WebProcessPool.h"
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "WKAPICast.h"
#import "WKGeolocationProviderIOS.h"
#import <WebCore/WebCoreThreadSystemInterface.h>
#endif

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
@implementation WKProcessGroup {
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
    RefPtr<WebKit::WebProcessPool> _processPool;

    WeakObjCPtr<id <WKProcessGroupDelegate>> _delegate;

#if PLATFORM(IOS_FAMILY)
    RetainPtr<WKGeolocationProviderIOS> _geolocationProvider;
#endif // PLATFORM(IOS_FAMILY)
}

static void didCreateConnection(WKContextRef, WKConnectionRef connectionRef, const void* clientInfo)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto processGroup = (__bridge WKProcessGroup *)clientInfo;
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto delegate = processGroup->_delegate.get();

    if ([delegate respondsToSelector:@selector(processGroup:didCreateConnectionToWebProcessPlugIn:)])
        [delegate processGroup:processGroup didCreateConnectionToWebProcessPlugIn:wrapper(*WebKit::toImpl(connectionRef))];
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
static void setUpConnectionClient(WKProcessGroup *processGroup, WKContextRef contextRef)
ALLOW_DEPRECATED_DECLARATIONS_END
{
    WKContextConnectionClientV0 connectionClient;
    memset(&connectionClient, 0, sizeof(connectionClient));

    connectionClient.base.version = 0;
    connectionClient.base.clientInfo = (__bridge CFTypeRef)processGroup;
    connectionClient.didCreateConnection = didCreateConnection;

    WKContextSetConnectionClient(contextRef, &connectionClient.base);
}

static WKTypeRef getInjectedBundleInitializationUserData(WKContextRef, const void* clientInfo)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto processGroup = (__bridge WKProcessGroup *)clientInfo;
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto delegate = processGroup->_delegate.get();

    if ([delegate respondsToSelector:@selector(processGroupWillCreateConnectionToWebProcessPlugIn:)]) {
        RetainPtr<id> initializationUserData = [delegate processGroupWillCreateConnectionToWebProcessPlugIn:processGroup];

        return toAPI(&WebKit::ObjCObjectGraph::create(initializationUserData.get()).leakRef());
    }

    return 0;
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
static void setUpInjectedBundleClient(WKProcessGroup *processGroup, WKContextRef contextRef)
ALLOW_DEPRECATED_DECLARATIONS_END
{
    WKContextInjectedBundleClientV1 injectedBundleClient;
    memset(&injectedBundleClient, 0, sizeof(injectedBundleClient));

    injectedBundleClient.base.version = 1;
    injectedBundleClient.base.clientInfo = (__bridge void*)processGroup;
    injectedBundleClient.getInjectedBundleInitializationUserData = getInjectedBundleInitializationUserData;

    WKContextSetInjectedBundleClient(contextRef, &injectedBundleClient.base);
}

static void didNavigateWithNavigationData(WKContextRef, WKPageRef pageRef, WKNavigationDataRef navigationDataRef, WKFrameRef frameRef, const void*)
{
    if (!WebKit::toImpl(frameRef)->isMainFrame())
        return;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKBrowsingContextController *controller = [WKBrowsingContextController _browsingContextControllerForPageRef:pageRef];
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto historyDelegate = controller->_historyDelegate.get();

    if ([historyDelegate respondsToSelector:@selector(browsingContextController:didNavigateWithNavigationData:)])
        [historyDelegate browsingContextController:controller didNavigateWithNavigationData:wrapper(*WebKit::toImpl(navigationDataRef))];
}

static void didPerformClientRedirect(WKContextRef, WKPageRef pageRef, WKURLRef sourceURLRef, WKURLRef destinationURLRef, WKFrameRef frameRef, const void*)
{
    if (!WebKit::toImpl(frameRef)->isMainFrame())
        return;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKBrowsingContextController *controller = [WKBrowsingContextController _browsingContextControllerForPageRef:pageRef];
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto historyDelegate = controller->_historyDelegate.get();

    if ([historyDelegate respondsToSelector:@selector(browsingContextController:didPerformClientRedirectFromURL:toURL:)])
        [historyDelegate browsingContextController:controller didPerformClientRedirectFromURL:wrapper(*WebKit::toImpl(sourceURLRef)) toURL:wrapper(*WebKit::toImpl(destinationURLRef))];
}

static void didPerformServerRedirect(WKContextRef, WKPageRef pageRef, WKURLRef sourceURLRef, WKURLRef destinationURLRef, WKFrameRef frameRef, const void*)
{
    if (!WebKit::toImpl(frameRef)->isMainFrame())
        return;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKBrowsingContextController *controller = [WKBrowsingContextController _browsingContextControllerForPageRef:pageRef];
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto historyDelegate = controller->_historyDelegate.get();

    if ([historyDelegate respondsToSelector:@selector(browsingContextController:didPerformServerRedirectFromURL:toURL:)])
        [historyDelegate browsingContextController:controller didPerformServerRedirectFromURL:wrapper(*WebKit::toImpl(sourceURLRef)) toURL:wrapper(*WebKit::toImpl(destinationURLRef))];
}

static void didUpdateHistoryTitle(WKContextRef, WKPageRef pageRef, WKStringRef titleRef, WKURLRef urlRef, WKFrameRef frameRef, const void*)
{
    if (!WebKit::toImpl(frameRef)->isMainFrame())
        return;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKBrowsingContextController *controller = [WKBrowsingContextController _browsingContextControllerForPageRef:pageRef];
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto historyDelegate = controller->_historyDelegate.get();

    if ([historyDelegate respondsToSelector:@selector(browsingContextController:didUpdateHistoryTitle:forURL:)])
        [historyDelegate browsingContextController:controller didUpdateHistoryTitle:wrapper(*WebKit::toImpl(titleRef)) forURL:wrapper(*WebKit::toImpl(urlRef))];
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
static void setUpHistoryClient(WKProcessGroup *processGroup, WKContextRef contextRef)
ALLOW_DEPRECATED_DECLARATIONS_END
{
    WKContextHistoryClientV0 historyClient;
    memset(&historyClient, 0, sizeof(historyClient));

    historyClient.base.version = 0;
    historyClient.base.clientInfo = (__bridge CFTypeRef)processGroup;
    historyClient.didNavigateWithNavigationData = didNavigateWithNavigationData;
    historyClient.didPerformClientRedirect = didPerformClientRedirect;
    historyClient.didPerformServerRedirect = didPerformServerRedirect;
    historyClient.didUpdateHistoryTitle = didUpdateHistoryTitle;

    WKContextSetHistoryClient(contextRef, &historyClient.base);
}

- (id)init
{
    return [self initWithInjectedBundleURL:nil];
}

- (id)initWithInjectedBundleURL:(NSURL *)bundleURL
{
    self = [super init];
    if (!self)
        return nil;

    auto configuration = API::ProcessPoolConfiguration::create();
    configuration->setInjectedBundlePath(bundleURL ? String(bundleURL.path) : String());

    _processPool = WebKit::WebProcessPool::create(configuration);

    setUpConnectionClient(self, toAPI(_processPool.get()));
    setUpInjectedBundleClient(self, toAPI(_processPool.get()));
    setUpHistoryClient(self, toAPI(_processPool.get()));

    return self;
}

static Vector<WTF::String> toStringVector(NSSet *input)
{
    Vector<WTF::String> vector;

    NSUInteger size = input.count;
    if (!size)
        return vector;

    vector.reserveInitialCapacity(size);
    for (id classObj : input) {
        if (auto* string = NSStringFromClass(classObj))
            vector.uncheckedAppend(string);
    }
    return vector;
}

- (id)initWithInjectedBundleURL:(NSURL *)bundleURL andCustomClassesForParameterCoder:(NSSet *)classesForCoder
{
    self = [super init];
    if (!self)
        return nil;

    auto configuration = API::ProcessPoolConfiguration::create();
    configuration->setInjectedBundlePath(bundleURL ? String(bundleURL.path) : String());
    configuration->setCustomClassesForParameterCoder(toStringVector(classesForCoder));

    _processPool = WebKit::WebProcessPool::create(configuration);

    setUpConnectionClient(self, toAPI(_processPool.get()));
    setUpInjectedBundleClient(self, toAPI(_processPool.get()));
    setUpHistoryClient(self, toAPI(_processPool.get()));

    return self;
}

- (id <WKProcessGroupDelegate>)delegate
{
    return _delegate.getAutoreleased();
}

- (void)setDelegate:(id <WKProcessGroupDelegate>)delegate
{
    _delegate = delegate;

    // If the client can observe when the connection to the WebProcess injected bundle is established, then we cannot
    // safely delay the launch of the WebProcess until something is loaded in the web view.
    if ([delegate respondsToSelector:@selector(processGroup:didCreateConnectionToWebProcessPlugIn:)])
        _processPool->disableDelayedWebProcessLaunch();
}

@end

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
@implementation WKProcessGroup (Private)
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (WKContextRef)_contextRef
{
    return toAPI(_processPool.get());
}

#if PLATFORM(IOS_FAMILY)
- (WKGeolocationProviderIOS *)_geolocationProvider
{
    if (!_geolocationProvider)
        _geolocationProvider = adoptNS([[WKGeolocationProviderIOS alloc] initWithProcessPool:*_processPool.get()]);
    return _geolocationProvider.get();
}
#endif // PLATFORM(IOS_FAMILY)

@end
ALLOW_DEPRECATED_DECLARATIONS_END
