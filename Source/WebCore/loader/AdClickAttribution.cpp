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
#include "AdClickAttribution.h"

#include "Logging.h"
#include "RuntimeEnabledFeatures.h"
#include <wtf/Expected.h>
#include <wtf/RandomNumber.h>
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>

namespace WebCore {

static const char adClickAttributionPathPrefix[] = "/.well-known/ad-click-attribution/";
const size_t adClickConversionDataPathSegmentSize = 2;
const size_t adClickPriorityPathSegmentSize = 2;
const Seconds maxAge { 24_h * 7 };

bool AdClickAttribution::isValid() const
{
    return m_conversion
        && m_conversion.value().isValid()
        && m_campaign.isValid()
        && !m_source.registrableDomain.isEmpty()
        && !m_destination.registrableDomain.isEmpty()
        && m_earliestTimeToSend;
}

Expected<AdClickAttribution::Conversion, String> AdClickAttribution::parseConversionRequest(const URL& redirectURL)
{
    if (!redirectURL.protocolIs("https") || redirectURL.hasCredentials() || redirectURL.hasQuery() || redirectURL.hasFragmentIdentifier()) {
        if (UNLIKELY(debugModeEnabled())) {
            RELEASE_LOG_INFO(AdClickAttribution, "Conversion was not accepted because the URL's protocol is not HTTPS or the URL contains one or more of username, password, query string, and fragment.");
            return makeUnexpected("[Ad Click Attribution] Conversion was not accepted because the URL's protocol is not HTTPS or the URL contains one or more of username, password, query string, and fragment."_s);
        }
        return makeUnexpected(nullString());
    }

    auto path = StringView(redirectURL.string()).substring(redirectURL.pathStart(), redirectURL.pathEnd() - redirectURL.pathStart());
    if (path.isEmpty() || !path.startsWith(adClickAttributionPathPrefix)) {
        if (UNLIKELY(debugModeEnabled())) {
            RELEASE_LOG_INFO(AdClickAttribution, "Conversion was not accepted because the URL path did not start with %" PUBLIC_LOG_STRING ".", adClickAttributionPathPrefix);
            return makeUnexpected(makeString("[Ad Click Attribution] Conversion was not accepted because the URL path did not start with "_s, adClickAttributionPathPrefix, "."_s));
        }
        return makeUnexpected(nullString());
    }

    auto prefixLength = sizeof(adClickAttributionPathPrefix) - 1;
    if (path.length() == prefixLength + adClickConversionDataPathSegmentSize) {
        auto conversionDataUInt64 = path.substring(prefixLength, adClickConversionDataPathSegmentSize).toUInt64Strict();
        if (!conversionDataUInt64 || *conversionDataUInt64 > MaxEntropy) {
            if (UNLIKELY(debugModeEnabled())) {
                RELEASE_LOG_INFO(AdClickAttribution, "Conversion was not accepted because the conversion data could not be parsed or was higher than the allowed maximum of %{public}u.", MaxEntropy);
                return makeUnexpected(makeString("[Ad Click Attribution] Conversion was not accepted because the conversion data could not be parsed or was higher than the allowed maximum of "_s, MaxEntropy, "."_s));
            }
            return makeUnexpected(nullString());
        }

        return Conversion { static_cast<uint32_t>(*conversionDataUInt64), Priority { 0 } };
    }
    
    if (path.length() == prefixLength + adClickConversionDataPathSegmentSize + 1 + adClickPriorityPathSegmentSize) {
        auto conversionDataUInt64 = path.substring(prefixLength, adClickConversionDataPathSegmentSize).toUInt64Strict();
        if (!conversionDataUInt64 || *conversionDataUInt64 > MaxEntropy) {
            if (UNLIKELY(debugModeEnabled())) {
                RELEASE_LOG_INFO(AdClickAttribution, "Conversion was not accepted because the conversion data could not be parsed or was higher than the allowed maximum of %{public}u.", MaxEntropy);
                return makeUnexpected(makeString("[Ad Click Attribution] Conversion was not accepted because the conversion data could not be parsed or was higher than the allowed maximum of "_s, MaxEntropy, "."_s));
            }
            return makeUnexpected(nullString());
        }

        auto conversionPriorityUInt64 = path.substring(prefixLength + adClickConversionDataPathSegmentSize + 1, adClickPriorityPathSegmentSize).toUInt64Strict();
        if (!conversionPriorityUInt64 || *conversionPriorityUInt64 > MaxEntropy) {
            if (UNLIKELY(debugModeEnabled())) {
                RELEASE_LOG_INFO(AdClickAttribution, "Conversion was not accepted because the priority could not be parsed or was higher than the allowed maximum of %{public}u.", MaxEntropy);
                return makeUnexpected(makeString("[Ad Click Attribution] Conversion was not accepted because the priority could not be parsed or was higher than the allowed maximum of "_s, MaxEntropy, "."_s));
            }
            return makeUnexpected(nullString());
        }

        return Conversion { static_cast<uint32_t>(*conversionDataUInt64), Priority { static_cast<uint32_t>(*conversionPriorityUInt64) } };
    }

    if (UNLIKELY(debugModeEnabled())) {
        RELEASE_LOG_INFO(AdClickAttribution, "Conversion was not accepted because the URL path contained unrecognized parts.");
        return makeUnexpected("[Ad Click Attribution] Conversion was not accepted because the URL path contained unrecognized parts."_s);
    }
    return makeUnexpected(nullString());
}

Optional<Seconds> AdClickAttribution::convertAndGetEarliestTimeToSend(Conversion&& conversion)
{
    if (!conversion.isValid() || (m_conversion && m_conversion->priority >= conversion.priority))
        return { };

    m_conversion = WTFMove(conversion);
    // 24-48 hour delay before sending. This helps privacy since the conversion and the attribution
    // requests are detached and the time of the attribution does not reveal the time of the conversion.
    auto seconds = 24_h + Seconds(randomNumber() * (24_h).value());
    m_earliestTimeToSend = WallTime::now() + seconds;
    return seconds;
}

void AdClickAttribution::markAsExpired()
{
    m_timeOfAdClick = { };
}

bool AdClickAttribution::hasExpired() const
{
    return WallTime::now() > m_timeOfAdClick + maxAge;
}

bool AdClickAttribution::hasHigherPriorityThan(const AdClickAttribution& other) const
{
    if (!other.m_conversion)
        return true;
    
    if (!m_conversion)
        return false;

    return m_conversion->priority > other.m_conversion->priority;
}

URL AdClickAttribution::url() const
{
    if (!isValid())
        return URL();

    StringBuilder builder;
    builder.appendLiteral("https://");
    builder.append(m_source.registrableDomain.string());
    builder.appendLiteral(adClickAttributionPathPrefix);
    builder.appendNumber(m_conversion.value().data);
    builder.append('/');
    builder.appendNumber(m_campaign.id);

    URL url { URL(), builder.toString() };
    if (url.isValid())
        return url;

    return URL();
}

URL AdClickAttribution::referrer() const
{
    if (!isValid())
        return URL();

    StringBuilder builder;
    builder.appendLiteral("https://");
    builder.append(m_destination.registrableDomain.string());
    builder.append('/');

    URL url { URL(), builder.toString() };
    if (url.isValid())
        return url;
    
    return URL();
}

URL AdClickAttribution::urlForTesting(const URL& baseURL) const
{
    auto host = m_source.registrableDomain.string();
    if (host != "localhost" && host != "127.0.0.1")
        return URL();
    String relativeURL;
    if (!baseURL.hasQuery())
        relativeURL = makeString("?conversion=", m_conversion.value().data, "&campaign=", m_campaign.id);
    else
        relativeURL = makeString("?conversion=", m_conversion.value().data, "&campaign=", m_campaign.id, '&', baseURL.query());
    return URL(baseURL, relativeURL);
}

void AdClickAttribution::markConversionAsSent()
{
    ASSERT(m_conversion);
    if (m_conversion)
        m_conversion->wasSent = Conversion::WasSent::Yes;
}

bool AdClickAttribution::wasConversionSent() const
{
    return m_conversion && m_conversion->wasSent == Conversion::WasSent::Yes;
}

String AdClickAttribution::toString() const
{
    StringBuilder builder;
    builder.appendLiteral("Source: ");
    builder.append(m_source.registrableDomain.string());
    builder.appendLiteral("\nDestination: ");
    builder.append(m_destination.registrableDomain.string());
    builder.appendLiteral("\nCampaign ID: ");
    builder.appendNumber(m_campaign.id);
    if (m_conversion) {
        builder.appendLiteral("\nConversion data: ");
        builder.appendNumber(m_conversion.value().data);
        builder.appendLiteral("\nConversion priority: ");
        builder.appendNumber(m_conversion.value().priority);
        builder.appendLiteral("\nConversion earliest time to send: ");
        if (!m_earliestTimeToSend)
            builder.appendLiteral("Not set");
        else {
            auto secondsUntilSend = *m_earliestTimeToSend - WallTime::now();
            builder.append((secondsUntilSend >= 24_h && secondsUntilSend <= 48_h) ? "Within 24-48 hours" : "Outside 24-48 hours");
        }
        builder.appendLiteral("\nConversion request sent: ");
        builder.append((wasConversionSent() ? "true" : "false"));
    } else
        builder.appendLiteral("\nNo conversion data.");
    builder.append('\n');

    return builder.toString();
}

bool AdClickAttribution::debugModeEnabled()
{
    return RuntimeEnabledFeatures::sharedFeatures().adClickAttributionDebugModeEnabled();
}

}
