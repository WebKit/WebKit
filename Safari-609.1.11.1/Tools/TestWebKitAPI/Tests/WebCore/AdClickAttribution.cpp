/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include <WebCore/AdClickAttribution.h>
#include <wtf/URL.h>
#include <wtf/WallTime.h>

using namespace WebCore;

namespace TestWebKitAPI {

constexpr uint32_t min6BitValue { 0 };

const URL webKitURL { { }, "https://webkit.org"_s };
const URL exampleURL { { }, "https://example.com"_s };
const URL emptyURL { };

// Positive test cases.

TEST(AdClickAttribution, ValidMinValues)
{
    AdClickAttribution attribution { AdClickAttribution::Campaign(min6BitValue), AdClickAttribution::Source { webKitURL }, AdClickAttribution::Destination { exampleURL } };
    attribution.convertAndGetEarliestTimeToSend(AdClickAttribution::Conversion(min6BitValue, AdClickAttribution::Priority(min6BitValue)));

    auto attributionURL = attribution.url();
    auto referrerURL = attribution.referrer();
    
    ASSERT_EQ(attributionURL.string(), "https://webkit.org/.well-known/ad-click-attribution/0/0");
    ASSERT_EQ(referrerURL.string(), "https://example.com/");
}

TEST(AdClickAttribution, ValidMidValues)
{
    AdClickAttribution attribution { AdClickAttribution::Campaign((uint32_t)12), AdClickAttribution::Source { webKitURL }, AdClickAttribution::Destination { exampleURL } };
    attribution.convertAndGetEarliestTimeToSend(AdClickAttribution::Conversion((uint32_t)44, AdClickAttribution::Priority((uint32_t)22)));

    auto attributionURL = attribution.url();
    auto referrerURL = attribution.referrer();

    ASSERT_EQ(attributionURL.string(), "https://webkit.org/.well-known/ad-click-attribution/44/12");
    ASSERT_EQ(referrerURL.string(), "https://example.com/");
}

TEST(AdClickAttribution, ValidMaxValues)
{
    AdClickAttribution attribution { AdClickAttribution::Campaign(AdClickAttribution::MaxEntropy), AdClickAttribution::Source { webKitURL }, AdClickAttribution::Destination { exampleURL } };
    attribution.convertAndGetEarliestTimeToSend(AdClickAttribution::Conversion(AdClickAttribution::MaxEntropy, AdClickAttribution::Priority(AdClickAttribution::MaxEntropy)));

    auto attributionURL = attribution.url();
    auto referrerURL = attribution.referrer();

    ASSERT_EQ(attributionURL.string(), "https://webkit.org/.well-known/ad-click-attribution/63/63");
    ASSERT_EQ(referrerURL.string(), "https://example.com/");
}

TEST(AdClickAttribution, EarliestTimeToSendAttributionMinimumDelay)
{
    AdClickAttribution attribution { AdClickAttribution::Campaign(AdClickAttribution::MaxEntropy), AdClickAttribution::Source { webKitURL }, AdClickAttribution::Destination { exampleURL } };
    auto now = WallTime::now();
    attribution.convertAndGetEarliestTimeToSend(AdClickAttribution::Conversion(AdClickAttribution::MaxEntropy, AdClickAttribution::Priority(AdClickAttribution::MaxEntropy)));
    auto earliestTimeToSend = attribution.earliestTimeToSend();
    ASSERT_TRUE(earliestTimeToSend);
    ASSERT_TRUE(earliestTimeToSend.value().secondsSinceEpoch() - 24_h >= now.secondsSinceEpoch());
}

TEST(AdClickAttribution, ValidConversionURLs)
{
    const URL conversionURLWithoutPriority { { }, "https://webkit.org/.well-known/ad-click-attribution/22"_s };
    auto optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithoutPriority);
    ASSERT_TRUE(optionalConversion);
    ASSERT_EQ(optionalConversion->data, (uint32_t)22);

    const URL conversionURLWithoutPriorityMaxEntropy { { }, "https://webkit.org/.well-known/ad-click-attribution/63"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithoutPriorityMaxEntropy);
    ASSERT_TRUE(optionalConversion);
    ASSERT_EQ(optionalConversion->data, (uint32_t)63);
    
    const URL conversionURLWithoutPriorityAndLeadingZero { { }, "https://webkit.org/.well-known/ad-click-attribution/02"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithoutPriorityAndLeadingZero);
    ASSERT_TRUE(optionalConversion);
    ASSERT_EQ(optionalConversion->data, (uint32_t)2);

    const URL conversionURLWithPriority { { }, "https://webkit.org/.well-known/ad-click-attribution/22/12"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithPriority);
    ASSERT_TRUE(optionalConversion);
    ASSERT_EQ(optionalConversion->data, (uint32_t)22);
    ASSERT_EQ(optionalConversion->priority, (uint32_t)12);

