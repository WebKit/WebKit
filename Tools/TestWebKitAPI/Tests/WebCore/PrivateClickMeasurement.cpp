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

const URL webKitURL { "https://webkit.org"_s };
const URL exampleURL { "https://example.com"_s };
const URL emptyURL { };

// Positive test cases.

TEST(PrivateClickMeasurement, WellKnownURLs)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(min6BitValue), PCM::SourceSite { webKitURL }, PCM::AttributionDestinationSite { exampleURL }, "test.bundle.identifier"_s, WallTime::now(), PCM::AttributionEphemeral::No };
    attribution.attributeAndGetEarliestTimeToSend(WebCore::PCM::AttributionTriggerData { min6BitValue, WebCore::PCM::AttributionTriggerData::Priority::PriorityValue(min6BitValue), { }, { }, { }, { }, { }, { } }, WebCore::PrivateClickMeasurement::IsRunningLayoutTest::No);

    auto attributionSourceURL = attribution.attributionReportClickSourceURL();
    ASSERT_EQ(attributionSourceURL.string(), "https://webkit.org/.well-known/private-click-measurement/report-attribution/"_s);
    auto attributionAttributeOnURL = attribution.attributionReportClickDestinationURL();
    ASSERT_EQ(attributionAttributeOnURL.string(), "https://example.com/.well-known/private-click-measurement/report-attribution/"_s);
}

TEST(PrivateClickMeasurement, ValidMinValues)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(min6BitValue), PCM::SourceSite { webKitURL }, PCM::AttributionDestinationSite { exampleURL }, "test.bundle.identifier"_s, WallTime::now(), PCM::AttributionEphemeral::No };
    attribution.attributeAndGetEarliestTimeToSend(WebCore::PCM::AttributionTriggerData { min6BitValue, WebCore::PCM::AttributionTriggerData::Priority::PriorityValue(min6BitValue), { }, { }, { }, { }, { }, { } }, WebCore::PrivateClickMeasurement::IsRunningLayoutTest::No);

    ASSERT_EQ(attribution.attributionReportJSON()->toJSONString(), "{\"source_engagement_type\":\"click\",\"source_site\":\"webkit.org\",\"source_id\":0,\"attributed_on_site\":\"example.com\",\"trigger_data\":0,\"version\":3}"_s);
}

TEST(PrivateClickMeasurement, ValidMidValues)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID((uint32_t)192), PCM::SourceSite { webKitURL }, PCM::AttributionDestinationSite { exampleURL }, "test.bundle.identifier"_s, WallTime::now(), PCM::AttributionEphemeral::No };
    attribution.attributeAndGetEarliestTimeToSend(WebCore::PCM::AttributionTriggerData { (uint32_t)9, WebCore::PCM::AttributionTriggerData::Priority::PriorityValue((uint32_t)22), { }, { }, { }, { }, { }, { } }, WebCore::PrivateClickMeasurement::IsRunningLayoutTest::No);

    ASSERT_EQ(attribution.attributionReportJSON()->toJSONString(), "{\"source_engagement_type\":\"click\",\"source_site\":\"webkit.org\",\"source_id\":192,\"attributed_on_site\":\"example.com\",\"trigger_data\":9,\"version\":3}"_s);
}

TEST(PrivateClickMeasurement, ValidMaxValues)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(std::numeric_limits<uint8_t>::max()), PCM::SourceSite { webKitURL }, PCM::AttributionDestinationSite { exampleURL }, "test.bundle.identifier"_s, WallTime::now(), PCM::AttributionEphemeral::No };
    attribution.attributeAndGetEarliestTimeToSend(WebCore::PCM::AttributionTriggerData { WebCore::PCM::AttributionTriggerData::MaxEntropy, WebCore::PCM::AttributionTriggerData::Priority::PriorityValue(WebCore::PCM::AttributionTriggerData::Priority::MaxEntropy), { }, { }, { }, { }, { }, { } }, WebCore::PrivateClickMeasurement::IsRunningLayoutTest::No);

    ASSERT_EQ(attribution.attributionReportJSON()->toJSONString(), "{\"source_engagement_type\":\"click\",\"source_site\":\"webkit.org\",\"source_id\":255,\"attributed_on_site\":\"example.com\",\"trigger_data\":15,\"version\":3}"_s);
}

