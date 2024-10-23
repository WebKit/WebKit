/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <wtf/EnumTraits.h>

namespace WebCore {

enum class UserAgentStringPlatform : uint8_t {
    Unknown,
    All,
    Ipad,
    Iphone,
    Mac,
};

enum class UserAgentStringAgent : uint8_t {
    Unknown,
    Chrome,
    Safari12,
    Safari13,
    Safari,
};

using DomainPlatformUserAgentString = std::tuple<String, String, UserAgentStringPlatform>;
using UserAgentOverridesAttributes = std::variant<UserAgentStringPlatform, UserAgentStringAgent>;
using UserAgentOverridesMap = HashMap<DomainPlatformUserAgentString, std::variant<Vector<UserAgentOverridesAttributes>, String>>;

class UserAgentStringOverrides {
    WTF_MAKE_NONCOPYABLE(UserAgentStringOverrides); WTF_MAKE_FAST_ALLOCATED;
public:
    UserAgentStringOverrides() = default;
    WEBCORE_EXPORT std::optional<std::variant<Vector<UserAgentOverridesAttributes>, String>> overrideAttributesForURLAndType(const URL&, UserAgentStringPlatform) const;
    WEBCORE_EXPORT void setUserAgentStringQuirks(const UserAgentOverridesMap&);

    static UserAgentStringAgent stringToUserAgent(const String&);
    WEBCORE_EXPORT static UserAgentStringAgent attributesToAgent(const Vector<UserAgentOverridesAttributes>&);
    WEBCORE_EXPORT static String attributesToAgentVersionString(const Vector<UserAgentOverridesAttributes>&);
    static String userAgentAttributeToString(UserAgentStringAgent);
    WEBCORE_EXPORT static UserAgentStringPlatform getPlatform();
    static String attributesToPlatformString(const Vector<UserAgentOverridesAttributes>&);
    WEBCORE_EXPORT static String attributesToNavigatorPlatformString(const Vector<UserAgentOverridesAttributes>&);
    static String attributesToPlatformVersionString(const Vector<UserAgentOverridesAttributes>&);
    static UserAgentStringPlatform stringToUserAgentPlatform(const String&);
    WEBCORE_EXPORT static UserAgentStringPlatform attributesToPlatform(const Vector<UserAgentOverridesAttributes>&);
    WEBCORE_EXPORT static String attributesToAgentString(const Vector<UserAgentOverridesAttributes>&);
    WEBCORE_EXPORT static String userAgentToString(UserAgentStringPlatform, const Vector<UserAgentOverridesAttributes>&);

    WEBCORE_EXPORT static UserAgentOverridesMap parseUserAgentOverrides(const String&);

private:
    UserAgentOverridesMap m_overrides;
};
} // namespace WebCore

namespace WTF {
template<typename T> struct DefaultHash;
template<> struct DefaultHash<WebCore::UserAgentStringPlatform> : IntHash<WebCore::UserAgentStringPlatform> { };

template<> struct EnumTraits<WebCore::UserAgentStringPlatform> {
    using values = EnumValues<
        WebCore::UserAgentStringPlatform,
        WebCore::UserAgentStringPlatform::Unknown,
        WebCore::UserAgentStringPlatform::All,
        WebCore::UserAgentStringPlatform::Ipad,
        WebCore::UserAgentStringPlatform::Iphone,
        WebCore::UserAgentStringPlatform::Mac
    >;
};

template<> struct EnumTraits<WebCore::UserAgentStringAgent> {
    using values = EnumValues<
        WebCore::UserAgentStringAgent,
        WebCore::UserAgentStringAgent::Unknown,
        WebCore::UserAgentStringAgent::Chrome,
        WebCore::UserAgentStringAgent::Safari12,
        WebCore::UserAgentStringAgent::Safari13,
        WebCore::UserAgentStringAgent::Safari
    >;
};
}
