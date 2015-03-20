/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "ParentalControlsContentFilter.h"

#import "ResourceResponse.h"
#import "SoftLinking.h"
#import "WebFilterEvaluatorSPI.h"
#import <objc/runtime.h>

SOFT_LINK_PRIVATE_FRAMEWORK(WebContentAnalysis);
SOFT_LINK_CLASS(WebContentAnalysis, WebFilterEvaluator);

namespace WebCore {

bool ParentalControlsContentFilter::canHandleResponse(const ResourceResponse& response)
{
    if (!response.url().protocolIsInHTTPFamily())
        return false;

    if ([getWebFilterEvaluatorClass() isManagedSession]) {
#if PLATFORM(MAC)
        if (response.url().protocolIs("https"))
#endif
            return true;
    }

    return false;
}

std::unique_ptr<ParentalControlsContentFilter> ParentalControlsContentFilter::create(const ResourceResponse& response)
{
    return std::make_unique<ParentalControlsContentFilter>(response);
}

ParentalControlsContentFilter::ParentalControlsContentFilter(const ResourceResponse& response)
    : m_webFilterEvaluator { adoptNS([allocWebFilterEvaluatorInstance() initWithResponse:response.nsURLResponse()]) }
{
    ASSERT([getWebFilterEvaluatorClass() isManagedSession]);
}

void ParentalControlsContentFilter::addData(const char* data, int length)
{
    ASSERT(![m_replacementData.get() length]);
    m_replacementData = [m_webFilterEvaluator addData:[NSData dataWithBytesNoCopy:(void*)data length:length freeWhenDone:NO]];
    ASSERT(needsMoreData() || [m_replacementData.get() length]);
}

void ParentalControlsContentFilter::finishedAddingData()
{
    ASSERT(![m_replacementData.get() length]);
    m_replacementData = [m_webFilterEvaluator dataComplete];
}

bool ParentalControlsContentFilter::needsMoreData() const
{
    return [m_webFilterEvaluator filterState] == kWFEStateBuffering;
}

bool ParentalControlsContentFilter::didBlockData() const
{
    return [m_webFilterEvaluator wasBlocked];
}

const char* ParentalControlsContentFilter::getReplacementData(int& length) const
{
    length = [m_replacementData length];
    return static_cast<const char*>([m_replacementData bytes]);
}

ContentFilterUnblockHandler ParentalControlsContentFilter::unblockHandler() const
{
#if PLATFORM(IOS)
    return ContentFilterUnblockHandler { "unblock", m_webFilterEvaluator };
#else
    return { };
#endif
}

} // namespace WebCore