TEST(PrivateClickMeasurement, EarliestTimeToSendAttributionMinimumDelay)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(std::numeric_limits<uint8_t>::max()), PCM::SourceSite { webKitURL }, PCM::AttributionDestinationSite { exampleURL }, "test.bundle.identifier"_s, WallTime::now(), PCM::AttributionEphemeral::No };
    auto now = WallTime::now();
    attribution.attributeAndGetEarliestTimeToSend(WebCore::PCM::AttributionTriggerData { WebCore::PCM::AttributionTriggerData::MaxEntropy, WebCore::PCM::AttributionTriggerData::Priority::PriorityValue(WebCore::PCM::AttributionTriggerData::Priority::MaxEntropy), { }, { }, { }, { }, { }, { } }, WebCore::PrivateClickMeasurement::IsRunningLayoutTest::No);
    auto earliestTimeToSend = attribution.timesToSend();
    ASSERT_TRUE(earliestTimeToSend.sourceEarliestTimeToSend && earliestTimeToSend.destinationEarliestTimeToSend);
    ASSERT_TRUE(earliestTimeToSend.sourceEarliestTimeToSend.value().secondsSinceEpoch() - 24_h >= now.secondsSinceEpoch());
    ASSERT_TRUE(earliestTimeToSend.destinationEarliestTimeToSend.value().secondsSinceEpoch() - 24_h >= now.secondsSinceEpoch());
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
    auto ephemeralNonce = PCM::EphemeralNonce { "ABCDEFabcdef0123456789"_s };
    ASSERT_TRUE(ephemeralNonce.isValid());
}

// Negative test cases.

TEST(PrivateClickMeasurement, InvalidSourceHost)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(std::numeric_limits<uint8_t>::max()), PCM::SourceSite { emptyURL }, PCM::AttributionDestinationSite { exampleURL }, "test.bundle.identifier"_s, WallTime::now(), PCM::AttributionEphemeral::No };
    attribution.attributeAndGetEarliestTimeToSend(WebCore::PCM::AttributionTriggerData { WebCore::PCM::AttributionTriggerData::MaxEntropy, WebCore::PCM::AttributionTriggerData::Priority::PriorityValue(WebCore::PCM::AttributionTriggerData::Priority::MaxEntropy), { }, { }, { }, { }, { }, { } }, WebCore::PrivateClickMeasurement::IsRunningLayoutTest::No);

    ASSERT_TRUE(attribution.attributionReportClickSourceURL().isEmpty());
    ASSERT_TRUE(attribution.attributionReportClickDestinationURL().isEmpty());
}

TEST(PrivateClickMeasurement, InvalidDestinationHost)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(std::numeric_limits<uint8_t>::max()), PCM::SourceSite { webKitURL }, PCM::AttributionDestinationSite { emptyURL }, "test.bundle.identifier"_s, WallTime::now(), PCM::AttributionEphemeral::No };
    attribution.attributeAndGetEarliestTimeToSend(WebCore::PCM::AttributionTriggerData { WebCore::PCM::AttributionTriggerData::MaxEntropy, WebCore::PCM::AttributionTriggerData::Priority::PriorityValue(WebCore::PCM::AttributionTriggerData::Priority::MaxEntropy), { }, { }, { }, { }, { }, { } }, WebCore::PrivateClickMeasurement::IsRunningLayoutTest::No);

    ASSERT_TRUE(attribution.attributionReportClickSourceURL().isEmpty());
    ASSERT_TRUE(attribution.attributionReportClickDestinationURL().isEmpty());
}

TEST(PrivateClickMeasurement, AttributionTriggerData)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(std::numeric_limits<uint8_t>::max()), PCM::SourceSite { webKitURL }, PCM::AttributionDestinationSite { exampleURL }, "test.bundle.identifier"_s, WallTime::now(), PCM::AttributionEphemeral::No };
    attribution.attributeAndGetEarliestTimeToSend(WebCore::PCM::AttributionTriggerData { (WebCore::PCM::AttributionTriggerData::MaxEntropy + 1), WebCore::PCM::AttributionTriggerData::Priority::PriorityValue(WebCore::PCM::AttributionTriggerData::Priority::MaxEntropy), { }, { }, { }, { }, { }, { } }, WebCore::PrivateClickMeasurement::IsRunningLayoutTest::No);

    ASSERT_TRUE(attribution.attributionReportClickSourceURL().isEmpty());
    ASSERT_TRUE(attribution.attributionReportClickDestinationURL().isEmpty());
}

