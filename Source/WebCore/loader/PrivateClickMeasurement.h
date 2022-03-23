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
    using PriorityValue = uint8_t;
    enum class AttributionEphemeral : bool { No, Yes };

    enum class PcmDataCarried : bool { NonPersonallyIdentifiable, PersonallyIdentifiable };
    enum class AttributionReportEndpoint : bool { Source, Destination };
    enum class IsRunningLayoutTest : bool { No, Yes };

    struct SourceID {
        static constexpr uint8_t MaxEntropy = 255;
        explicit SourceID(uint8_t id)
            : id { id }
        {
        }

        uint8_t id { 0 };
    };

    struct SourceSite {
        explicit SourceSite(const URL& url)
            : registrableDomain { url }
        {
        }

        explicit SourceSite(RegistrableDomain&& domain)
            : registrableDomain { WTFMove(domain) }
        {
        }

        SourceSite isolatedCopy() const & { return SourceSite { registrableDomain.isolatedCopy() }; }
        SourceSite isolatedCopy() && { return SourceSite { WTFMove(registrableDomain).isolatedCopy() }; }

        bool operator==(const SourceSite& other) const
        {
            return registrableDomain == other.registrableDomain;
        }

        bool operator!=(const SourceSite& other) const
        {
            return registrableDomain != other.registrableDomain;
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

        AttributionDestinationSite isolatedCopy() const & { return AttributionDestinationSite { registrableDomain.isolatedCopy() }; }
        AttributionDestinationSite isolatedCopy() && { return AttributionDestinationSite { WTFMove(registrableDomain).isolatedCopy() }; }

        bool operator==(const AttributionDestinationSite& other) const
        {
            return registrableDomain == other.registrableDomain;
        }

        bool operator!=(const AttributionDestinationSite& other) const
        {
            return registrableDomain != other.registrableDomain;
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
        static constexpr uint8_t MaxEntropy = 63;

        explicit Priority(PriorityValue value)
            : value { value }
        {
        }
        
        PriorityValue value;
    };

    struct EphemeralNonce {
        String nonce;

        EphemeralNonce isolatedCopy() const & { return { nonce.isolatedCopy() }; }
        EphemeralNonce isolatedCopy() &&  { return { WTFMove(nonce).isolatedCopy() }; }

        WEBCORE_EXPORT bool isValid() const;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<EphemeralNonce> decode(Decoder&);
    };

    struct UnlinkableToken {
#if PLATFORM(COCOA)
        RetainPtr<RSABSSATokenBlinder> blinder;
        RetainPtr<RSABSSATokenWaitingActivation> waitingToken;
        RetainPtr<RSABSSATokenReady> readyToken;
#endif
        String valueBase64URL;
        
        UnlinkableToken isolatedCopy() const &;
        UnlinkableToken isolatedCopy() &&;
    };

    struct SourceUnlinkableToken : UnlinkableToken {
        SourceUnlinkableToken isolatedCopy() const & { return { UnlinkableToken::isolatedCopy() }; }
        SourceUnlinkableToken isolatedCopy() && { return { UnlinkableToken::isolatedCopy() }; }
    };

    struct DestinationUnlinkableToken : UnlinkableToken {
        DestinationUnlinkableToken isolatedCopy() const & { return { UnlinkableToken::isolatedCopy() }; }
        DestinationUnlinkableToken isolatedCopy() && { return { UnlinkableToken::isolatedCopy() }; }
    };

    struct SecretToken {
        String tokenBase64URL;
        String signatureBase64URL;
        String keyIDBase64URL;
        SecretToken isolatedCopy() const & { return { tokenBase64URL.isolatedCopy(), signatureBase64URL.isolatedCopy(), keyIDBase64URL.isolatedCopy() }; }
        SecretToken isolatedCopy() && { return { WTFMove(tokenBase64URL).isolatedCopy(), WTFMove(signatureBase64URL).isolatedCopy(), WTFMove(keyIDBase64URL).isolatedCopy() }; }
        bool isValid() const;
    };

    struct SourceSecretToken : SecretToken {
        SourceSecretToken isolatedCopy() const & { return { SecretToken::isolatedCopy() }; }
        SourceSecretToken isolatedCopy() && { return { SecretToken::isolatedCopy() }; }
    };

    struct DestinationSecretToken : SecretToken {
        DestinationSecretToken isolatedCopy() const & { return { SecretToken::isolatedCopy() }; }
        DestinationSecretToken isolatedCopy() && { return { SecretToken::isolatedCopy() }; }
    };

    struct AttributionTriggerData {
        static constexpr uint8_t MaxEntropy = 15;

        enum class WasSent : bool { No, Yes };

        AttributionTriggerData() = default;
        AttributionTriggerData(uint8_t data, Priority priority, WasSent wasSent = WasSent::No, std::optional<EphemeralNonce> nonce = std::nullopt)
            : data { data }
            , priority { priority.value }
            , wasSent { wasSent }
            , ephemeralDestinationNonce { nonce }
        {
        }

        bool isValid() const
        {
            return data <= MaxEntropy && priority <= Priority::MaxEntropy;
        }

        void setDestinationUnlinkableTokenValue(const String& value)
        {
            if (!destinationUnlinkableToken)
                destinationUnlinkableToken = DestinationUnlinkableToken { };
            destinationUnlinkableToken->valueBase64URL = value;
        }
        void setDestinationSecretToken(const DestinationSecretToken& token) { destinationSecretToken = token; }
        const std::optional<const URL> tokenPublicKeyURL() const { return destinationSite ? PrivateClickMeasurement::tokenPublicKeyURL(*destinationSite) : URL(); }
        const std::optional<const URL> tokenSignatureURL() const { return destinationSite ? PrivateClickMeasurement::tokenSignatureURL(*destinationSite) : URL(); }
        WEBCORE_EXPORT Ref<JSON::Object> tokenSignatureJSON() const;

        uint8_t data { 0 };
        PriorityValue priority;
        WasSent wasSent = WasSent::No;
        std::optional<RegistrableDomain> sourceRegistrableDomain;
        std::optional<EphemeralNonce> ephemeralDestinationNonce;
        std::optional<DestinationUnlinkableToken> destinationUnlinkableToken;
        std::optional<DestinationSecretToken> destinationSecretToken;
        std::optional<RegistrableDomain> destinationSite;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<AttributionTriggerData> decode(Decoder&);
    };

    struct AttributionSecondsUntilSendData {
        std::optional<Seconds> sourceSeconds;
        std::optional<Seconds> destinationSeconds;

        bool hasValidSecondsUntilSendValues()
        {
            return sourceSeconds && destinationSeconds;
        }

        std::optional<Seconds> minSecondsUntilSend()
        {
            if (!sourceSeconds && !destinationSeconds)
                return std::nullopt;

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
        static std::optional<AttributionSecondsUntilSendData> decode(Decoder& decoder)
        {
            std::optional<std::optional<Seconds>> sourceSeconds;
            decoder >> sourceSeconds;
            if (!sourceSeconds)
                return std::nullopt;

            std::optional<std::optional<Seconds>> destinationSeconds;
            decoder >> destinationSeconds;
            if (!destinationSeconds)
                return std::nullopt;

            return AttributionSecondsUntilSendData { WTFMove(*sourceSeconds), WTFMove(*destinationSeconds) };
        }
    };

    struct AttributionTimeToSendData {
        std::optional<WallTime> sourceEarliestTimeToSend;
        std::optional<WallTime> destinationEarliestTimeToSend;

        std::optional<WallTime> earliestTimeToSend()
        {
            if (!sourceEarliestTimeToSend && !destinationEarliestTimeToSend)
                return std::nullopt;

            if (sourceEarliestTimeToSend && destinationEarliestTimeToSend)
                return std::min(sourceEarliestTimeToSend, destinationEarliestTimeToSend);

            return sourceEarliestTimeToSend ? sourceEarliestTimeToSend : destinationEarliestTimeToSend;
        }

        std::optional<WallTime> latestTimeToSend()
        {
            if (!sourceEarliestTimeToSend && !destinationEarliestTimeToSend)
                return std::nullopt;

            if (sourceEarliestTimeToSend && destinationEarliestTimeToSend)
                return std::max(sourceEarliestTimeToSend, destinationEarliestTimeToSend);

            return sourceEarliestTimeToSend ? sourceEarliestTimeToSend : destinationEarliestTimeToSend;
        }

        std::optional<AttributionReportEndpoint> attributionReportEndpoint()
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

            return std::nullopt;
        }

        template<class Encoder>
        void encode(Encoder& encoder) const
        {
            encoder << sourceEarliestTimeToSend << destinationEarliestTimeToSend;
        }

        template<class Decoder>
        static std::optional<AttributionTimeToSendData> decode(Decoder& decoder)
        {
            std::optional<std::optional<WallTime>> sourceEarliestTimeToSend;
            decoder >> sourceEarliestTimeToSend;
            if (!sourceEarliestTimeToSend)
                return std::nullopt;

            std::optional<std::optional<WallTime>> destinationEarliestTimeToSend;
            decoder >> destinationEarliestTimeToSend;
            if (!destinationEarliestTimeToSend)
                return std::nullopt;

            return AttributionTimeToSendData { WTFMove(*sourceEarliestTimeToSend), WTFMove(*destinationEarliestTimeToSend) };
        }
    };

    PrivateClickMeasurement(SourceID sourceID, const SourceSite& sourceSite, const AttributionDestinationSite& destinationSite, const String& sourceApplicationBundleID, WallTime timeOfAdClick, AttributionEphemeral isEphemeral)
        : m_sourceID { sourceID }
        , m_sourceSite { sourceSite }
        , m_destinationSite { destinationSite }
        , m_timeOfAdClick { timeOfAdClick }
        , m_isEphemeral { isEphemeral }
        , m_sourceApplicationBundleID { sourceApplicationBundleID }
    {
    }

    WEBCORE_EXPORT static const Seconds maxAge();
    WEBCORE_EXPORT bool isNeitherSameSiteNorCrossSiteTriggeringEvent(const RegistrableDomain& redirectDomain, const URL& firstPartyURL, const AttributionTriggerData&);
    WEBCORE_EXPORT static Expected<AttributionTriggerData, String> parseAttributionRequest(const URL& redirectURL);
    WEBCORE_EXPORT AttributionSecondsUntilSendData attributeAndGetEarliestTimeToSend(AttributionTriggerData&&, IsRunningLayoutTest);
    WEBCORE_EXPORT bool hasHigherPriorityThan(const PrivateClickMeasurement&) const;
    WEBCORE_EXPORT URL attributionReportClickSourceURL() const;
    WEBCORE_EXPORT URL attributionReportClickDestinationURL() const;
    WEBCORE_EXPORT Ref<JSON::Object> attributionReportJSON() const;
    const SourceSite& sourceSite() const { return m_sourceSite; };
    const AttributionDestinationSite& destinationSite() const { return m_destinationSite; };
    WallTime timeOfAdClick() const { return m_timeOfAdClick; }
    WEBCORE_EXPORT bool hasPreviouslyBeenReported();
    AttributionTimeToSendData timesToSend() const { return m_timesToSend; };
    void setTimesToSend(AttributionTimeToSendData data) { m_timesToSend = data; }
    const SourceID& sourceID() const { return m_sourceID; }
    const std::optional<AttributionTriggerData>& attributionTriggerData() const { return m_attributionTriggerData; }
    void setAttribution(AttributionTriggerData&& attributionTriggerData) { m_attributionTriggerData = WTFMove(attributionTriggerData); }
    const String& sourceApplicationBundleID() const { return m_sourceApplicationBundleID; }
    WEBCORE_EXPORT void setSourceApplicationBundleIDForTesting(const String&);

    bool isEphemeral() const { return m_isEphemeral == AttributionEphemeral::Yes; }
    void setEphemeral(AttributionEphemeral isEphemeral) { m_isEphemeral = isEphemeral; }

    // MARK: - Fraud Prevention
    WEBCORE_EXPORT const std::optional<const URL> tokenPublicKeyURL() const;
    WEBCORE_EXPORT static const std::optional<const URL> tokenPublicKeyURL(const RegistrableDomain&);
    WEBCORE_EXPORT const std::optional<const URL> tokenSignatureURL() const;
    WEBCORE_EXPORT static const std::optional<const URL> tokenSignatureURL(const RegistrableDomain&);

    WEBCORE_EXPORT Ref<JSON::Object> tokenSignatureJSON() const;

    WEBCORE_EXPORT void setEphemeralSourceNonce(EphemeralNonce&&);
    std::optional<EphemeralNonce> ephemeralSourceNonce() const { return m_ephemeralSourceNonce; };
    void clearEphemeralSourceNonce() { m_ephemeralSourceNonce.reset(); };

#if PLATFORM(COCOA)
    WEBCORE_EXPORT std::optional<String> calculateAndUpdateSourceUnlinkableToken(const String& serverPublicKeyBase64URL);
    WEBCORE_EXPORT static Expected<DestinationUnlinkableToken, String> calculateAndUpdateDestinationUnlinkableToken(const String& serverPublicKeyBase64URL);
    WEBCORE_EXPORT std::optional<String> calculateAndUpdateSourceSecretToken(const String& serverResponseBase64URL);
    WEBCORE_EXPORT static Expected<DestinationSecretToken, String> calculateAndUpdateDestinationSecretToken(const String& serverResponseBase64URL, DestinationUnlinkableToken&);
#endif

    SourceUnlinkableToken& sourceUnlinkableToken() { return m_sourceUnlinkableToken; }
    void setSourceUnlinkableTokenValue(const String& value) { m_sourceUnlinkableToken.valueBase64URL = value; }
    const std::optional<SourceSecretToken>& sourceSecretToken() const { return m_sourceSecretToken; }
    WEBCORE_EXPORT void setSourceSecretToken(SourceSecretToken&&);
    WEBCORE_EXPORT void setDestinationSecretToken(DestinationSecretToken&&);

    static std::optional<uint64_t> appStoreURLAdamID(const URL&);
    bool isSKAdNetworkAttribution() const { return !!m_adamID; }
    std::optional<uint64_t> adamID() const { return m_adamID; };
    void setAdamID(uint64_t adamID) { m_adamID = adamID; };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<PrivateClickMeasurement> decode(Decoder&);

    WEBCORE_EXPORT PrivateClickMeasurement isolatedCopy() const &;
    WEBCORE_EXPORT PrivateClickMeasurement isolatedCopy() &&;

private:
    static Expected<AttributionTriggerData, String> parseAttributionRequestQuery(const URL&);
    bool isValid() const;

#if PLATFORM(COCOA)
    static std::optional<String> calculateAndUpdateUnlinkableToken(const String& serverPublicKeyBase64URL, UnlinkableToken&, const String& contextForLogMessage);
    static std::optional<String> calculateAndUpdateSecretToken(const String& serverResponseBase64URL, UnlinkableToken&, SecretToken&, const String& contextForLogMessage);
#endif

    SourceID m_sourceID;
    SourceSite m_sourceSite;
    AttributionDestinationSite m_destinationSite;
    WallTime m_timeOfAdClick;
    AttributionEphemeral m_isEphemeral;
    std::optional<uint64_t> m_adamID;

    std::optional<AttributionTriggerData> m_attributionTriggerData;
    AttributionTimeToSendData m_timesToSend;

    std::optional<EphemeralNonce> m_ephemeralSourceNonce;
    SourceUnlinkableToken m_sourceUnlinkableToken;
    std::optional<SourceSecretToken> m_sourceSecretToken;
    String m_sourceApplicationBundleID;
};

template<class Encoder>
void PrivateClickMeasurement::encode(Encoder& encoder) const
{
    encoder << m_sourceID.id
        << m_sourceSite.registrableDomain
        << m_destinationSite.registrableDomain
        << m_timeOfAdClick
        << m_ephemeralSourceNonce
        << m_isEphemeral
        << m_attributionTriggerData
        << m_sourceApplicationBundleID
        << m_timesToSend
        << m_adamID;
}

template<class Decoder>
std::optional<PrivateClickMeasurement> PrivateClickMeasurement::decode(Decoder& decoder)
{
    std::optional<uint8_t> sourceID;
    decoder >> sourceID;
    if (!sourceID)
        return std::nullopt;
    
    std::optional<RegistrableDomain> sourceRegistrableDomain;
    decoder >> sourceRegistrableDomain;
    if (!sourceRegistrableDomain)
        return std::nullopt;
    
    std::optional<RegistrableDomain> destinationRegistrableDomain;
    decoder >> destinationRegistrableDomain;
    if (!destinationRegistrableDomain)
        return std::nullopt;
    
    std::optional<WallTime> timeOfAdClick;
    decoder >> timeOfAdClick;
    if (!timeOfAdClick)
        return std::nullopt;

    std::optional<std::optional<EphemeralNonce>> ephemeralSourceNonce;
    decoder >> ephemeralSourceNonce;
    if (!ephemeralSourceNonce)
        return std::nullopt;

    std::optional<AttributionEphemeral> isEphemeral;
    decoder >> isEphemeral;
    if (!isEphemeral)
        return std::nullopt;

    std::optional<std::optional<AttributionTriggerData>> attributionTriggerData;
    decoder >> attributionTriggerData;
    if (!attributionTriggerData)
        return std::nullopt;

    std::optional<String> sourceApplicationBundleID;
    decoder >> sourceApplicationBundleID;
    if (!sourceApplicationBundleID)
        return std::nullopt;

    std::optional<AttributionTimeToSendData> timesToSend;
    decoder >> timesToSend;
    if (!timesToSend)
        return std::nullopt;

    std::optional<std::optional<uint64_t>> adamID;
    decoder >> adamID;
    if (!adamID)
        return std::nullopt;

    PrivateClickMeasurement attribution {
        SourceID { WTFMove(*sourceID) },
        SourceSite { WTFMove(*sourceRegistrableDomain) },
        AttributionDestinationSite { WTFMove(*destinationRegistrableDomain) },
        WTFMove(*sourceApplicationBundleID),
        WTFMove(*timeOfAdClick),
        WTFMove(*isEphemeral)
    };
    attribution.m_ephemeralSourceNonce = WTFMove(*ephemeralSourceNonce);
    attribution.m_attributionTriggerData = WTFMove(*attributionTriggerData);
    attribution.m_timesToSend = WTFMove(*timesToSend);
    attribution.m_adamID = WTFMove(*adamID);

    return attribution;
}

template<class Encoder>
void PrivateClickMeasurement::EphemeralNonce::encode(Encoder& encoder) const
{
    encoder << nonce;
}

template<class Decoder>
std::optional<PrivateClickMeasurement::EphemeralNonce> PrivateClickMeasurement::EphemeralNonce::decode(Decoder& decoder)
{
    std::optional<String> nonce;
    decoder >> nonce;
    if (!nonce)
        return std::nullopt;
    
    return EphemeralNonce { WTFMove(*nonce) };
}

template<class Encoder>
void PrivateClickMeasurement::AttributionTriggerData::encode(Encoder& encoder) const
{
    encoder << data << priority << wasSent << sourceRegistrableDomain << ephemeralDestinationNonce << destinationSite;
}

template<class Decoder>
std::optional<PrivateClickMeasurement::AttributionTriggerData> PrivateClickMeasurement::AttributionTriggerData::decode(Decoder& decoder)
{
    std::optional<uint8_t> data;
    decoder >> data;
    if (!data)
        return std::nullopt;
    
    std::optional<PriorityValue> priority;
    decoder >> priority;
    if (!priority)
        return std::nullopt;
    
    std::optional<WasSent> wasSent;
    decoder >> wasSent;
    if (!wasSent)
        return std::nullopt;
    
    std::optional<std::optional<RegistrableDomain>> sourceRegistrableDomain;
    decoder >> sourceRegistrableDomain;
    if (!sourceRegistrableDomain)
        return std::nullopt;

    std::optional<std::optional<EphemeralNonce>> ephemeralDestinationNonce;
    decoder >> ephemeralDestinationNonce;
    if (!ephemeralDestinationNonce)
        return std::nullopt;

    std::optional<std::optional<RegistrableDomain>> destinationSite;
    decoder >> destinationSite;
    if (!destinationSite)
        return std::nullopt;

    AttributionTriggerData attributionTriggerData { WTFMove(*data), Priority { *priority }, *wasSent };
    attributionTriggerData.sourceRegistrableDomain = WTFMove(*sourceRegistrableDomain);
    attributionTriggerData.ephemeralDestinationNonce = WTFMove(*ephemeralDestinationNonce);
    attributionTriggerData.destinationSite = WTFMove(*destinationSite);
    return attributionTriggerData;
}

} // namespace WebCore

namespace WTF {
template<typename T> struct DefaultHash;

template<> struct DefaultHash<WebCore::PrivateClickMeasurement::SourceSite> : WebCore::PrivateClickMeasurement::SourceSiteHash { };
template<> struct HashTraits<WebCore::PrivateClickMeasurement::SourceSite> : GenericHashTraits<WebCore::PrivateClickMeasurement::SourceSite> {
    static WebCore::PrivateClickMeasurement::SourceSite emptyValue() { return WebCore::PrivateClickMeasurement::SourceSite(WebCore::RegistrableDomain()); }
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
