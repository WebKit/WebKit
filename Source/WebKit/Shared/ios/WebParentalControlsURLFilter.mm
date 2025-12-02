/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "WebParentalControlsURLFilter.h"

#if HAVE(BROWSERENGINEKIT_WEBCONTENTFILTER)

#import "Logging.h"
#import <WebCore/ParentalControlsContentFilter.h>
#import <pal/spi/ios/BrowserEngineKitSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/CrossThreadCopier.h>
#import <wtf/WorkQueue.h>

namespace WebKit {

Ref<WebParentalControlsURLFilter> WebParentalControlsURLFilter::create()
{
    return adoptRef(*new WebParentalControlsURLFilter);
}

WebParentalControlsURLFilter::WebParentalControlsURLFilter() = default;

WebParentalControlsURLFilter::~WebParentalControlsURLFilter() = default;

BEWebContentFilter* WebParentalControlsURLFilter::ensureWebContentFilter()
{
    assertIsCurrent(workQueueSingleton());
    if (!m_webContentFilter)
        lazyInitialize(m_webContentFilter, adoptNS([[BEWebContentFilter alloc] init]));

    return m_webContentFilter.get();
}

bool WebParentalControlsURLFilter::isEnabledImpl() const
{
    return [BEWebContentFilter shouldEvaluateURLs];
}

void WebParentalControlsURLFilter::isURLAllowed(const URL& url, WebCore::ParentalControlsContentFilter& filter)
{
    workQueueSingleton().dispatch([this, protectedThis = Ref { *this }, currentIsEnabled = isEnabled(), url = crossThreadCopy(url), weakFilter = ThreadSafeWeakPtr { filter }]() mutable {
        RefPtr filter = weakFilter.get();
        if (!currentIsEnabled && filter) {
            filter->didReceiveAllowDecisionOnQueue(true, nullptr);
            return;
        }

        [ensureWebContentFilter() evaluateURL:url.createNSURL().get() completionHandler:makeBlockPtr([weakFilter = WTFMove(weakFilter)](BOOL shouldBlock, NSData *replacementData) mutable {
            if (RefPtr filter = weakFilter.get())
                filter->didReceiveAllowDecisionOnQueue(!shouldBlock, replacementData);
        }).get()];
    });
}

void WebParentalControlsURLFilter::allowURL(const URL& url, CompletionHandler<void(bool)>&& completionHandler)
{
    workQueueSingleton().dispatch([this, protectedThis = Ref { *this }, currentIsEnabled = isEnabled(), url = crossThreadCopy(url), completionHandler = WTFMove(completionHandler)]() mutable {
        if (!currentIsEnabled) {
            callOnMainRunLoop([completionHandler = WTFMove(completionHandler)] mutable {
                completionHandler(true);
            });
            return;
        }

        [ensureWebContentFilter() allowURL:url.createNSURL().get() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)](BOOL didAllow, NSError *) mutable {
            RELEASE_LOG(Loading, "WebParentalControlsURLFilter::allowURL result %d.\n", didAllow);
            callOnMainRunLoop([didAllow, completionHandler = WTFMove(completionHandler)] mutable {
                completionHandler(didAllow);
            });
        }).get()];
    });
}

} // namespace WebKit

#endif
