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
#import "ResourceRequest.h"
#import <pal/spi/cocoa/WebFilterEvaluatorSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/CString.h>

#if PLATFORM(IOS_FAMILY)
#import "WebCoreThreadRun.h"
#endif

static NSString * const unblockURLHostKey { @"unblockURLHost" };
static NSString * const unreachableURLKey { @"unreachableURL" };
#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
static NSString * const unblockedAfterRequestKey { @"unblockedAfterRequest" };
#endif

#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
static NSString * const webFilterEvaluatorKey { @"webFilterEvaluator" };

SOFT_LINK_PRIVATE_FRAMEWORK(WebContentAnalysis);
SOFT_LINK_CLASS(WebContentAnalysis, WebFilterEvaluator);
#endif

namespace WebCore {

ContentFilterUnblockHandler::ContentFilterUnblockHandler(String unblockURLHost, UnblockRequesterFunction unblockRequester)
    : m_unblockURLHost { WTFMove(unblockURLHost) }
    , m_unblockRequester { WTFMove(unblockRequester) }
{
    LOG(ContentFiltering, "Creating ContentFilterUnblockHandler with an unblock requester and unblock URL host <%s>.\n", m_unblockURLHost.ascii().data());
}

#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
ContentFilterUnblockHandler::ContentFilterUnblockHandler(String unblockURLHost, RetainPtr<WebFilterEvaluator> evaluator)
    : m_unblockURLHost { WTFMove(unblockURLHost) }
    , m_webFilterEvaluator { WTFMove(evaluator) }
{
    LOG(ContentFiltering, "Creating ContentFilterUnblockHandler with a WebFilterEvaluator and unblock URL host <%s>.\n", m_unblockURLHost.ascii().data());
}
#endif

void ContentFilterUnblockHandler::wrapWithDecisionHandler(const DecisionHandlerFunction& decisionHandler)
{
    ContentFilterUnblockHandler wrapped { *this };
    UnblockRequesterFunction wrappedRequester { [wrapped, decisionHandler](DecisionHandlerFunction wrappedDecisionHandler) {
        wrapped.requestUnblockAsync([wrappedDecisionHandler, decisionHandler](bool unblocked) {
            wrappedDecisionHandler(unblocked);
            decisionHandler(unblocked);
        });
    }};
#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    m_webFilterEvaluator = nullptr;
#endif
    std::swap(m_unblockRequester, wrappedRequester);
}

bool ContentFilterUnblockHandler::needsUIProcess() const
{
#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    return m_webFilterEvaluator;
#else
    return false;
#endif
}

void ContentFilterUnblockHandler::encode(NSCoder *coder) const
{
    ASSERT_ARG(coder, coder.allowsKeyedCoding && coder.requiresSecureCoding);
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [coder encodeObject:m_unblockURLHost forKey:unblockURLHostKey];
    [coder encodeObject:(NSURL *)m_unreachableURL forKey:unreachableURLKey];
#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    [coder encodeObject:m_webFilterEvaluator.get() forKey:webFilterEvaluatorKey];
#endif
#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    [coder encodeObject:m_unblockedAfterRequest forKey:unblockedAfterRequestKey];
#endif

    END_BLOCK_OBJC_EXCEPTIONS
}

bool ContentFilterUnblockHandler::decode(NSCoder *coder, ContentFilterUnblockHandler& unblockHandler)
{
    ASSERT_ARG(coder, coder.allowsKeyedCoding && coder.requiresSecureCoding);
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    unblockHandler.m_unblockURLHost = [coder decodeObjectOfClass:[NSString class] forKey:unblockURLHostKey];
    unblockHandler.m_unreachableURL = [coder decodeObjectOfClass:[NSURL class] forKey:unreachableURLKey];
#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    unblockHandler.m_webFilterEvaluator = [coder decodeObjectOfClass:getWebFilterEvaluatorClass() forKey:webFilterEvaluatorKey];
#endif
#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    unblockHandler.m_unblockedAfterRequest = [coder decodeObjectOfClass:[NSNumber class] forKey:unblockedAfterRequestKey];
#endif
    return true;
    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

bool ContentFilterUnblockHandler::canHandleRequest(const ResourceRequest& request) const
{
    if (!m_unblockRequester
#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
        && !m_unblockedAfterRequest
#endif
        ) {
#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
        if (!m_webFilterEvaluator)
            return false;
#else
        return false;
#endif
    }

    bool isUnblockRequest = request.url().protocolIs(ContentFilter::urlScheme()) && equalIgnoringASCIICase(request.url().host(), m_unblockURLHost);
#if !LOG_DISABLED
    if (isUnblockRequest)
        LOG(ContentFiltering, "ContentFilterUnblockHandler will handle <%s> as an unblock request.\n", request.url().string().ascii().data());
#endif
    return isUnblockRequest;
}

void ContentFilterUnblockHandler::requestUnblockAsync(DecisionHandlerFunction decisionHandler) const
{
#if HAVE(PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER)
    if (m_webFilterEvaluator) {
        [m_webFilterEvaluator unblockWithCompletion:[decisionHandler](BOOL unblocked, NSError *) {
            callOnMainThread([decisionHandler, unblocked] {
                LOG(ContentFiltering, "WebFilterEvaluator %s the unblock request.\n", unblocked ? "allowed" : "did not allow");
                decisionHandler(unblocked);
            });
        }];
        return;
    }
#endif
    auto unblockRequester = m_unblockRequester;
#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    if (!unblockRequester && m_unblockedAfterRequest) {
        unblockRequester = [unblocked = m_unblockedAfterRequest.boolValue](ContentFilterUnblockHandler::DecisionHandlerFunction function) {
            function(unblocked);
        };
    }
#endif
    if (unblockRequester) {
        unblockRequester([decisionHandler](bool unblocked) {
            callOnMainThread([decisionHandler, unblocked] {
                decisionHandler(unblocked);
            });
        });
    } else {
        callOnMainThread([decisionHandler] {
            auto unblocked = false;
            decisionHandler(unblocked);
        });
    }
}

#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
void ContentFilterUnblockHandler::setUnblockedAfterRequest(bool unblocked)
{
    m_unblockedAfterRequest = [NSNumber numberWithBool:unblocked];
}
#endif

} // namespace WebCore

#endif // ENABLE(CONTENT_FILTERING)
