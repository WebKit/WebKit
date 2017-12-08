/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <WebCore/HTTPHeaderField.h>
#include <wtf/EnumTraits.h>
#include <wtf/OptionSet.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>

namespace WebKit {

enum class WebsiteAutoplayPolicy {
    Default,
    Allow,
    AllowWithoutSound,
    Deny
};

enum class WebsiteAutoplayQuirk {
    SynthesizedPauseEvents = 1 << 0,
    InheritedUserGestures = 1 << 1,
    ArbitraryUserGestures = 1 << 2,
};

class WebsitePolicies {
public:

    bool contentBlockersEnabled { true };
    OptionSet<WebsiteAutoplayQuirk> allowedAutoplayQuirks;
    WebsiteAutoplayPolicy autoplayPolicy { WebsiteAutoplayPolicy::Default };
    Vector<WebCore::HTTPHeaderField> customHeaderFields;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<WebsitePolicies> decode(Decoder&);
};

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::WebsiteAutoplayPolicy> {
    using values = EnumValues<
        WebKit::WebsiteAutoplayPolicy,
        WebKit::WebsiteAutoplayPolicy::Default,
        WebKit::WebsiteAutoplayPolicy::Allow,
        WebKit::WebsiteAutoplayPolicy::AllowWithoutSound,
        WebKit::WebsiteAutoplayPolicy::Deny
    >;
};

} // namespace WTF

namespace WebKit {

template<class Encoder> void WebsitePolicies::encode(Encoder& encoder) const
{
    encoder << contentBlockersEnabled;
    encoder << autoplayPolicy;
    encoder << allowedAutoplayQuirks;
    encoder << customHeaderFields;
}

template<class Decoder> std::optional<WebsitePolicies> WebsitePolicies::decode(Decoder& decoder)
{
    std::optional<bool> contentBlockersEnabled;
    decoder >> contentBlockersEnabled;
    if (!contentBlockersEnabled)
        return std::nullopt;
    
    std::optional<WebsiteAutoplayPolicy> autoplayPolicy;
    decoder >> autoplayPolicy;
    if (!autoplayPolicy)
        return std::nullopt;

    std::optional<OptionSet<WebsiteAutoplayQuirk>> allowedAutoplayQuirks;
    decoder >> allowedAutoplayQuirks;
    if (!allowedAutoplayQuirks)
        return std::nullopt;
    
    std::optional<Vector<WebCore::HTTPHeaderField>> customHeaderFields;
    decoder >> customHeaderFields;
    if (!customHeaderFields)
        return std::nullopt;

    return { {
        WTFMove(*contentBlockersEnabled),
        WTFMove(*allowedAutoplayQuirks),
        WTFMove(*autoplayPolicy),
        WTFMove(*customHeaderFields),
    } };
}

} // namespace WebKit
