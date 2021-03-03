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
#include "Test.h"

#include <WebCore/PrivateClickMeasurement.h>
#include <wtf/URL.h>
#include <wtf/WallTime.h>

using namespace WebCore;

namespace TestWebKitAPI {

constexpr uint32_t min6BitValue { 0 };

const URL webKitURL { { }, "https://webkit.org"_s };
const URL exampleURL { { }, "https://example.com"_s };
const URL emptyURL { };

// Positive test cases.

TEST(PrivateClickMeasurement, ValidMinValues)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(min6BitValue), PrivateClickMeasurement::SourceSite { webKitURL }, PrivateClickMeasurement::AttributeOnSite { exampleURL } };
    attribution.attributeAndGetEarliestTimeToSend(PrivateClickMeasurement::AttributionTriggerData(min6BitValue, PrivateClickMeasurement::Priority(min6BitValue)));

    auto attributionURL = attribution.attributionReportURL();
    
    ASSERT_EQ(attributionURL.string(), "https://webkit.org/.well-known/private-click-measurement/report-attribution/");

    ASSERT_EQ(attribution.attributionReportJSON()->toJSONString(), "{\"source_engagement_type\":\"click\",\"source_site\":\"webkit.org\",\"source_id\":0,\"attributed_on_site\":\"example.com\",\"trigger_data\":0,\"version\":2}");
}

TEST(PrivateClickMeasurement, ValidMidValues)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID((uint32_t)192), PrivateClickMeasurement::SourceSite { webKitURL }, PrivateClickMeasurement::AttributeOnSite { exampleURL } };
    attribution.attributeAndGetEarliestTimeToSend(PrivateClickMeasurement::AttributionTriggerData((uint32_t)9, PrivateClickMeasurement::Priority((uint32_t)22)));

    auto attributionURL = attribution.attributionReportURL();
    
    ASSERT_EQ(attributionURL.string(), "https://webkit.org/.well-known/private-click-measurement/report-attribution/");

    ASSERT_EQ(attribution.attributionReportJSON()->toJSONString(), "{\"source_engagement_type\":\"click\",\"source_site\":\"webkit.org\",\"source_id\":192,\"attributed_on_site\":\"example.com\",\"trigger_data\":9,\"version\":2}");
}

TEST(PrivateClickMeasurement, ValidMaxValues)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(PrivateClickMeasurement::SourceID::MaxEntropy), PrivateClickMeasurement::SourceSite { webKitURL }, PrivateClickMeasurement::AttributeOnSite { exampleURL } };
    attribution.attributeAndGetEarliestTimeToSend(PrivateClickMeasurement::AttributionTriggerData(PrivateClickMeasurement::AttributionTriggerData::MaxEntropy, PrivateClickMeasurement::Priority(PrivateClickMeasurement::Priority::MaxEntropy)));

    auto attributionURL = attribution.attributionReportURL();
    
    ASSERT_EQ(attributionURL.string(), "https://webkit.org/.well-known/private-click-measurement/report-attribution/");

    ASSERT_EQ(attribution.attributionReportJSON()->toJSONString(), "{\"source_engagement_type\":\"click\",\"source_site\":\"webkit.org\",\"source_id\":255,\"attributed_on_site\":\"example.com\",\"trigger_data\":15,\"version\":2}");
}

TEST(PrivateClickMeasurement, EarliestTimeToSendAttributionMinimumDelay)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(PrivateClickMeasurement::SourceID::MaxEntropy), PrivateClickMeasurement::SourceSite { webKitURL }, PrivateClickMeasurement::AttributeOnSite { exampleURL } };
    auto now = WallTime::now();
    attribution.attributeAndGetEarliestTimeToSend(PrivateClickMeasurement::AttributionTriggerData(PrivateClickMeasurement::AttributionTriggerData::MaxEntropy, PrivateClickMeasurement::Priority(PrivateClickMeasurement::Priority::MaxEntropy)));
    auto earliestTimeToSend = attribution.earliestTimeToSend();
    ASSERT_TRUE(earliestTimeToSend);
    ASSERT_TRUE(earliestTimeToSend.value().secondsSinceEpoch() - 24_h >= now.secondsSinceEpoch());
}

