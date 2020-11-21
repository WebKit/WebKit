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
#include "PrivateClickMeasurement.h"

#include "Logging.h"
#include "RuntimeEnabledFeatures.h"
#include <wtf/Expected.h>
#include <wtf/RandomNumber.h>
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>

namespace WebCore {

static const char privateClickMeasurementPathPrefix[] = "/.well-known/private-click-measurement/";
const size_t privateClickMeasurementAttributionTriggerDataPathSegmentSize = 2;
const size_t privateClickMeasurementPriorityPathSegmentSize = 2;

const Seconds PrivateClickMeasurement::maxAge()
{
    return 24_h * 7;
};

bool PrivateClickMeasurement::isValid() const
{
    return m_attributionTriggerData
        && m_attributionTriggerData.value().isValid()
        && m_sourceID.isValid()
        && !m_sourceSite.registrableDomain.isEmpty()
        && !m_attributeOnSite.registrableDomain.isEmpty()
        && m_earliestTimeToSend;
}

Expected<PrivateClickMeasurement::AttributionTriggerData, String> PrivateClickMeasurement::parseAttributionRequest(const URL& redirectURL)
{
    if (!redirectURL.protocolIs("https") || redirectURL.hasCredentials() || redirectURL.hasQuery() || redirectURL.hasFragmentIdentifier()) {
        if (UNLIKELY(debugModeEnabled())) {
            RELEASE_LOG_INFO(PrivateClickMeasurement, "Conversion was not accepted because the URL's protocol is not HTTPS or the URL contains one or more of username, password, query string, and fragment.");
            return makeUnexpected("[Private Click Measurement] Conversion was not accepted because the URL's protocol is not HTTPS or the URL contains one or more of username, password, query string, and fragment."_s);
        }
        return makeUnexpected(nullString());
    }

    auto path = StringView(redirectURL.string()).substring(redirectURL.pathStart(), redirectURL.pathEnd() - redirectURL.pathStart());
    if (path.isEmpty() || !path.startsWith(privateClickMeasurementPathPrefix)) {
        if (UNLIKELY(debugModeEnabled())) {
            RELEASE_LOG_INFO(PrivateClickMeasurement, "Conversion was not accepted because the URL path did not start with %" PUBLIC_LOG_STRING ".", privateClickMeasurementPathPrefix);
            return makeUnexpected(makeString("[Private Click Measurement] Conversion was not accepted because the URL path did not start with "_s, privateClickMeasurementPathPrefix, "."_s));
        }
        return makeUnexpected(nullString());
    }

    auto prefixLength = sizeof(privateClickMeasurementPathPrefix) - 1;
    if (path.length() == prefixLength + privateClickMeasurementAttributionTriggerDataPathSegmentSize) {
        auto attributionTriggerDataUInt64 = path.substring(prefixLength, privateClickMeasurementAttributionTriggerDataPathSegmentSize).toUInt64Strict();
        if (!attributionTriggerDataUInt64 || *attributionTriggerDataUInt64 > MaxEntropy) {
            if (UNLIKELY(debugModeEnabled())) {
                RELEASE_LOG_INFO(PrivateClickMeasurement, "Conversion was not accepted because the conversion data could not be parsed or was higher than the allowed maximum of %{public}u.", MaxEntropy);
                return makeUnexpected(makeString("[Private Click Measurement] Conversion was not accepted because the conversion data could not be parsed or was higher than the allowed maximum of "_s, MaxEntropy, "."_s));
            }
            return makeUnexpected(nullString());
        }

        return AttributionTriggerData { static_cast<uint32_t>(*attributionTriggerDataUInt64), Priority { 0 } };
    }
    
    if (path.length() == prefixLength + privateClickMeasurementAttributionTriggerDataPathSegmentSize + 1 + privateClickMeasurementPriorityPathSegmentSize) {
        auto attributionTriggerDataUInt64 = path.substring(prefixLength, privateClickMeasurementAttributionTriggerDataPathSegmentSize).toUInt64Strict();
        if (!attributionTriggerDataUInt64 || *attributionTriggerDataUInt64 > MaxEntropy) {
            if (UNLIKELY(debugModeEnabled())) {
                RELEASE_LOG_INFO(PrivateClickMeasurement, "Conversion was not accepted because the conversion data could not be parsed or was higher than the allowed maximum of %{public}u.", MaxEntropy);
                return makeUnexpected(makeString("[Private Click Measurement] Conversion was not accepted because the conversion data could not be parsed or was higher than the allowed maximum of "_s, MaxEntropy, "."_s));
            }
            return makeUnexpected(nullString());
        }

        auto attributionPriorityUInt64 = path.substring(prefixLength + privateClickMeasurementAttributionTriggerDataPathSegmentSize + 1, privateClickMeasurementPriorityPathSegmentSize).toUInt64Strict();
        if (!attributionPriorityUInt64 || *attributionPriorityUInt64 > MaxEntropy) {
            if (UNLIKELY(debugModeEnabled())) {
                RELEASE_LOG_INFO(PrivateClickMeasurement, "Conversion was not accepted because the priority could not be parsed or was higher than the allowed maximum of %{public}u.", MaxEntropy);
                return makeUnexpected(makeString("[Private Click Measurement] Conversion was not accepted because the priority could not be parsed or was higher than the allowed maximum of "_s, MaxEntropy, "."_s));
            }
            return makeUnexpected(nullString());
        }

        return AttributionTriggerData { static_cast<uint32_t>(*attributionTriggerDataUInt64), Priority { static_cast<uint32_t>(*attributionPriorityUInt64) } };
    }

    if (UNLIKELY(debugModeEnabled())) {
        RELEASE_LOG_INFO(PrivateClickMeasurement, "Conversion was not accepted because the URL path contained unrecognized parts.");
        return makeUnexpected("[Private Click Measurement] Conversion was not accepted because the URL path contained unrecognized parts."_s);
    }
    return makeUnexpected(nullString());
}

Optional<Seconds> PrivateClickMeasurement::attributeAndGetEarliestTimeToSend(AttributionTriggerData&& attributionTriggerData)
{
    if (!attributionTriggerData.isValid() || (m_attributionTriggerData && m_attributionTriggerData->priority >= attributionTriggerData.priority))
        return { };

    m_attributionTriggerData = WTFMove(attributionTriggerData);
    // 24-48 hour delay before sending. This helps privacy since the conversion and the attribution
    // requests are detached and the time of the attribution does not reveal the time of the conversion.
    auto seconds = 24_h + Seconds(randomNumber() * (24_h).value());
    m_earliestTimeToSend = WallTime::now() + seconds;
    return seconds;
}

bool PrivateClickMeasurement::hasHigherPriorityThan(const PrivateClickMeasurement& other) const
{
    if (!other.m_attributionTriggerData)
        return true;
    
    if (!m_attributionTriggerData)
        return false;

    return m_attributionTriggerData->priority > other.m_attributionTriggerData->priority;
}

URL PrivateClickMeasurement::reportURL() const
{
    if (!isValid())
        return URL();

    StringBuilder builder;
    builder.appendLiteral("https://");
    builder.append(m_sourceSite.registrableDomain.string());
    builder.appendLiteral(privateClickMeasurementPathPrefix);

    URL url { URL(), builder.toString() };
    if (url.isValid())
        return url;

    return URL();
}

Ref<JSON::Object> PrivateClickMeasurement::json() const
{
    auto reportDetails = JSON::Object::create();
    if (!m_attributionTriggerData)
        return reportDetails;

    reportDetails->setString("source-engagement-type"_s, "click"_s);
    reportDetails->setString("source-site"_s, m_sourceSite.registrableDomain.string());
    reportDetails->setInteger("source-id"_s, m_sourceID.id);
    reportDetails->setString("attributed-on-site"_s, m_attributeOnSite.registrableDomain.string());
    reportDetails->setInteger("trigger-data"_s, m_attributionTriggerData->data);
    reportDetails->setInteger("report-version"_s, 1);
    return reportDetails;
}

bool PrivateClickMeasurement::debugModeEnabled()
{
    return RuntimeEnabledFeatures::sharedFeatures().privateClickMeasurementDebugModeEnabled();
}

}
