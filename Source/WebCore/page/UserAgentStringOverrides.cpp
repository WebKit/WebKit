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

namespace WebCore {

UserAgentStringPlatform UserAgentStringOverrides::getPlatform()
{
#if OS(MACOS)
    return UserAgentStringPlatform::MACOS;
#else
    return UserAgentStringPlatform::Unknown;
#endif
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
    for (auto&& [domain, platformOverride] : *root) {
        if (domain == "version"_s)
            continue;
        RefPtr platformObject = platformOverride->asObject();
        if (!platformObject) {
            RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: platform is not an Object. Skipping");
            continue;
        }
        for (auto&& [platform, attributesValue] : *platformObject) {
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
                if (auto entry = overridesMap.add({ domain, targetPlatform }, String { })) {
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
                    auto entry = overridesMap.add({ domain, targetPlatform }, Vector<UserAgentOverridesAttributes> { });
                    if (auto* attributeVector = std::get_if<Vector<UserAgentOverridesAttributes>>(&entry.iterator->value))
                        attributeVector->append(userAgent);
                    else
                        RELEASE_LOG(Network, "UserAgentStringOverrides::parseUserAgentOverrides: Skipping %" PUBLIC_LOG_STRING " user agent attribute (%" PUBLIC_LOG_STRING ") because explicit string was previously set.", platform.ascii().data(), attribute.ascii().data());
                } else if (auto overridePlatform = stringToUserAgentPlatform(attribute); overridePlatform != UserAgentStringPlatform::Unknown) {
                    auto entry = overridesMap.add({ domain, targetPlatform }, Vector<UserAgentOverridesAttributes> { });
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

    return overridesMap;
}

StringView UserAgentStringOverrides::userAgentToString(UserAgentStringPlatform defaultPlatform, const Vector<UserAgentOverridesAttributes>& attributes)
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

    // Note that Chrome UAs advertise *both* Chrome/X and Safari/X, but it does
    // not advertise Version/X.
    if (auto agent = attributesToAgent(attributes); agent == UserAgentStringAgent::CHROME) {
        uaString.append(attributesToAgentString(attributes), ' ');
    // Version/X is mandatory *before* Safari/X to be a valid Safari UA. See
    // https://bugs.webkit.org/show_bug.cgi?id=133403 for details.
    } else
        uaString.append("Version/17.0 "_s);

    uaString.append("Safari/605.1.15"_s);

    return uaString.toString();
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

std::optional<String> UserAgentStringOverrides::getUserAgentStringOverrideForDomain(const URL& url, UserAgentStringPlatform userAgentStringPlatform) const
{
    auto entry = m_overrides.find({ url.host().toStringWithoutCopying(), userAgentStringPlatform });
    if (entry == m_overrides.end())
        entry = m_overrides.find({ RegistrableDomain { url }.string(), userAgentStringPlatform });
    if (entry == m_overrides.end())
        entry = m_overrides.find({ url.host().toStringWithoutCopying(), UserAgentStringPlatform::All });
    if (entry == m_overrides.end())
        entry = m_overrides.find({ RegistrableDomain { url }.string(), UserAgentStringPlatform::All });
    if (entry != m_overrides.end()) {
        return WTF::switchOn(entry->value, [userAgentStringPlatform] (const Vector<UserAgentOverridesAttributes>& attributes) -> std::optional<String> {
            if (auto overrideString = userAgentToString(userAgentStringPlatform, attributes); !overrideString.isEmpty())
                return overrideString.toString();
            return std::nullopt;
        }, [] (const String& custom) {
            return std::optional { custom };
        });
    }
    return std::nullopt;
}

} // namespace WebCore
