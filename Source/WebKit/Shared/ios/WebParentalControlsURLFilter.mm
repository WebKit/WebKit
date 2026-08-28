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
#import <UIKit/UIView.h>
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

void WebParentalControlsURLFilter::isURLAllowedImpl(WebCore::IsMainFrameLoad isMainFrame, const URL& mainDocumentURL, const URL& url, CompletionHandler<void(bool, NSData *)>&& completionHandler)
{
    // mainDocumentURL acts as a root for Parental Controls policies. Accordingly, mainDocumentURL and url are required to match on mainframe navigations.
    auto& effectiveMainDocumentURL = (isMainFrame == WebCore::IsMainFrameLoad::Yes) ? url : mainDocumentURL;

    workQueueSingleton().dispatch([this,
        protectedThis = Ref { *this },
        currentIsEnabled = isEnabled(),
        mainDocumentURL = crossThreadCopy(effectiveMainDocumentURL),
        url = crossThreadCopy(url),
        isMainFrame,
        completionHandler = WTF::move(completionHandler)]() mutable {

        // TODO: Remove once rdar://175796135 is merged.
        UNUSED_PARAM(isMainFrame);

        if (!currentIsEnabled) {
            completionHandler(true, nullptr);
            return;
        }

        RetainPtr filter = ensureWebContentFilter();
#if HAVE(WEBCONTENTRESTRICTIONS_TRANSITIVE_TRUST)
#if __has_include(<WebKitAdditions/BEKAdditions.h>)
        if (WebCore::DeprecatedGlobalSettings::webContentRestrictionsTransitiveTrustEnabled()) {
            BOOL isMainFrameForEvaluation = (isMainFrame == WebCore::IsMainFrameLoad::Yes);
            if ([filter respondsToSelector:@selector(evaluateURL:mainFrameURL:isMainFrame:completionHandler:)]) {
                [filter evaluateURL:url.createNSURL().get() mainFrameURL:mainDocumentURL.createNSURL().get() isMainFrame:isMainFrameForEvaluation completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)](BOOL shouldBlock, NSData *replacementData) mutable {
                    if (completionHandler)
                        completionHandler(!shouldBlock, replacementData);
                }).get()];
                return;
            }
        }
#endif
#endif
        [filter evaluateURL:url.createNSURL().get() completionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)](BOOL shouldBlock, NSData *replacementData) mutable {
            // Make sure we don't crash even if [BEWebContentFilter evaluateURL:completionHandler:] calls its
            // completion handler more than once (which seems to happen in practice).
            if (completionHandler)
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
                if (completionHandler)
                    completionHandler(didAllow);
            });
        }).get()];
    });
}

void WebParentalControlsURLFilter::setSharedParentalControlsURLFilterIfNecessary()
{
#if !HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    ASSERT(isMainRunLoop());
    if (!WebCore::ParentalControlsURLFilter::hasGlobalFilter()) {
        WebCore::ParentalControlsURLFilter::setGlobalFilter(WebParentalControlsURLFilter::create());
    }
#endif
}

#if HAVE(WEBCONTENTRESTRICTIONS_ASK_TO)
void WebParentalControlsURLFilter::requestPermissionForURL(const URL& url, const URL& referrerURL, CompletionHandler<void(bool)>&& completionHandler, CocoaView* presentingView)
{
    UIView* presentingViewAsUIView = (UIView *)presentingView;
    workQueueSingleton().dispatch([this, protectedThis = Ref { *this },
        currentIsEnabled = isEnabled(),
        url = crossThreadCopy(url),
        referrerURL = crossThreadCopy(referrerURL),
        presentingViewAsUIView,
        completionHandler = WTF::move(completionHandler)]() mutable {
        if (!currentIsEnabled) {
            callOnMainRunLoop([completionHandler = WTF::move(completionHandler)] mutable {
                completionHandler(true);
            });
            return;
        }
        RetainPtr filter = ensureWebContentFilter();
#if __has_include(<WebKitAdditions/BEKAdditions.h>)
        if ([filter respondsToSelector:@selector(requestPermissionForURL:referrerURL:presentingView:completionHandler:)]) {
            auto permissionDecisionCompletionHandler = makeBlockPtr([completionHandler = WTF::move(completionHandler)](BEWebContentFilterPermissionDecision result, NSError *) mutable {
                switch (result) {
                case BEWebContentFilterPermissionDecisionError:
                    RELEASE_LOG(Loading, "WebParentalControlsURLFilter::requestPermissionForURL result is error");
                    break;
                case BEWebContentFilterPermissionDecisionAllowed:
                    RELEASE_LOG(Loading, "WebParentalControlsURLFilter::requestPermissionForURL result is allowed");
                    break;
                case BEWebContentFilterPermissionDecisionDenied:
                    RELEASE_LOG(Loading, "WebParentalControlsURLFilter::requestPermissionForURL result is denied");
                    break;
                case BEWebContentFilterPermissionDecisionPending:
                    RELEASE_LOG(Loading, "WebParentalControlsURLFilter::requestPermissionForURL result is pending");
                    break;
                default:
                    RELEASE_LOG_ERROR(Loading, "WebParentalControlsURLFilter::requestPermissionForURL result is invalid, result:%ld", (long)result);
                    break;
                }

                bool didAllow = (result == BEWebContentFilterPermissionDecisionAllowed);
                callOnMainRunLoop([didAllow, completionHandler = WTF::move(completionHandler)] mutable {
                    if (completionHandler)
                        completionHandler(didAllow);
                });
            });
            [filter requestPermissionForURL:url.createNSURL().get() referrerURL:referrerURL.createNSURL().get() presentingView:presentingViewAsUIView completionHandler:permissionDecisionCompletionHandler.get()];
            return;
        }
#endif
        RELEASE_LOG_ERROR(Loading, "WebParentalControlsURLFilter::requestPermissionForURL is running an unsupported configuration - default to denying permission");
        callOnMainRunLoop([completionHandler = WTF::move(completionHandler)] mutable {
            if (completionHandler)
                completionHandler(false);
        });
    });
}
#endif

} // namespace WebKit

#endif