TEST(PrivateClickMeasurement, InvalidPriority)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(std::numeric_limits<uint8_t>::max()), PCM::SourceSite { webKitURL }, PCM::AttributionDestinationSite { exampleURL }, "test.bundle.identifier"_s, WallTime::now(), PCM::AttributionEphemeral::No };
    attribution.attributeAndGetEarliestTimeToSend(WebCore::PCM::AttributionTriggerData { WebCore::PCM::AttributionTriggerData::MaxEntropy, WebCore::PCM::AttributionTriggerData::Priority::PriorityValue(WebCore::PCM::AttributionTriggerData::Priority::MaxEntropy + 1), { }, { }, { }, { }, { }, { } }, WebCore::PrivateClickMeasurement::IsRunningLayoutTest::No);

    ASSERT_TRUE(attribution.attributionReportClickSourceURL().isEmpty());
    ASSERT_TRUE(attribution.attributionReportClickDestinationURL().isEmpty());
}

TEST(PrivateClickMeasurement, InvalidMissingConversion)
{
    PrivateClickMeasurement attribution { PrivateClickMeasurement::SourceID(std::numeric_limits<uint8_t>::max()), PCM::SourceSite { webKitURL }, PCM::AttributionDestinationSite { exampleURL }, "test.bundle.identifier"_s, WallTime::now(), PCM::AttributionEphemeral::No };

    ASSERT_TRUE(attribution.attributionReportClickSourceURL().isEmpty());
    ASSERT_TRUE(attribution.attributionReportClickDestinationURL().isEmpty());
    ASSERT_FALSE(attribution.timesToSend().sourceEarliestTimeToSend && attribution.timesToSend().destinationEarliestTimeToSend);
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
    auto ephemeralNonce = PCM::EphemeralNonce { "ABCDabcd0123456789"_s };
    ASSERT_FALSE(ephemeralNonce.isValid());
    // More than the requried number of bytes.
    ephemeralNonce = PCM::EphemeralNonce { "ABCDEFGHIabcdefghi0123456789"_s };
    ASSERT_FALSE(ephemeralNonce.isValid());
    // Illegal, ASCII character '/'.
    ephemeralNonce = PCM::EphemeralNonce { "ABCDEFabcde/0123456789"_s };
    ASSERT_FALSE(ephemeralNonce.isValid());
    // Illegal, non-ASCII character 'å'.
    ephemeralNonce = PCM::EphemeralNonce { String::fromLatin1("ABCDEFabcdeå0123456789") };
    ASSERT_FALSE(ephemeralNonce.isValid());
    // Empty string.
    ephemeralNonce = PCM::EphemeralNonce { StringImpl::empty() };
    ASSERT_FALSE(ephemeralNonce.isValid());
}

