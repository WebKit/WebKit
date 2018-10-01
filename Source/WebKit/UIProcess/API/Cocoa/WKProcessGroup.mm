/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#if WK_API_ENABLED

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
#import "WebCertificateInfo.h"
#import "WebFrameProxy.h"
#import "WebProcessPool.h"
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

#if PLATFORM(IOS)
#import "WKAPICast.h"
#import "WKGeolocationProviderIOS.h"
#import <WebCore/WebCoreThreadSystemInterface.h>
#endif

@implementation WKProcessGroup {
    RefPtr<WebKit::WebProcessPool> _processPool;

    WeakObjCPtr<id <WKProcessGroupDelegate>> _delegate;

#if PLATFORM(IOS)
    RetainPtr<WKGeolocationProviderIOS> _geolocationProvider;
#endif // PLATFORM(IOS)
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

static void setUpConnectionClient(WKProcessGroup *processGroup, WKContextRef contextRef)
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

static void setUpInjectedBundleClient(WKProcessGroup *processGroup, WKContextRef contextRef)
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

static void setUpHistoryClient(WKProcessGroup *processGroup, WKContextRef contextRef)
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

    auto configuration = API::ProcessPoolConfiguration::createWithLegacyOptions();
    configuration->setInjectedBundlePath(bundleURL ? String(bundleURL.path) : String());

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
}

@end

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
@implementation WKProcessGroup (Private)

- (WKContextRef)_contextRef
{
    return toAPI(_processPool.get());
}

- (void)_setAllowsSpecificHTTPSCertificate:(NSArray *)certificateChain forHost:(NSString *)host
{
    _processPool->allowSpecificHTTPSCertificateForHost(WebKit::WebCertificateInfo::create(WebCore::CertificateInfo((__bridge CFArrayRef)certificateChain)).ptr(), host);
}

#if PLATFORM(IOS)
- (WKGeolocationProviderIOS *)_geolocationProvider
{
    if (!_geolocationProvider)
        _geolocationProvider = adoptNS([[WKGeolocationProviderIOS alloc] initWithProcessPool:*_processPool.get()]);
    return _geolocationProvider.get();
}
#endif // PLATFORM(IOS)

@end
ALLOW_DEPRECATED_DECLARATIONS_END

#endif // WK_API_ENABLED
