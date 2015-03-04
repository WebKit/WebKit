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

#import "ResourceRequest.h"
#import "SoftLinking.h"
#import "WebFilterEvaluatorSPI.h"
#import <objc/runtime.h>

SOFT_LINK_PRIVATE_FRAMEWORK(WebContentAnalysis);
SOFT_LINK_CLASS(WebContentAnalysis, WebFilterEvaluator);

namespace WebCore {

static NSString * const platformContentFilterKey = @"platformContentFilter";

void ContentFilterUnblockHandler::encode(NSKeyedArchiver *archiver) const
{
    if ([getWebFilterEvaluatorClass() conformsToProtocol:@protocol(NSSecureCoding)])
        [archiver encodeObject:m_webFilterEvaluator.get() forKey:platformContentFilterKey];
}

bool ContentFilterUnblockHandler::decode(NSKeyedUnarchiver *unarchiver, ContentFilterUnblockHandler& unblockHandler)
{
    @try {
        if ([getWebFilterEvaluatorClass() conformsToProtocol:@protocol(NSSecureCoding)])
            unblockHandler.m_webFilterEvaluator = (WebFilterEvaluator *)[unarchiver decodeObjectOfClass:getWebFilterEvaluatorClass() forKey:platformContentFilterKey];
        return true;
    } @catch (NSException *exception) {
        LOG_ERROR("The platform content filter being decoded is not a WebFilterEvaluator.");
    }
    
    return false;
}

#if PLATFORM(IOS)
static inline const char* scheme()
{
    return "x-apple-content-filter";
}

bool ContentFilterUnblockHandler::handleUnblockRequestAndDispatchIfSuccessful(const ResourceRequest& request, std::function<void()> function)
{
    if (!m_webFilterEvaluator)
        return false;

    if (!request.url().protocolIs(scheme()))
        return false;

    if (!equalIgnoringCase(request.url().host(), "unblock"))
        return false;

    [m_webFilterEvaluator unblockWithCompletion:^(BOOL unblocked, NSError *) {
        if (unblocked)
            function();
    }];
    return true;
}
#endif

} // namespace WebCore

#endif // PLATFORM(IOS) && ENABLE(CONTENT_FILTERING)
