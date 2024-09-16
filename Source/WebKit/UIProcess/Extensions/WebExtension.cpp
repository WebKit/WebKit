/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
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
#include "WebExtension.h"

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

static constexpr auto manifestVersionManifestKey = "manifest_version"_s;

static constexpr auto nameManifestKey = "name"_s;
static constexpr auto shortNameManifestKey = "short_name"_s;
static constexpr auto versionManifestKey = "version"_s;
static constexpr auto versionNameManifestKey = "version_name"_s;
static constexpr auto descriptionManifestKey = "description"_s;

static constexpr auto contentSecurityPolicyManifestKey = "content_security_policy"_s;
static constexpr auto contentSecurityPolicyExtensionPagesManifestKey = "extension_pages"_s;

static constexpr auto optionsUIManifestKey = "options_ui"_s;
static constexpr auto optionsUIPageManifestKey = "page"_s;
static constexpr auto optionsPageManifestKey = "options_page"_s;
static constexpr auto chromeURLOverridesManifestKey = "chrome_url_overrides"_s;
static constexpr auto browserURLOverridesManifestKey = "browser_url_overrides"_s;
static constexpr auto newTabManifestKey = "newtab"_s;

bool WebExtension::manifestParsedSuccessfully()
{
    if (m_parsedManifest)
        return !!m_manifestJSON->asObject();

    // If we haven't parsed yet, trigger a parse by calling the getter.
    return !!manifest() && !!manifestObject();
}

double WebExtension::manifestVersion()
{
    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return 0;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/manifest_version
    if (auto value = manifestObject->getDouble(manifestVersionManifestKey))
        return *value;

    return 0;
}

const String& WebExtension::displayName()
{
    populateDisplayStringsIfNeeded();
    return m_displayName;
}

const String& WebExtension::displayShortName()
{
    populateDisplayStringsIfNeeded();
    return m_displayShortName;
}

const String& WebExtension::displayVersion()
{
    populateDisplayStringsIfNeeded();
    return m_displayVersion;
}

const String& WebExtension::displayDescription()
{
    populateDisplayStringsIfNeeded();
    return m_displayDescription;
}

const String& WebExtension::version()
{
    populateDisplayStringsIfNeeded();
    return m_version;
}

void WebExtension::populateDisplayStringsIfNeeded()
{
    if (m_parsedManifestDisplayStrings)
        return;

    m_parsedManifestDisplayStrings = true;

    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/name

    m_displayName = manifestObject->getString(nameManifestKey);
    m_displayShortName = manifestObject->getString(shortNameManifestKey);

    if (m_displayShortName.isEmpty())
        m_displayShortName = m_displayName;

    if (m_displayName.isEmpty())
        recordError(createError(Error::InvalidName));

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/version

    m_version = manifestObject->getString(versionManifestKey);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/version_name

    m_displayVersion = manifestObject->getString(versionNameManifestKey);

    if (m_displayVersion.isEmpty())
        m_displayVersion = m_version;

    if (m_version.isEmpty())
        recordError(createError(Error::InvalidVersion));

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/description

    m_displayDescription = manifestObject->getString(descriptionManifestKey);
    if (m_displayDescription.isEmpty())
        recordError(createError(Error::InvalidDescription));
}

const String& WebExtension::contentSecurityPolicy()
{
    populateContentSecurityPolicyStringsIfNeeded();
    return m_contentSecurityPolicy;
}

void WebExtension::populateContentSecurityPolicyStringsIfNeeded()
{
    if (m_parsedManifestContentSecurityPolicyStrings)
        return;

    m_parsedManifestContentSecurityPolicyStrings = true;

    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/content_security_policy

    if (supportsManifestVersion(3)) {
        if (RefPtr policyObject = manifestObject->getObject(contentSecurityPolicyManifestKey)) {
            m_contentSecurityPolicy = policyObject->getString(contentSecurityPolicyExtensionPagesManifestKey);
            if (!m_contentSecurityPolicy && (!policyObject->size() || policyObject->getValue(contentSecurityPolicyExtensionPagesManifestKey)))
                recordError(createError(Error::InvalidContentSecurityPolicy));
        }
    } else {
        m_contentSecurityPolicy = manifestObject->getString(contentSecurityPolicyManifestKey);
        if (!m_contentSecurityPolicy && manifestObject->getValue(contentSecurityPolicyManifestKey))
            recordError(createError(Error::InvalidContentSecurityPolicy));
    }

    if (!m_contentSecurityPolicy)
        m_contentSecurityPolicy = "script-src 'self'"_s;
}

bool WebExtension::hasOptionsPage()
{
    populatePagePropertiesIfNeeded();
    return !m_optionsPagePath.isEmpty();
}

bool WebExtension::hasOverrideNewTabPage()
{
    populatePagePropertiesIfNeeded();
    return !m_overrideNewTabPagePath.isEmpty();
}

const String& WebExtension::optionsPagePath()
{
    populatePagePropertiesIfNeeded();
    return m_optionsPagePath;
}

const String& WebExtension::overrideNewTabPagePath()
{
    populatePagePropertiesIfNeeded();
    return m_overrideNewTabPagePath;
}

void WebExtension::populatePagePropertiesIfNeeded()
{
    if (m_parsedManifestPageProperties)
        return;

    m_parsedManifestPageProperties = true;

    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/options_ui
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/options_page
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/chrome_url_overrides

    RefPtr optionsObject = manifestObject->getObject(optionsUIManifestKey);
    if (optionsObject) {
        m_optionsPagePath = optionsObject->getString(optionsUIPageManifestKey);
        if (m_optionsPagePath.isEmpty())
            recordError(createError(Error::InvalidOptionsPage));
    } else {
        m_optionsPagePath = manifestObject->getString(optionsPageManifestKey);
        if (m_optionsPagePath.isEmpty() && manifestObject->getValue(optionsPageManifestKey))
            recordError(createError(Error::InvalidOptionsPage));
    }

    RefPtr overridesObject = manifestObject->getObject(browserURLOverridesManifestKey);
    if (!overridesObject)
        overridesObject = manifestObject->getObject(chromeURLOverridesManifestKey);

    if (overridesObject && overridesObject->size()) {
        m_overrideNewTabPagePath = overridesObject->getString(newTabManifestKey);
        if (m_overrideNewTabPagePath.isEmpty() && overridesObject->getValue(newTabManifestKey))
            recordError(createError(Error::InvalidURLOverrides, WEB_UI_STRING("Empty or invalid `newtab` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid new tab entry")));
    } else if (overridesObject)
        recordError(createError(Error::InvalidURLOverrides));
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
