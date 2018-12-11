/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "IconLoadingDelegate.h"

#if WK_API_ENABLED

#include "WKNSData.h"
#include "_WKIconLoadingDelegate.h"
#include "_WKLinkIconParametersInternal.h"
#include <wtf/BlockPtr.h>

namespace WebKit {

IconLoadingDelegate::IconLoadingDelegate(WKWebView *webView)
    : m_webView(webView)
{
}

IconLoadingDelegate::~IconLoadingDelegate()
{
}

std::unique_ptr<API::IconLoadingClient> IconLoadingDelegate::createIconLoadingClient()
{
    return std::make_unique<IconLoadingClient>(*this);
}

RetainPtr<id <_WKIconLoadingDelegate> > IconLoadingDelegate::delegate()
{
    return m_delegate.get();
}

void IconLoadingDelegate::setDelegate(id <_WKIconLoadingDelegate> delegate)
{
    m_delegate = delegate;

    m_delegateMethods.webViewShouldLoadIconWithParametersCompletionHandler = [delegate respondsToSelector:@selector(webView:shouldLoadIconWithParameters:completionHandler:)];
}

IconLoadingDelegate::IconLoadingClient::IconLoadingClient(IconLoadingDelegate& iconLoadingDelegate)
    : m_iconLoadingDelegate(iconLoadingDelegate)
{
}

IconLoadingDelegate::IconLoadingClient::~IconLoadingClient()
{
}

typedef void (^IconLoadCompletionHandler)(NSData*);

void IconLoadingDelegate::IconLoadingClient::getLoadDecisionForIcon(const WebCore::LinkIcon& linkIcon, WTF::CompletionHandler<void(WTF::Function<void(API::Data*, WebKit::CallbackBase::Error)>&&)>&& completionHandler)
{
    if (!m_iconLoadingDelegate.m_delegateMethods.webViewShouldLoadIconWithParametersCompletionHandler) {
        completionHandler(nullptr);
        return;
    }

    auto delegate = m_iconLoadingDelegate.m_delegate.get();
    if (!delegate) {
        completionHandler(nullptr);
        return;
    }

    RetainPtr<_WKLinkIconParameters> parameters = adoptNS([[_WKLinkIconParameters alloc] _initWithLinkIcon:linkIcon]);

    [delegate webView:m_iconLoadingDelegate.m_webView shouldLoadIconWithParameters:parameters.get() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)] (IconLoadCompletionHandler loadCompletionHandler) mutable {
        ASSERT(RunLoop::isMain());
        if (loadCompletionHandler) {
            completionHandler([loadCompletionHandler = Block_copy(loadCompletionHandler)](API::Data* data, WebKit::CallbackBase::Error error) {
                if (error != CallbackBase::Error::None || !data)
                    loadCompletionHandler(nil);
                else
                    loadCompletionHandler(wrapper(*data));
            });
        } else
            completionHandler(nullptr);
    }).get()];
}

} // namespace WebKit

#endif // WK_API_ENABLED
