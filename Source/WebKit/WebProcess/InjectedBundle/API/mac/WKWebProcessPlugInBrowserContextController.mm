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
#import "CompletionHandlerCallChecker.h"
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
#import "WKWebProcessPlugInEditingDelegate.h"
#import "WKWebProcessPlugInFrameInternal.h"
#import "WKWebProcessPlugInInternal.h"
#import "WKWebProcessPlugInFormDelegatePrivate.h"
#import "WKWebProcessPlugInLoadDelegate.h"
#import "WKWebProcessPlugInNodeHandleInternal.h"
#import "WKWebProcessPlugInPageGroupInternal.h"
#import "WKWebProcessPlugInRangeHandleInternal.h"
#import "WKWebProcessPlugInScriptWorldInternal.h"
#import "WebPage.h"
#import "WebProcess.h"
#import "_WKRemoteObjectRegistryInternal.h"
#import "_WKRenderingProgressEventsInternal.h"
#import "_WKSameDocumentNavigationTypeInternal.h"
#import <WebCore/Document.h>
#import <WebCore/DocumentFragment.h>
#import <WebCore/Frame.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLInputElement.h>
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/WeakObjCPtr.h>

@interface NSObject (WKDeprecatedDelegateMethods)
- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller didSameDocumentNavigationForFrame:(WKWebProcessPlugInFrame *)frame;
@end

class ResourceLoadClient : public API::InjectedBundle::ResourceLoadClient {
public:
    explicit ResourceLoadClient(WKWebProcessPlugInBrowserContextController *controller, id <WKWebProcessPlugInLoadDelegate> delegate)
        : m_controller(controller)
        , m_delegate(delegate) { }
    
    void didInitiateLoadForResource(WebKit::WebPage&, WebKit::WebFrame&, uint64_t identifier, const WebCore::ResourceRequest&, bool) override;
    void willSendRequestForFrame(WebKit::WebPage&, WebKit::WebFrame&, uint64_t identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse&) override;
    void didFinishLoadForResource(WebKit::WebPage&, WebKit::WebFrame&, uint64_t identifier) override;
    void didFailLoadForResource(WebKit::WebPage&, WebKit::WebFrame&, uint64_t identifier, const WebCore::ResourceError&) override;

private:
    id <WKWebProcessPlugInLoadDelegate> loadDelegate() const { return m_delegate.get().get(); }
    WKWebProcessPlugInBrowserContextController *pluginContextController() const { return m_controller.get().get(); }
    
    WeakObjCPtr<WKWebProcessPlugInBrowserContextController> m_controller;
    WeakObjCPtr<id <WKWebProcessPlugInLoadDelegate>> m_delegate;
};

class PageLoaderClient : public API::InjectedBundle::PageLoaderClient {
public:
    explicit PageLoaderClient(WKWebProcessPlugInBrowserContextController *controller, id <WKWebProcessPlugInLoadDelegate> delegate)
        : m_controller(controller)
        , m_delegate(delegate) { }
    
    void didStartProvisionalLoadForFrame(WebKit::WebPage&, WebKit::WebFrame&, CompletionHandler<void(RefPtr<API::Object>&&)>&&) override;
    void didReceiveServerRedirectForProvisionalLoadForFrame(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) override;
    void didFailProvisionalLoadWithErrorForFrame(WebKit::WebPage&, WebKit::WebFrame&, const WebCore::ResourceError&, RefPtr<API::Object>&) override;
    void didCommitLoadForFrame(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) override;
    void didFinishDocumentLoadForFrame(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) override;
    void didFinishLoadForFrame(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) override;
    void didFailLoadWithErrorForFrame(WebKit::WebPage&, WebKit::WebFrame&, const WebCore::ResourceError&, RefPtr<API::Object>&) override;
    void didSameDocumentNavigationForFrame(WebKit::WebPage&, WebKit::WebFrame&, WebKit::SameDocumentNavigationType, RefPtr<API::Object>&) override;
    void didRemoveFrameFromHierarchy(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) override;
    
