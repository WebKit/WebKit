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

#import "APIData.h"
#import "RemoteObjectRegistry.h"
#import "RemoteObjectRegistryMessages.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKBundleAPICast.h"
#import "WKBundlePage.h"
#import "WKBundlePagePrivate.h"
#import "WKDOMInternals.h"
#import "WKNSDictionary.h"
#import "WKNSError.h"
#import "WKNSString.h"
#import "WKNSURL.h"
#import "WKNSURLRequest.h"
#import "WKRetainPtr.h"
#import "WKStringCF.h"
#import "WKURLRequestNS.h"
#import "WKWebProcessPluginFrameInternal.h"
#import "WKWebProcessPlugInInternal.h"
#import "WKWebProcessPlugInFormDelegatePrivate.h"
#import "WKWebProcessPlugInLoadDelegate.h"
#import "WKWebProcessPlugInNodeHandleInternal.h"
#import "WKWebProcessPlugInPageGroupInternal.h"
#import "WKWebProcessPlugInScriptWorldInternal.h"
#import "WeakObjCPtr.h"
#import "WebPage.h"
#import "WebProcess.h"
#import "_WKRemoteObjectRegistryInternal.h"
#import "_WKRenderingProgressEventsInternal.h"
#import "_WKSameDocumentNavigationTypeInternal.h"
#import <WebCore/Document.h>
#import <WebCore/Frame.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLInputElement.h>
#import <WebCore/MainFrame.h>

using namespace WebCore;
using namespace WebKit;

@interface NSObject (WKDeprecatedDelegateMethods)
- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller didSameDocumentNavigationForFrame:(WKWebProcessPlugInFrame *)frame;
@end

@implementation WKWebProcessPlugInBrowserContextController {
    API::ObjectStorage<WebPage> _page;
    WeakObjCPtr<id <WKWebProcessPlugInLoadDelegate>> _loadDelegate;
    WeakObjCPtr<id <WKWebProcessPlugInFormDelegatePrivate>> _formDelegate;
    
    RetainPtr<_WKRemoteObjectRegistry> _remoteObjectRegistry;
}

static void didStartProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userDataRef, const void *clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:didStartProvisionalLoadForFrame:)])
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController didStartProvisionalLoadForFrame:wrapper(*toImpl(frame))];
}

static void didReceiveServerRedirectForProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef *userDataRef, const void *clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:didReceiveServerRedirectForProvisionalLoadForFrame:)])
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController didReceiveServerRedirectForProvisionalLoadForFrame:wrapper(*toImpl(frame))];
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

static void didRemoveFrameFromHierarchy(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void* clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:didRemoveFrameFromHierarchy:)])
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController didRemoveFrameFromHierarchy:wrapper(*toImpl(frame))];
}

static void didCommitLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:didCommitLoadForFrame:)])
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController didCommitLoadForFrame:wrapper(*toImpl(frame))];
}

static void didFinishDocumentLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:didFinishDocumentLoadForFrame:)])
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController didFinishDocumentLoadForFrame:wrapper(*toImpl(frame))];
}

static void didFailProvisionalLoadWithErrorForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKErrorRef wkError, WKTypeRef* userData, const void *clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:didFailProvisionalLoadWithErrorForFrame:error:)])
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController didFailProvisionalLoadWithErrorForFrame:wrapper(*toImpl(frame)) error:wrapper(*toImpl(wkError))];
}

static void didFailLoadWithErrorForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKErrorRef wkError, WKTypeRef* userData, const void *clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:didFailLoadWithErrorForFrame:error:)])
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController didFailLoadWithErrorForFrame:wrapper(*toImpl(frame)) error:wrapper(*toImpl(wkError))];
}

static void didSameDocumentNavigationForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKSameDocumentNavigationType type, WKTypeRef* userData, const void *clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:didSameDocumentNavigation:forFrame:)])
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController didSameDocumentNavigation:toWKSameDocumentNavigationType(toSameDocumentNavigationType(type)) forFrame:wrapper(*toImpl(frame))];
    else {
        // FIXME: Remove this once clients switch to implementing the above delegate method.
        if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:didSameDocumentNavigationForFrame:)])
            [(NSObject *)loadDelegate webProcessPlugInBrowserContextController:pluginContextController didSameDocumentNavigationForFrame:wrapper(*toImpl(frame))];
    }
}

static void didLayoutForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void* clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:didLayoutForFrame:)])
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController didLayoutForFrame:wrapper(*toImpl(frame))];
}

static void didLayout(WKBundlePageRef page, WKLayoutMilestones milestones, WKTypeRef* userData, const void *clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:renderingProgressDidChange:)])
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController renderingProgressDidChange:renderingProgressEvents(toLayoutMilestones(milestones))];
}

