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

#include "PublicSuffix.h"
#include <wtf/Noncopyable.h>
#include <wtf/Optional.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

namespace WTF {
class URL;
}

namespace WebCore {

class AdClickAttribution {
public:
    using CampaignId = uint32_t;
    using ConversionData = uint32_t;
    using PriorityValue = uint32_t;

    static constexpr uint32_t MaxEntropy = 64;

    struct Campaign {
        explicit Campaign(CampaignId id)
            : id { id }
        {
        }
        
        bool isValid() const
        {
            return id < MaxEntropy;
        }
        
        CampaignId id;
    };

    struct Source {
        explicit Source(const String& host)
#if ENABLE(PUBLIC_SUFFIX_LIST)
            : registrableDomain { WebCore::topPrivatelyControlledDomain(host) }
#else
            : registrableDomain { emptyString() }
#endif
        {
        }

        String registrableDomain;
    };

    struct Destination {
        explicit Destination(const String& host)
#if ENABLE(PUBLIC_SUFFIX_LIST)
            : registrableDomain { WebCore::topPrivatelyControlledDomain(host) }
#else
            : registrableDomain { emptyString() }
#endif
        {
        }

        String registrableDomain;
    };

    struct Priority {
        explicit Priority(PriorityValue value)
        : value { value }
        {
        }
        
        PriorityValue value;
    };
    
    struct Conversion {
        explicit Conversion(ConversionData data, Priority priority)
            : data { data }
            , priority { priority.value }
        {
        }

        bool isValid() const
        {
            return data < MaxEntropy && priority < MaxEntropy;
        }
        
        ConversionData data;
        PriorityValue priority;
    };

    AdClickAttribution(Campaign campaign, const Source& source, const Destination& destination)
        : m_campaign { campaign }
        , m_source { source }
        , m_destination { destination }
        , m_timeOfAdClick { WallTime::now() }
    {
    }

    WEBCORE_EXPORT void setConversion(Conversion&&);
    WEBCORE_EXPORT URL url() const;
    WEBCORE_EXPORT URL referrer() const;
    Optional<WallTime> earliestTimeToSend() const { return m_earliestTimeToSend; };

private:
    bool isValid() const;

    Campaign m_campaign;
    Source m_source;
    Destination m_destination;
    WallTime m_timeOfAdClick;

    Optional<Conversion> m_conversion;
    Optional<WallTime> m_earliestTimeToSend;
};
    
} // namespace WebCore
