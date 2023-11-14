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

#import "WKNSData.h"
#import "_WKIconLoadingDelegate.h"
#import "_WKLinkIconParametersInternal.h"
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>

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
    return makeUnique<IconLoadingClient>(*this);
}

RetainPtr<id <_WKIconLoadingDelegate> > IconLoadingDelegate::delegate()
{
    return m_delegate.get();
}

void IconLoadingDelegate::setDelegate(id <_WKIconLoadingDelegate> delegate)
{
    m_delegate = delegate;

    m_delegateMethods.webViewShouldLoadIconWithParametersCompletionHandler = [delegate respondsToSelector:@selector(webView:shouldLoadIconWithParameters:completionHandler:)];
    m_delegateMethods.webViewShouldLoadIconsWithParametersCompletionHandler = [delegate respondsToSelector:@selector(webView:shouldLoadIconsWithParameters:completionHandler:)];
}

IconLoadingDelegate::IconLoadingClient::IconLoadingClient(IconLoadingDelegate& iconLoadingDelegate)
    : m_iconLoadingDelegate(iconLoadingDelegate)
{
}

IconLoadingDelegate::IconLoadingClient::~IconLoadingClient()
{
}

typedef void (^IconLoadCompletionHandler)(NSData*);

typedef void (^BatchIconLoadCompletionHandler)(_WKLinkIconParameters*, NSData*);

void IconLoadingDelegate::IconLoadingClient::getLoadDecisionsForIcons(const Vector<std::pair<WebCore::LinkIcon, uint64_t>>& linkIconIdentifierPairs, Function<void(uint64_t, CompletionHandler<void(API::Data*)>&&)>&& callback)
{
    if (!m_iconLoadingDelegate.m_delegateMethods.webViewShouldLoadIconsWithParametersCompletionHandler
        && !m_iconLoadingDelegate.m_delegateMethods.webViewShouldLoadIconWithParametersCompletionHandler) {
        for (auto& linkIconIdentifierPair : linkIconIdentifierPairs)
            callback(linkIconIdentifierPair.second, nullptr);
        return;
    }

    auto delegate = m_iconLoadingDelegate.m_delegate.get();
    if (!delegate) {
        for (auto& linkIconIdentifierPair : linkIconIdentifierPairs)
            callback(linkIconIdentifierPair.second, nullptr);
        return;
    }

    Vector<std::pair<RetainPtr<_WKLinkIconParameters>, uint64_t>> iconParametersIdentifierPairs;
    RetainPtr<NSMutableArray<_WKLinkIconParameters *>> allIconParameters = [NSMutableArray arrayWithCapacity:linkIconIdentifierPairs.size()];
    for (auto& linkIconIdentifierPair : linkIconIdentifierPairs) {
        RetainPtr<_WKLinkIconParameters> iconParameters = adoptNS([[_WKLinkIconParameters alloc] _initWithLinkIcon:linkIconIdentifierPair.first]);
        [allIconParameters addObject:iconParameters.get()];
        iconParametersIdentifierPairs.append({ iconParameters, linkIconIdentifierPair.second });
    }

    if (!m_iconLoadingDelegate.m_delegateMethods.webViewShouldLoadIconsWithParametersCompletionHandler) {
        for (auto& iconParametersIdentifierPair : iconParametersIdentifierPairs) {
            uint64_t identifier = iconParametersIdentifierPair.second;
            [delegate webView:m_iconLoadingDelegate.m_webView shouldLoadIconWithParameters:iconParametersIdentifierPair.first.get() completionHandler:makeBlockPtr([identifier, callback = WTFMove(callback)] (IconLoadCompletionHandler loadCompletionHandler) mutable {
                ASSERT(RunLoop::isMain());
                if (loadCompletionHandler) {
                    callback(identifier, [loadCompletionHandler = makeBlockPtr(loadCompletionHandler)](API::Data* data) {
                        loadCompletionHandler(wrapper(data));
                    });
                } else
                    callback(identifier, nullptr);
            }).get()];
        }
        return;
    }

    [delegate webView:m_iconLoadingDelegate.m_webView shouldLoadIconsWithParameters:allIconParameters.get() completionHandler:makeBlockPtr([iconParametersIdentifierPairs = WTFMove(iconParametersIdentifierPairs), callback = WTFMove(callback)] (NSSet<_WKLinkIconParameters *> *iconParametersToLoad, BatchIconLoadCompletionHandler loadCompletionHandler) mutable {
        ASSERT(RunLoop::isMain());
        for (auto& iconParametersIdentifierPair : iconParametersIdentifierPairs) {
            RetainPtr<_WKLinkIconParameters> iconParameters = iconParametersIdentifierPair.first;
            if ([iconParametersToLoad containsObject:iconParameters.get()]) {
                callback(iconParametersIdentifierPair.second, [iconParameters = WTFMove(iconParameters), loadCompletionHandler = makeBlockPtr(loadCompletionHandler)](API::Data* data) {
                    loadCompletionHandler(iconParameters.get(), wrapper(data));
                });
            } else
                callback(iconParametersIdentifierPair.second, nullptr);
        }
    }).get()];
}

} // namespace WebKit