    void didFirstVisuallyNonEmptyLayoutForFrame(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) override;
    void didLayoutForFrame(WebKit::WebPage&, WebKit::WebFrame&) override;
    void didReachLayoutMilestone(WebKit::WebPage&, OptionSet<WebCore::LayoutMilestone>, RefPtr<API::Object>&) override;
    
    void didHandleOnloadEventsForFrame(WebKit::WebPage&, WebKit::WebFrame&) override;
    
    void globalObjectIsAvailableForFrame(WebKit::WebPage&, WebKit::WebFrame&, WebCore::DOMWrapperWorld&) override;

    WTF::String userAgentForURL(WebKit::WebFrame&, const URL&) const override;
    
private:
    id <WKWebProcessPlugInLoadDelegate> loadDelegate() const { return m_delegate.get().get(); }
    WKWebProcessPlugInBrowserContextController *pluginContextController() const { return m_controller.get().get(); }

    WeakObjCPtr<WKWebProcessPlugInBrowserContextController> m_controller;
    WeakObjCPtr<id <WKWebProcessPlugInLoadDelegate>> m_delegate;
};

@implementation WKWebProcessPlugInBrowserContextController {
    API::ObjectStorage<WebKit::WebPage> _page;
    WeakObjCPtr<id <WKWebProcessPlugInLoadDelegate>> _loadDelegate;
    WeakObjCPtr<id <WKWebProcessPlugInFormDelegatePrivate>> _formDelegate;
    WeakObjCPtr<id <WKWebProcessPlugInEditingDelegate>> _editingDelegate;
    
    RetainPtr<_WKRemoteObjectRegistry> _remoteObjectRegistry;
}

void PageLoaderClient::didStartProvisionalLoadForFrame(WebKit::WebPage&, WebKit::WebFrame& frame, CompletionHandler<void(RefPtr<API::Object>&&)>&& completionHandler)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:didStartProvisionalLoadForFrame:)]) {
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() didStartProvisionalLoadForFrame:wrapper(frame)];
        completionHandler(nullptr);
        return;
    }
    SEL selector = @selector(webProcessPlugInBrowserContextController:willStartProvisionalLoadForFrame:completionHandler:);
    if ([loadDelegate() respondsToSelector:selector]) {
        auto checker = WebKit::CompletionHandlerCallChecker::create(loadDelegate(), selector);
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() willStartProvisionalLoadForFrame:wrapper(frame) completionHandler:BlockPtr<void()>::fromCallable([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] () mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(nullptr);
        }).get()];
        return;
    }
    completionHandler(nullptr);
}

void PageLoaderClient::didReceiveServerRedirectForProvisionalLoadForFrame(WebKit::WebPage&, WebKit::WebFrame& frame, RefPtr<API::Object>&)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:didReceiveServerRedirectForProvisionalLoadForFrame:)])
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() didReceiveServerRedirectForProvisionalLoadForFrame:wrapper(frame)];
}

void PageLoaderClient::didFinishLoadForFrame(WebKit::WebPage&, WebKit::WebFrame& frame, RefPtr<API::Object>&)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:didFinishLoadForFrame:)])
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() didFinishLoadForFrame:wrapper(frame)];
}

void PageLoaderClient::globalObjectIsAvailableForFrame(WebKit::WebPage&, WebKit::WebFrame& frame, WebCore::DOMWrapperWorld& scriptWorld)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:globalObjectIsAvailableForFrame:inScriptWorld:)])
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() globalObjectIsAvailableForFrame:wrapper(frame) inScriptWorld:API::wrapper(WebKit::InjectedBundleScriptWorld::getOrCreate(scriptWorld))];
}

void PageLoaderClient::didRemoveFrameFromHierarchy(WebKit::WebPage&, WebKit::WebFrame& frame, RefPtr<API::Object>&)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:didRemoveFrameFromHierarchy:)])
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() didRemoveFrameFromHierarchy:wrapper(frame)];
}

void PageLoaderClient::didCommitLoadForFrame(WebKit::WebPage&, WebKit::WebFrame& frame, RefPtr<API::Object>&)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:didCommitLoadForFrame:)])
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() didCommitLoadForFrame:wrapper(frame)];
}

