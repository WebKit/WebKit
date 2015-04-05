/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import "BlockExceptions.h"
#import "ContentFilter.h"
#import "ResourceRequest.h"

#if PLATFORM(IOS)
#import "SoftLinking.h"
#import "WebCoreThreadRun.h"
#import "WebFilterEvaluatorSPI.h"

SOFT_LINK_PRIVATE_FRAMEWORK(WebContentAnalysis);
SOFT_LINK_CLASS(WebContentAnalysis, WebFilterEvaluator);

static NSString * const webFilterEvaluatorKey { @"webFilterEvaluator" };
#endif

static NSString * const unblockURLHostKey { @"unblockURLHost" };
static NSString * const unreachableURLKey { @"unreachableURL" };

namespace WebCore {

ContentFilterUnblockHandler::ContentFilterUnblockHandler(String unblockURLHost, UnblockRequesterFunction unblockRequester)
    : m_unblockURLHost { WTF::move(unblockURLHost) }
    , m_unblockRequester { WTF::move(unblockRequester) }
{
}

#if PLATFORM(IOS)
ContentFilterUnblockHandler::ContentFilterUnblockHandler(String unblockURLHost, RetainPtr<WebFilterEvaluator> evaluator)
    : m_unblockURLHost { WTF::move(unblockURLHost) }
    , m_webFilterEvaluator { WTF::move(evaluator) }
{
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
#if PLATFORM(IOS)
    m_webFilterEvaluator = nullptr;
#endif
    std::swap(m_unblockRequester, wrappedRequester);
}

bool ContentFilterUnblockHandler::needsUIProcess() const
{
#if PLATFORM(IOS)
    return m_webFilterEvaluator;
#else
    return false;
#endif
}

void ContentFilterUnblockHandler::encode(NSCoder *coder) const
{
    ASSERT_ARG(coder, coder.allowsKeyedCoding && coder.requiresSecureCoding);
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [coder encodeObject:m_unblockURLHost forKey:unblockURLHostKey];
    [coder encodeObject:(NSURL *)m_unreachableURL forKey:unreachableURLKey];
#if PLATFORM(IOS)
    [coder encodeObject:m_webFilterEvaluator.get() forKey:webFilterEvaluatorKey];
#endif
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool ContentFilterUnblockHandler::decode(NSCoder *coder, ContentFilterUnblockHandler& unblockHandler)
{
    ASSERT_ARG(coder, coder.allowsKeyedCoding && coder.requiresSecureCoding);
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    unblockHandler.m_unblockURLHost = [coder decodeObjectOfClass:[NSString class] forKey:unblockURLHostKey];
    unblockHandler.m_unreachableURL = [coder decodeObjectOfClass:[NSURL class] forKey:unreachableURLKey];
#if PLATFORM(IOS)
    unblockHandler.m_webFilterEvaluator = [coder decodeObjectOfClass:getWebFilterEvaluatorClass() forKey:webFilterEvaluatorKey];
#endif
    return true;
    END_BLOCK_OBJC_EXCEPTIONS;
    return false;
}

bool ContentFilterUnblockHandler::canHandleRequest(const ResourceRequest& request) const
{
    if (!m_unblockRequester) {
#if PLATFORM(IOS)
        if (!m_webFilterEvaluator)
            return false;
#else
        return false;
#endif
    }

    return request.url().protocolIs(ContentFilter::urlScheme()) && equalIgnoringCase(request.url().host(), m_unblockURLHost);
}

static inline void dispatchToMainThread(void (^block)())
{
    dispatch_async(dispatch_get_main_queue(), ^{
#if PLATFORM(IOS)
        WebThreadRun(block);
#else
        block();
#endif
    });
}

void ContentFilterUnblockHandler::requestUnblockAsync(DecisionHandlerFunction decisionHandler) const
{
#if PLATFORM(IOS)
    if (m_webFilterEvaluator) {
        [m_webFilterEvaluator unblockWithCompletion:[decisionHandler](BOOL unblocked, NSError *) {
            dispatchToMainThread([decisionHandler, unblocked] {
                decisionHandler(unblocked);
            });
        }];
        return;
    }
#endif

    if (m_unblockRequester) {
        m_unblockRequester([decisionHandler](bool unblocked) {
            dispatchToMainThread([decisionHandler, unblocked] {
                decisionHandler(unblocked);
            });
        });
    }
}

} // namespace WebCore

#endif // PLATFORM(IOS) && ENABLE(CONTENT_FILTERING)
