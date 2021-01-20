/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "ResourceLoadDelegate.h"

#import <WebCore/AuthenticationMac.h>
#import "AuthenticationChallengeProxy.h"
#import "WKNSURLAuthenticationChallenge.h"
#import "_WKResourceLoadDelegate.h"
#import "_WKResourceLoadInfoInternal.h"

namespace WebKit {

ResourceLoadDelegate::ResourceLoadDelegate(WKWebView *webView)
    : m_webView(webView)
{
}

ResourceLoadDelegate::~ResourceLoadDelegate() = default;

std::unique_ptr<API::ResourceLoadClient> ResourceLoadDelegate::createResourceLoadClient()
{
    return makeUnique<ResourceLoadClient>(*this);
}

RetainPtr<id<_WKResourceLoadDelegate>> ResourceLoadDelegate::delegate()
{
    return m_delegate.get();
}

void ResourceLoadDelegate::setDelegate(id <_WKResourceLoadDelegate> delegate)
{
    m_delegate = delegate;

    // resourceWithID:
    // type:
    // _WKFrameHandle frame:
    // _WKFrameHandle parentFrame:
    
    m_delegateMethods.didSendRequest = [delegate respondsToSelector:@selector(webView:resourceLoad:didSendRequest:)];
    m_delegateMethods.didPerformHTTPRedirection = [delegate respondsToSelector:@selector(webView:resourceLoad:didPerformHTTPRedirection:newRequest:)];
    m_delegateMethods.didReceiveChallenge = [delegate respondsToSelector:@selector(webView:resourceLoad:didReceiveChallenge:)];
    m_delegateMethods.didReceiveResponse = [delegate respondsToSelector:@selector(webView:resourceLoad:didReceiveResponse:)];
    m_delegateMethods.didCompleteWithError = [delegate respondsToSelector:@selector(webView:resourceLoad:didCompleteWithError:response:)];
}

ResourceLoadDelegate::ResourceLoadClient::ResourceLoadClient(ResourceLoadDelegate& delegate)
    : m_resourceLoadDelegate(delegate)
{
}

ResourceLoadDelegate::ResourceLoadClient::~ResourceLoadClient() = default;

void ResourceLoadDelegate::ResourceLoadClient::didSendRequest(WebKit::ResourceLoadInfo&& loadInfo, WebCore::ResourceRequest&& request) const
{
    if (!m_resourceLoadDelegate.m_delegateMethods.didSendRequest)
        return;

    auto delegate = m_resourceLoadDelegate.m_delegate.get();
    if (!delegate)
        return;

    [delegate webView:m_resourceLoadDelegate.m_webView.get().get() resourceLoad:wrapper(API::ResourceLoadInfo::create(WTFMove(loadInfo)).get()) didSendRequest:request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody)];
}

void ResourceLoadDelegate::ResourceLoadClient::didPerformHTTPRedirection(WebKit::ResourceLoadInfo&& loadInfo, WebCore::ResourceResponse&& response, WebCore::ResourceRequest&& request) const
{
    if (!m_resourceLoadDelegate.m_delegateMethods.didPerformHTTPRedirection)
        return;

    auto delegate = m_resourceLoadDelegate.m_delegate.get();
    if (!delegate)
        return;

    [delegate webView:m_resourceLoadDelegate.m_webView.get().get() resourceLoad:wrapper(API::ResourceLoadInfo::create(WTFMove(loadInfo)).get()) didPerformHTTPRedirection:response.nsURLResponse() newRequest:request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody)];
}

void ResourceLoadDelegate::ResourceLoadClient::didReceiveChallenge(WebKit::ResourceLoadInfo&& loadInfo, WebCore::AuthenticationChallenge&& challenge) const
{
    if (!m_resourceLoadDelegate.m_delegateMethods.didReceiveChallenge)
        return;

    auto delegate = m_resourceLoadDelegate.m_delegate.get();
    if (!delegate)
        return;

    [delegate webView:m_resourceLoadDelegate.m_webView.get().get() resourceLoad:wrapper(API::ResourceLoadInfo::create(WTFMove(loadInfo)).get()) didReceiveChallenge:mac(challenge)];
}

void ResourceLoadDelegate::ResourceLoadClient::didReceiveResponse(WebKit::ResourceLoadInfo&& loadInfo, WebCore::ResourceResponse&& response) const
{
    if (!m_resourceLoadDelegate.m_delegateMethods.didReceiveResponse)
        return;

    auto delegate = m_resourceLoadDelegate.m_delegate.get();
    if (!delegate)
        return;

    [delegate webView:m_resourceLoadDelegate.m_webView.get().get() resourceLoad:wrapper(API::ResourceLoadInfo::create(WTFMove(loadInfo)).get()) didReceiveResponse:response.nsURLResponse()];
}

void ResourceLoadDelegate::ResourceLoadClient::didCompleteWithError(WebKit::ResourceLoadInfo&& loadInfo, WebCore::ResourceResponse&& response, WebCore::ResourceError&& error) const
{
    if (!m_resourceLoadDelegate.m_delegateMethods.didCompleteWithError)
        return;

    auto delegate = m_resourceLoadDelegate.m_delegate.get();
    if (!delegate)
        return;

    [delegate webView:m_resourceLoadDelegate.m_webView.get().get() resourceLoad:wrapper(API::ResourceLoadInfo::create(WTFMove(loadInfo)).get()) didCompleteWithError:error.nsError() response:response.nsURLResponse()];
}

} // namespace WebKit
