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

#pragma once

#include "RegistrableDomain.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/JSONValues.h>
#include <wtf/Optional.h>
#include <wtf/URL.h>
#include <wtf/WallTime.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#endif

#if PLATFORM(COCOA)
OBJC_CLASS RSABSSATokenReady;
OBJC_CLASS RSABSSATokenWaitingActivation;
OBJC_CLASS RSABSSATokenBlinder;
#endif

namespace WebCore {

class PrivateClickMeasurement {
public:
    using PriorityValue = uint32_t;

    enum class PcmDataCarried : bool { NonPersonallyIdentifiable, PersonallyIdentifiable };
    enum class AttributionReportEndpoint : bool { Source, Destination };

    struct SourceID {
        static constexpr uint32_t MaxEntropy = 255;

        SourceID() = default;
        explicit SourceID(uint32_t id)
            : id { id }
        {
        }
        
        bool isValid() const
        {
            return id <= MaxEntropy;
        }
        
        uint32_t id { 0 };
    };

    struct SourceSite {
        SourceSite() = default;
        explicit SourceSite(const URL& url)
            : registrableDomain { url }
        {
        }

        explicit SourceSite(const RegistrableDomain& domain)
            : registrableDomain { domain }
        {
        }

        bool operator==(const SourceSite& other) const
        {
            return registrableDomain == other.registrableDomain;
        }

        bool matches(const URL& url) const
        {
            return registrableDomain.matches(url);
        }

        RegistrableDomain registrableDomain;
    };

    struct SourceSiteHash {
        static unsigned hash(const SourceSite& sourceSite)
        {
            return sourceSite.registrableDomain.hash();
        }
        
        static bool equal(const SourceSite& a, const SourceSite& b)
        {
            return a == b;
        }

        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    struct AttributionDestinationSite {
        AttributionDestinationSite() = default;
        explicit AttributionDestinationSite(const URL& url)
            : registrableDomain { RegistrableDomain { url } }
        {
        }

        explicit AttributionDestinationSite(RegistrableDomain&& domain)
            : registrableDomain { WTFMove(domain) }
        {
        }
        
        bool operator==(const AttributionDestinationSite& other) const
        {
            return registrableDomain == other.registrableDomain;
        }

        bool matches(const URL& url) const
        {
            return registrableDomain == RegistrableDomain { url };
        }

        RegistrableDomain registrableDomain;
    };

    struct AttributionDestinationSiteHash {
        static unsigned hash(const AttributionDestinationSite& destinationSite)
        {
            return destinationSite.registrableDomain.hash();
        }
        
        static bool equal(const AttributionDestinationSite& a, const AttributionDestinationSite& b)
        {
            return a == b;
        }

        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    struct Priority {
        static constexpr uint32_t MaxEntropy = 63;

        explicit Priority(PriorityValue value)
        : value { value }
        {
        }
        
        PriorityValue value;
    };
    
    struct AttributionTriggerData {
        static constexpr uint32_t MaxEntropy = 15;

        enum class WasSent : bool { No, Yes };
        
        AttributionTriggerData(uint32_t data, Priority priority, WasSent wasSent = WasSent::No)
            : data { data }
            , priority { priority.value }
            , wasSent { wasSent }
        {
        }

        bool isValid() const
        {
            return data <= MaxEntropy && priority <= Priority::MaxEntropy;
        }
        
        uint32_t data;
        PriorityValue priority;
        WasSent wasSent = WasSent::No;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<AttributionTriggerData> decode(Decoder&);
    };

    struct AttributionSecondsUntilSendData {
        Optional<Seconds> sourceSeconds;
        Optional<Seconds> destinationSeconds;

        bool hasValidSecondsUntilSendValues()
        {
            return sourceSeconds && destinationSeconds;
        }

        Optional<Seconds> minSecondsUntilSend()
        {
            if (!sourceSeconds && !destinationSeconds)
                return WTF::nullopt;

            if (sourceSeconds && destinationSeconds)
                return std::min(sourceSeconds, destinationSeconds);

            return sourceSeconds ? sourceSeconds : destinationSeconds;
        }

        template<class Encoder>
        void encode(Encoder& encoder) const
        {
            encoder << sourceSeconds << destinationSeconds;
        }

