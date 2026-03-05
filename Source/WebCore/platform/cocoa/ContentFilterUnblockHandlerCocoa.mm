/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#import "ContentFilterUnblockHandler.h"

#if ENABLE(CONTENT_FILTERING)

#import "ContentFilter.h"
#import "Logging.h"
#import <WebCore/ResourceRequest.h>
#import <pal/spi/cocoa/NSKeyedUnarchiverSPI.h>
#import <pal/spi/cocoa/WebFilterEvaluatorSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/CString.h>

#if PLATFORM(IOS_FAMILY)
#import "WebCoreThreadRun.h"
#endif

#if HAVE(WEBCONTENTRESTRICTIONS)
#import <WebCore/ParentalControlsURLFilter.h>
#import <wtf/CompletionHandler.h>
#endif

namespace WebCore {

ContentFilterUnblockHandler::ContentFilterUnblockHandler(String unblockURLHost, UnblockRequesterFunction&& unblockRequester)
    : m_unblockURLHost { WTF::move(unblockURLHost) }
    , m_unblockRequester { WTF::move(unblockRequester) }
{
    LOG(ContentFiltering, "Creating ContentFilterUnblockHandler with an unblock requester and unblock URL host <%s>.\n", m_unblockURLHost.ascii().data());
}

#if HAVE(WEBCONTENTRESTRICTIONS)
ContentFilterUnblockHandler::ContentFilterUnblockHandler(const URL& evaluatedURL)
    : m_evaluatedURL(evaluatedURL)
{
}
#elif HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
ContentFilterUnblockHandler::ContentFilterUnblockHandler(String unblockURLHost, RetainPtr<WebFilterEvaluator> evaluator)
    : m_unblockURLHost { WTF::move(unblockURLHost) }
    , m_webFilterEvaluator { WTF::move(evaluator) }
{
    LOG(ContentFiltering, "Creating ContentFilterUnblockHandler with a WebFilterEvaluator and unblock URL host <%s>.\n", m_unblockURLHost.ascii().data());
}
#endif

ContentFilterUnblockHandler::ContentFilterUnblockHandler(
    String&& unblockURLHost,
    URL&& unreachableURL,
#if HAVE(WEBCONTENTRESTRICTIONS)
    std::optional<URL>&& evaluatedURL,
#elif HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    RetainPtr<WebFilterEvaluator>&& webFilterEvaluator,
#endif
    bool unblockedAfterRequest
)
    : m_unblockURLHost(WTF::move(unblockURLHost))
    , m_unreachableURL(WTF::move(unreachableURL))
#if HAVE(WEBCONTENTRESTRICTIONS)
    , m_evaluatedURL(WTF::move(evaluatedURL))
#elif HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    , m_webFilterEvaluator(WTF::move(webFilterEvaluator))
#endif
    , m_unblockedAfterRequest(unblockedAfterRequest)
{
}

void ContentFilterUnblockHandler::wrapWithDecisionHandler(const DecisionHandlerFunction& decisionHandler)
{
    ContentFilterUnblockHandler wrapped { *this };
    UnblockRequesterFunction wrappedRequester { [wrapped, decisionHandler](DecisionHandlerFunction wrappedDecisionHandler) mutable {
        wrapped.requestUnblockAsync([wrappedDecisionHandler, decisionHandler](bool unblocked) {
            wrappedDecisionHandler(unblocked);
            decisionHandler(unblocked);
        });
    }};
#if HAVE(WEBCONTENTRESTRICTIONS)
    m_evaluatedURL = { };
#elif HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    m_webFilterEvaluator = nullptr;
#endif
    std::swap(m_unblockRequester, wrappedRequester);
}

bool ContentFilterUnblockHandler::needsUIProcess() const
{
#if HAVE(WEBCONTENTRESTRICTIONS)
    return !!m_evaluatedURL;
#elif HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    return !!m_webFilterEvaluator;
#else
    return false;
#endif
}

#if HAVE(WEBCONTENTRESTRICTIONS)
bool ContentFilterUnblockHandler::needsNetworkProcess() const
{
#if PLATFORM(MAC)
    return !!m_evaluatedURL;
#else
    return false;
#endif
}
#endif

bool ContentFilterUnblockHandler::canHandleRequest(const ResourceRequest& request) const
{
    if (!m_unblockRequester && !m_unblockedAfterRequest) {
        bool hasEvaluatedURL = false;
        bool hasWebFilterEvaluator = false;
#if HAVE(WEBCONTENTRESTRICTIONS)
        hasEvaluatedURL = !!m_evaluatedURL;
#elif HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
        hasWebFilterEvaluator = !!m_webFilterEvaluator;
#endif
        if (!hasEvaluatedURL && !hasWebFilterEvaluator)
            return false;
    }

#if HAVE(WEBCONTENTRESTRICTIONS)
    if (!!m_evaluatedURL)
        return ContentFilter::isWebContentRestrictionsUnblockURL(request.url());
#endif

    bool isUnblockRequest = request.url().protocolIs(ContentFilter::urlScheme()) && equalIgnoringASCIICase(request.url().host(), m_unblockURLHost);
#if !LOG_DISABLED
    if (isUnblockRequest)
        LOG(ContentFiltering, "ContentFilterUnblockHandler will handle <%s> as an unblock request.\n", request.url().string().ascii().data());
#endif
    return isUnblockRequest;
}

void ContentFilterUnblockHandler::requestUnblockAsync(DecisionHandlerFunction&& decisionHandler, std::optional<URL> requestURL)
{
    // FIXME: Remove once all platforms use the same flow to request unblocking content, rdar://170455406
    UNUSED_PARAM(requestURL);
#if HAVE(WEBCONTENTRESTRICTIONS)
    if (m_evaluatedURL) {
#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
        Ref filter = WebCore::ParentalControlsURLFilter::filterWithConfigurationPath(configurationPath());
#else
        Ref filter = WebCore::ParentalControlsURLFilter::singleton();
#endif
#if HAVE(WEBCONTENTRESTRICTIONS_ASK_TO)
        if (requestURL) {
            filter->requestPermissionForURL(*m_evaluatedURL, *requestURL, [decisionHandler = WTF::move(decisionHandler)](bool didAllow) mutable {
                callOnMainThread([decisionHandler = WTF::move(decisionHandler), didAllow]() {
                    decisionHandler(didAllow);
                });
            });
            return;
        }
#endif
        filter->allowURL(*m_evaluatedURL, [decisionHandler = WTF::move(decisionHandler)](bool didAllow) mutable {
            callOnMainThread([decisionHandler = WTF::move(decisionHandler), didAllow]() {
                decisionHandler(didAllow);
            });
        });
        return;
    }
#elif HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    if (RetainPtr evaluator = webFilterEvaluator()) {
        [evaluator unblockWithCompletion:[decisionHandler = WTF::move(decisionHandler)](BOOL unblocked, NSError *) mutable {
            callOnMainThread([decisionHandler = WTF::move(decisionHandler), unblocked] {
                LOG(ContentFiltering, "WebFilterEvaluator %s the unblock request.\n", unblocked ? "allowed" : "did not allow");
                decisionHandler(unblocked);
            });
        }];
        return;
    }
#endif
    auto unblockRequester = m_unblockRequester;
    if (!unblockRequester && m_unblockedAfterRequest) {
        unblockRequester = [unblocked = m_unblockedAfterRequest](ContentFilterUnblockHandler::DecisionHandlerFunction function) {
            function(unblocked);
        };
    }
    if (unblockRequester) {
        unblockRequester([decisionHandler = WTF::move(decisionHandler)](bool unblocked) mutable {
            callOnMainThread([decisionHandler = WTF::move(decisionHandler), unblocked] {
                decisionHandler(unblocked);
            });
        });
    } else {
        callOnMainThread([decisionHandler = WTF::move(decisionHandler)] {
            auto unblocked = false;
            decisionHandler(unblocked);
        });
    }
}

void ContentFilterUnblockHandler::setUnblockedAfterRequest(bool unblocked)
{
    m_unblockedAfterRequest = unblocked;
}

} // namespace WebCore

#endif // ENABLE(CONTENT_FILTERING)
