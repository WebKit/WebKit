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

#include <wtf/KeyValuePair.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
    
struct ContentRuleListResults {
    struct Result {
        bool blockedLoad { false };
        bool madeHTTPS { false };
        bool blockedCookies { false };
        Vector<String> notifications;
        
        bool shouldNotifyApplication() const
        {
            return blockedLoad
                || madeHTTPS
                || blockedCookies
                || !notifications.isEmpty();
        }

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<Result> decode(Decoder&);
    };
    struct Summary {
        bool blockedLoad { false };
        bool madeHTTPS { false };
        bool blockedCookies { false };
        bool hasNotifications { false };

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<Summary> decode(Decoder&);
    };
    using ContentRuleListIdentifier = String;

    Summary summary;
    Vector<std::pair<ContentRuleListIdentifier, Result>> results;
    
    bool shouldNotifyApplication() const
    {
        return summary.blockedLoad
            || summary.madeHTTPS
            || summary.blockedCookies
            || summary.hasNotifications;
    }
    
    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << summary;
        encoder << results;
    }
    template<class Decoder> static Optional<ContentRuleListResults> decode(Decoder& decoder)
    {
        Optional<Summary> summary;
        decoder >> summary;
        if (!summary)
            return WTF::nullopt;
        
        Optional<Vector<std::pair<ContentRuleListIdentifier, Result>>> results;
        decoder >> results;
        if (!results)
            return WTF::nullopt;
        
        return {{
            WTFMove(*summary),
            WTFMove(*results)
        }};
    }
};

template<class Encoder> void ContentRuleListResults::Result::encode(Encoder& encoder) const
{
    encoder << blockedLoad;
    encoder << madeHTTPS;
    encoder << blockedCookies;
    encoder << notifications;
}

template<class Decoder> auto ContentRuleListResults::Result::decode(Decoder& decoder) -> Optional<Result>
{
    Optional<bool> blockedLoad;
    decoder >> blockedLoad;
    if (!blockedLoad)
        return WTF::nullopt;
    
    Optional<bool> madeHTTPS;
    decoder >> madeHTTPS;
    if (!madeHTTPS)
        return WTF::nullopt;
    
    Optional<bool> blockedCookies;
    decoder >> blockedCookies;
    if (!blockedCookies)
        return WTF::nullopt;
    
    Optional<Vector<String>> notifications;
    decoder >> notifications;
    if (!notifications)
        return WTF::nullopt;

    return {{
        WTFMove(*blockedLoad),
        WTFMove(*madeHTTPS),
        WTFMove(*blockedCookies),
        WTFMove(*notifications)
    }};
}

template<class Encoder> void ContentRuleListResults::Summary::encode(Encoder& encoder) const
{
    encoder << blockedLoad;
    encoder << madeHTTPS;
    encoder << blockedCookies;
    encoder << hasNotifications;
}

template<class Decoder> auto ContentRuleListResults::Summary::decode(Decoder& decoder) -> Optional<Summary>
{
    Optional<bool> blockedLoad;
    decoder >> blockedLoad;
    if (!blockedLoad)
        return WTF::nullopt;
    
    Optional<bool> madeHTTPS;
    decoder >> madeHTTPS;
    if (!madeHTTPS)
        return WTF::nullopt;
    
    Optional<bool> blockedCookies;
    decoder >> blockedCookies;
    if (!blockedCookies)
        return WTF::nullopt;
    
    Optional<bool> hasNotifications;
    decoder >> hasNotifications;
    if (!hasNotifications)
        return WTF::nullopt;
    
    return {{
        WTFMove(*blockedLoad),
        WTFMove(*madeHTTPS),
        WTFMove(*blockedCookies),
        WTFMove(*hasNotifications),
    }};
}

}