        template<class Decoder>
        static Optional<AttributionSecondsUntilSendData> decode(Decoder& decoder)
        {
            Optional<Optional<Seconds>> sourceSeconds;
            decoder >> sourceSeconds;
            if (!sourceSeconds)
                return WTF::nullopt;

            Optional<Optional<Seconds>> destinationSeconds;
            decoder >> destinationSeconds;
            if (!destinationSeconds)
                return WTF::nullopt;

            return AttributionSecondsUntilSendData { WTFMove(*sourceSeconds), WTFMove(*destinationSeconds) };
        }
    };

    struct AttributionTimeToSendData {
        Optional<WallTime> sourceEarliestTimeToSend;
        Optional<WallTime> destinationEarliestTimeToSend;

        Optional<WallTime> earliestTimeToSend()
        {
            if (!sourceEarliestTimeToSend && !destinationEarliestTimeToSend)
                return WTF::nullopt;

            if (sourceEarliestTimeToSend && destinationEarliestTimeToSend)
                return std::min(sourceEarliestTimeToSend, destinationEarliestTimeToSend);

            return sourceEarliestTimeToSend ? sourceEarliestTimeToSend : destinationEarliestTimeToSend;
        }

        Optional<WallTime> latestTimeToSend()
        {
            if (!sourceEarliestTimeToSend && !destinationEarliestTimeToSend)
                return WTF::nullopt;

            if (sourceEarliestTimeToSend && destinationEarliestTimeToSend)
                return std::max(sourceEarliestTimeToSend, destinationEarliestTimeToSend);

            return sourceEarliestTimeToSend ? sourceEarliestTimeToSend : destinationEarliestTimeToSend;
        }

        Optional<AttributionReportEndpoint> attributionReportEndpoint()
        {
            if (sourceEarliestTimeToSend && destinationEarliestTimeToSend) {
                if (*sourceEarliestTimeToSend < *destinationEarliestTimeToSend)
                    return AttributionReportEndpoint::Source;

                return AttributionReportEndpoint::Destination;
            }

            if (sourceEarliestTimeToSend)
                return AttributionReportEndpoint::Source;

            if (destinationEarliestTimeToSend)
                return AttributionReportEndpoint::Destination;

            return WTF::nullopt;
        }

        template<class Encoder>
        void encode(Encoder& encoder) const
        {
            encoder << sourceEarliestTimeToSend << destinationEarliestTimeToSend;
        }

        template<class Decoder>
        static Optional<AttributionTimeToSendData> decode(Decoder& decoder)
        {
            Optional<Optional<WallTime>> sourceEarliestTimeToSend;
            decoder >> sourceEarliestTimeToSend;
            if (!sourceEarliestTimeToSend)
                return WTF::nullopt;

            Optional<Optional<WallTime>> destinationEarliestTimeToSend;
            decoder >> destinationEarliestTimeToSend;
            if (!destinationEarliestTimeToSend)
                return WTF::nullopt;

            return AttributionTimeToSendData { WTFMove(*sourceEarliestTimeToSend), WTFMove(*destinationEarliestTimeToSend) };
        }
    };

    PrivateClickMeasurement() = default;
    PrivateClickMeasurement(SourceID sourceID, const SourceSite& sourceSite, const AttributionDestinationSite& destinationSite, String&& sourceDescription = { }, String&& purchaser = { }, WallTime timeOfAdClick = WallTime::now())
        : m_sourceID { sourceID }
        , m_sourceSite { sourceSite }
        , m_destinationSite { destinationSite }
        , m_sourceDescription { WTFMove(sourceDescription) }
        , m_purchaser { WTFMove(purchaser) }
        , m_timeOfAdClick { timeOfAdClick }
    {
    }

    WEBCORE_EXPORT static const Seconds maxAge();
    WEBCORE_EXPORT static Expected<AttributionTriggerData, String> parseAttributionRequest(const URL& redirectURL);
    WEBCORE_EXPORT AttributionSecondsUntilSendData attributeAndGetEarliestTimeToSend(AttributionTriggerData&&);
    WEBCORE_EXPORT bool hasHigherPriorityThan(const PrivateClickMeasurement&) const;
    WEBCORE_EXPORT URL attributionReportSourceURL() const;
    WEBCORE_EXPORT URL attributionReportAttributeOnURL() const;
    WEBCORE_EXPORT Ref<JSON::Object> attributionReportJSON() const;
    const SourceSite& sourceSite() const { return m_sourceSite; };
    const AttributionDestinationSite& destinationSite() const { return m_destinationSite; };
    WallTime timeOfAdClick() const { return m_timeOfAdClick; }
    WEBCORE_EXPORT bool hasPreviouslyBeenReported();
    AttributionTimeToSendData timesToSend() const { return m_timesToSend; };
    void setTimesToSend(AttributionTimeToSendData data) { m_timesToSend = data; }
    const SourceID& sourceID() const { return m_sourceID; }
    Optional<AttributionTriggerData> attributionTriggerData() { return m_attributionTriggerData; }
    void setAttribution(AttributionTriggerData&& attributionTriggerData) { m_attributionTriggerData = WTFMove(attributionTriggerData); }

