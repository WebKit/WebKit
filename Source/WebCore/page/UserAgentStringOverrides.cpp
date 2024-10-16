/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2012, 2014, 2016 Igalia S.L.
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
#include "UserAgentStringOverrides.h"

#include "Logging.h"
#include "RegistrableDomain.h"
#include <wtf/FileSystem.h>
#include <wtf/JSONValues.h>
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(IOS_FAMILY)
#include <pal/system/ios/UserInterfaceIdiom.h>
#endif

namespace WebCore {

UserAgentStringPlatform UserAgentStringOverrides::getPlatform()
{
#if OS(MACOS)
    return UserAgentStringPlatform::Mac;
#elif PLATFORM(IOS)
    return PAL::currentUserInterfaceIdiomIsSmallScreen() ? UserAgentStringPlatform::Iphone : UserAgentStringPlatform::Ipad;
#else
    return UserAgentStringPlatform::Unknown;
#endif
}

UserAgentStringPlatform UserAgentStringOverrides::stringToUserAgentPlatform(const String& platform)
{
    if (platform == "ipad"_s)
        return UserAgentStringPlatform::Ipad;
    if (platform == "iphone"_s)
        return UserAgentStringPlatform::Iphone;
    if (platform == "mac"_s)
        return UserAgentStringPlatform::Mac;
    if (platform == "all"_s)
        return UserAgentStringPlatform::All;
    return UserAgentStringPlatform::Unknown;
}

UserAgentStringAgent UserAgentStringOverrides::stringToUserAgent(const String& agent)
{
    if (agent == "chrome"_s)
        return UserAgentStringAgent::Chrome;
    if (agent == "safari12"_s)
        return UserAgentStringAgent::Safari12;
    if (agent == "safari13"_s)
        return UserAgentStringAgent::Safari13;
    if (agent == "safari13"_s)
        return UserAgentStringAgent::Safari13;
    if (agent == "safari"_s)
        return UserAgentStringAgent::Safari;
    return UserAgentStringAgent::Unknown;
}

String UserAgentStringOverrides::userAgentAttributeToString(UserAgentStringAgent agent)
{
    switch (agent) {
    case UserAgentStringAgent::Chrome:
        return "Chrome"_s;
    case UserAgentStringAgent::Safari12:
        FALLTHROUGH;
    case UserAgentStringAgent::Safari13:
        FALLTHROUGH;
    case UserAgentStringAgent::Safari:
        return "Safari"_s;
    case UserAgentStringAgent::Unknown:
        return emptyString();
    }
    return emptyString();
}

static String platformToNavigatorPlatformString(UserAgentStringPlatform userAgentStringPlatform)
{
    switch (userAgentStringPlatform) {
    case UserAgentStringPlatform::Ipad:
        return "iPad"_s;
    case UserAgentStringPlatform::Iphone:
        return "iPhone"_s;
    case UserAgentStringPlatform::Mac:
        return "MacIntel"_s;
    case UserAgentStringPlatform::All:
        FALLTHROUGH;
    case UserAgentStringPlatform::Unknown:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
    return emptyString();
}

static String platformToPlatformString(UserAgentStringPlatform userAgentStringPlatform)
{
    switch (userAgentStringPlatform) {
    case UserAgentStringPlatform::Ipad:
        return "iPad"_s;
    case UserAgentStringPlatform::Iphone:
        return "iPhone"_s;
    case UserAgentStringPlatform::Mac:
        return "Macintosh"_s;
    case UserAgentStringPlatform::All:
        FALLTHROUGH;
    case UserAgentStringPlatform::Unknown:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
    return emptyString();
}

static String agentToAgentVersionString(UserAgentStringAgent userAgentStringAgent)
{
    switch (userAgentStringAgent) {
    case UserAgentStringAgent::Chrome:
        return "110.0.0.0"_s;
    case UserAgentStringAgent::Safari12:
        return "12.1.2"_s;
    case UserAgentStringAgent::Safari13:
        return "13.1.2"_s;
    case UserAgentStringAgent::Safari:
        FALLTHROUGH;
    case UserAgentStringAgent::Unknown:
        return "18.1"_s;
    }

    ASSERT_NOT_REACHED();
    return emptyString();
}

static String agentToAgentString(UserAgentStringAgent userAgentStringAgent)
{
    switch (userAgentStringAgent) {
    case UserAgentStringAgent::Chrome:
        return "Chrome"_s;
    case UserAgentStringAgent::Safari12:
        FALLTHROUGH;
    case UserAgentStringAgent::Safari13:
        FALLTHROUGH;
    case UserAgentStringAgent::Safari:
        return "Safari"_s;
    case UserAgentStringAgent::Unknown:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
    ASSERT_NOT_REACHED();
    return emptyString();
}

String UserAgentStringOverrides::attributesToPlatformString(const Vector<UserAgentOverridesAttributes>& attributes)
{
    for (auto attribute : attributes) {
        if (auto platform = std::get_if<UserAgentStringPlatform>(&attribute))
            return platformToPlatformString(*platform);
    }
    return emptyString();
}

String UserAgentStringOverrides::attributesToNavigatorPlatformString(const Vector<UserAgentOverridesAttributes>& attributes)
{
    for (auto attribute : attributes) {
        if (auto platform = std::get_if<UserAgentStringPlatform>(&attribute))
            return platformToNavigatorPlatformString(*platform);
    }
    return emptyString();
}

String UserAgentStringOverrides::attributesToAgentString(const Vector<UserAgentOverridesAttributes>& attributes)
{
    for (auto attribute : attributes) {
        if (auto agent = std::get_if<UserAgentStringAgent>(&attribute))
            return agentToAgentString(*agent);
    }
    return emptyString();
}

String UserAgentStringOverrides::attributesToPlatformVersionString(const Vector<UserAgentOverridesAttributes>& attributes)
{
    UserAgentStringAgent userAgent { UserAgentStringAgent::Unknown };
    UserAgentStringPlatform userAgentPlatform { UserAgentStringPlatform::Unknown };

    for (auto attribute : attributes) {
        if (auto agent = std::get_if<UserAgentStringAgent>(&attribute))
            userAgent = *agent;
        else if (auto platform = std::get_if<UserAgentStringPlatform>(&attribute))
            userAgentPlatform = *platform;
    }

    auto safariVersion = agentToAgentVersionString(userAgent);
    if (userAgent == UserAgentStringAgent::Safari || userAgent == UserAgentStringAgent::Chrome) {
        if (userAgentPlatform == UserAgentStringPlatform::Iphone || userAgentPlatform == UserAgentStringPlatform::Ipad) {
            StringBuilder builder;
            builder.append("CPU "_s);
            if (userAgentPlatform == UserAgentStringPlatform::Iphone)
                builder.append("iPhone "_s);
            builder.append("OS "_s, makeStringByReplacingAll(safariVersion, "."_s, "_"_s), "like Mac OS X"_s);
            return builder.toString();
        }
        return "Intel Mac OS X 10_15_7"_s;
    }
    return "Intel Mac OS X 10_15_7"_s;
}

String UserAgentStringOverrides::attributesToAgentVersionString(const Vector<UserAgentOverridesAttributes>& attributes)
{
    for (auto attribute : attributes) {
        if (auto agent = std::get_if<UserAgentStringAgent>(&attribute))
            return agentToAgentVersionString(*agent);
    }
    return emptyString();
}

String UserAgentStringOverrides::userAgentToString(UserAgentStringPlatform defaultPlatform, const Vector<UserAgentOverridesAttributes>& attributes)
{
    StringBuilder uaString;

    uaString.append("Mozilla/5.0 ("_s);
    auto platform = attributesToPlatformString(attributes);
    if (platform.isEmpty())
        platform = attributesToPlatformString({ defaultPlatform });
    auto platformVersion = attributesToPlatformVersionString(attributes);
    if (platformVersion.isEmpty())
        platformVersion = attributesToPlatformVersionString({ defaultPlatform });
    uaString.append(platform, "; "_s, platformVersion);
    uaString.append(") AppleWebKit/605.1.15 (KHTML, like Gecko) "_s);

    auto agent = attributesToAgent(attributes);
    if (agent == UserAgentStringAgent::Chrome) {
        // Note that Chrome UAs advertise *both* Chrome/X and Safari/X, but it does
        // not advertise Version/X.
        uaString.append(agentToAgentString(agent), "/"_s, agentToAgentVersionString(agent), " "_s);
    } else {
        // Version/X is mandatory *before* Safari/X to be a valid Safari UA. See
        // https://bugs.webkit.org/show_bug.cgi?id=133403 for details.
        uaString.append("Version/"_s, agentToAgentVersionString(agent), " "_s);
    }

    uaString.append("Safari/605.1.15"_s);

    return uaString.toString();
}

UserAgentOverridesMap UserAgentStringOverrides::parseUserAgentOverrides(const String& resourcePath)
{
    constexpr auto overridesFileName = "UserAgentStringOverrides.json"_s;
    auto path = FileSystem::pathByAppendingComponent(resourcePath, overridesFileName);
    auto buffer = FileSystem::readEntireFile(path);
    if (!buffer) {
        RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: Failed to read file: %" PUBLIC_LOG_STRING ". Aborting", path.ascii().data());
        return { };
    }

    RefPtr value = JSON::Value::parseJSON(buffer->span());
    if (!value) {
        RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: Failed to parse JSON from file. Aborting");
        return { };
    }

    RefPtr root = value->asObject();
    if (!root) {
        RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: root is not an Object. Aborting");
        return { };
    }

    if (auto version = root->getInteger("version"_s)) {
        if (*version != 1) {
            RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: Version number not recognized: %d. Aborting.", *version);
            return { };
        }
    } else {
        RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: Version number not specified. Aborting.");
        return { };
    }
    UserAgentOverridesMap overridesMap;
    for (auto&& [domain, pathsOverride] : *root) {
        if (domain == "version"_s)
            continue;
        RefPtr pathsObject = pathsOverride->asObject();
        if (!pathsObject) {
            RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: paths is not an Object. Skipping");
            continue;
        }
        for (auto&& [path, platformOverride] : *pathsObject) {
            RefPtr platformObject = platformOverride->asObject();
            if (!platformObject) {
                RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: platform is not an Object. Skipping");
                continue;
            }
            for (auto&& [platform, attributesValue] : *platformObject) {
                Ref protectedAttributesValue { attributesValue };
                auto targetPlatform = stringToUserAgentPlatform(platform);
                if (targetPlatform == UserAgentStringPlatform::Unknown) {
                    RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: Unknown platform %" PUBLIC_LOG_STRING ". Skipping", platform.ascii().data());
                    continue;
                }
                RefPtr attributes = attributesValue->asArray();
                if (!attributes) {
                    String attributeString = attributesValue->asString();
                    if (attributeString.isEmpty()) {
                        RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: %" PUBLIC_LOG_STRING " attribute are not an array or string. Skipping", platform.ascii().data());
                        continue;
                    }
                    if (auto entry = overridesMap.add({ domain, path, targetPlatform }, String { })) {
                        if (auto* customString = std::get_if<String>(&entry.iterator->value))
                            *customString = attributeString;
                        else
                            RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: Skipping custom %" PUBLIC_LOG_STRING " user agent string (%" PUBLIC_LOG_STRING ") because attributes were previously set.", platform.ascii().data(), attributeString.ascii().data());
                    }
                    continue;
                }
                if (attributes->length() > 2) {
                    RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: Too many attributes for %" PUBLIC_LOG_STRING ". Found %zu. Skipping", platform.ascii().data(), attributes->length());
                    continue;
                }
                for (auto&& attributeString : *attributes) {
                    auto attribute = attributeString->asString();
                    if (attribute.isEmpty()) {
                        RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: Attributes for %" PUBLIC_LOG_STRING " is not a String. Skipping", platform.ascii().data());
                        continue;
                    }

                    if (auto userAgent = stringToUserAgent(attribute); userAgent != UserAgentStringAgent::Unknown) {
                        RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: domain: %s, platform: %s, path: %s, attribute: %s, targetPlatform: %hhu", domain.ascii().data(), platform.ascii().data(), path.ascii().data(), attribute.ascii().data(), targetPlatform);
                        auto entry = overridesMap.add({ domain, path, targetPlatform }, Vector<UserAgentOverridesAttributes> { });
                        if (auto* attributeVector = std::get_if<Vector<UserAgentOverridesAttributes>>(&entry.iterator->value))
                            attributeVector->append(userAgent);
                        else
                            RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: Skipping %" PUBLIC_LOG_STRING " user agent attribute (%" PUBLIC_LOG_STRING ") because explicit string was previously set.", platform.ascii().data(), attribute.ascii().data());
                    } else if (auto overridePlatform = stringToUserAgentPlatform(attribute); overridePlatform != UserAgentStringPlatform::Unknown) {
                        auto entry = overridesMap.add({ domain, path, targetPlatform }, Vector<UserAgentOverridesAttributes> { });
                        if (auto* attributeVector = std::get_if<Vector<UserAgentOverridesAttributes>>(&entry.iterator->value))
                            attributeVector->append(overridePlatform);
                        else
                            RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: Skipping %" PUBLIC_LOG_STRING " platform attribute (%" PUBLIC_LOG_STRING ") because explicit string was previously set.", platform.ascii().data(), attribute.ascii().data());
                    } else {
                        RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: Attributes for %" PUBLIC_LOG_STRING " is not recognized: %" PUBLIC_LOG_STRING ". Skipping", platform.ascii().data(), attribute.ascii().data());
                        continue;
                    }
                }
            }
        }
    }

    return overridesMap;
}

UserAgentStringPlatform UserAgentStringOverrides::attributesToPlatform(const Vector<UserAgentOverridesAttributes>& attributes)
{
    for (auto attribute : attributes) {
        if (auto platform = std::get_if<UserAgentStringPlatform>(&attribute))
            return *platform;
    }
    return UserAgentStringPlatform::Unknown;
}

UserAgentStringAgent UserAgentStringOverrides::attributesToAgent(const Vector<UserAgentOverridesAttributes>& attributes)
{
    for (auto attribute : attributes) {
        if (auto agent = std::get_if<UserAgentStringAgent>(&attribute))
            return *agent;
    }
    return UserAgentStringAgent::Unknown;
}

void UserAgentStringOverrides::setUserAgentStringQuirks(const UserAgentOverridesMap& userAgentStringQuirks)
{
    for (const auto& [domain, userAgent] : userAgentStringQuirks)
        m_overrides.add(domain, userAgent);
}

std::optional<std::variant<Vector<UserAgentOverridesAttributes>, String>> UserAgentStringOverrides::overrideAttributesForURLAndType(const URL& url, UserAgentStringPlatform userAgentStringPlatform) const
{
    if (url.host().isEmpty() || url.path().isEmpty() || userAgentStringPlatform == UserAgentStringPlatform::Unknown)
        return std::nullopt;
    auto entry = m_overrides.find({ url.host().toString(), url.path().toString(), userAgentStringPlatform });
    if (entry == m_overrides.end())
        entry = m_overrides.find({ url.host().toString(), "*"_s, userAgentStringPlatform });
    if (entry == m_overrides.end())
        entry = m_overrides.find({ RegistrableDomain { url }.string(), url.path().toString(), userAgentStringPlatform });
    if (entry == m_overrides.end())
        entry = m_overrides.find({ RegistrableDomain { url }.string(), "*"_s, userAgentStringPlatform });
    if (entry == m_overrides.end())
        entry = m_overrides.find({ url.host().toString(), url.path().toString(), UserAgentStringPlatform::All });
    if (entry == m_overrides.end())
        entry = m_overrides.find({ url.host().toString(), "*"_s, UserAgentStringPlatform::All });
    if (entry == m_overrides.end())
        entry = m_overrides.find({ RegistrableDomain { url }.string(), url.path().toString(), UserAgentStringPlatform::All });
    if (entry == m_overrides.end())
        entry = m_overrides.find({ RegistrableDomain { url }.string(), "*"_s, UserAgentStringPlatform::All });
    if (entry != m_overrides.end())
        return std::optional { entry->value };
    return std::nullopt;
}

} // namespace WebCore