static void didFirstVisuallyNonEmptyLayoutForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef* userData, const void *clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:didFirstVisuallyNonEmptyLayoutForFrame:)])
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController didFirstVisuallyNonEmptyLayoutForFrame:wrapper(*toImpl(frame))];
}

static void didHandleOnloadEventsForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void* clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:didHandleOnloadEventsForFrame:)])
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController didHandleOnloadEventsForFrame:wrapper(*toImpl(frame))];
}

static WKStringRef userAgentForURL(WKBundleFrameRef frame, WKURLRef url, const void* clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();
    
    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:frame:userAgentForURL:)]) {
        WKWebProcessPlugInFrame *newFrame = wrapper(*toImpl(frame));
        NSString *string = [loadDelegate webProcessPlugInBrowserContextController:pluginContextController frame:newFrame userAgentForURL:wrapper(*toImpl(url))];
        if (!string)
            return nullptr;

        WKStringRef wkString = WKStringCreateWithCFString((CFStringRef)string);
        return wkString;
    }
    
    return nullptr;
}

static void setUpPageLoaderClient(WKWebProcessPlugInBrowserContextController *contextController, WebPage& page)
{
    WKBundlePageLoaderClientV8 client;
    memset(&client, 0, sizeof(client));

    client.base.version = 8;
    client.base.clientInfo = contextController;
    client.didStartProvisionalLoadForFrame = didStartProvisionalLoadForFrame;
    client.didReceiveServerRedirectForProvisionalLoadForFrame = didReceiveServerRedirectForProvisionalLoadForFrame;
    client.didCommitLoadForFrame = didCommitLoadForFrame;
    client.didFinishDocumentLoadForFrame = didFinishDocumentLoadForFrame;
    client.didFailProvisionalLoadWithErrorForFrame = didFailProvisionalLoadWithErrorForFrame;
    client.didFailLoadWithErrorForFrame = didFailLoadWithErrorForFrame;
    client.didSameDocumentNavigationForFrame = didSameDocumentNavigationForFrame;
    client.didFinishLoadForFrame = didFinishLoadForFrame;
    client.globalObjectIsAvailableForFrame = globalObjectIsAvailableForFrame;
    client.didRemoveFrameFromHierarchy = didRemoveFrameFromHierarchy;
    client.didHandleOnloadEventsForFrame = didHandleOnloadEventsForFrame;
    client.didFirstVisuallyNonEmptyLayoutForFrame = didFirstVisuallyNonEmptyLayoutForFrame;
    client.userAgentForURL = userAgentForURL;

    client.didLayoutForFrame = didLayoutForFrame;
    client.didLayout = didLayout;

    page.initializeInjectedBundleLoaderClient(&client.base);
}

static WKURLRequestRef willSendRequestForFrame(WKBundlePageRef, WKBundleFrameRef frame, uint64_t resourceIdentifier, WKURLRequestRef request, WKURLResponseRef redirectResponse, const void* clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:frame:willSendRequestForResource:request:redirectResponse:)]) {
        NSURLRequest *originalRequest = wrapper(*toImpl(request));
        RetainPtr<NSURLRequest> substituteRequest = [loadDelegate webProcessPlugInBrowserContextController:pluginContextController frame:wrapper(*toImpl(frame)) willSendRequestForResource:resourceIdentifier
            request:originalRequest redirectResponse:toImpl(redirectResponse)->resourceResponse().nsURLResponse()];

        if (substituteRequest != originalRequest)
            return substituteRequest ? WKURLRequestCreateWithNSURLRequest(substituteRequest.get()) : nullptr;
    } else if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:frame:willSendRequest:redirectResponse:)]) {
        NSURLRequest *originalRequest = wrapper(*toImpl(request));
        RetainPtr<NSURLRequest> substituteRequest = [loadDelegate webProcessPlugInBrowserContextController:pluginContextController frame:wrapper(*toImpl(frame)) willSendRequest:originalRequest
            redirectResponse:toImpl(redirectResponse)->resourceResponse().nsURLResponse()];

        if (substituteRequest != originalRequest)
            return substituteRequest ? WKURLRequestCreateWithNSURLRequest(substituteRequest.get()) : nullptr;
    }

    WKRetain(request);
    return request;
}

static void didInitiateLoadForResource(WKBundlePageRef, WKBundleFrameRef frame, uint64_t resourceIdentifier, WKURLRequestRef request, bool pageIsProvisionallyLoading, const void* clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:frame:didInitiateLoadForResource:request:pageIsProvisionallyLoading:)]) {
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController frame:wrapper(*toImpl(frame)) didInitiateLoadForResource:resourceIdentifier request:wrapper(*toImpl(request))
            pageIsProvisionallyLoading:pageIsProvisionallyLoading];
    } else if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:frame:didInitiateLoadForResource:request:)]) {
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController frame:wrapper(*toImpl(frame)) didInitiateLoadForResource:resourceIdentifier request:wrapper(*toImpl(request))];
    }
}

