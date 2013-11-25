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
#import "ObjCObjectGraph.h"
#import "WKAPICast.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKBrowsingContextHistoryDelegate.h"
#import "WKConnectionInternal.h"
#import "WKContext.h"
#import "WKNSString.h"
#import "WKNSURL.h"
#import "WKNavigationDataInternal.h"
#import "WKRetainPtr.h"
#import "WKStringCF.h"
#import "WebFrameProxy.h"
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS)
#import "WKAPICast.h"
#import "WKGeolocationProviderIOS.h"
#import <WebCore/WebCoreThreadSystemInterface.h>
#endif

using namespace WebKit;

@implementation WKProcessGroup {
    // Underlying context object.
    WKRetainPtr<WKContextRef> _contextRef;

#if PLATFORM(IOS)
    RetainPtr<WKGeolocationProviderIOS> _geolocationProvider;
#endif // PLATFORM(IOS)
}

static void didCreateConnection(WKContextRef, WKConnectionRef connectionRef, const void* clientInfo)
{
    WKProcessGroup *processGroup = (WKProcessGroup *)clientInfo;
    if ([processGroup.delegate respondsToSelector:@selector(processGroup:didCreateConnectionToWebProcessPlugIn:)]) {
        RetainPtr<WKConnection> connection = adoptNS([[WKConnection alloc] _initWithConnectionRef:connectionRef]);
        [processGroup.delegate processGroup:processGroup didCreateConnectionToWebProcessPlugIn:connection.get()];
    }
}

static void setUpConnectionClient(WKProcessGroup *processGroup, WKContextRef contextRef)
{
    WKContextConnectionClient connectionClient;
    memset(&connectionClient, 0, sizeof(connectionClient));

    connectionClient.version = kWKContextConnectionClientCurrentVersion;
    connectionClient.clientInfo = processGroup;
    connectionClient.didCreateConnection = didCreateConnection;

    WKContextSetConnectionClient(contextRef, &connectionClient);
}

static WKTypeRef getInjectedBundleInitializationUserData(WKContextRef, const void* clientInfo)
{
    WKProcessGroup *processGroup = (WKProcessGroup *)clientInfo;
    if ([processGroup.delegate respondsToSelector:@selector(processGroupWillCreateConnectionToWebProcessPlugIn:)]) {
        RetainPtr<id> initializationUserData = [processGroup.delegate processGroupWillCreateConnectionToWebProcessPlugIn:processGroup];
        RefPtr<WebKit::ObjCObjectGraph> wkMessageBody = WebKit::ObjCObjectGraph::create(initializationUserData.get());
        return (WKTypeRef)wkMessageBody.release().leakRef();
    }

    return 0;
}

static void setUpInectedBundleClient(WKProcessGroup *processGroup, WKContextRef contextRef)
{
    WKContextInjectedBundleClient injectedBundleClient;
    memset(&injectedBundleClient, 0, sizeof(injectedBundleClient));

    injectedBundleClient.version = kWKContextInjectedBundleClientCurrentVersion;
    injectedBundleClient.clientInfo = processGroup;
    injectedBundleClient.getInjectedBundleInitializationUserData = getInjectedBundleInitializationUserData;

    WKContextSetInjectedBundleClient(contextRef, &injectedBundleClient);
}

static void didNavigateWithNavigationData(WKContextRef, WKPageRef pageRef, WKNavigationDataRef navigationDataRef, WKFrameRef frameRef, const void*)
{
    if (!toImpl(frameRef)->isMainFrame())
        return;

    WKBrowsingContextController *controller = [WKBrowsingContextController _browsingContextControllerForPageRef:pageRef];
    if ([controller.historyDelegate respondsToSelector:@selector(browsingContextController:didNavigateWithNavigationData:)])
        [controller.historyDelegate browsingContextController:controller didNavigateWithNavigationData:wrapper(*toImpl(navigationDataRef))];
}