    const URL conversionURLWithPriorityMaxEntropy { { }, "https://webkit.org/.well-known/ad-click-attribution/63/63"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithPriorityMaxEntropy);
    ASSERT_TRUE(optionalConversion);
    ASSERT_EQ(optionalConversion->data, (uint32_t)63);
    ASSERT_EQ(optionalConversion->priority, (uint32_t)63);
    
    const URL conversionURLWithPriorityAndLeadingZero { { }, "https://webkit.org/.well-known/ad-click-attribution/22/02"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithPriorityAndLeadingZero);
    ASSERT_TRUE(optionalConversion);
    ASSERT_EQ(optionalConversion->data, (uint32_t)22);
    ASSERT_EQ(optionalConversion->priority, (uint32_t)2);
}

// Negative test cases.

TEST(AdClickAttribution, InvalidCampaignId)
{
    AdClickAttribution attribution { AdClickAttribution::Campaign(AdClickAttribution::MaxEntropy + 1), AdClickAttribution::Source { webKitURL }, AdClickAttribution::Destination { exampleURL } };
    attribution.convertAndGetEarliestTimeToSend(AdClickAttribution::Conversion(AdClickAttribution::MaxEntropy, AdClickAttribution::Priority(AdClickAttribution::MaxEntropy)));

    auto attributionURL = attribution.url();
    auto referrerURL = attribution.referrer();

    ASSERT_TRUE(attributionURL.string().isEmpty());
    ASSERT_TRUE(referrerURL.string().isEmpty());
}

TEST(AdClickAttribution, InvalidSourceHost)
{
    AdClickAttribution attribution { AdClickAttribution::Campaign(AdClickAttribution::MaxEntropy), AdClickAttribution::Source { emptyURL }, AdClickAttribution::Destination { exampleURL } };
    attribution.convertAndGetEarliestTimeToSend(AdClickAttribution::Conversion(AdClickAttribution::MaxEntropy, AdClickAttribution::Priority(AdClickAttribution::MaxEntropy)));

    auto attributionURL = attribution.url();
    auto referrerURL = attribution.referrer();

    ASSERT_TRUE(attributionURL.string().isEmpty());
    ASSERT_TRUE(referrerURL.string().isEmpty());
}

TEST(AdClickAttribution, InvalidDestinationHost)
{
    AdClickAttribution attribution { AdClickAttribution::Campaign(AdClickAttribution::MaxEntropy + 1), AdClickAttribution::Source { webKitURL }, AdClickAttribution::Destination { emptyURL } };
    attribution.convertAndGetEarliestTimeToSend(AdClickAttribution::Conversion(AdClickAttribution::MaxEntropy, AdClickAttribution::Priority(AdClickAttribution::MaxEntropy)));

    auto attributionURL = attribution.url();
    auto referrerURL = attribution.referrer();

    ASSERT_TRUE(attributionURL.string().isEmpty());
    ASSERT_TRUE(referrerURL.string().isEmpty());
}

TEST(AdClickAttribution, InvalidConversionData)
{
    AdClickAttribution attribution { AdClickAttribution::Campaign(AdClickAttribution::MaxEntropy), AdClickAttribution::Source { webKitURL }, AdClickAttribution::Destination { exampleURL } };
    attribution.convertAndGetEarliestTimeToSend(AdClickAttribution::Conversion((AdClickAttribution::MaxEntropy + 1), AdClickAttribution::Priority(AdClickAttribution::MaxEntropy)));

    auto attributionURL = attribution.url();
    auto referrerURL = attribution.referrer();

    ASSERT_TRUE(attributionURL.string().isEmpty());
    ASSERT_TRUE(referrerURL.string().isEmpty());
}

TEST(AdClickAttribution, InvalidPriority)
{
    AdClickAttribution attribution { AdClickAttribution::Campaign(AdClickAttribution::MaxEntropy), AdClickAttribution::Source { webKitURL }, AdClickAttribution::Destination { exampleURL } };
    attribution.convertAndGetEarliestTimeToSend(AdClickAttribution::Conversion(AdClickAttribution::MaxEntropy, AdClickAttribution::Priority(AdClickAttribution::MaxEntropy + 1)));

    auto attributionURL = attribution.url();
    auto referrerURL = attribution.referrer();

    ASSERT_TRUE(attributionURL.string().isEmpty());
    ASSERT_TRUE(referrerURL.string().isEmpty());
}

