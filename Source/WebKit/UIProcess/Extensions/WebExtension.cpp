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

static constexpr auto contentScriptsManifestKey = "content_scripts"_s;
static constexpr auto contentScriptsMatchesManifestKey = "matches"_s;
static constexpr auto contentScriptsExcludeMatchesManifestKey = "exclude_matches"_s;
static constexpr auto contentScriptsIncludeGlobsManifestKey = "include_globs"_s;
static constexpr auto contentScriptsExcludeGlobsManifestKey = "exclude_globs"_s;
static constexpr auto contentScriptsMatchesAboutBlankManifestKey = "match_about_blank"_s;
static constexpr auto contentScriptsRunAtManifestKey = "run_at"_s;
static constexpr auto contentScriptsDocumentIdleManifestKey = "document_idle"_s;
static constexpr auto contentScriptsDocumentStartManifestKey = "document_start"_s;
static constexpr auto contentScriptsDocumentEndManifestKey = "document_end"_s;
static constexpr auto contentScriptsAllFramesManifestKey = "all_frames"_s;
static constexpr auto contentScriptsJSManifestKey = "js"_s;
static constexpr auto contentScriptsCSSManifestKey = "css"_s;
static constexpr auto contentScriptsWorldManifestKey = "world"_s;
static constexpr auto contentScriptsIsolatedManifestKey = "ISOLATED"_s;
static constexpr auto contentScriptsMainManifestKey = "MAIN"_s;
static constexpr auto contentScriptsCSSOriginManifestKey = "css_origin"_s;
static constexpr auto contentScriptsAuthorManifestKey = "author"_s;
static constexpr auto contentScriptsUserManifestKey = "user"_s;

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

const Vector<WebExtension::InjectedContentData>& WebExtension::staticInjectedContents()
{
    populateContentScriptPropertiesIfNeeded();
    return m_staticInjectedContents;
}

bool WebExtension::hasStaticInjectedContentForURL(const URL& url)
{
    populateContentScriptPropertiesIfNeeded();

    for (auto& injectedContent : m_staticInjectedContents) {
        // FIXME: <https://webkit.org/b/246492> Add support for exclude globs.
        bool isExcluded = false;
        for (auto& excludeMatchPattern : injectedContent.excludeMatchPatterns) {
            if (excludeMatchPattern->matchesURL(url)) {
                isExcluded = true;
                break;
            }
        }

        if (isExcluded)
            continue;

        // FIXME: <https://webkit.org/b/246492> Add support for include globs.
        for (auto& includeMatchPattern : injectedContent.includeMatchPatterns) {
            if (includeMatchPattern->matchesURL(url))
                return true;
        }
    }

    return false;
}

bool WebExtension::hasStaticInjectedContent()
{
    populateContentScriptPropertiesIfNeeded();
    return !m_staticInjectedContents.isEmpty();
}

Vector<String> WebExtension::InjectedContentData::expandedIncludeMatchPatternStrings() const
{
    Vector<String> result;

    for (auto& includeMatchPattern : includeMatchPatterns)
        result.appendVector(includeMatchPattern->expandedStrings());

    return result;
}

Vector<String> WebExtension::InjectedContentData::expandedExcludeMatchPatternStrings() const
{
    Vector<String> result;

    for (auto& excludeMatchPattern : excludeMatchPatterns)
        result.appendVector(excludeMatchPattern->expandedStrings());

    return result;
}

