/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "_WKDataTaskInternal.h"

#import "APIDataTask.h"
#import "APIDataTaskClient.h"
#import "AuthenticationChallengeDispositionCocoa.h"
#import "CompletionHandlerCallChecker.h"
#import "WebPageProxy.h"
#import "_WKDataTaskDelegate.h"
#import <WebCore/AuthenticationMac.h>
#import <WebCore/Credential.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/BlockPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/SpanCocoa.h>

class WKDataTaskClient final : public API::DataTaskClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WKDataTaskClient);
public:
    static Ref<WKDataTaskClient> create(id <_WKDataTaskDelegate> delegate) { return adoptRef(*new WKDataTaskClient(delegate)); }
private:
    explicit WKDataTaskClient(id <_WKDataTaskDelegate> delegate)
        : m_delegate(delegate)
        , m_respondsToDidReceiveAuthenticationChallenge([delegate respondsToSelector:@selector(dataTask:didReceiveAuthenticationChallenge:completionHandler:)])
        , m_respondsToWillPerformHTTPRedirection([delegate respondsToSelector:@selector(dataTask:willPerformHTTPRedirection:newRequest:decisionHandler:)])
        , m_respondsToDidReceiveResponse([delegate respondsToSelector:@selector(dataTask:didReceiveResponse:decisionHandler:)])
        , m_respondsToDidReceiveData([delegate respondsToSelector:@selector(dataTask:didReceiveData:)])
        , m_respondsToDidCompleteWithError([delegate respondsToSelector:@selector(dataTask:didCompleteWithError:)]) { }

    void didReceiveChallenge(API::DataTask& task, WebCore::AuthenticationChallenge&& challenge, CompletionHandler<void(WebKit::AuthenticationChallengeDisposition, WebCore::Credential&&)>&& completionHandler) const final
    {
        if (!m_delegate || !m_respondsToDidReceiveAuthenticationChallenge)
            return completionHandler(WebKit::AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue, { });
        auto checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(dataTask:didReceiveAuthenticationChallenge:completionHandler:));
        [m_delegate dataTask:wrapper(task) didReceiveAuthenticationChallenge:mac(challenge) completionHandler:makeBlockPtr([checker = WTFMove(checker), completionHandler = WTFMove(completionHandler)](NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(WebKit::toAuthenticationChallengeDisposition(disposition), WebCore::Credential(credential));
        }).get()];
    }

    void willPerformHTTPRedirection(API::DataTask& task, WebCore::ResourceResponse&& response, WebCore::ResourceRequest&& request, CompletionHandler<void(bool)>&& completionHandler) const final
    {
        if (!m_delegate || !m_respondsToWillPerformHTTPRedirection)
            return completionHandler(true);
        auto checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(dataTask:willPerformHTTPRedirection:newRequest:decisionHandler:));
        [m_delegate dataTask:wrapper(task) willPerformHTTPRedirection:(NSHTTPURLResponse *)response.nsURLResponse() newRequest:request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody) decisionHandler:makeBlockPtr([checker = WTFMove(checker), completionHandler = WTFMove(completionHandler)] (_WKDataTaskRedirectPolicy policy) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(policy == _WKDataTaskRedirectPolicyAllow);
        }).get()];
    }

    void didReceiveResponse(API::DataTask& task, WebCore::ResourceResponse&& response, CompletionHandler<void(bool)>&& completionHandler) const final
    {
        if (!m_delegate || !m_respondsToDidReceiveResponse)
            return completionHandler(true);
        auto checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(dataTask:didReceiveResponse:decisionHandler:));
        [m_delegate dataTask:wrapper(task) didReceiveResponse:response.nsURLResponse() decisionHandler:makeBlockPtr([checker = WTFMove(checker), completionHandler = WTFMove(completionHandler)] (_WKDataTaskResponsePolicy policy) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(policy == _WKDataTaskResponsePolicyAllow);
        }).get()];
    }

    void didReceiveData(API::DataTask& task, std::span<const uint8_t> data) const final
    {
        if (!m_delegate || !m_respondsToDidReceiveData)
            return;
        [m_delegate dataTask:wrapper(task) didReceiveData:toNSData(data).get()];
    }

    void didCompleteWithError(API::DataTask& task, WebCore::ResourceError&& error) const final
    {
        if (!m_delegate || !m_respondsToDidCompleteWithError)
            return;
        [m_delegate dataTask:wrapper(task) didCompleteWithError:error.nsError()];
        wrapper(task)->_delegate = nil;
    }

    WeakObjCPtr<id <_WKDataTaskDelegate> > m_delegate;

    bool m_respondsToDidReceiveAuthenticationChallenge : 1;
    bool m_respondsToWillPerformHTTPRedirection : 1;
    bool m_respondsToDidReceiveResponse : 1;
    bool m_respondsToDidReceiveData : 1;
    bool m_respondsToDidCompleteWithError : 1;
};

@implementation _WKDataTask

- (void)cancel
{
    _dataTask->cancel();
    _delegate = nil;
}

- (WKWebView *)webView
{
    auto* page = _dataTask->page();
    if (!page)
        return nil;
    return page->cocoaView().get();
}

- (id <_WKDataTaskDelegate>)delegate
{
    return _delegate.get();
}

- (void)setDelegate:(id <_WKDataTaskDelegate>)delegate
{
    _delegate = delegate;
    _dataTask->setClient(WKDataTaskClient::create(delegate));
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKDataTask.class, self))
        return;
    _dataTask->~DataTask();
    [super dealloc];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_dataTask;
}

@end