#if HAVE(RSA_BSSA)
TEST(PrivateClickMeasurement, InvalidBlindedSecret)
{
#if HAVE(RSA_PSS_OID)
    constexpr auto serverPublicKeyBase64URL = "MIICUjA9BgkqhkiG9w0BAQowMKANMAsGCWCGSAFlAwQCAqEaMBgGCSqGSIb3DQEBCDALBglghkgBZQMEAgKiAwIBMAOCAg8AMIICCgKCAgEAoGhU5Mgsbb51ZbJVPHSgf8c93TJdtkxeKfyxQ5fCpwE2Fe9xJ7tByExdGKj4XO-HFi7npmtEPzR4cRXdsAL7YcH5UXNbVhXmVcbFCBXks-Ih-jqLwfNac0wPLG5K1Zzhf1gZ--JBzVjw87zvqpWrzzxviuV__0sn_u7f01E1OdaD83110fhfiXp_Ex62Q2uhcek0hqbqEvyKlLVBOjlJFJc2FLyw-l8-9xd7GcX1ZRyPx4lITvYG7KIbSMrFTfuQNOyJf4DlO97qq08R6Utl249AnBfLe3ZDbWBnl0fDOwkJgBmbaa7EnRlQ3p6Ir2SY1hNTnzW-p2ceytIMYwTMSES7-j21oeTUC-OmcC_5g05AgxROzUJPZdyY33m4Q7lqkHkLAYtdN2TVCP79MuswS-fJJQOD_dDCqq_hk0MySLCbnUGe5lyFBoO5vBMH5k38LjSQuN6jfP7quYA6cOONzmn842eLT61tIjRoX2czeUJrSmx89SfY8WnFE2fhk9G52cXp6L2Vzr5IV7rOws3ZPw-RnjKnZZaejs0bKGOXC1-jl-u4A5ip55ohlUjm7lvDtKFAeJ7gajJBtiNnq3s3m_IMkv7ztCQpv0pBxst6MmvNOO0jOQvYkzQbGooI1_qjeDup0BYY67xxyNRaA9V4CKEJ7j_hznrAmjSiz0LSqTkCAwEAAQ=="_s;
#else
    constexpr auto serverPublicKeyBase64URL = "MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAzb1dThrtYwVh46SjInegKhAqpbJwm1XnTBCvybSK8zk53R0Am1hG33AVF5J1lqYf36wp663GasclHtqzvxFZIvDA1DUSH4aZz_fDHCTTxEeJVPORS3zNN2UjWwbtnwsh4BmDTi-z_cDn0LAz2JuZyKlyFt5GgVLAQvL9H3VLHU9_XHNK-uboyXfcHRTtrDnpu3c6wvX5dd-AJoLmIQTZBEJfVkxBGznk1qKHjc6nASAirKF_wJCnuwAK8C6BAcjNcwUWCeKp0YECzCXU--JXd2OEU-QhxPC67faiDOh3V0vlfqZLtrlbnanUCKrvhw7GaGOGYotIrnZtuNfxC14d_XNVd1FS8nHjRTHnEgw_jnlSssfgStz0uJtcmkfgoJBvOE4mIRpi7iSlRfXNkKsWX1J-gwcnCVo5u0uJEW6X6NyvEGYJ8w5BPfwsQuK9y-4Z7ikt9IOucEHY7ThDmi9TNNhHBVj0Gu4wGoSjq3a6vL5N10ZSHXoq1XgfGPrmHhhL90cjvWonoyOXsUqlXEzTjD2W9897Q-Mx9BUNrGQPqmIx8F5MwxWcOrye8WRp4Q88n2YSUnV7C8ayld3v1Fh7N5jeSqeVmtDVRYTn2sVfNqgXrzgdigJcQR8vFENu6nzFPwsrXPMaCiLUnZNUmQ1ZSLQeQyhYXxHqRJrnuCDWXLkCAwEAAQ"_s;
#endif

    WebCore::PrivateClickMeasurement pcm(
        WebCore::PrivateClickMeasurement::SourceID(0),
        PCM::SourceSite(URL()),
        PCM::AttributionDestinationSite(URL()),
        { },
        WallTime::now(),
        PCM::AttributionEphemeral::No
    );
    auto sourceUnlinkableToken = pcm.tokenSignatureJSON();
    EXPECT_EQ(sourceUnlinkableToken->asObject()->size(), 0ul);

    auto ephemeralNonce = PCM::EphemeralNonce { "ABCDEFabcdef0123456789"_s };
    EXPECT_TRUE(ephemeralNonce.isValid());
    pcm.setEphemeralSourceNonce(WTFMove(ephemeralNonce));

    auto errorMessage = pcm.calculateAndUpdateSourceUnlinkableToken(serverPublicKeyBase64URL);
    EXPECT_FALSE(errorMessage);
    sourceUnlinkableToken = pcm.tokenSignatureJSON();
    EXPECT_EQ(sourceUnlinkableToken->asObject()->size(), 4ul);

    errorMessage = pcm.calculateAndUpdateSourceSecretToken(emptyString());
    EXPECT_TRUE(errorMessage);
}
#endif

} // namespace TestWebKitAPI