static void didFinishLoadForResource(WKBundlePageRef, WKBundleFrameRef frame, uint64_t resourceIdentifier, const void* clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:frame:didFinishLoadForResource:)]) {
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController
                                                         frame:wrapper(*toImpl(frame))
                                      didFinishLoadForResource:resourceIdentifier];
    }
}

static void didFailLoadForResource(WKBundlePageRef, WKBundleFrameRef frame, uint64_t resourceIdentifier, WKErrorRef error, const void* clientInfo)
{
    WKWebProcessPlugInBrowserContextController *pluginContextController = (WKWebProcessPlugInBrowserContextController *)clientInfo;
    auto loadDelegate = pluginContextController->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(webProcessPlugInBrowserContextController:frame:didFailLoadForResource:error:)]) {
        [loadDelegate webProcessPlugInBrowserContextController:pluginContextController
                                                         frame:wrapper(*toImpl(frame))
                                        didFailLoadForResource:resourceIdentifier
                                                         error:wrapper(*toImpl(error))];
    }
}

static void setUpResourceLoadClient(WKWebProcessPlugInBrowserContextController *contextController, WebPage& page)
{
    WKBundlePageResourceLoadClientV1 client;
    memset(&client, 0, sizeof(client));

    client.base.version = 1;
    client.base.clientInfo = contextController;
    client.willSendRequestForFrame = willSendRequestForFrame;
    client.didInitiateLoadForResource = didInitiateLoadForResource;
    client.didFinishLoadForResource = didFinishLoadForResource;
    client.didFailLoadForResource = didFailLoadForResource;

    page.initializeInjectedBundleResourceLoadClient(&client.base);
}

- (id <WKWebProcessPlugInLoadDelegate>)loadDelegate
{
    return _loadDelegate.getAutoreleased();
}

- (void)setLoadDelegate:(id <WKWebProcessPlugInLoadDelegate>)loadDelegate
{
    _loadDelegate = loadDelegate;

    if (loadDelegate) {
        setUpPageLoaderClient(self, *_page);
        setUpResourceLoadClient(self, *_page);
    } else {
        _page->initializeInjectedBundleLoaderClient(nullptr);
        _page->initializeInjectedBundleResourceLoadClient(nullptr);
    }
}

- (void)dealloc
{
    if (_remoteObjectRegistry) {
        WebProcess::singleton().removeMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), _page->pageID());
        [_remoteObjectRegistry _invalidate];
    }

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

@implementation WKWebProcessPlugInBrowserContextController (WKPrivate)

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
    WebPage* webPage = WebProcess::singleton().webPage(handle.pageID);
    if (!webPage)
        return nil;

    return wrapper(*webPage);
}

- (_WKRemoteObjectRegistry *)_remoteObjectRegistry
{
    if (!_remoteObjectRegistry) {
        _remoteObjectRegistry = adoptNS([[_WKRemoteObjectRegistry alloc] _initWithMessageSender:*_page]);
        WebProcess::singleton().addMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), _page->pageID(), [_remoteObjectRegistry remoteObjectRegistry]);
    }

    return _remoteObjectRegistry.get();
}

- (id <WKWebProcessPlugInFormDelegatePrivate>)_formDelegate
{
    return _formDelegate.getAutoreleased();
}