void WebExtension::populateContentScriptPropertiesIfNeeded()
{
    if (m_parsedManifestContentScriptProperties)
        return;

    m_parsedManifestContentScriptProperties = true;

    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/content_scripts

    RefPtr contentScriptsManifestArray = manifestObject->getArray(contentScriptsManifestKey);
    if (!contentScriptsManifestArray || !contentScriptsManifestArray->length()) {
        if (manifestObject->getValue(contentScriptsManifestKey))
            recordError(createError(Error::InvalidContentScripts));
        return;
    }

    auto addInjectedContentData = [this](auto& injectedContentObject) {
        HashSet<Ref<WebExtensionMatchPattern>> includeMatchPatterns;

        // Required. Specifies which pages the specified scripts and stylesheets will be injected into.
        RefPtr matchesArray = injectedContentObject->getArray(contentScriptsMatchesManifestKey);
        if (!matchesArray) {
            recordError(createError(Error::InvalidContentScripts));
            return;
        }

        for (Ref matchPatternStringValue : *matchesArray) {
            auto matchPatternString = matchPatternStringValue->asString();

            if (!matchPatternString)
                continue;

            if (RefPtr matchPattern = WebExtensionMatchPattern::getOrCreate(matchPatternString)) {
                if (matchPattern->isSupported())
                    includeMatchPatterns.add(matchPattern.releaseNonNull());
            }
        }

        if (includeMatchPatterns.isEmpty()) {
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has no specified `matches` entry.", "WKWebExtensionErrorInvalidContentScripts description for missing matches entry")));
            return;
        }

        // Optional. The list of JavaScript files to be injected into matching pages. These are injected in the order they appear in this array.
        RefPtr scriptPaths = injectedContentObject->getArray(contentScriptsJSManifestKey);
        if (!scriptPaths)
            scriptPaths = JSON::Array::create();

        scriptPaths = filterObjects(*scriptPaths, [](auto& value) {
            return !value.asString().isEmpty();
        });

        // Optional. The list of CSS files to be injected into matching pages. These are injected in the order they appear in this array, before any DOM is constructed or displayed for the page.
        RefPtr styleSheetPaths = injectedContentObject->getArray(contentScriptsCSSManifestKey);
        if (!styleSheetPaths)
            styleSheetPaths = JSON::Array::create();

        styleSheetPaths = filterObjects(*styleSheetPaths, [](auto& value) {
            return !value.asString().isEmpty();
        });

        if (!scriptPaths->length() && !styleSheetPaths->length()) {
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has missing or empty 'js' and 'css' arrays.", "WKWebExtensionErrorInvalidContentScripts description for missing or empty 'js' and 'css' arrays")));
            return;
        }

        // Optional. Whether the script should inject into an about:blank frame where the parent or opener frame matches one of the patterns declared in matches. Defaults to false.
        auto matchesAboutBlank = injectedContentObject->getBoolean(contentScriptsMatchesAboutBlankManifestKey).value_or(false);

        HashSet<Ref<WebExtensionMatchPattern>> excludeMatchPatterns;

        // Optional. Excludes pages that this content script would otherwise be injected into.
        RefPtr excludeMatchesArray = injectedContentObject->getArray(contentScriptsExcludeMatchesManifestKey);
        if (!excludeMatchesArray)
            excludeMatchesArray = JSON::Array::create();

        for (Ref matchPatternStringValue : *excludeMatchesArray) {
            auto matchPatternString = matchPatternStringValue->asString();

            if (matchPatternString.isEmpty())
                continue;

            if (RefPtr matchPattern = WebExtensionMatchPattern::getOrCreate(matchPatternString)) {
                if (matchPattern->isSupported())
                    excludeMatchPatterns.add(matchPattern.releaseNonNull());
            }
        }

        // Optional. Applied after matches to include only those URLs that also match this glob.
        RefPtr includeGlobPatternStrings = injectedContentObject->getArray(contentScriptsIncludeGlobsManifestKey);
        if (!includeGlobPatternStrings)
            includeGlobPatternStrings = JSON::Array::create();

        includeGlobPatternStrings = filterObjects(*includeGlobPatternStrings, [](auto& value) {
            return !!value.asString().isEmpty();
        });

        // Optional. Applied after matches to exclude URLs that match this glob.
        RefPtr excludeGlobPatternStrings = injectedContentObject->getArray(contentScriptsExcludeGlobsManifestKey);
        if (!excludeGlobPatternStrings)
            excludeGlobPatternStrings = JSON::Array::create();

        excludeGlobPatternStrings = filterObjects(*excludeGlobPatternStrings, [](auto& value) -> bool {
            return !!value.asString().isEmpty();
        });

        // Optional. The "all_frames" field allows the extension to specify if JavaScript and CSS files should be injected into all frames matching the specified URL requirements or only into the
        // topmost frame in a tab. Defaults to false, meaning that only the top frame is matched. If specified true, it will inject into all frames, even if the frame is not the topmost frame in
        // the tab. Each frame is checked independently for URL requirements, it will not inject into child frames if the URL requirements are not met.
        auto injectsIntoAllFrames = injectedContentObject->getBoolean(contentScriptsAllFramesManifestKey).value_or(false);

        auto injectionTime = InjectionTime::DocumentIdle;
        auto runsAtString = injectedContentObject->getString(contentScriptsRunAtManifestKey);
        if (!runsAtString || runsAtString == contentScriptsDocumentIdleManifestKey)
            injectionTime = InjectionTime::DocumentIdle;
        else if (runsAtString == contentScriptsDocumentStartManifestKey)
            injectionTime = InjectionTime::DocumentStart;
        else if (runsAtString == contentScriptsDocumentEndManifestKey)
            injectionTime = InjectionTime::DocumentEnd;
        else
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has unknown `run_at` value.", "WKWebExtensionErrorInvalidContentScripts description for unknown 'run_at' value")));

        auto contentWorldType = WebExtensionContentWorldType::ContentScript;
        auto worldString = injectedContentObject->getString(contentScriptsWorldManifestKey);
        if (!worldString || worldString == contentScriptsIsolatedManifestKey)
            contentWorldType = WebExtensionContentWorldType::ContentScript;
        else if (worldString == contentScriptsMainManifestKey)
            contentWorldType = WebExtensionContentWorldType::Main;
        else
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has unknown `world` value.", "WKWebExtensionErrorInvalidContentScripts description for unknown 'world' value")));

        auto styleLevel = WebCore::UserStyleLevel::Author;
        auto cssOriginString = injectedContentObject->getString(contentScriptsCSSOriginManifestKey);
        if (!cssOriginString || cssOriginString == contentScriptsAuthorManifestKey)
            styleLevel = WebCore::UserStyleLevel::Author;
        else if (cssOriginString == contentScriptsUserManifestKey)
            styleLevel = WebCore::UserStyleLevel::User;
        else
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has unknown `css_origin` value.", "WKWebExtensionErrorInvalidContentScripts description for unknown 'css_origin' value")));

        InjectedContentData injectedContentData;
        injectedContentData.includeMatchPatterns = WTFMove(includeMatchPatterns);
        injectedContentData.excludeMatchPatterns = WTFMove(excludeMatchPatterns);
        injectedContentData.injectionTime = injectionTime;
        injectedContentData.matchesAboutBlank = matchesAboutBlank;
        injectedContentData.injectsIntoAllFrames = injectsIntoAllFrames;
        injectedContentData.contentWorldType = contentWorldType;
        injectedContentData.styleLevel = styleLevel;
        injectedContentData.scriptPaths = makeStringVector(*scriptPaths);
        injectedContentData.styleSheetPaths = makeStringVector(*styleSheetPaths);
        injectedContentData.includeGlobPatternStrings = makeStringVector(*includeGlobPatternStrings);
        injectedContentData.excludeGlobPatternStrings = makeStringVector(*excludeGlobPatternStrings);

        m_staticInjectedContents.append(WTFMove(injectedContentData));
    };

    for (Ref injectedContentValue : *contentScriptsManifestArray) {
        if (RefPtr injectedContentObject = injectedContentValue->asObject())
            addInjectedContentData(injectedContentObject);
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