void PageLoaderClient::didFinishDocumentLoadForFrame(WebKit::WebPage&, WebKit::WebFrame& frame, RefPtr<API::Object>&)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:didFinishDocumentLoadForFrame:)])
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() didFinishDocumentLoadForFrame:wrapper(frame)];
}

void PageLoaderClient::didFailProvisionalLoadWithErrorForFrame(WebKit::WebPage&, WebKit::WebFrame& frame, const WebCore::ResourceError& error, RefPtr<API::Object>&)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:didFailProvisionalLoadWithErrorForFrame:error:)])
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() didFailProvisionalLoadWithErrorForFrame:wrapper(frame) error:error];
}

void PageLoaderClient::didFailLoadWithErrorForFrame(WebKit::WebPage&, WebKit::WebFrame& frame, const WebCore::ResourceError& error, RefPtr<API::Object>&)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:didFailLoadWithErrorForFrame:error:)])
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() didFailLoadWithErrorForFrame:wrapper(frame) error:error];
}

void PageLoaderClient::didSameDocumentNavigationForFrame(WebKit::WebPage&, WebKit::WebFrame& frame, WebKit::SameDocumentNavigationType type, RefPtr<API::Object>&)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:didSameDocumentNavigation:forFrame:)])
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() didSameDocumentNavigation:toWKSameDocumentNavigationType(type) forFrame:wrapper(frame)];
    else {
        // FIXME: Remove this once clients switch to implementing the above delegate method.
        if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:didSameDocumentNavigationForFrame:)])
            [(NSObject *)loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() didSameDocumentNavigationForFrame:wrapper(frame)];
    }
}

void PageLoaderClient::didLayoutForFrame(WebKit::WebPage&, WebKit::WebFrame& frame)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:didLayoutForFrame:)])
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() didLayoutForFrame:wrapper(frame)];
}

void PageLoaderClient::didReachLayoutMilestone(WebKit::WebPage&, OptionSet<WebCore::LayoutMilestone> milestones, RefPtr<API::Object>&)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:renderingProgressDidChange:)])
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() renderingProgressDidChange:renderingProgressEvents(milestones)];
}

void PageLoaderClient::didFirstVisuallyNonEmptyLayoutForFrame(WebKit::WebPage&, WebKit::WebFrame& frame, RefPtr<API::Object>&)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:didFirstVisuallyNonEmptyLayoutForFrame:)])
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() didFirstVisuallyNonEmptyLayoutForFrame:wrapper(frame)];
}

void PageLoaderClient::didHandleOnloadEventsForFrame(WebKit::WebPage&, WebKit::WebFrame& frame)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:didHandleOnloadEventsForFrame:)])
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() didHandleOnloadEventsForFrame:wrapper(frame)];
}

WTF::String PageLoaderClient::userAgentForURL(WebKit::WebFrame& frame, const URL& url) const
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:frame:userAgentForURL:)])
        return [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() frame:wrapper(frame) userAgentForURL:url];
    return { };
}

void ResourceLoadClient::willSendRequestForFrame(WebKit::WebPage&, WebKit::WebFrame& frame, uint64_t resourceIdentifier, WebCore::ResourceRequest& request, const WebCore::ResourceResponse& redirectResponse)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:frame:willSendRequestForResource:request:redirectResponse:)]) {
        RetainPtr<NSURLRequest> originalRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
        RetainPtr<NSURLRequest> substituteRequest = [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() frame:wrapper(frame) willSendRequestForResource:resourceIdentifier
            request:originalRequest.get() redirectResponse:redirectResponse.nsURLResponse()];

        if (substituteRequest != originalRequest) {
            request = substituteRequest.get();
            return;
        }
    } else if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:frame:willSendRequest:redirectResponse:)]) {
        RetainPtr<NSURLRequest> originalRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
        RetainPtr<NSURLRequest> substituteRequest = [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() frame:wrapper(frame) willSendRequest:originalRequest.get()
            redirectResponse:redirectResponse.nsURLResponse()];

        if (substituteRequest != originalRequest) {
            request = substituteRequest.get();
            return;
        }
    }
}