TEST(PrivateClickMeasurement, ValidConversionURLs)
{
    const URL conversionURLWithoutPriority { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/10"_s };
    auto optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithoutPriority);
    ASSERT_TRUE(optionalConversion);
    ASSERT_EQ(optionalConversion->data, (uint32_t)10);

    const URL conversionURLWithoutPriorityMaxEntropy { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/15"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithoutPriorityMaxEntropy);
    ASSERT_TRUE(optionalConversion);
    ASSERT_EQ(optionalConversion->data, (uint32_t)15);
    
    const URL conversionURLWithoutPriorityAndLeadingZero { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/02"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithoutPriorityAndLeadingZero);
    ASSERT_TRUE(optionalConversion);
    ASSERT_EQ(optionalConversion->data, (uint32_t)2);

    const URL conversionURLWithPriority { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/10/12"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithPriority);
    ASSERT_TRUE(optionalConversion);
    ASSERT_EQ(optionalConversion->data, (uint32_t)10);
    ASSERT_EQ(optionalConversion->priority, (uint32_t)12);

    const URL conversionURLWithPriorityMaxEntropy { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/15/63"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithPriorityMaxEntropy);
    ASSERT_TRUE(optionalConversion);
    ASSERT_EQ(optionalConversion->data, (uint32_t)15);
    ASSERT_EQ(optionalConversion->priority, (uint32_t)63);
    
    const URL conversionURLWithPriorityAndLeadingZero { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/10/02"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithPriorityAndLeadingZero);
    ASSERT_TRUE(optionalConversion);
    ASSERT_EQ(optionalConversion->data, (uint32_t)10);
    ASSERT_EQ(optionalConversion->priority, (uint32_t)2);
}

TEST(PrivateClickMeasurement, ValidSourceNonce)
{
    auto ephemeralNonce = PrivateClickMeasurement::EphemeralSourceNonce { "ABCDEFabcdef0123456789"_s };
    ASSERT_TRUE(ephemeralNonce.isValid());
}

// Negative test cases.

TEST(PrivateClickMeasurement, InvalidSourceID)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(PrivateClickMeasurement::SourceID::MaxEntropy + 1), PrivateClickMeasurement::SourceSite { webKitURL }, PrivateClickMeasurement::AttributeOnSite { exampleURL } };
    attribution.attributeAndGetEarliestTimeToSend(PrivateClickMeasurement::AttributionTriggerData(PrivateClickMeasurement::AttributionTriggerData::MaxEntropy, PrivateClickMeasurement::Priority(PrivateClickMeasurement::Priority::MaxEntropy)));

    ASSERT_TRUE(attribution.attributionReportURL().isEmpty());
}

TEST(PrivateClickMeasurement, InvalidSourceHost)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(PrivateClickMeasurement::SourceID::MaxEntropy), PrivateClickMeasurement::SourceSite { emptyURL }, PrivateClickMeasurement::AttributeOnSite { exampleURL } };
    attribution.attributeAndGetEarliestTimeToSend(PrivateClickMeasurement::AttributionTriggerData(PrivateClickMeasurement::AttributionTriggerData::MaxEntropy, PrivateClickMeasurement::Priority(PrivateClickMeasurement::Priority::MaxEntropy)));

    ASSERT_TRUE(attribution.attributionReportURL().isEmpty());
}

TEST(PrivateClickMeasurement, InvalidDestinationHost)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(PrivateClickMeasurement::SourceID::MaxEntropy + 1), PrivateClickMeasurement::SourceSite { webKitURL }, PrivateClickMeasurement::AttributeOnSite { emptyURL } };
    attribution.attributeAndGetEarliestTimeToSend(PrivateClickMeasurement::AttributionTriggerData(PrivateClickMeasurement::AttributionTriggerData::MaxEntropy, PrivateClickMeasurement::Priority(PrivateClickMeasurement::Priority::MaxEntropy)));

    ASSERT_TRUE(attribution.attributionReportURL().isEmpty());
}

TEST(PrivateClickMeasurement, AttributionTriggerData)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(PrivateClickMeasurement::SourceID::MaxEntropy), PrivateClickMeasurement::SourceSite { webKitURL }, PrivateClickMeasurement::AttributeOnSite { exampleURL } };
    attribution.attributeAndGetEarliestTimeToSend(PrivateClickMeasurement::AttributionTriggerData((PrivateClickMeasurement::AttributionTriggerData::MaxEntropy + 1), PrivateClickMeasurement::Priority(PrivateClickMeasurement::Priority::MaxEntropy)));

    ASSERT_TRUE(attribution.attributionReportURL().isEmpty());
}