- (void)_setFormDelegate:(id <WKWebProcessPlugInFormDelegatePrivate>)formDelegate
{
    _formDelegate = formDelegate;

    class FormClient : public API::InjectedBundle::FormClient {
    public:
        explicit FormClient(WKWebProcessPlugInBrowserContextController *controller)
            : m_controller(controller)
        {
        }

        virtual ~FormClient() { }

        virtual void didFocusTextField(WebPage*, HTMLInputElement* inputElement, WebFrame* frame) override
        {
            auto formDelegate = m_controller->_formDelegate.get();

            if ([formDelegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:didFocusTextField:inFrame:)])
                [formDelegate _webProcessPlugInBrowserContextController:m_controller didFocusTextField:wrapper(*InjectedBundleNodeHandle::getOrCreate(inputElement).get()) inFrame:wrapper(*frame)];
        }

        virtual void willSendSubmitEvent(WebPage*, HTMLFormElement* formElement, WebFrame* targetFrame, WebFrame* sourceFrame, const Vector<std::pair<String, String>>& values) override
        {
            auto formDelegate = m_controller->_formDelegate.get();

            if ([formDelegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:willSendSubmitEventToForm:inFrame:targetFrame:values:)]) {
                auto valueMap = adoptNS([[NSMutableDictionary alloc] initWithCapacity:values.size()]);
                for (const auto& pair : values)
                    [valueMap setObject:pair.second forKey:pair.first];

                [formDelegate _webProcessPlugInBrowserContextController:m_controller willSendSubmitEventToForm:wrapper(*InjectedBundleNodeHandle::getOrCreate(formElement).get())
                    inFrame:wrapper(*sourceFrame) targetFrame:wrapper(*targetFrame) values:valueMap.get()];
            }
        }

        static void encodeUserObject(NSObject <NSSecureCoding> *userObject, RefPtr<API::Object>& userData)
        {
            if (!userObject)
                return;

            auto data = adoptNS([[NSMutableData alloc] init]);
            auto archiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);
            [archiver setRequiresSecureCoding:YES];
            @try {
                [archiver encodeObject:userObject forKey:@"userObject"];
            } @catch (NSException *exception) {
                LOG_ERROR("Failed to encode user object: %@", exception);
                return;
            }
            [archiver finishEncoding];

            userData = API::Data::createWithoutCopying(WTFMove(data));
        }

        virtual void willSubmitForm(WebPage*, HTMLFormElement* formElement, WebFrame* frame, WebFrame* sourceFrame, const Vector<std::pair<WTF::String, WTF::String>>& values, RefPtr<API::Object>& userData) override
        {
            auto formDelegate = m_controller->_formDelegate.get();

            if ([formDelegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:willSubmitForm:toFrame:fromFrame:withValues:)]) {
                auto valueMap = adoptNS([[NSMutableDictionary alloc] initWithCapacity:values.size()]);
                for (const auto& pair : values)
                    [valueMap setObject:pair.second forKey:pair.first];

                NSObject <NSSecureCoding> *userObject = [formDelegate _webProcessPlugInBrowserContextController:m_controller willSubmitForm:wrapper(*InjectedBundleNodeHandle::getOrCreate(formElement).get()) toFrame:wrapper(*frame) fromFrame:wrapper(*sourceFrame) withValues:valueMap.get()];
                encodeUserObject(userObject, userData);
            }
        }

        virtual void textDidChangeInTextField(WebPage*, HTMLInputElement* inputElement, WebFrame* frame, bool initiatedByUserTyping) override
        {
            auto formDelegate = m_controller->_formDelegate.get();

            if ([formDelegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:textDidChangeInTextField:inFrame:initiatedByUserTyping:)])
                [formDelegate _webProcessPlugInBrowserContextController:m_controller textDidChangeInTextField:wrapper(*WebKit::InjectedBundleNodeHandle::getOrCreate(inputElement)) inFrame:wrapper(*frame) initiatedByUserTyping:initiatedByUserTyping];
        }

        virtual void willBeginInputSession(WebPage*, Element* element, WebFrame* frame, RefPtr<API::Object>& userData) override
        {
            auto formDelegate = m_controller->_formDelegate.get();

            if (![formDelegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:willBeginInputSessionForElement:inFrame:)])
                return;

            NSObject <NSSecureCoding> *userObject = [formDelegate _webProcessPlugInBrowserContextController:m_controller willBeginInputSessionForElement:wrapper(*WebKit::InjectedBundleNodeHandle::getOrCreate(element)) inFrame:wrapper(*frame)];
            encodeUserObject(userObject, userData);
        }

        virtual bool shouldNotifyOnFormChanges(WebKit::WebPage*) override
        {
            auto formDelegate = m_controller->_formDelegate.get();

            if (![formDelegate respondsToSelector:@selector(_webProcessPlugInBrowserContextControllerShouldNotifyOnFormChanges:)])
                return false;

            return [formDelegate _webProcessPlugInBrowserContextControllerShouldNotifyOnFormChanges:m_controller];
        }

        virtual void didAssociateFormControls(WebKit::WebPage*, const Vector<RefPtr<WebCore::Element>>& elements) override
        {
            auto formDelegate = m_controller->_formDelegate.get();

            if (![formDelegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:didAssociateFormControls:)])
                return;

            auto controls = adoptNS([[NSMutableArray alloc] initWithCapacity:elements.size()]);
            for (const auto& element : elements)
                [controls addObject:wrapper(*WebKit::InjectedBundleNodeHandle::getOrCreate(element.get()))];
            return [formDelegate _webProcessPlugInBrowserContextController:m_controller didAssociateFormControls:controls.get()];
        }

    private:
        WKWebProcessPlugInBrowserContextController *m_controller;
    };

    if (formDelegate)
        _page->setInjectedBundleFormClient(std::make_unique<FormClient>(self));
    else
        _page->setInjectedBundleFormClient(nullptr);
}

- (BOOL)_defersLoading
{
    return _page->defersLoading();
}

- (void)_setDefersLoading:(BOOL)defersLoading
{
    _page->setDefersLoading(defersLoading);
}

- (BOOL)_usesNonPersistentWebsiteDataStore
{
    return _page->usesEphemeralSession();
}

@end

#endif // WK_API_ENABLED