void ResourceLoadClient::didInitiateLoadForResource(WebKit::WebPage&, WebKit::WebFrame& frame, uint64_t resourceIdentifier, const WebCore::ResourceRequest& request, bool pageIsProvisionallyLoading)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:frame:didInitiateLoadForResource:request:pageIsProvisionallyLoading:)]) {
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() frame:wrapper(frame) didInitiateLoadForResource:resourceIdentifier request:request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody)
            pageIsProvisionallyLoading:pageIsProvisionallyLoading];
    } else if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:frame:didInitiateLoadForResource:request:)]) {
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() frame:wrapper(frame) didInitiateLoadForResource:resourceIdentifier request:request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody)];
    }
}

void ResourceLoadClient::didFinishLoadForResource(WebKit::WebPage&, WebKit::WebFrame& frame, uint64_t resourceIdentifier)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:frame:didFinishLoadForResource:)]) {
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() frame:wrapper(frame) didFinishLoadForResource:resourceIdentifier];
    }
}

void ResourceLoadClient::didFailLoadForResource(WebKit::WebPage&, WebKit::WebFrame& frame, uint64_t resourceIdentifier, const WebCore::ResourceError& error)
{
    if ([loadDelegate() respondsToSelector:@selector(webProcessPlugInBrowserContextController:frame:didFailLoadForResource:error:)]) {
        [loadDelegate() webProcessPlugInBrowserContextController:pluginContextController() frame:wrapper(frame) didFailLoadForResource:resourceIdentifier error:error];
    }
}

- (id <WKWebProcessPlugInLoadDelegate>)loadDelegate
{
    return _loadDelegate.getAutoreleased();
}

- (void)setLoadDelegate:(id <WKWebProcessPlugInLoadDelegate>)loadDelegate
{
    _loadDelegate = loadDelegate;

    if (loadDelegate) {
        _page->setInjectedBundlePageLoaderClient(std::make_unique<PageLoaderClient>(self, loadDelegate));
        _page->setInjectedBundleResourceLoadClient(std::make_unique<ResourceLoadClient>(self, loadDelegate));
    } else {
        _page->setInjectedBundlePageLoaderClient(nullptr);
        _page->setInjectedBundleResourceLoadClient(nullptr);
    }
}

- (void)dealloc
{
    if (_remoteObjectRegistry) {
        WebKit::WebProcess::singleton().removeMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), _page->pageID());
        [_remoteObjectRegistry _invalidate];
    }

    _page->~WebPage();

    [super dealloc];
}

- (WKDOMDocument *)mainFrameDocument
{
    WebCore::Frame* webCoreMainFrame = _page->mainFrame();
    if (!webCoreMainFrame)
        return nil;

    return WebKit::toWKDOMDocument(webCoreMainFrame->document());
}

- (WKDOMRange *)selectedRange
{
    RefPtr<WebCore::Range> range = _page->currentSelectionAsRange();
    if (!range)
        return nil;

    return WebKit::toWKDOMRange(range.get());
}

- (WKWebProcessPlugInFrame *)mainFrame
{
    return wrapper(_page->mainWebFrame());
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
    return wrapper(WebKit::WebProcess::singleton().webPage(handle.pageID));
}

