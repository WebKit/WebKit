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

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/BEKAdditions.h>
#endif

#if HAVE(WEBCONTENTRESTRICTIONS_TRANSITIVE_TRUST)
#import <WebCore/DeprecatedGlobalSettings.h>
#endif

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

void WebParentalControlsURLFilter::isURLAllowedImpl(const URL& mainDocumentURL, const URL& url, CompletionHandler<void(bool, NSData *)>&& completionHandler)
{
    workQueueSingleton().dispatch([this, protectedThis = Ref { *this }, currentIsEnabled = isEnabled(), mainDocumentURL = crossThreadCopy(mainDocumentURL), url = crossThreadCopy(url), completionHandler = WTF::move(completionHandler)]() mutable {
        if (!currentIsEnabled) {
            completionHandler(true, nullptr);
            return;
        }

        RetainPtr filter = ensureWebContentFilter();
#if HAVE(WEBCONTENTRESTRICTIONS_TRANSITIVE_TRUST)
#if __has_include(<WebKitAdditions/BEKAdditions.h>)
    if (WebCore::DeprecatedGlobalSettings::webContentRestrictionsTransitiveTrustEnabled()) {
        MAYBE_EVALUATE_URL_WITH_TRANSITIVE_TRUST
    }
#endif
#endif
        [filter evaluateURL:url.createNSURL().get() completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)](BOOL shouldBlock, NSData *replacementData) mutable {
            completionHandler(!shouldBlock, replacementData);
        }).get()];
    });
}

void WebParentalControlsURLFilter::allowURL(const URL& url, CompletionHandler<void(bool)>&& completionHandler)
{
    workQueueSingleton().dispatch([this, protectedThis = Ref { *this }, currentIsEnabled = isEnabled(), url = crossThreadCopy(url), completionHandler = WTF::move(completionHandler)]() mutable {
        if (!currentIsEnabled) {
            callOnMainRunLoop([completionHandler = WTF::move(completionHandler)] mutable {
                completionHandler(true);
            });
            return;
        }

        [protect(ensureWebContentFilter()) allowURL:url.createNSURL().get() completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)](BOOL didAllow, NSError *) mutable {
            RELEASE_LOG(Loading, "WebParentalControlsURLFilter::allowURL result %d.\n", didAllow);
            callOnMainRunLoop([didAllow, completionHandler = WTF::move(completionHandler)] mutable {
                completionHandler(didAllow);
            });
        }).get()];
    });
}

void WebParentalControlsURLFilter::setSharedParentalControlsURLFilterIfNecessary()
{
#if !HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    ASSERT(isMainRunLoop());
    static bool initialized = false;
    if (!initialized) {
        WebCore::ParentalControlsURLFilter::setGlobalFilter(WebParentalControlsURLFilter::create());
        initialized = true;
    }
#endif
}

#if HAVE(WEBCONTENTRESTRICTIONS_ASK_TO)
void WebParentalControlsURLFilter::requestPermissionForURL(const URL& url, const URL& referrerURL, CompletionHandler<void(bool)>&& completionHandler)
{
    workQueueSingleton().dispatchSync([this, protectedThis = Ref { *this }, currentIsEnabled = isEnabled(), url = crossThreadCopy(url), referrerURL = crossThreadCopy(referrerURL), completionHandler = WTF::move(completionHandler)]() mutable {
        if (!currentIsEnabled) {
            callOnMainRunLoop([completionHandler = WTF::move(completionHandler)] mutable {
                completionHandler(true);
            });
            return;
        }
        auto filter = ensureWebContentFilter();
#if __has_include(<WebKitAdditions/BEKAdditions.h>)
        RELEASE_LOG(Loading, "WebParentalControlsURLFilter::requestPermissionForURL starts execution");
        MAYBE_REQUEST_PERMISSION_ASK_TO
#endif
    });
}
#endif

} // namespace WebKit

#endif