static void didPerformClientRedirect(WKContextRef, WKPageRef pageRef, WKURLRef sourceURLRef, WKURLRef destinationURLRef, WKFrameRef frameRef, const void*)
{
    if (!toImpl(frameRef)->isMainFrame())
        return;

    WKBrowsingContextController *controller = [WKBrowsingContextController _browsingContextControllerForPageRef:pageRef];
    if ([controller.historyDelegate respondsToSelector:@selector(browsingContextController:didPerformClientRedirectFromURL:toURL:)])
        [controller.historyDelegate browsingContextController:controller didPerformClientRedirectFromURL:wrapper(*toImpl(sourceURLRef)) toURL:wrapper(*toImpl(destinationURLRef))];
}

static void didPerformServerRedirect(WKContextRef, WKPageRef pageRef, WKURLRef sourceURLRef, WKURLRef destinationURLRef, WKFrameRef frameRef, const void*)
{
    if (!toImpl(frameRef)->isMainFrame())
        return;

    WKBrowsingContextController *controller = [WKBrowsingContextController _browsingContextControllerForPageRef:pageRef];
    if ([controller.historyDelegate respondsToSelector:@selector(browsingContextController:didPerformServerRedirectFromURL:toURL:)])
        [controller.historyDelegate browsingContextController:controller didPerformServerRedirectFromURL:wrapper(*toImpl(sourceURLRef)) toURL:wrapper(*toImpl(destinationURLRef))];
}

static void didUpdateHistoryTitle(WKContextRef, WKPageRef pageRef, WKStringRef titleRef, WKURLRef urlRef, WKFrameRef frameRef, const void*)
{
    if (!toImpl(frameRef)->isMainFrame())
        return;

    WKBrowsingContextController *controller = [WKBrowsingContextController _browsingContextControllerForPageRef:pageRef];
    if ([controller.historyDelegate respondsToSelector:@selector(browsingContextController:didUpdateHistoryTitle:forURL:)])
        [controller.historyDelegate browsingContextController:controller didUpdateHistoryTitle:wrapper(*toImpl(titleRef)) forURL:wrapper(*toImpl(urlRef))];
}

static void setUpHistoryClient(WKProcessGroup *processGroup, WKContextRef contextRef)
{
    WKContextHistoryClient historyClient;
    memset(&historyClient, 0, sizeof(historyClient));

    historyClient.version = kWKContextHistoryClientCurrentVersion;
    historyClient.clientInfo = processGroup;
    historyClient.didNavigateWithNavigationData = didNavigateWithNavigationData;
    historyClient.didPerformClientRedirect = didPerformClientRedirect;
    historyClient.didPerformServerRedirect = didPerformServerRedirect;
    historyClient.didUpdateHistoryTitle = didUpdateHistoryTitle;

    WKContextSetHistoryClient(contextRef, &historyClient);
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

#if PLATFORM(IOS)
    // FIXME: Remove once <rdar://problem/15256572> is fixed.
    InitWebCoreThreadSystemInterface();
#endif

    if (bundleURL)
        _contextRef = adoptWK(WKContextCreateWithInjectedBundlePath(adoptWK(WKStringCreateWithCFString((CFStringRef)[bundleURL path])).get()));
    else
        _contextRef = adoptWK(WKContextCreate());

    setUpConnectionClient(self, _contextRef.get());
    setUpInectedBundleClient(self, _contextRef.get());
    setUpHistoryClient(self, _contextRef.get());

    return self;
}

- (void)dealloc
{
    WKContextSetConnectionClient(_contextRef.get(), 0);
    WKContextSetInjectedBundleClient(_contextRef.get(), 0);
    WKContextSetHistoryClient(_contextRef.get(), 0);

    [super dealloc];
}

@end

@implementation WKProcessGroup (Private)

- (WKContextRef)_contextRef
{
    return _contextRef.get();
}

#if PLATFORM(IOS)
- (WKGeolocationProviderIOS *)_geolocationProvider
{
    if (!_geolocationProvider)
        _geolocationProvider = adoptNS([[WKGeolocationProviderIOS alloc] initWithContext:toImpl(_contextRef.get())]);
    return _geolocationProvider.get();
}
#endif // PLATFORM(IOS)

@end

#endif // WK_API_ENABLED
