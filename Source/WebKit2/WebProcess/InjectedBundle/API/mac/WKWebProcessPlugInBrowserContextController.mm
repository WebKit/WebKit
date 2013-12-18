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
#import "WKWebProcessPlugInBrowserContextControllerInternal.h"

#if WK_API_ENABLED

#import "RemoteObjectRegistry.h"
#import "RemoteObjectRegistryMessages.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKBundleAPICast.h"
#import "WKBundlePage.h"
#import "WKBundlePagePrivate.h"
#import "WKDOMInternals.h"
#import "WKRemoteObjectRegistryInternal.h"
#import "WKRetainPtr.h"
#import "WKWebProcessPluginFrameInternal.h"
#import "WKWebProcessPlugInInternal.h"
#import "WKWebProcessPlugInLoadDelegate.h"
#import "WKWebProcessPlugInPageGroupInternal.h"
#import "WKWebProcessPlugInScriptWorldInternal.h"
#import "WeakObjCPtr.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <WebCore/Document.h>
#import <WebCore/Frame.h>

using namespace WebCore;
using namespace WebKit;

@implementation WKWebProcessPlugInBrowserContextController {
    API::ObjectStorage<WebPage> _page;
    WeakObjCPtr<id <WKWebProcessPlugInLoadDelegate>> _loadDelegate;
    
    RetainPtr<WKRemoteObjectRegistry> _remoteObjectRegistry;
}

static void didStartProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userDataRef, const void *clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:didStartProvisionalLoadForFrame:)])
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController didStartProvisionalLoadForFrame:wrapper(*toImpl(frame))];
}

static void didFinishLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:didFinishLoadForFrame:)])
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController didFinishLoadForFrame:wrapper(*toImpl(frame))];
}

static void globalObjectIsAvailableForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKBundleScriptWorldRef scriptWorld, const void* clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:globalObjectIsAvailableForFrame:inScriptWorld:)])
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController globalObjectIsAvailableForFrame:wrapper(*toImpl(frame)) inScriptWorld:wrapper(*toImpl(scriptWorld))];
}

static void setUpPageLoaderClient(WKWebProcessPlugInBrowserContextController *contextController, WebPage& page)
{
    WKBundlePageLoaderClientV7 client;
    memset(&client, 0, sizeof(client));

    client.base.version = 7;
    client.base.clientInfo = contextController;
    client.didStartProvisionalLoadForFrame = didStartProvisionalLoadForFrame;
    client.didFinishLoadForFrame = didFinishLoadForFrame;
    client.globalObjectIsAvailableForFrame = globalObjectIsAvailableForFrame;

    page.initializeInjectedBundleLoaderClient(&client.base);
}

- (id <WKWebProcessPlugInLoadDelegate>)loadDelegate
{
    return _loadDelegate.getAutoreleased();
}

- (void)setLoadDelegate:(id <WKWebProcessPlugInLoadDelegate>)loadDelegate
{
    _loadDelegate = loadDelegate;

    if (loadDelegate)
        setUpPageLoaderClient(self, *_page);
    else
        _page->initializeInjectedBundleLoaderClient(nullptr);
}

- (void)dealloc
{
    _page->~WebPage();

    [super dealloc];
}

- (WKDOMDocument *)mainFrameDocument
{
    Frame* webCoreMainFrame = _page->mainFrame();
    if (!webCoreMainFrame)
        return nil;

    return toWKDOMDocument(webCoreMainFrame->document());
}

- (WKDOMRange *)selectedRange
{
    RefPtr<Range> range = _page->currentSelectionAsRange();
    if (!range)
        return nil;

    return toWKDOMRange(range.get());
}

- (WKWebProcessPlugInFrame *)mainFrame
{
    WebFrame *webKitMainFrame = _page->mainWebFrame();
    if (!webKitMainFrame)
        return nil;

    return wrapper(*webKitMainFrame);
}

- (WKWebProcessPlugInPageGroup *)pageGroup
{
    return wrapper(*_page->pageGroup());
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_page;
}

@end

@implementation WKWebProcessPlugInBrowserContextController (Private)

- (WKBundlePageRef)_bundlePageRef
{
    return toAPI(_page.get());
}

- (WKBrowsingContextHandle *)handle
{
    return [[[WKBrowsingContextHandle alloc] _initWithPageID:_page->pageID()] autorelease];
}

+ (instancetype)lookUpBrowsingContextFromHandle:(WKBrowsingContextHandle *)handle
{
    WebPage* webPage = WebProcess::shared().webPage(handle.pageID);
    if (!webPage)
        return nil;

    return wrapper(*webPage);
}

- (WKRemoteObjectRegistry *)remoteObjectRegistry
{
    if (!_remoteObjectRegistry) {
        _remoteObjectRegistry = [[WKRemoteObjectRegistry alloc] _initWithMessageSender:*_page];
        WebProcess::shared().addMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), _page->pageID(), [_remoteObjectRegistry remoteObjectRegistry]);
    }

    return _remoteObjectRegistry.get();
}

@end

#endif // WK_API_ENABLED
