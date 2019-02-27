/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ResourceLoadStatisticsClassifier.h"

#include "Logging.h"
#include <WebCore/ResourceLoadStatistics.h>

namespace WebKit {
using namespace WebCore;

static double vectorLength(unsigned a, unsigned b, unsigned c)
{
    return sqrt(a * a + b * b + c * c);
}

static const auto featureVectorLengthThresholdHigh = 3;
static const auto featureVectorLengthThresholdVeryHigh = 30;
ResourceLoadPrevalence ResourceLoadStatisticsClassifier::calculateResourcePrevalence(const ResourceLoadStatistics& resourceStatistic, ResourceLoadPrevalence currentPrevalence)
{
    ASSERT(currentPrevalence != VeryHigh);

    auto subresourceUnderTopFrameDomainsCount = resourceStatistic.subresourceUnderTopFrameDomains.size();
    auto subresourceUniqueRedirectsToCount = resourceStatistic.subresourceUniqueRedirectsTo.size();
    auto subframeUnderTopFrameDomainsCount = resourceStatistic.subframeUnderTopFrameDomains.size();
    auto topFrameUniqueRedirectsToCount = resourceStatistic.topFrameUniqueRedirectsTo.size();
    
    if (!subresourceUnderTopFrameDomainsCount
        && !subresourceUniqueRedirectsToCount
        && !subframeUnderTopFrameDomainsCount
        && !topFrameUniqueRedirectsToCount) {
        return Low;
    }

    if (vectorLength(subresourceUnderTopFrameDomainsCount, subresourceUniqueRedirectsToCount, subframeUnderTopFrameDomainsCount) > featureVectorLengthThresholdVeryHigh)
        return VeryHigh;

    if (currentPrevalence == High
        || subresourceUnderTopFrameDomainsCount > featureVectorLengthThresholdHigh
        || subresourceUniqueRedirectsToCount > featureVectorLengthThresholdHigh
        || subframeUnderTopFrameDomainsCount > featureVectorLengthThresholdHigh
        || topFrameUniqueRedirectsToCount > featureVectorLengthThresholdHigh
        || classify(subresourceUnderTopFrameDomainsCount, subresourceUniqueRedirectsToCount, subframeUnderTopFrameDomainsCount))
        return High;

    return Low;
}

bool ResourceLoadStatisticsClassifier::classifyWithVectorThreshold(unsigned a, unsigned b, unsigned c)
{
    LOG(ResourceLoadStatistics, "ResourceLoadStatisticsClassifier::classifyWithVectorThreshold(): Classified with threshold.");
    return vectorLength(a, b, c) > featureVectorLengthThresholdHigh;
}
    
}