- (_WKRemoteObjectRegistry *)_remoteObjectRegistry
{
    if (!_remoteObjectRegistry) {
        _remoteObjectRegistry = adoptNS([[_WKRemoteObjectRegistry alloc] _initWithWebPage:*_page]);
        WebKit::WebProcess::singleton().addMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), _page->pageID(), [_remoteObjectRegistry remoteObjectRegistry]);
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

        void didFocusTextField(WebKit::WebPage*, WebCore::HTMLInputElement* inputElement, WebKit::WebFrame* frame) override
        {
            auto formDelegate = m_controller->_formDelegate.get();

            if ([formDelegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:didFocusTextField:inFrame:)])
                [formDelegate _webProcessPlugInBrowserContextController:m_controller didFocusTextField:wrapper(*WebKit::InjectedBundleNodeHandle::getOrCreate(inputElement).get()) inFrame:wrapper(*frame)];
        }

        void willSendSubmitEvent(WebKit::WebPage*, WebCore::HTMLFormElement* formElement, WebKit::WebFrame* targetFrame, WebKit::WebFrame* sourceFrame, const Vector<std::pair<String, String>>& values) override
        {
            auto formDelegate = m_controller->_formDelegate.get();

            if ([formDelegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:willSendSubmitEventToForm:inFrame:targetFrame:values:)]) {
                auto valueMap = adoptNS([[NSMutableDictionary alloc] initWithCapacity:values.size()]);
                for (const auto& pair : values)
                    [valueMap setObject:pair.second forKey:pair.first];

                [formDelegate _webProcessPlugInBrowserContextController:m_controller willSendSubmitEventToForm:wrapper(*WebKit::InjectedBundleNodeHandle::getOrCreate(formElement).get())
                    inFrame:wrapper(*sourceFrame) targetFrame:wrapper(*targetFrame) values:valueMap.get()];
            }
        }

        static void encodeUserObject(NSObject <NSSecureCoding> *userObject, RefPtr<API::Object>& userData)
        {
            if (!userObject)
                return;

            auto archiver = secureArchiver();
            @try {
                [archiver encodeObject:userObject forKey:@"userObject"];
            } @catch (NSException *exception) {
                LOG_ERROR("Failed to encode user object: %@", exception);
                return;
            }
            [archiver finishEncoding];

            userData = API::Data::createWithoutCopying(archiver.get().encodedData);
        }

        void willSubmitForm(WebKit::WebPage*, WebCore::HTMLFormElement* formElement, WebKit::WebFrame* frame, WebKit::WebFrame* sourceFrame, const Vector<std::pair<WTF::String, WTF::String>>& values, RefPtr<API::Object>& userData) override
        {
            auto formDelegate = m_controller->_formDelegate.get();

            if ([formDelegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:willSubmitForm:toFrame:fromFrame:withValues:)]) {
                auto valueMap = adoptNS([[NSMutableDictionary alloc] initWithCapacity:values.size()]);
                for (const auto& pair : values)
                    [valueMap setObject:pair.second forKey:pair.first];

                NSObject <NSSecureCoding> *userObject = [formDelegate _webProcessPlugInBrowserContextController:m_controller willSubmitForm:wrapper(*WebKit::InjectedBundleNodeHandle::getOrCreate(formElement).get()) toFrame:wrapper(*frame) fromFrame:wrapper(*sourceFrame) withValues:valueMap.get()];
                encodeUserObject(userObject, userData);
            }
        }

        void textDidChangeInTextField(WebKit::WebPage*, WebCore::HTMLInputElement* inputElement, WebKit::WebFrame* frame, bool initiatedByUserTyping) override
        {
            auto formDelegate = m_controller->_formDelegate.get();

            if ([formDelegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:textDidChangeInTextField:inFrame:initiatedByUserTyping:)])
                [formDelegate _webProcessPlugInBrowserContextController:m_controller textDidChangeInTextField:wrapper(*WebKit::InjectedBundleNodeHandle::getOrCreate(inputElement)) inFrame:wrapper(*frame) initiatedByUserTyping:initiatedByUserTyping];
        }

        void willBeginInputSession(WebKit::WebPage*, WebCore::Element* element, WebKit::WebFrame* frame, bool userIsInteracting, RefPtr<API::Object>& userData) override
        {
            auto formDelegate = m_controller->_formDelegate.get();

            if ([formDelegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:willBeginInputSessionForElement:inFrame:userIsInteracting:)]) {
                NSObject<NSSecureCoding> *userObject = [formDelegate _webProcessPlugInBrowserContextController:m_controller willBeginInputSessionForElement:wrapper(*WebKit::InjectedBundleNodeHandle::getOrCreate(element)) inFrame:wrapper(*frame) userIsInteracting:userIsInteracting];
                encodeUserObject(userObject, userData);
            } else if (userIsInteracting && [formDelegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:willBeginInputSessionForElement:inFrame:)]) {
                // FIXME: We check userIsInteracting so that we don't begin an input session for a
                // programmatic focus that doesn't cause the keyboard to appear. But this misses the case of
                // a programmatic focus happening while the keyboard is already shown. Once we have a way to
                // know the keyboard state in the Web Process, we should refine the condition.
                NSObject<NSSecureCoding> *userObject = [formDelegate _webProcessPlugInBrowserContextController:m_controller willBeginInputSessionForElement:wrapper(*WebKit::InjectedBundleNodeHandle::getOrCreate(element)) inFrame:wrapper(*frame)];
                encodeUserObject(userObject, userData);
            }
        }

        bool shouldNotifyOnFormChanges(WebKit::WebPage*) override
        {
            auto formDelegate = m_controller->_formDelegate.get();

            if (![formDelegate respondsToSelector:@selector(_webProcessPlugInBrowserContextControllerShouldNotifyOnFormChanges:)])
                return false;

            return [formDelegate _webProcessPlugInBrowserContextControllerShouldNotifyOnFormChanges:m_controller];
        }

        void didAssociateFormControls(WebKit::WebPage*, const Vector<RefPtr<WebCore::Element>>& elements) override
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