    const String& sourceDescription() const { return m_sourceDescription; }
    const String& purchaser() const { return m_purchaser; }

    // MARK: - Fraud Prevention
    WEBCORE_EXPORT URL tokenPublicKeyURL() const;
    WEBCORE_EXPORT URL tokenSignatureURL() const;

    WEBCORE_EXPORT Ref<JSON::Object> tokenSignatureJSON() const;

    struct EphemeralSourceNonce {
        String nonce;

        WEBCORE_EXPORT bool isValid() const;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<EphemeralSourceNonce> decode(Decoder&);
    };

    WEBCORE_EXPORT void setEphemeralSourceNonce(EphemeralSourceNonce&&);
    Optional<EphemeralSourceNonce> ephemeralSourceNonce() const { return m_ephemeralSourceNonce; };
    void clearEphemeralSourceNonce() { m_ephemeralSourceNonce.reset(); };

    struct SourceSecretToken {
        String tokenBase64URL;
        String signatureBase64URL;
        String keyIDBase64URL;

        bool isValid() const;
    };

#if PLATFORM(COCOA)
    WEBCORE_EXPORT bool calculateAndUpdateSourceUnlinkableToken(const String& serverPublicKeyBase64URL);
    WEBCORE_EXPORT bool calculateAndUpdateSourceSecretToken(const String& serverResponseBase64URL);
#endif

    void setSourceUnlinkableTokenValue(const String& value) { m_sourceUnlinkableToken.valueBase64URL = value; }
    const Optional<SourceSecretToken>& sourceUnlinkableToken() const { return m_sourceSecretToken; }
    WEBCORE_EXPORT void setSourceSecretToken(SourceSecretToken&&);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<PrivateClickMeasurement> decode(Decoder&);

private:
    bool isValid() const;

    SourceID m_sourceID;
    SourceSite m_sourceSite;
    AttributionDestinationSite m_destinationSite;
    String m_sourceDescription;
    String m_purchaser;
    WallTime m_timeOfAdClick;

    Optional<AttributionTriggerData> m_attributionTriggerData;
    AttributionTimeToSendData m_timesToSend;

    struct SourceUnlinkableToken {
#if PLATFORM(COCOA)
        RetainPtr<RSABSSATokenBlinder> blinder;
        RetainPtr<RSABSSATokenWaitingActivation> waitingToken;
        RetainPtr<RSABSSATokenReady> readyToken;
#endif
        String valueBase64URL;
    };

