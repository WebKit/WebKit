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
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class PrivateClickMeasurement {
public:
    using CampaignId = uint32_t;
    using ConversionData = uint32_t;
    using PriorityValue = uint32_t;

    static constexpr uint32_t MaxEntropy = 63;

    struct Campaign {
        Campaign() = default;
        explicit Campaign(CampaignId id)
            : id { id }
        {
        }
        
        bool isValid() const
        {
            return id <= MaxEntropy;
        }
        
        CampaignId id { 0 };
    };

    struct Source {
        Source() = default;
        explicit Source(const URL& url)
            : registrableDomain { url }
        {
        }

        explicit Source(const RegistrableDomain& domain)
            : registrableDomain { domain }
        {
        }

        explicit Source(WTF::HashTableDeletedValueType)
            : registrableDomain(WTF::HashTableDeletedValue)
        {
        }

        bool operator==(const Source& other) const
        {
            return registrableDomain == other.registrableDomain;
        }

        bool matches(const URL& url) const
        {
            return registrableDomain.matches(url);
        }

        bool isHashTableDeletedValue() const
        {
            return registrableDomain.isHashTableDeletedValue();
        }

        static Source deletedValue()
        {
            return Source { WTF::HashTableDeletedValue };
        }

        static void constructDeletedValue(Source& source)
        {
            new (&source) Source;
            source = Source::deletedValue();
        }

        void deleteValue()
        {
            registrableDomain = RegistrableDomain { WTF::HashTableDeletedValue };
        }

        bool isDeletedValue() const
        {
            return isHashTableDeletedValue();
        }

        RegistrableDomain registrableDomain;
    };

    struct SourceHash {
        static unsigned hash(const Source& source)
        {
            return source.registrableDomain.hash();
        }
        
        static bool equal(const Source& a, const Source& b)
        {
            return a == b;
        }

        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    struct Destination {
        Destination() = default;
        explicit Destination(const URL& url)
            : registrableDomain { RegistrableDomain { url } }
        {
        }

        explicit Destination(WTF::HashTableDeletedValueType)
            : registrableDomain { WTF::HashTableDeletedValue }
        {
        }

        explicit Destination(RegistrableDomain&& domain)
            : registrableDomain { WTFMove(domain) }
        {
        }
        
        bool operator==(const Destination& other) const
        {
            return registrableDomain == other.registrableDomain;
        }

        bool matches(const URL& url) const
        {
            return registrableDomain == RegistrableDomain { url };
        }
        
        bool isHashTableDeletedValue() const
        {
            return registrableDomain.isHashTableDeletedValue();
        }

        static Destination deletedValue()
        {
            return Destination { WTF::HashTableDeletedValue };
        }

        static void constructDeletedValue(Destination& destination)
        {
            new (&destination) Destination;
            destination = Destination::deletedValue();
        }

        void deleteValue()
        {
            registrableDomain = RegistrableDomain { WTF::HashTableDeletedValue };
        }

        bool isDeletedValue() const
        {
            return isHashTableDeletedValue();
        }

        RegistrableDomain registrableDomain;
    };

    struct DestinationHash {
        static unsigned hash(const Destination& destination)
        {
            return destination.registrableDomain.hash();
        }
        
        static bool equal(const Destination& a, const Destination& b)
        {
            return a == b;
        }

        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    struct Priority {
        explicit Priority(PriorityValue value)
        : value { value }
        {
        }
        
        PriorityValue value;
    };
    
    struct Conversion {
        enum class WasSent : bool { No, Yes };
        
        Conversion(ConversionData data, Priority priority, WasSent wasSent = WasSent::No)
            : data { data }
            , priority { priority.value }
            , wasSent { wasSent }
        {
        }

        bool isValid() const
        {
            return data <= MaxEntropy && priority <= MaxEntropy;
        }
        
        ConversionData data;
        PriorityValue priority;
        WasSent wasSent = WasSent::No;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<Conversion> decode(Decoder&);
    };

    PrivateClickMeasurement() = default;
    PrivateClickMeasurement(Campaign campaign, const Source& source, const Destination& destination)
        : m_campaign { campaign }
        , m_source { source }
        , m_destination { destination }
        , m_timeOfAdClick { WallTime::now() }
    {
    }

    WEBCORE_EXPORT static Expected<Conversion, String> parseConversionRequest(const URL& redirectURL);
    WEBCORE_EXPORT Optional<Seconds> convertAndGetEarliestTimeToSend(Conversion&&);
    WEBCORE_EXPORT bool hasHigherPriorityThan(const PrivateClickMeasurement&) const;
    WEBCORE_EXPORT URL reportURL() const;
    WEBCORE_EXPORT Ref<JSON::Object> json() const;
    const Source& source() const { return m_source; };
    const Destination& destination() const { return m_destination; };
    Optional<WallTime> earliestTimeToSend() const { return m_earliestTimeToSend; };
    WEBCORE_EXPORT void markAsExpired();
    WEBCORE_EXPORT bool hasExpired() const;
    WEBCORE_EXPORT void markConversionAsSent();
    WEBCORE_EXPORT bool wasConversionSent() const;

    bool isEmpty() const { return m_source.registrableDomain.isEmpty(); };

    WEBCORE_EXPORT String toString() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<PrivateClickMeasurement> decode(Decoder&);

private:
    bool isValid() const;
    static bool debugModeEnabled();

    Campaign m_campaign;
    Source m_source;
    Destination m_destination;
    WallTime m_timeOfAdClick;

    Optional<Conversion> m_conversion;
    Optional<WallTime> m_earliestTimeToSend;
};

template<class Encoder>
void PrivateClickMeasurement::encode(Encoder& encoder) const
{
    encoder << m_campaign.id << m_source.registrableDomain << m_destination.registrableDomain << m_timeOfAdClick << m_conversion << m_earliestTimeToSend;
}

template<class Decoder>
Optional<PrivateClickMeasurement> PrivateClickMeasurement::decode(Decoder& decoder)
{
    Optional<CampaignId> campaignId;
    decoder >> campaignId;
    if (!campaignId)
        return WTF::nullopt;
    
    Optional<RegistrableDomain> sourceRegistrableDomain;
    decoder >> sourceRegistrableDomain;
    if (!sourceRegistrableDomain)
        return WTF::nullopt;
    
    Optional<RegistrableDomain> destinationRegistrableDomain;
    decoder >> destinationRegistrableDomain;
    if (!destinationRegistrableDomain)
        return WTF::nullopt;
    
    Optional<WallTime> timeOfAdClick;
    decoder >> timeOfAdClick;
    if (!timeOfAdClick)
        return WTF::nullopt;
    
    Optional<Optional<Conversion>> conversion;
    decoder >> conversion;
    if (!conversion)
        return WTF::nullopt;
    
    Optional<Optional<WallTime>> earliestTimeToSend;
    decoder >> earliestTimeToSend;
    if (!earliestTimeToSend)
        return WTF::nullopt;
    
    PrivateClickMeasurement attribution { Campaign { WTFMove(*campaignId) }, Source { WTFMove(*sourceRegistrableDomain) }, Destination { WTFMove(*destinationRegistrableDomain) } };
    attribution.m_conversion = WTFMove(*conversion);
    attribution.m_earliestTimeToSend = WTFMove(*earliestTimeToSend);
    
    return attribution;
}

template<class Encoder>
void PrivateClickMeasurement::Conversion::encode(Encoder& encoder) const
{
    encoder << data << priority << wasSent;
}

template<class Decoder>
Optional<PrivateClickMeasurement::Conversion> PrivateClickMeasurement::Conversion::decode(Decoder& decoder)
{
    Optional<ConversionData> data;
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
    
    return Conversion { WTFMove(*data), Priority { *priority }, *wasSent };
}

} // namespace WebCore

namespace WTF {
template<typename T> struct DefaultHash;

template<> struct DefaultHash<WebCore::PrivateClickMeasurement::Source> : WebCore::PrivateClickMeasurement::SourceHash { };
template<> struct HashTraits<WebCore::PrivateClickMeasurement::Source> : GenericHashTraits<WebCore::PrivateClickMeasurement::Source> {
    static WebCore::PrivateClickMeasurement::Source emptyValue() { return { }; }
    static void constructDeletedValue(WebCore::PrivateClickMeasurement::Source& slot) { WebCore::PrivateClickMeasurement::Source::constructDeletedValue(slot); }
    static bool isDeletedValue(const WebCore::PrivateClickMeasurement::Source& slot) { return slot.isDeletedValue(); }
};

template<> struct DefaultHash<WebCore::PrivateClickMeasurement::Destination> : WebCore::PrivateClickMeasurement::DestinationHash { };
template<> struct HashTraits<WebCore::PrivateClickMeasurement::Destination> : GenericHashTraits<WebCore::PrivateClickMeasurement::Destination> {
    static WebCore::PrivateClickMeasurement::Destination emptyValue() { return { }; }
    static void constructDeletedValue(WebCore::PrivateClickMeasurement::Destination& slot) { WebCore::PrivateClickMeasurement::Destination::constructDeletedValue(slot); }
    static bool isDeletedValue(const WebCore::PrivateClickMeasurement::Destination& slot) { return slot.isDeletedValue(); }
};
}