- (id <WKWebProcessPlugInEditingDelegate>)_editingDelegate
{
    return _editingDelegate.getAutoreleased();
}

static inline WKEditorInsertAction toWK(WebCore::EditorInsertAction action)
{
    switch (action) {
    case WebCore::EditorInsertAction::Typed:
        return WKEditorInsertActionTyped;
    case WebCore::EditorInsertAction::Pasted:
        return WKEditorInsertActionPasted;
    case WebCore::EditorInsertAction::Dropped:
        return WKEditorInsertActionDropped;
    }
}

- (void)_setEditingDelegate:(id <WKWebProcessPlugInEditingDelegate>)editingDelegate
{
    _editingDelegate = editingDelegate;

    class Client final : public API::InjectedBundle::EditorClient {
    public:
        explicit Client(WKWebProcessPlugInBrowserContextController *controller)
            : m_controller { controller }
            , m_delegateMethods { m_controller->_editingDelegate.get() }
        {
        }

    private:
        bool shouldInsertText(WebKit::WebPage&, StringImpl* text, WebCore::Range* rangeToReplace, WebCore::EditorInsertAction action) final
        {
            if (!m_delegateMethods.shouldInsertText)
                return true;

            return [m_controller->_editingDelegate.get() _webProcessPlugInBrowserContextController:m_controller shouldInsertText:String(text) replacingRange:wrapper(*WebKit::InjectedBundleRangeHandle::getOrCreate(rangeToReplace)) givenAction:toWK(action)];
        }

        bool shouldChangeSelectedRange(WebKit::WebPage&, WebCore::Range* fromRange, WebCore::Range* toRange, WebCore::EAffinity affinity, bool stillSelecting) final
        {
            if (!m_delegateMethods.shouldChangeSelectedRange)
                return true;

            auto apiFromRange = fromRange ? adoptNS([[WKDOMRange alloc] _initWithImpl:fromRange]) : nil;
            auto apiToRange = toRange ? adoptNS([[WKDOMRange alloc] _initWithImpl:toRange]) : nil;
#if PLATFORM(IOS_FAMILY)
            UITextStorageDirection apiAffinity = affinity == WebCore::UPSTREAM ? UITextStorageDirectionBackward : UITextStorageDirectionForward;
#else
            NSSelectionAffinity apiAffinity = affinity == WebCore::UPSTREAM ? NSSelectionAffinityUpstream : NSSelectionAffinityDownstream;
#endif

            return [m_controller->_editingDelegate.get() _webProcessPlugInBrowserContextController:m_controller shouldChangeSelectedRange:apiFromRange.get() toRange:apiToRange.get() affinity:apiAffinity stillSelecting:stillSelecting];
        }

        void didChange(WebKit::WebPage&, StringImpl*) final
        {
            if (!m_delegateMethods.didChange)
                return;

            [m_controller->_editingDelegate.get() _webProcessPlugInBrowserContextControllerDidChangeByEditing:m_controller];
        }

        void willWriteToPasteboard(WebKit::WebPage&, WebCore::Range* range) final
        {
            if (!m_delegateMethods.willWriteToPasteboard)
                return;

            [m_controller->_editingDelegate.get() _webProcessPlugInBrowserContextController:m_controller willWriteRangeToPasteboard:wrapper(*WebKit::InjectedBundleRangeHandle::getOrCreate(range).get())];
        }

        void getPasteboardDataForRange(WebKit::WebPage&, WebCore::Range* range, Vector<String>& pasteboardTypes, Vector<RefPtr<WebCore::SharedBuffer>>& pasteboardData) final
        {
            if (!m_delegateMethods.getPasteboardDataForRange)
                return;

            auto dataByType = [m_controller->_editingDelegate.get() _webProcessPlugInBrowserContextController:m_controller pasteboardDataForRange:wrapper(*WebKit::InjectedBundleRangeHandle::getOrCreate(range).get())];
            for (NSString *type in dataByType) {
                pasteboardTypes.append(type);
                pasteboardData.append(WebCore::SharedBuffer::create(dataByType[type]));
            };
        }

        void didWriteToPasteboard(WebKit::WebPage&) final
        {
            if (!m_delegateMethods.didWriteToPasteboard)
                return;

            [m_controller->_editingDelegate.get() _webProcessPlugInBrowserContextControllerDidWriteToPasteboard:m_controller];
        }

        bool performTwoStepDrop(WebKit::WebPage&, WebCore::DocumentFragment& fragment, WebCore::Range& range, bool isMove) final
        {
            if (!m_delegateMethods.performTwoStepDrop)
                return false;

            auto rangeHandle = WebKit::InjectedBundleRangeHandle::getOrCreate(&range);
            auto nodeHandle = WebKit::InjectedBundleNodeHandle::getOrCreate(&fragment);
            return [m_controller->_editingDelegate.get() _webProcessPlugInBrowserContextController:m_controller performTwoStepDrop:wrapper(*nodeHandle) atDestination:wrapper(*rangeHandle) isMove:isMove];
        }

        WTF::String replacementURLForResource(WebKit::WebPage&, Ref<WebCore::SharedBuffer>&& resourceData, const WTF::String& mimeType)
        {
            if (!m_delegateMethods.replacementURLForResource)
                return { };

            NSString *type = (NSString *)mimeType;
            auto data = resourceData->createNSData();
            return [m_controller->_editingDelegate.get() _webProcessPlugInBrowserContextController:m_controller replacementURLForResource:data.get() mimeType:type];
        }

        WKWebProcessPlugInBrowserContextController *m_controller;
        const struct DelegateMethods {
            DelegateMethods(RetainPtr<id <WKWebProcessPlugInEditingDelegate>> delegate)
                : shouldInsertText([delegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:shouldInsertText:replacingRange:givenAction:)])
                , shouldChangeSelectedRange([delegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:shouldChangeSelectedRange:toRange:affinity:stillSelecting:)])
                , didChange([delegate respondsToSelector:@selector(_webProcessPlugInBrowserContextControllerDidChangeByEditing:)])
                , willWriteToPasteboard([delegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:willWriteRangeToPasteboard:)])
                , getPasteboardDataForRange([delegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:pasteboardDataForRange:)])
                , didWriteToPasteboard([delegate respondsToSelector:@selector(_webProcessPlugInBrowserContextControllerDidWriteToPasteboard:)])
                , performTwoStepDrop([delegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:performTwoStepDrop:atDestination:isMove:)])
                , replacementURLForResource([delegate respondsToSelector:@selector(_webProcessPlugInBrowserContextController:replacementURLForResource:mimeType:)])
            {
            }

            bool shouldInsertText;
            bool shouldChangeSelectedRange;
            bool didChange;
            bool willWriteToPasteboard;
            bool getPasteboardDataForRange;
            bool didWriteToPasteboard;
            bool performTwoStepDrop;
            bool replacementURLForResource;
        } m_delegateMethods;
    };

    if (editingDelegate)
        _page->setInjectedBundleEditorClient(std::make_unique<Client>(self));
    else
        _page->setInjectedBundleEditorClient(nullptr);
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