    Optional<EphemeralSourceNonce> m_ephemeralSourceNonce;
    SourceUnlinkableToken m_sourceUnlinkableToken;
    Optional<SourceSecretToken> m_sourceSecretToken;
};

template<class Encoder>
void PrivateClickMeasurement::encode(Encoder& encoder) const
{
    encoder << m_sourceID.id
        << m_sourceSite.registrableDomain
        << m_destinationSite.registrableDomain
        << m_sourceDescription
        << m_purchaser
        << m_timeOfAdClick
        << m_ephemeralSourceNonce
        << m_attributionTriggerData
        << m_timesToSend;
}

template<class Decoder>
Optional<PrivateClickMeasurement> PrivateClickMeasurement::decode(Decoder& decoder)
{
    Optional<uint32_t> sourceID;
    decoder >> sourceID;
    if (!sourceID)
        return WTF::nullopt;
    
    Optional<RegistrableDomain> sourceRegistrableDomain;
    decoder >> sourceRegistrableDomain;
    if (!sourceRegistrableDomain)
        return WTF::nullopt;
    
    Optional<RegistrableDomain> destinationRegistrableDomain;
    decoder >> destinationRegistrableDomain;
    if (!destinationRegistrableDomain)
        return WTF::nullopt;
    
    Optional<String> sourceDescription;
    decoder >> sourceDescription;
    if (!sourceDescription)
        return WTF::nullopt;
    
    Optional<String> purchaser;
    decoder >> purchaser;
    if (!purchaser)
        return WTF::nullopt;
    
    Optional<WallTime> timeOfAdClick;
    decoder >> timeOfAdClick;
    if (!timeOfAdClick)
        return WTF::nullopt;

    Optional<Optional<EphemeralSourceNonce>> ephemeralSourceNonce;
    decoder >> ephemeralSourceNonce;
    if (!ephemeralSourceNonce)
        return WTF::nullopt;

    Optional<Optional<AttributionTriggerData>> attributionTriggerData;
    decoder >> attributionTriggerData;
    if (!attributionTriggerData)
        return WTF::nullopt;
    
    Optional<AttributionTimeToSendData> timesToSend;
    decoder >> timesToSend;
    if (!timesToSend)
        return WTF::nullopt;
    
    PrivateClickMeasurement attribution {
        SourceID { WTFMove(*sourceID) },
        SourceSite { WTFMove(*sourceRegistrableDomain) },
        AttributionDestinationSite { WTFMove(*destinationRegistrableDomain) },
        WTFMove(*sourceDescription),
        WTFMove(*purchaser),
        WTFMove(*timeOfAdClick)
    };
    attribution.m_ephemeralSourceNonce = WTFMove(*ephemeralSourceNonce);
    attribution.m_attributionTriggerData = WTFMove(*attributionTriggerData);
    attribution.m_timesToSend = WTFMove(*timesToSend);
    
    return attribution;
}

template<class Encoder>
void PrivateClickMeasurement::EphemeralSourceNonce::encode(Encoder& encoder) const
{
    encoder << nonce;
}

template<class Decoder>
Optional<PrivateClickMeasurement::EphemeralSourceNonce> PrivateClickMeasurement::EphemeralSourceNonce::decode(Decoder& decoder)
{
    Optional<String> nonce;
    decoder >> nonce;
    if (!nonce)
        return WTF::nullopt;
    
    return EphemeralSourceNonce { WTFMove(*nonce) };
}

template<class Encoder>
void PrivateClickMeasurement::AttributionTriggerData::encode(Encoder& encoder) const
{
    encoder << data << priority << wasSent;
}

template<class Decoder>
Optional<PrivateClickMeasurement::AttributionTriggerData> PrivateClickMeasurement::AttributionTriggerData::decode(Decoder& decoder)
{
    Optional<uint32_t> data;
    decoder >> data;
    if (!data)
        return WTF::nullopt;
    
    Optional<PriorityValue> priority;
    decoder >> priority;
    if (!priority)
        return WTF::nullopt;
    
    Optional<WasSent> wasSent;
    decoder >> wasSent;
    if (!wasSent)
        return WTF::nullopt;
    
    return AttributionTriggerData { WTFMove(*data), Priority { *priority }, *wasSent };
}

} // namespace WebCore

namespace WTF {
template<typename T> struct DefaultHash;

template<> struct DefaultHash<WebCore::PrivateClickMeasurement::SourceSite> : WebCore::PrivateClickMeasurement::SourceSiteHash { };
template<> struct HashTraits<WebCore::PrivateClickMeasurement::SourceSite> : GenericHashTraits<WebCore::PrivateClickMeasurement::SourceSite> {
    static WebCore::PrivateClickMeasurement::SourceSite emptyValue() { return { }; }
    static void constructDeletedValue(WebCore::PrivateClickMeasurement::SourceSite& slot) { new (NotNull, &slot.registrableDomain) WebCore::RegistrableDomain(WTF::HashTableDeletedValue); }
    static bool isDeletedValue(const WebCore::PrivateClickMeasurement::SourceSite& slot) { return slot.registrableDomain.isHashTableDeletedValue(); }
};

template<> struct DefaultHash<WebCore::PrivateClickMeasurement::AttributionDestinationSite> : WebCore::PrivateClickMeasurement::AttributionDestinationSiteHash { };
template<> struct HashTraits<WebCore::PrivateClickMeasurement::AttributionDestinationSite> : GenericHashTraits<WebCore::PrivateClickMeasurement::AttributionDestinationSite> {
    static WebCore::PrivateClickMeasurement::AttributionDestinationSite emptyValue() { return { }; }
    static void constructDeletedValue(WebCore::PrivateClickMeasurement::AttributionDestinationSite& slot) { new (NotNull, &slot.registrableDomain) WebCore::RegistrableDomain(WTF::HashTableDeletedValue); }
    static bool isDeletedValue(const WebCore::PrivateClickMeasurement::AttributionDestinationSite& slot) { return slot.registrableDomain.isHashTableDeletedValue(); }
};
}