TEST(AdClickAttribution, InvalidMissingConversion)
{
    AdClickAttribution attribution { AdClickAttribution::Campaign(AdClickAttribution::MaxEntropy), AdClickAttribution::Source { webKitURL }, AdClickAttribution::Destination { exampleURL } };

    auto attributionURL = attribution.url();
    auto referrerURL = attribution.referrer();

    ASSERT_TRUE(attributionURL.string().isEmpty());
    ASSERT_TRUE(referrerURL.string().isEmpty());
    ASSERT_FALSE(attribution.earliestTimeToSend());
}

TEST(AdClickAttribution, InvalidConversionURLs)
{
    const URL conversionURLWithSingleDigitConversionData { { }, "https://webkit.org/.well-known/ad-click-attribution/2"_s };
    auto optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithSingleDigitConversionData);
    ASSERT_FALSE(optionalConversion);
    
    const URL conversionURLWithNonNumeralConversionData { { }, "https://webkit.org/.well-known/ad-click-attribution/2s"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithNonNumeralConversionData);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithNegativeConversionData { { }, "https://webkit.org/.well-known/ad-click-attribution/-2"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithNegativeConversionData);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithTooLargeConversionData { { }, "https://webkit.org/.well-known/ad-click-attribution/64"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithTooLargeConversionData);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithSingleDigitPriority { { }, "https://webkit.org/.well-known/ad-click-attribution/22/2"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithSingleDigitPriority);
    ASSERT_FALSE(optionalConversion);
    
    const URL conversionURLWithNonNumeralPriority { { }, "https://webkit.org/.well-known/ad-click-attribution/22/2s"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithNonNumeralPriority);
    ASSERT_FALSE(optionalConversion);
    
    const URL conversionURLWithNegativePriority { { }, "https://webkit.org/.well-known/ad-click-attribution/22/-2"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithNegativePriority);
    ASSERT_FALSE(optionalConversion);
    
    const URL conversionURLWithTooLargePriority { { }, "https://webkit.org/.well-known/ad-click-attribution/22/64"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithTooLargePriority);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithTooLargeConversionDataAndPriority { { }, "https://webkit.org/.well-known/ad-click-attribution/64/22"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithTooLargeConversionDataAndPriority);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithTooLargeConversionDataAndTooLargePriority { { }, "https://webkit.org/.well-known/ad-click-attribution/64/64"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithTooLargeConversionDataAndTooLargePriority);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithExtraLeadingSlash = { { }, "https://webkit.org/.well-known/ad-click-attribution//22/12"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithExtraLeadingSlash);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithExtraTrailingSlash = { { }, "https://webkit.org/.well-known/ad-click-attribution/22/12/"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithExtraTrailingSlash);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithTrailingQuestionMark = { { }, "https://webkit.org/.well-known/ad-click-attribution/22/12?"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithTrailingQuestionMark);
    ASSERT_FALSE(optionalConversion);
}

TEST(AdClickAttribution, InvalidConversionWithDisallowedURLComponents)
{
    // Protocol.
    const URL conversionURLWithHttpProtocol { { }, "http://webkit.org/.well-known/ad-click-attribution/2"_s };
    auto optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithHttpProtocol);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithWssProtocol { { }, "wss://webkit.org/.well-known/ad-click-attribution/2"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithWssProtocol);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithFileProtocol { { }, "file:///.well-known/ad-click-attribution/2"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithFileProtocol);
    ASSERT_FALSE(optionalConversion);

    // Username and password.
    const URL conversionURLWithUserName { { }, "https://user@webkit.org/.well-known/ad-click-attribution/2"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithUserName);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithPassword = { { }, "https://:pwd@webkit.org/.well-known/ad-click-attribution/22/12?"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithPassword);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithUsernameAndPassword = { { }, "https://user:pwd@webkit.org/.well-known/ad-click-attribution/22/12?"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithUsernameAndPassword);
    ASSERT_FALSE(optionalConversion);

    // Query string.
    const URL conversionURLWithTrailingQuestionMark = { { }, "https://webkit.org/.well-known/ad-click-attribution/22/12?"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithTrailingQuestionMark);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithQueryString = { { }, "https://webkit.org/.well-known/ad-click-attribution/22/12?extra=data"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithQueryString);
    ASSERT_FALSE(optionalConversion);
    
    // Fragment.
    const URL conversionURLWithTrailingHash = { { }, "https://webkit.org/.well-known/ad-click-attribution/22/12#"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithTrailingHash);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithFragment = { { }, "https://webkit.org/.well-known/ad-click-attribution/22/12#fragment"_s };
    optionalConversion = AdClickAttribution::parseConversionRequest(conversionURLWithFragment);
    ASSERT_FALSE(optionalConversion);
}

} // namespace TestWebKitAPI