TEST(PrivateClickMeasurement, InvalidPriority)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(PrivateClickMeasurement::SourceID::MaxEntropy), PrivateClickMeasurement::SourceSite { webKitURL }, PrivateClickMeasurement::AttributeOnSite { exampleURL } };
    attribution.attributeAndGetEarliestTimeToSend(PrivateClickMeasurement::AttributionTriggerData(PrivateClickMeasurement::AttributionTriggerData::MaxEntropy, PrivateClickMeasurement::Priority(PrivateClickMeasurement::Priority::MaxEntropy + 1)));

    ASSERT_TRUE(attribution.attributionReportURL().isEmpty());
}

TEST(PrivateClickMeasurement, InvalidMissingConversion)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(PrivateClickMeasurement::SourceID::MaxEntropy), PrivateClickMeasurement::SourceSite { webKitURL }, PrivateClickMeasurement::AttributeOnSite { exampleURL } };

    ASSERT_TRUE(attribution.attributionReportURL().isEmpty());
    ASSERT_FALSE(attribution.earliestTimeToSend());
}

TEST(PrivateClickMeasurement, InvalidConversionURLs)
{
    const URL conversionURLWithSingleDigitConversionData { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/2"_s };
    auto optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithSingleDigitConversionData);
    ASSERT_FALSE(optionalConversion);
    
    const URL conversionURLWithNonNumeralConversionData { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/2s"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithNonNumeralConversionData);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithNegativeConversionData { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/-2"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithNegativeConversionData);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithTooLargeConversionData { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/16"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithTooLargeConversionData);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithSingleDigitPriority { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/10/2"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithSingleDigitPriority);
    ASSERT_FALSE(optionalConversion);
    
    const URL conversionURLWithNonNumeralPriority { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/10/2s"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithNonNumeralPriority);
    ASSERT_FALSE(optionalConversion);
    
    const URL conversionURLWithNegativePriority { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/10/-2"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithNegativePriority);
    ASSERT_FALSE(optionalConversion);
    
    const URL conversionURLWithTooLargePriority { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/10/64"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithTooLargePriority);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithTooLargeConversionDataAndPriority { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/16/22"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithTooLargeConversionDataAndPriority);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithTooLargeConversionDataAndTooLargePriority { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/16/64"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithTooLargeConversionDataAndTooLargePriority);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithExtraLeadingSlash = { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution//10/12"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithExtraLeadingSlash);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithExtraTrailingSlash = { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/10/12/"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithExtraTrailingSlash);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithTrailingQuestionMark = { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/10/12?"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithTrailingQuestionMark);
    ASSERT_FALSE(optionalConversion);
}

TEST(PrivateClickMeasurement, InvalidConversionWithDisallowedURLComponents)
{
    // Protocol.
    const URL conversionURLWithHttpProtocol { { }, "http://webkit.org/.well-known/private-click-measurement/trigger-attribution/2"_s };
    auto optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithHttpProtocol);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithWssProtocol { { }, "wss://webkit.org/.well-known/private-click-measurement/trigger-attribution/2"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithWssProtocol);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithFileProtocol { { }, "file:///.well-known/private-click-measurement/trigger-attribution/2"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithFileProtocol);
    ASSERT_FALSE(optionalConversion);

    // Username and password.
    const URL conversionURLWithUserName { { }, "https://user@webkit.org/.well-known/private-click-measurement/trigger-attribution/2"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithUserName);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithPassword = { { }, "https://:pwd@webkit.org/.well-known/private-click-measurement/trigger-attribution/10/12"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithPassword);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithUsernameAndPassword = { { }, "https://user:pwd@webkit.org/.well-known/private-click-measurement/trigger-attribution/10/12"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithUsernameAndPassword);
    ASSERT_FALSE(optionalConversion);

    // Query string.
    const URL conversionURLWithTrailingQuestionMark = { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/10/12?"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithTrailingQuestionMark);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithQueryString = { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/10/12?extra=data"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithQueryString);
    ASSERT_FALSE(optionalConversion);
    
    // Fragment.
    const URL conversionURLWithTrailingHash = { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/10/12#"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithTrailingHash);
    ASSERT_FALSE(optionalConversion);

    const URL conversionURLWithFragment = { { }, "https://webkit.org/.well-known/private-click-measurement/trigger-attribution/10/12#fragment"_s };
    optionalConversion = PrivateClickMeasurement::parseAttributionRequest(conversionURLWithFragment);
    ASSERT_FALSE(optionalConversion);
}

TEST(PrivateClickMeasurement, InvalidSourceNonce)
{
    // Fewer than the requried number of bytes.
    auto ephemeralNonce = PrivateClickMeasurement::EphemeralSourceNonce { "ABCDabcd0123456789"_s };
    ASSERT_FALSE(ephemeralNonce.isValid());
    // More than the requried number of bytes.
    ephemeralNonce = PrivateClickMeasurement::EphemeralSourceNonce { "ABCDEFGHIabcdefghi0123456789"_s };
    ASSERT_FALSE(ephemeralNonce.isValid());
    // Illegal, ASCII character '/'.
    ephemeralNonce = PrivateClickMeasurement::EphemeralSourceNonce { "ABCDEFabcde/0123456789"_s };
    ASSERT_FALSE(ephemeralNonce.isValid());
    // Illegal, non-ASCII character 'å'.
    ephemeralNonce = PrivateClickMeasurement::EphemeralSourceNonce { "ABCDEFabcdeå0123456789" };
    ASSERT_FALSE(ephemeralNonce.isValid());
    // Empty string.
    ephemeralNonce = PrivateClickMeasurement::EphemeralSourceNonce { StringImpl::empty() };
    ASSERT_FALSE(ephemeralNonce.isValid());
}

#if HAVE(RSA_BSSA)
TEST(PrivateClickMeasurement, InvalidBlindedSecret)
{
    const char serverPublicKeyBase64URL[] = "MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAzb1dThrtYwVh46SjInegKhAqpbJwm1XnTBCvybSK8zk53R0Am1hG33AVF5J1lqYf36wp663GasclHtqzvxFZIvDA1DUSH4aZz_fDHCTTxEeJVPORS3zNN2UjWwbtnwsh4BmDTi-z_cDn0LAz2JuZyKlyFt5GgVLAQvL9H3VLHU9_XHNK-uboyXfcHRTtrDnpu3c6wvX5dd-AJoLmIQTZBEJfVkxBGznk1qKHjc6nASAirKF_wJCnuwAK8C6BAcjNcwUWCeKp0YECzCXU--JXd2OEU-QhxPC67faiDOh3V0vlfqZLtrlbnanUCKrvhw7GaGOGYotIrnZtuNfxC14d_XNVd1FS8nHjRTHnEgw_jnlSssfgStz0uJtcmkfgoJBvOE4mIRpi7iSlRfXNkKsWX1J-gwcnCVo5u0uJEW6X6NyvEGYJ8w5BPfwsQuK9y-4Z7ikt9IOucEHY7ThDmi9TNNhHBVj0Gu4wGoSjq3a6vL5N10ZSHXoq1XgfGPrmHhhL90cjvWonoyOXsUqlXEzTjD2W9897Q-Mx9BUNrGQPqmIx8F5MwxWcOrye8WRp4Q88n2YSUnV7C8ayld3v1Fh7N5jeSqeVmtDVRYTn2sVfNqgXrzgdigJcQR8vFENu6nzFPwsrXPMaCiLUnZNUmQ1ZSLQeQyhYXxHqRJrnuCDWXLkCAwEAAQ";

    PrivateClickMeasurement pcm;
    auto sourceSecretToken = pcm.tokenSignatureJSON();
    EXPECT_EQ(sourceSecretToken->asObject()->size(), 0ul);

    auto ephemeralNonce = PrivateClickMeasurement::EphemeralSourceNonce { "ABCDEFabcdef0123456789"_s };
    EXPECT_TRUE(ephemeralNonce.isValid());
    pcm.setEphemeralSourceNonce(WTFMove(ephemeralNonce));

    EXPECT_TRUE(pcm.calculateAndUpdateSourceSecretToken(serverPublicKeyBase64URL));
    sourceSecretToken = pcm.tokenSignatureJSON();
    EXPECT_EQ(sourceSecretToken->asObject()->size(), 4ul);

    EXPECT_FALSE(pcm.calculateAndUpdateSourceUnlinkableToken(emptyString()));
}
#endif

} // namespace TestWebKitAPI
