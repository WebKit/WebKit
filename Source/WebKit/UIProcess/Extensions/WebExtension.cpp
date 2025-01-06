/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
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

#include "config.h"
#include "WebExtension.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "Logging.h"

#include "WebExtensionConstants.h"
#include "WebExtensionPermission.h"
#include "WebExtensionUtilities.h"
#include <WebCore/LocalizedStrings.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/TextResourceDecoder.h>
#include <wtf/FileSystem.h>
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

using namespace WebCore;

static constexpr auto defaultLocaleManifestKey = "default_locale"_s;

static constexpr auto iconsManifestKey = "icons"_s;

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
static constexpr auto iconVariantsManifestKey = "icon_variants"_s;
static constexpr auto colorSchemesManifestKey = "color_schemes"_s;
static constexpr auto lightManifestKey = "light"_s;
static constexpr auto darkManifestKey = "dark"_s;
static constexpr auto anyManifestKey = "any"_s;
#endif

static constexpr auto actionManifestKey = "action"_s;
static constexpr auto browserActionManifestKey = "browser_action"_s;
static constexpr auto pageActionManifestKey = "page_action"_s;

static constexpr auto defaultIconManifestKey = "default_icon"_s;
static constexpr auto defaultTitleManifestKey = "default_title"_s;
static constexpr auto defaultPopupManifestKey = "default_popup"_s;

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
static constexpr auto contentScriptsIsolatedManifestKey = "isolated"_s;
static constexpr auto contentScriptsMainManifestKey = "main"_s;
static constexpr auto contentScriptsCSSOriginManifestKey = "css_origin"_s;
static constexpr auto contentScriptsAuthorManifestKey = "author"_s;
static constexpr auto contentScriptsUserManifestKey = "user"_s;

static constexpr auto optionsUIManifestKey = "options_ui"_s;
static constexpr auto optionsUIPageManifestKey = "page"_s;
static constexpr auto optionsPageManifestKey = "options_page"_s;
static constexpr auto chromeURLOverridesManifestKey = "chrome_url_overrides"_s;
static constexpr auto browserURLOverridesManifestKey = "browser_url_overrides"_s;
static constexpr auto newTabManifestKey = "newtab"_s;

static constexpr auto backgroundManifestKey = "background"_s;
static constexpr auto backgroundPageManifestKey = "page"_s;
static constexpr auto backgroundServiceWorkerManifestKey = "service_worker"_s;
static constexpr auto backgroundScriptsManifestKey = "scripts"_s;
static constexpr auto backgroundPersistentManifestKey = "persistent"_s;
static constexpr auto backgroundPageTypeKey = "type"_s;
static constexpr auto backgroundPageTypeModuleValue = "module"_s;
static constexpr auto backgroundPreferredEnvironmentManifestKey = "preferred_environment"_s;
static constexpr auto backgroundDocumentManifestKey = "document"_s;

static constexpr auto generatedBackgroundPageFilename = "_generated_background_page.html"_s;
static constexpr auto generatedBackgroundServiceWorkerFilename = "_generated_service_worker.js"_s;

static constexpr auto permissionsManifestKey = "permissions"_s;
static constexpr auto optionalPermissionsManifestKey = "optional_permissions"_s;
static constexpr auto hostPermissionsManifestKey = "host_permissions"_s;
static constexpr auto optionalHostPermissionsManifestKey = "optional_host_permissions"_s;

static constexpr auto externallyConnectableManifestKey = "externally_connectable"_s;
static constexpr auto externallyConnectableMatchesManifestKey = "matches"_s;
static constexpr auto externallyConnectableIDsManifestKey = "ids"_s;

static constexpr auto devtoolsPageManifestKey = "devtools_page"_s;

static constexpr auto webAccessibleResourcesManifestKey = "web_accessible_resources"_s;
static constexpr auto webAccessibleResourcesResourcesManifestKey = "resources"_s;
static constexpr auto webAccessibleResourcesMatchesManifestKey = "matches"_s;

static constexpr auto commandsManifestKey = "commands"_s;
static constexpr auto commandsSuggestedKeyManifestKey = "suggested_key"_s;
static constexpr auto commandsDescriptionKeyManifestKey = "description"_s;

static constexpr auto declarativeNetRequestManifestKey = "declarative_net_request"_s;
static constexpr auto declarativeNetRequestRulesManifestKey = "rule_resources"_s;
static constexpr auto declarativeNetRequestRulesetIDManifestKey = "id"_s;
static constexpr auto declarativeNetRequestRuleEnabledManifestKey = "enabled"_s;
static constexpr auto declarativeNetRequestRulePathManifestKey = "path"_s;

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
static constexpr auto sidebarActionManifestKey = "sidebar_action"_s;
static constexpr auto sidePanelManifestKey = "side_panel"_s;
static constexpr auto sidebarActionTitleManifestKey = "default_title"_s;
static constexpr auto sidebarActionPathManifestKey = "default_panel"_s;
static constexpr auto sidePanelPathManifestKey = "default_path"_s;
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

static const size_t maximumNumberOfShortcutCommands = 4;

WebExtension::WebExtension(Resources&& resources)
    : m_manifestJSON(JSON::Value::null())
    , m_resources(WTFMove(resources))
{
}

bool WebExtension::parseManifest(StringView manifestString)
{
    RefPtr manifestValue = JSON::Value::parseJSON(manifestString);
    if (!manifestValue) {
        recordError(createError(Error::InvalidManifest));
        return false;
    }

    RefPtr manifestObject = manifestValue->asObject();
    if (!manifestObject) {
        recordError(createError(Error::InvalidManifest));
        return false;
    }

    // Set to the unlocalized manifest for now so calls to manifestParsedSuccessfully() during this will be true.
    // This is needed for WebExtensionLocalization to properly get the defaultLocale() while we are mid-parse.
    m_manifestJSON = *manifestObject;

    if (auto defaultLocale = manifestObject->getString(defaultLocaleManifestKey); !defaultLocale.isNull()) {
        auto parsedLocale = parseLocale(manifestObject->getString(defaultLocaleManifestKey));
        if (!parsedLocale.languageCode.isEmpty()) {
            if (supportedLocales().contains(defaultLocale))
                m_defaultLocale = defaultLocale;
            else
                recordError(createError(Error::InvalidDefaultLocale, WEB_UI_STRING("Unable to find `default_locale` in “_locales” folder.", "Error description for missing default_locale")));
        } else
            recordError(createError(Error::InvalidDefaultLocale));
    }

    Ref localization = WebExtensionLocalization::create(*this);
    m_localization = localization.copyRef();

    RefPtr localizedManifestObject = localization->localizedJSONforJSON(manifestObject);
    if (!localizedManifestObject) {
        m_manifestJSON = JSON::Value::null();
        recordError(createError(Error::InvalidManifest));
        return false;
    }

    m_manifestJSON = localizedManifestObject.releaseNonNull();

    return true;
}

RefPtr<const JSON::Object> WebExtension::manifestObject()
{
    if (m_parsedManifest)
        return m_manifestJSON->asObject();

    m_parsedManifest = true;

    RefPtr<API::Error> error;
    auto manifestString = resourceStringForPath("manifest.json"_s, error);
    if (error) {
        recordErrorIfNeeded(error);
        return nullptr;
    }

    if (!parseManifest(manifestString))
        return nullptr;

    return m_manifestJSON->asObject();
}

bool WebExtension::manifestParsedSuccessfully()
{
    return !!manifestObject();
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

RefPtr<API::Data> WebExtension::serializeManifest()
{
    Ref manifestJSON = m_manifestJSON;
    if (!manifestJSON)
        return nullptr;

    return API::Data::create(manifestJSON->toJSONString().utf8().span());
}

RefPtr<API::Data> WebExtension::serializeLocalization()
{
    if (!m_localization || !m_localization->localizationJSON())
        return nullptr;

    return API::Data::create(m_localization->localizationJSON()->toJSONString().utf8().span());
}

RefPtr<WebExtensionLocalization> WebExtension::localization()
{
    if (!manifestParsedSuccessfully())
        return nullptr;

    return m_localization;
}

bool WebExtension::hasRequestedPermission(String permission)
{
    populatePermissionsPropertiesIfNeeded();
    return m_permissions.contains(permission);
}

bool WebExtension::isWebAccessibleResource(const URL& resourceURL, const URL& pageURL)
{
    populateWebAccessibleResourcesIfNeeded();

    auto resourcePath = resourceURL.path().toString();

    // The path is expected to match without the prefix slash.
    ASSERT(resourcePath.startsWith('/'));
    resourcePath = resourcePath.substring(1);

    for (auto& data : m_webAccessibleResources) {
        // If matchPatterns is empty, these resources are allowed on any page.
        bool allowed = data.matchPatterns.isEmpty();
        for (Ref matchPattern : data.matchPatterns) {
            if (matchPattern->matchesURL(pageURL)) {
                allowed = true;
                break;
            }
        }

        if (!allowed)
            continue;

        for (auto& pathPattern : data.resourcePathPatterns) {
            if (WebCore::matchesWildcardPattern(pathPattern, resourcePath))
                return true;
        }
    }

    return false;
}

void WebExtension::parseWebAccessibleResourcesVersion3()
{
    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    if (RefPtr resourcesArray = manifestObject->getArray(webAccessibleResourcesManifestKey)) {
        bool errorOccured = false;
        for (Ref resource : *resourcesArray) {
            if (RefPtr resourceObject = resource->asObject()) {
                RefPtr pathsArray = resourceObject->getArray(webAccessibleResourcesResourcesManifestKey);
                if (pathsArray) {
                    pathsArray = filterObjects(*pathsArray, [](auto& value) {
                        return !value.asString().isEmpty();
                    });
                } else {
                    errorOccured = true;
                    continue;
                }

                RefPtr matchesArray = resourceObject->getArray(webAccessibleResourcesMatchesManifestKey);
                if (matchesArray) {
                    matchesArray = filterObjects(*matchesArray, [](auto& value) {
                        return !value.asString().isEmpty();
                    });
                } else {
                    errorOccured = true;
                    continue;
                }

                if (!pathsArray->length() || !matchesArray->length())
                    continue;

                MatchPatternSet matchPatterns;
                for (Ref match : *matchesArray) {
                    if (RefPtr matchPattern = WebExtensionMatchPattern::getOrCreate(match->asString())) {
                        if (matchPattern->isSupported())
                            matchPatterns.add(matchPattern.releaseNonNull());
                        else
                            errorOccured = true;
                    }
                }

                if (matchPatterns.isEmpty()) {
                    errorOccured = true;
                    continue;
                }

                m_webAccessibleResources.append({ WTFMove(matchPatterns), makeStringVector(*pathsArray) });
            }
        }

        if (errorOccured)
            recordError(createError(Error::InvalidWebAccessibleResources));
    } else if (manifestObject->getValue(webAccessibleResourcesManifestKey))
        recordError(createError(Error::InvalidWebAccessibleResources));
}

void WebExtension::parseWebAccessibleResourcesVersion2()
{
    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    if (RefPtr resourcesArray = manifestObject->getArray(webAccessibleResourcesManifestKey)) {
        resourcesArray = filterObjects(*resourcesArray, [](auto& value) {
            return !value.asString().isEmpty();
        });

        m_webAccessibleResources.append({ { }, makeStringVector(*resourcesArray) });
    } else if (manifestObject->getValue(webAccessibleResourcesManifestKey))
        recordError(createError(Error::InvalidWebAccessibleResources));
}

void WebExtension::populateWebAccessibleResourcesIfNeeded()
{
    if (m_parsedManifestWebAccessibleResources)
        return;

    m_parsedManifestWebAccessibleResources = true;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/web_accessible_resources
    if (supportsManifestVersion(3))
        parseWebAccessibleResourcesVersion3();
    else
        parseWebAccessibleResourcesVersion2();
}

URL WebExtension::resourceFileURLForPath(const String& originalPath)
{
    ASSERT(originalPath);

    String path = originalPath;
    if (path.startsWith('/'))
        path = path.substring(1);

    if (!path.length() || m_resourceBaseURL.isEmpty())
        return { };

    URL result { m_resourceBaseURL, path };
    if (!FileSystem::fileExists(result.fileSystemPath()))
        return { };

    // Don't allow escaping the base URL with "../".
    auto basePath = FileSystem::realPath(m_resourceBaseURL.fileSystemPath());
    auto resourcePath = FileSystem::realPath(result.fileSystemPath());
    if (!resourcePath.startsWith(basePath)) {
        RELEASE_LOG_ERROR(Extensions, "Resource URL path escape attempt: %s", resourcePath.utf8().data());
        return { };
    }

    return result;
}

String WebExtension::resourceMIMETypeForPath(const String& path)
{
    auto dataPrefix = "data:"_s;
    if (path.startsWith(dataPrefix)) {
        auto mimeTypePosition = path.find(';');
        if (mimeTypePosition != notFound)
            return path.substring(dataPrefix.length(), mimeTypePosition - dataPrefix.length());
        return defaultMIMEType();
    }

    return MIMETypeRegistry::mimeTypeForPath(path);
}

String WebExtension::resourceStringForPath(const String& originalPath, RefPtr<API::Error>& outError, CacheResult cacheResult, SuppressNotFoundErrors suppressErrors)
{
    ASSERT(originalPath);

    String path = originalPath;

    // Remove leading slash to normalize the path for lookup/storage in the cache dictionary.
    if (path.startsWith('/'))
        path = path.substring(1);

    if (RefPtr cachedData = m_resources.get(path))
        return String::fromUTF8(cachedData->span());

    if (path == generatedBackgroundPageFilename || path == generatedBackgroundServiceWorkerFilename)
        return generatedBackgroundContent();

    RefPtr data = resourceDataForPath(path, outError, cacheResult, suppressErrors);
    if (!data)
        return nullString();

    if (!data->size())
        return emptyString();

    auto mimeType = MIMETypeRegistry::mimeTypeForPath(path);
    RefPtr decoder = TextResourceDecoder::create(mimeType, { }, true);
    return decoder->decode(data->span());
}

static int toAPI(WebExtension::Error error)
{
    switch (error) {
    case WebExtension::Error::Unknown:
        return static_cast<int>(WebExtension::APIError::Unknown);
    case WebExtension::Error::ResourceNotFound:
        return static_cast<int>(WebExtension::APIError::ResourceNotFound);
    case WebExtension::Error::InvalidManifest:
        return static_cast<int>(WebExtension::APIError::InvalidManifest);
    case WebExtension::Error::UnsupportedManifestVersion:
        return static_cast<int>(WebExtension::APIError::UnsupportedManifestVersion);
    case WebExtension::Error::InvalidDeclarativeNetRequest:
        return static_cast<int>(WebExtension::APIError::InvalidDeclarativeNetRequestEntry);
    case WebExtension::Error::InvalidBackgroundPersistence:
        return static_cast<int>(WebExtension::APIError::InvalidBackgroundPersistence);
    case WebExtension::Error::InvalidResourceCodeSignature:
        return static_cast<int>(WebExtension::APIError::InvalidResourceCodeSignature);
    case WebExtension::Error::InvalidArchive:
        return static_cast<int>(WebExtension::APIError::InvalidArchive);
    case WebExtension::Error::InvalidAction:
    case WebExtension::Error::InvalidActionIcon:
    case WebExtension::Error::InvalidBackgroundContent:
    case WebExtension::Error::InvalidCommands:
    case WebExtension::Error::InvalidContentScripts:
    case WebExtension::Error::InvalidContentSecurityPolicy:
    case WebExtension::Error::InvalidDefaultLocale:
    case WebExtension::Error::InvalidDescription:
    case WebExtension::Error::InvalidExternallyConnectable:
    case WebExtension::Error::InvalidIcon:
    case WebExtension::Error::InvalidName:
    case WebExtension::Error::InvalidOptionsPage:
    case WebExtension::Error::InvalidURLOverrides:
    case WebExtension::Error::InvalidVersion:
    case WebExtension::Error::InvalidWebAccessibleResources:
        return static_cast<int>(WebExtension::APIError::InvalidManifestEntry);
    }

    ASSERT_NOT_REACHED();
    return static_cast<int>(WebExtension::APIError::Unknown);
}

Ref<API::Error> WebExtension::createError(Error error, const String& customLocalizedDescription, RefPtr<API::Error> underlyingError)
{
    auto errorCode = toAPI(error);
    String localizedDescription;

    switch (error) {
    case Error::Unknown:
        localizedDescription = WEB_UI_STRING("An unknown error has occurred.", "WKWebExtensionErrorUnknown description");
        break;

    case Error::ResourceNotFound:
        ASSERT(customLocalizedDescription);
        break;

    case Error::InvalidManifest:
        if (underlyingError && !underlyingError->localizedDescription().isEmpty())
            localizedDescription = WEB_UI_FORMAT_STRING("Unable to parse manifest: %s", "WKWebExtensionErrorInvalidManifest description, because of a JSON error", underlyingError->localizedDescription().utf8().data());
        else
            localizedDescription = WEB_UI_STRING("Unable to parse manifest because of an unexpected format.", "WKWebExtensionErrorInvalidManifest description");
        break;

    case Error::UnsupportedManifestVersion:
        localizedDescription = WEB_UI_STRING("An unsupported `manifest_version` was specified.", "WKWebExtensionErrorUnsupportedManifestVersion description");
        break;

    case Error::InvalidAction:
        if (supportsManifestVersion(3))
            localizedDescription = WEB_UI_STRING("Missing or empty `action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for action only");
        else
            localizedDescription = WEB_UI_STRING("Missing or empty `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for browser_action or page_action");
        break;

    case Error::InvalidActionIcon: {
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
        if (RefPtr actionObject = m_actionObject) {
            if (actionObject->getValue(iconVariantsManifestKey)) {
                if (supportsManifestVersion(3))
                    localizedDescription = WEB_UI_STRING("Empty or invalid `icon_variants` for the `action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for icon_variants in action only");
                else
                    localizedDescription = WEB_UI_STRING("Empty or invalid `icon_variants` for the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for icon_variants in browser_action or page_action");
            } else {
                if (supportsManifestVersion(3))
                    localizedDescription = WEB_UI_STRING("Empty or invalid `default_icon` for the `action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for default_icon in action only");
                else
                    localizedDescription = WEB_UI_STRING("Empty or invalid `default_icon` for the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for default_icon in browser_action or page_action");
            }
        } else
#endif
        if (supportsManifestVersion(3))
            localizedDescription = WEB_UI_STRING("Empty or invalid `default_icon` for the `action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for default_icon in action only");
        else
            localizedDescription = WEB_UI_STRING("Empty or invalid `default_icon` for the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for default_icon in browser_action or page_action");
        break;
    }

    case Error::InvalidBackgroundContent:
        localizedDescription = WEB_UI_STRING("Empty or invalid `background` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for background");
        break;

    case Error::InvalidCommands:
        localizedDescription = WEB_UI_STRING("Invalid `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for commands");
        break;

    case Error::InvalidContentScripts:
        localizedDescription = WEB_UI_STRING("Empty or invalid `content_scripts` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for content_scripts");
        break;

    case Error::InvalidContentSecurityPolicy:
        localizedDescription = WEB_UI_STRING("Empty or invalid `content_security_policy` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for content_security_policy");
        break;

    case Error::InvalidDeclarativeNetRequest:
        if (underlyingError && !underlyingError->localizedDescription().isEmpty())
            localizedDescription = WEB_UI_FORMAT_STRING("Unable to parse `declarativeNetRequest` rules: %s", "WKWebExtensionErrorInvalidDeclarativeNetRequest description, because of a JSON error", underlyingError->localizedDescription().utf8().data());
        else
            localizedDescription = WEB_UI_STRING("Unable to parse `declarativeNetRequest` rules because of an unexpected error.", "WKWebExtensionErrorInvalidDeclarativeNetRequest description");
        break;

    case Error::InvalidDefaultLocale:
        localizedDescription = WEB_UI_STRING("Empty or invalid `default_locale` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for default_locale");
        break;

    case Error::InvalidDescription:
        localizedDescription = WEB_UI_STRING("Missing or empty `description` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for description");
        break;

    case Error::InvalidExternallyConnectable:
        localizedDescription = WEB_UI_STRING("Empty or invalid `externally_connectable` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for externally_connectable");
        break;

    case Error::InvalidIcon:
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
        if (manifestObject()->getValue(iconVariantsManifestKey))
            localizedDescription = WEB_UI_STRING("Empty or invalid `icon_variants` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for icon_variants");
        else
#endif
            localizedDescription = WEB_UI_STRING("Missing or empty `icons` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for icons");
        break;

    case Error::InvalidName:
        localizedDescription = WEB_UI_STRING("Missing or empty `name` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for name");
        break;

    case Error::InvalidOptionsPage:
        if (manifestObject()->getValue(optionsUIManifestKey))
            localizedDescription = WEB_UI_STRING("Empty or invalid `options_ui` manifest entry", "WKWebExtensionErrorInvalidManifestEntry description for options UI");
        else
            localizedDescription = WEB_UI_STRING("Empty or invalid `options_page` manifest entry", "WKWebExtensionErrorInvalidManifestEntry description for options page");
        break;

    case Error::InvalidURLOverrides:
        if (manifestObject()->getValue(browserURLOverridesManifestKey))
            localizedDescription = WEB_UI_STRING("Empty or invalid `browser_url_overrides` manifest entry", "WKWebExtensionErrorInvalidManifestEntry description for browser URL overrides");
        else
            localizedDescription = WEB_UI_STRING("Empty or invalid `chrome_url_overrides` manifest entry", "WKWebExtensionErrorInvalidManifestEntry description for chrome URL overrides");
        break;

    case Error::InvalidVersion:
        localizedDescription = WEB_UI_STRING("Missing or empty `version` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for version");
        break;

    case Error::InvalidWebAccessibleResources:
        localizedDescription = WEB_UI_STRING("Invalid `web_accessible_resources` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for web_accessible_resources");
        break;

    case Error::InvalidBackgroundPersistence:
        localizedDescription = WEB_UI_STRING("Invalid `persistent` manifest entry.", "WKWebExtensionErrorInvalidBackgroundPersistence description");
        break;

    case Error::InvalidArchive:
        localizedDescription = WEB_UI_STRING("Invalid or corrupt extension archive.", "WKWebExtensionErrorInvalidArchive description");
        break;

    case Error::InvalidResourceCodeSignature:
        ASSERT(customLocalizedDescription);
        break;
    }

    if (!customLocalizedDescription.isEmpty())
        localizedDescription = customLocalizedDescription;

    return API::Error::create({ "WKWebExtensionErrorDomain"_s, errorCode, { }, localizedDescription });
}

Vector<Ref<API::Error>> WebExtension::errors()
{
    populateDisplayStringsIfNeeded();
    populateActionPropertiesIfNeeded();
    populateBackgroundPropertiesIfNeeded();
    populateContentScriptPropertiesIfNeeded();
    populatePermissionsPropertiesIfNeeded();
    populatePagePropertiesIfNeeded();
    populateContentSecurityPolicyStringsIfNeeded();
    populateWebAccessibleResourcesIfNeeded();
    populateCommandsIfNeeded();
    populateDeclarativeNetRequestPropertiesIfNeeded();
    populateExternallyConnectableIfNeeded();

    return m_errors;
}

const Vector<String>& WebExtension::supportedLocales()
{
    if (!m_supportedLocales.isEmpty())
        return m_supportedLocales;

    auto localesString = "_locales/"_s;
    auto localeDirectoryPath = resourceFileURLForPath(localesString).fileSystemPath();
    if (!localeDirectoryPath.isEmpty()) {
        m_supportedLocales = FileSystem::listDirectory(localeDirectoryPath);
        return m_supportedLocales;
    }

    // For tests that don't have a file system location, check the resource cache.
    auto prefixLength = localesString.length();
    for (const auto& resourceEntry : m_resources) {
        auto path = resourceEntry.key;
        if (!path.startsWith(localesString))
            continue;

        auto localeEnd = path.find('/', prefixLength);
        if (localeEnd == notFound)
            continue;

        auto locale = path.substring(prefixLength, localeEnd - prefixLength);
        if (!m_supportedLocales.contains(locale))
            m_supportedLocales.append(locale);
    }

    return m_supportedLocales;
}

const String& WebExtension::defaultLocale()
{
    if (!manifestParsedSuccessfully())
        return nullString();

    return m_defaultLocale;
}

String WebExtension::bestMatchLocale()
{
    const auto& supportedLocales = this->supportedLocales();
    if (supportedLocales.isEmpty())
        return nullString();

    if (supportedLocales.size() == 1)
        return supportedLocales.first();

    bool exactMatch = false;
    auto bestMatchIndex = indexOfBestMatchingLanguageInList(defaultLanguage(ShouldMinimizeLanguages::No), supportedLocales, exactMatch);
    if (bestMatchIndex != notFound)
        return supportedLocales[bestMatchIndex];

    return defaultLocale();
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

bool WebExtension::hasBackgroundContent()
{
    populateBackgroundPropertiesIfNeeded();
    return !m_backgroundScriptPaths.isEmpty() || !m_backgroundPagePath.isEmpty() || !m_backgroundServiceWorkerPath.isEmpty();
}

bool WebExtension::backgroundContentIsPersistent()
{
    populateBackgroundPropertiesIfNeeded();
    return hasBackgroundContent() && m_backgroundContentIsPersistent;
}

bool WebExtension::backgroundContentUsesModules()
{
    populateBackgroundPropertiesIfNeeded();
    return hasBackgroundContent() && m_backgroundContentUsesModules;
}

bool WebExtension::backgroundContentIsServiceWorker()
{
    populateBackgroundPropertiesIfNeeded();
    return m_backgroundContentEnvironment == Environment::ServiceWorker;
}

const String& WebExtension::backgroundContentPath()
{
    populateBackgroundPropertiesIfNeeded();

    if (!m_backgroundServiceWorkerPath.isEmpty())
        return m_backgroundServiceWorkerPath;

    if (!m_backgroundScriptPaths.isEmpty()) {
        if (backgroundContentIsServiceWorker()) {
            static const NeverDestroyed<String> backgroundContentString = generatedBackgroundServiceWorkerFilename;
            return backgroundContentString;
        }

        static const NeverDestroyed<String> backgroundContentString = generatedBackgroundPageFilename;
        return backgroundContentString;
    }

    if (!m_backgroundPagePath.isEmpty())
        return m_backgroundPagePath;

    ASSERT_NOT_REACHED();
    return nullString();
}

const String& WebExtension::generatedBackgroundContent()
{
    if (!m_generatedBackgroundContent.isEmpty())
        return m_generatedBackgroundContent;

    populateBackgroundPropertiesIfNeeded();

    if (!m_backgroundServiceWorkerPath.isEmpty() || !m_backgroundPagePath.isEmpty())
        return nullString();

    if (m_backgroundScriptPaths.isEmpty())
        return nullString();

    bool isServiceWorker = backgroundContentIsServiceWorker();
    bool usesModules = backgroundContentUsesModules();

    Vector<String> scripts;
    for (auto& scriptPath : m_backgroundScriptPaths) {
        StringBuilder format;
        if (isServiceWorker) {
            if (usesModules) {
                format.append("import \"./"_s, scriptPath, "\";"_s);

                scripts.append(format.toString());
                continue;
            }

            format.append("importScripts(\""_s, scriptPath, "\");"_s);

            scripts.append(format.toString());
            continue;
        }

        format.append("<script"_s);
        if (usesModules)
            format.append(" type=\"module\""_s);
        format.append(" src=\""_s, scriptPath, "\"></script>"_s);

        scripts.append(format.toString());
    }

    StringBuilder generatedBackgroundContent;
    if (!isServiceWorker)
        generatedBackgroundContent.append("<!DOCTYPE html>\n<body>\n"_s);

    for (auto& scriptPath : scripts)
        generatedBackgroundContent.append(scriptPath, "\n"_s);

    if (!isServiceWorker)
        generatedBackgroundContent.append("\n</body>"_s);

    m_generatedBackgroundContent = generatedBackgroundContent.toString();
    return m_generatedBackgroundContent;
}

void WebExtension::populateBackgroundPropertiesIfNeeded()
{
    if (m_parsedManifestBackgroundProperties)
        return;

    m_parsedManifestBackgroundProperties = true;

    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/background

    RefPtr backgroundManifestObject = manifestObject->getObject(backgroundManifestKey);
    if (!backgroundManifestObject || !backgroundManifestObject->size()) {
        if (manifestObject->getValue(backgroundManifestKey))
            recordError(createError(Error::InvalidBackgroundContent));
        return;
    }

    m_backgroundPagePath = backgroundManifestObject->getString(backgroundPageManifestKey);
    m_backgroundServiceWorkerPath = backgroundManifestObject->getString(backgroundServiceWorkerManifestKey);
    m_backgroundContentUsesModules = (backgroundManifestObject->getString(backgroundPageTypeKey) == backgroundPageTypeModuleValue);

    if (RefPtr backgroundScriptPaths = backgroundManifestObject->getArray(backgroundScriptsManifestKey)) {
        backgroundScriptPaths = filterObjects(*backgroundScriptPaths, [](auto& value) {
            return !value.asString().isEmpty();
        });

        m_backgroundScriptPaths = makeStringVector(*backgroundScriptPaths);
    }

    Vector<String> supportedEnvironments = { backgroundDocumentManifestKey, backgroundServiceWorkerManifestKey };

    Vector<String> preferredEnvironments;
    if (auto environment = backgroundManifestObject->getString(backgroundPreferredEnvironmentManifestKey); !environment.isEmpty()) {
        if (supportedEnvironments.contains(environment))
            preferredEnvironments.append(environment);
    } else if (RefPtr environments = backgroundManifestObject->getArray(backgroundPreferredEnvironmentManifestKey); environments && environments->length()) {
        Ref filteredEnvironments = filterObjects(*environments, [supportedEnvironments](auto& value) {
            return supportedEnvironments.contains(value.asString());
        });

        for (Ref environment : filteredEnvironments.get())
            preferredEnvironments.append(environment->asString());
    } else if (backgroundManifestObject->getValue(backgroundPreferredEnvironmentManifestKey))
        recordError(createError(Error::InvalidBackgroundContent, WEB_UI_STRING("Manifest `background` entry has an empty or invalid `preferred_environment` key.", "WKWebExtensionErrorInvalidBackgroundContent description for empty or invalid preferred environment key")));

    for (auto& environment : preferredEnvironments) {
        if (environment == backgroundDocumentManifestKey) {
            m_backgroundContentEnvironment = Environment::Document;
            m_backgroundServiceWorkerPath = nullString();

            if (!m_backgroundPagePath.isEmpty()) {
                // Page takes precedence over scripts and service worker.
                m_backgroundScriptPaths = { };
                break;
            }

            if (!m_backgroundScriptPaths.isEmpty()) {
                // Scripts takes precedence over service worker.
                break;
            }

            recordError(createError(Error::InvalidBackgroundContent, WEB_UI_STRING("Manifest `background` entry has missing or empty required `page` or `scripts` key for `preferred_environment` of `document`.", "WKWebExtensionErrorInvalidBackgroundContent description for missing background page or scripts keys")));
            break;
        }

        if (environment == backgroundServiceWorkerManifestKey) {
            m_backgroundContentEnvironment = Environment::ServiceWorker;
            m_backgroundPagePath = nullString();

            if (!m_backgroundServiceWorkerPath.isEmpty()) {
                // Page takes precedence over scripts and service worker.
                m_backgroundScriptPaths = { };
                break;
            }

            if (!m_backgroundScriptPaths.isEmpty()) {
                // Scripts takes precedence over service worker.
                break;
            }

            recordError(createError(Error::InvalidBackgroundContent, WEB_UI_STRING("Manifest `background` entry has missing or empty required `service_worker` or `scripts` key for `preferred_environment` of `service_worker`.", "WKWebExtensionErrorInvalidBackgroundContent description for missing background service_worker or scripts keys")));
            break;
        }
    }

    if (!preferredEnvironments.size()) {
        // Page takes precedence over service worker.
        if (!m_backgroundPagePath.isEmpty())
            m_backgroundServiceWorkerPath = nullString();

        // Scripts takes precedence over page and service worker.
        if (!m_backgroundScriptPaths.isEmpty()) {
            m_backgroundServiceWorkerPath = nullString();
            m_backgroundPagePath = nullString();
        }

        m_backgroundContentEnvironment = !m_backgroundServiceWorkerPath.isEmpty() ? Environment::ServiceWorker : Environment::Document;

        if (m_backgroundScriptPaths.isEmpty() && m_backgroundPagePath.isEmpty() && m_backgroundServiceWorkerPath.isEmpty())
            recordError(createError(Error::InvalidBackgroundContent, WEB_UI_STRING("Manifest `background` entry has missing or empty required `scripts`, `page`, or `service_worker` key.", "WKWebExtensionErrorInvalidBackgroundContent description for missing background required keys")));
    }

    auto persistentBoolean = backgroundManifestObject->getBoolean(backgroundPersistentManifestKey);
    m_backgroundContentIsPersistent = persistentBoolean ? *persistentBoolean : !(supportsManifestVersion(3) || !m_backgroundServiceWorkerPath.isEmpty());

    if (m_backgroundContentIsPersistent && supportsManifestVersion(3)) {
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Invalid `persistent` manifest entry. A `manifest_version` greater-than or equal to `3` must be non-persistent.", "WKWebExtensionErrorInvalidBackgroundPersistence description for manifest v3")));
        m_backgroundContentIsPersistent = false;
    }

    if (m_backgroundContentIsPersistent && !m_backgroundServiceWorkerPath.isEmpty()) {
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Invalid `persistent` manifest entry. A `service_worker` must be non-persistent.", "WKWebExtensionErrorInvalidBackgroundPersistence description for service worker")));
        m_backgroundContentIsPersistent = false;
    }

    if (!m_backgroundContentIsPersistent && hasRequestedPermission("webRequest"_s))
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Non-persistent background content cannot listen to `webRequest` events.", "WKWebExtensionErrorInvalidBackgroundPersistence description for webRequest events")));

#if PLATFORM(VISION)
    if (m_backgroundContentIsPersistent)
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Invalid `persistent` manifest entry. A non-persistent background is required on visionOS.", "WKWebExtensionErrorInvalidBackgroundPersistence description for visionOS")));
#elif PLATFORM(IOS)
    if (m_backgroundContentIsPersistent)
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Invalid `persistent` manifest entry. A non-persistent background is required on iOS and iPadOS.", "WKWebExtensionErrorInvalidBackgroundPersistence description for iOS")));
#endif
}

bool WebExtension::hasInspectorBackgroundPage()
{
    populateInspectorPropertiesIfNeeded();
    return !m_inspectorBackgroundPagePath.isEmpty();
}

const String& WebExtension::inspectorBackgroundPagePath()
{
    populateInspectorPropertiesIfNeeded();
    return m_inspectorBackgroundPagePath;
}

void WebExtension::populateInspectorPropertiesIfNeeded()
{
    if (m_parsedManifestInspectorProperties)
        return;

    m_parsedManifestInspectorProperties = true;

    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/devtools_page

    m_inspectorBackgroundPagePath = manifestObject->getString(devtoolsPageManifestKey);
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
        if (!worldString || equalIgnoringASCIICase(worldString, contentScriptsIsolatedManifestKey))
            contentWorldType = WebExtensionContentWorldType::ContentScript;
        else if (equalIgnoringASCIICase(worldString, contentScriptsMainManifestKey))
            contentWorldType = WebExtensionContentWorldType::Main;
        else
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has unknown `world` value.", "WKWebExtensionErrorInvalidContentScripts description for unknown 'world' value")));

        auto styleLevel = WebCore::UserStyleLevel::Author;
        auto cssOriginString = injectedContentObject->getString(contentScriptsCSSOriginManifestKey);
        if (!cssOriginString || equalIgnoringASCIICase(cssOriginString, contentScriptsAuthorManifestKey))
            styleLevel = WebCore::UserStyleLevel::Author;
        else if (equalIgnoringASCIICase(cssOriginString, contentScriptsUserManifestKey))
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

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
bool WebExtension::hasSidebarAction()
{
    if (RefPtr manifestObject = this->manifestObject())
        return manifestObject->getValue(sidebarActionManifestKey);

    return false;
}

bool WebExtension::hasSidePanel()
{
    return hasRequestedPermission(WebExtensionPermission::sidePanel());
}

bool WebExtension::hasAnySidebar()
{
    return hasSidebarAction() || hasSidePanel();
}

RefPtr<WebCore::Icon> WebExtension::sidebarIcon(WebCore::FloatSize idealSize)
{
    // FIXME: <https://webkit.org/b/276833> implement this
    return nullptr;
}

const String& WebExtension::sidebarDocumentPath()
{
    populateSidebarPropertiesIfNeeded();
    return m_sidebarDocumentPath;
}

const String& WebExtension::sidebarTitle()
{
    populateSidebarPropertiesIfNeeded();
    return m_sidebarTitle;
}

void WebExtension::populateSidebarPropertiesIfNeeded()
{
    if (m_parsedManifestSidebarProperties)
        return;

    m_parsedManifestSidebarProperties = true;

    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    // sidePanel documentation: https://developer.chrome.com/docs/extensions/reference/manifest#side-panel
    // see "Examples" header -> "Side Panel" tab (doesn't mention `default_path` key elsewhere)
    // sidebarAction documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/sidebar_action

    if (RefPtr sidebarActionObject = manifestObject->getObject(sidebarActionManifestKey)) {
        populateSidebarActionProperties(sidebarActionObject);
        return;
    }

    if (RefPtr sidePanelObject = manifestObject->getObject(sidePanelManifestKey))
        populateSidePanelProperties(sidePanelObject);
}

void WebExtension::populateSidebarActionProperties(const JSON::Object& sidebarActionObject)
{
    // FIXME: <https://webkit.org/b/276833> implement sidebar icon parsing
    m_sidebarIconsCache = nullptr;
    m_sidebarTitle = sidebarActionObject.getString(sidebarActionTitleManifestKey);
    m_sidebarDocumentPath = sidebarActionObject.getString(sidebarActionPathManifestKey);
}

void WebExtension::populateSidePanelProperties(const JSON::Object& sidePanelObject)
{
    // Since sidePanel cannot set a default title or icon from the manifest, setting these to null here is intentional.
    m_sidebarIconsCache = nullptr;
    m_sidebarTitle = nullString();
    m_sidebarDocumentPath = sidePanelObject.getString(sidePanelPathManifestKey);
}
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

const WebExtension::PermissionsSet& WebExtension::supportedPermissions()
{
    static MainThreadNeverDestroyed<PermissionsSet> permissions = std::initializer_list<String> { WebExtensionPermission::activeTab(), WebExtensionPermission::alarms(), WebExtensionPermission::clipboardWrite(),
        WebExtensionPermission::contextMenus(), WebExtensionPermission::cookies(), WebExtensionPermission::declarativeNetRequest(), WebExtensionPermission::declarativeNetRequestFeedback(),
        WebExtensionPermission::declarativeNetRequestWithHostAccess(), WebExtensionPermission::menus(), WebExtensionPermission::nativeMessaging(), WebExtensionPermission::notifications(), WebExtensionPermission::scripting(),
        WebExtensionPermission::storage(), WebExtensionPermission::tabs(), WebExtensionPermission::unlimitedStorage(), WebExtensionPermission::webNavigation(), WebExtensionPermission::webRequest(),
#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
        WebExtensionPermission::sidePanel(),
#endif
    };
    return permissions;
}

const WebExtension::PermissionsSet& WebExtension::requestedPermissions()
{
    populatePermissionsPropertiesIfNeeded();
    return m_permissions;
}

const WebExtension::PermissionsSet& WebExtension::optionalPermissions()
{
    populatePermissionsPropertiesIfNeeded();
    return m_optionalPermissions;
}

const WebExtension::MatchPatternSet& WebExtension::requestedPermissionMatchPatterns()
{
    populatePermissionsPropertiesIfNeeded();
    return m_permissionMatchPatterns;
}

const WebExtension::MatchPatternSet& WebExtension::optionalPermissionMatchPatterns()
{
    populatePermissionsPropertiesIfNeeded();
    return m_optionalPermissionMatchPatterns;
}

const WebExtension::MatchPatternSet& WebExtension::externallyConnectableMatchPatterns()
{
    populateExternallyConnectableIfNeeded();
    return m_externallyConnectableMatchPatterns;
}

WebExtension::MatchPatternSet WebExtension::allRequestedMatchPatterns()
{
    populatePermissionsPropertiesIfNeeded();
    populateContentScriptPropertiesIfNeeded();
    populateExternallyConnectableIfNeeded();

    WebExtension::MatchPatternSet result;

    for (Ref matchPattern : m_permissionMatchPatterns)
        result.add(matchPattern);

    for (Ref matchPattern : m_externallyConnectableMatchPatterns)
        result.add(matchPattern);

    for (auto& injectedContent : m_staticInjectedContents) {
        for (Ref matchPattern : injectedContent.includeMatchPatterns)
            result.add(matchPattern);
    }

    return result;
}

void WebExtension::populateExternallyConnectableIfNeeded()
{
    if (m_parsedExternallyConnectable)
        return;

    m_parsedExternallyConnectable = true;

    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/externally_connectable

    RefPtr externallyConnectableObject = manifestObject->getObject(externallyConnectableManifestKey);
    if (!externallyConnectableObject)
        return;

    if (!externallyConnectableObject->size()) {
        recordError(createError(Error::InvalidExternallyConnectable));
        return;
    }

    bool shouldReportError = false;
    MatchPatternSet matchPatterns;

    if (RefPtr matchPatternStrings = externallyConnectableObject->getArray(externallyConnectableMatchesManifestKey)) {
        for (auto matchPatternStringValue : *matchPatternStrings) {
            auto matchPatternString = matchPatternStringValue->asString();
            if (matchPatternString.isEmpty())
                continue;

            if (RefPtr matchPattern = WebExtensionMatchPattern::getOrCreate(matchPatternString)) {
                if (matchPattern->matchesAllURLs() || !matchPattern->isSupported()) {
                    shouldReportError = true;
                    continue;
                }

                // URL patterns must contain at least a second-level domain. Top level domains and wildcards are not standalone patterns.
                if (matchPattern->hostIsPublicSuffix()) {
                    shouldReportError = true;
                    continue;
                }

                matchPatterns.add(matchPattern.releaseNonNull());
            }
        }
    }

    m_externallyConnectableMatchPatterns = WTFMove(matchPatterns);

    RefPtr extensionIDs = externallyConnectableObject->getArray(externallyConnectableIDsManifestKey);
    if (extensionIDs) {
        extensionIDs = filterObjects(*extensionIDs, [](auto& value) {
            return !value.asString().isEmpty();
        });
    }

    if (shouldReportError || (m_externallyConnectableMatchPatterns.isEmpty() && (!extensionIDs || (extensionIDs && !extensionIDs->length()))))
        recordError(createError(Error::InvalidExternallyConnectable));
}

void WebExtension::populatePermissionsPropertiesIfNeeded()
{
    if (m_parsedManifestPermissionProperties)
        return;

    m_parsedManifestPermissionProperties = true;

    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    bool findMatchPatternsInPermissions = !supportsManifestVersion(3);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/permissions
    if (RefPtr permissionsManifestArray = manifestObject->getArray(permissionsManifestKey)) {
        for (Ref permissionObject : *permissionsManifestArray) {
            auto permission = permissionObject->asString();
            if (permission.isEmpty())
                continue;

            if (findMatchPatternsInPermissions) {
                if (RefPtr matchPattern = WebExtensionMatchPattern::getOrCreate(permission)) {
                    if (matchPattern->isSupported())
                        m_permissionMatchPatterns.add(matchPattern.releaseNonNull());
                    continue;
                }
            }

            if (supportedPermissions().contains(permission))
                m_permissions.add(permission);
        }
    }

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/host_permissions

    if (!findMatchPatternsInPermissions) {
        if (RefPtr hostPermissionsManifestArray = manifestObject->getArray(hostPermissionsManifestKey)) {
            for (Ref permissionObject : *hostPermissionsManifestArray) {
                auto permission = permissionObject->asString();
                if (permission.isEmpty())
                    continue;

                if (RefPtr matchPattern = WebExtensionMatchPattern::getOrCreate(permission)) {
                    if (matchPattern->isSupported())
                        m_permissionMatchPatterns.add(matchPattern.releaseNonNull());
                }
            }
        }
    }

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/optional_permissions

    if (RefPtr optionalPermissionsManifestArray = manifestObject->getArray(optionalPermissionsManifestKey)) {
        for (Ref permissionObject : *optionalPermissionsManifestArray) {
            auto permission = permissionObject->asString();
            if (permission.isEmpty())
                continue;

            if (findMatchPatternsInPermissions) {
                if (RefPtr matchPattern = WebExtensionMatchPattern::getOrCreate(permission)) {
                    if (matchPattern->isSupported() && !m_permissionMatchPatterns.contains(*matchPattern))
                        m_optionalPermissionMatchPatterns.add(matchPattern.releaseNonNull());
                    continue;
                }
            }

            if (!m_permissions.contains(permission) && supportedPermissions().contains(permission))
                m_optionalPermissions.add(permission);
        }
    }

    // Documentation: https://github.com/w3c/webextensions/issues/119

    if (!findMatchPatternsInPermissions) {
        if (RefPtr hostPermissionsManifestArray = manifestObject->getArray(optionalHostPermissionsManifestKey)) {
            for (Ref permissionObject : *hostPermissionsManifestArray) {
                auto permission = permissionObject->asString();
                if (permission.isEmpty())
                    continue;

                if (RefPtr matchPattern = WebExtensionMatchPattern::getOrCreate(permission)) {
                    if (matchPattern->isSupported() && !m_permissionMatchPatterns.contains(*matchPattern))
                        m_optionalPermissionMatchPatterns.add(matchPattern.releaseNonNull());
                }
            }
        }
    }
}

void WebExtension::populateActionPropertiesIfNeeded()
{
    if (m_parsedManifestActionProperties)
        return;

    m_parsedManifestActionProperties = true;

    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/action
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/browser_action
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/page_action

    RefPtr<JSON::Object> actionObject;
    if (supportsManifestVersion(3))
        actionObject = manifestObject->getObject(actionManifestKey);
    else {
        actionObject = manifestObject->getObject(browserActionManifestKey);
        if (!actionObject)
            actionObject = manifestObject->getObject(pageActionManifestKey);
    }

    if (!actionObject)
        return;

    // Look for the "default_icon" as a string, which is useful for SVG icons. Only supported by Firefox currently.
    if (auto defaultIconPath = actionObject->getString(defaultIconManifestKey); !defaultIconPath.isEmpty()) {
        RefPtr<API::Error> resourceError;
        m_defaultActionIcon = iconForPath(defaultIconPath, resourceError);

        if (!m_defaultActionIcon) {
            recordErrorIfNeeded(resourceError);

            String localizedErrorDescription;
            if (supportsManifestVersion(3))
                localizedErrorDescription = WEB_UI_STRING("Failed to load image for `default_icon` in the `action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load single image for action");
            else
                localizedErrorDescription = WEB_UI_STRING("Failed to load image for `default_icon` in the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load single image for browser_action or page_action");

            recordError(createError(Error::InvalidActionIcon, localizedErrorDescription));
        }
    }

    m_displayActionLabel = actionObject->getString(defaultTitleManifestKey);
    m_actionPopupPath = actionObject->getString(defaultPopupManifestKey);

    m_actionObject = actionObject;
}

const String& WebExtension::displayActionLabel()
{
    populateActionPropertiesIfNeeded();
    return m_displayActionLabel;
}

const String& WebExtension::actionPopupPath()
{
    populateActionPropertiesIfNeeded();
    return m_actionPopupPath;
}

bool WebExtension::hasAction()
{
    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return false;

    return supportsManifestVersion(3) && manifestObject->getValue(actionManifestKey);
}

bool WebExtension::hasBrowserAction()
{
    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return false;

    return !supportsManifestVersion(3) && manifestObject->getValue(browserActionManifestKey);
}

bool WebExtension::hasPageAction()
{
    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return false;

    return !supportsManifestVersion(3) && manifestObject->getValue(pageActionManifestKey);
}

RefPtr<WebCore::Icon> WebExtension::icon(WebCore::FloatSize size)
{
    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return nullptr;

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    if (manifestObject->getValue(iconVariantsManifestKey)) {
        String localizedErrorDescription = WEB_UI_STRING("Failed to load images in `icon_variants` manifest entry.", "WKWebExtensionErrorInvalidIcon description for failing to load image variants");
        return bestIconVariantForManifestKey(*manifestObject, iconVariantsManifestKey, size, m_iconsCache, Error::InvalidIcon, localizedErrorDescription);
    }
#endif

    String localizedErrorDescription = WEB_UI_STRING("Failed to load images in `icons` manifest entry.", "WKWebExtensionErrorInvalidIcon description for failing to load images");
    return bestIconForManifestKey(*manifestObject, iconsManifestKey, size, m_iconsCache, Error::InvalidIcon, localizedErrorDescription);
}

RefPtr<WebCore::Icon> WebExtension::actionIcon(WebCore::FloatSize size)
{
    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return nullptr;

    populateActionPropertiesIfNeeded();

    if (m_defaultActionIcon)
        return m_defaultActionIcon;

    if (RefPtr actionObject = m_actionObject) {
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
        if (actionObject->getValue(iconVariantsManifestKey)) {
            String localizedErrorDescription = WEB_UI_STRING("Failed to load images in `icon_variants` for the `action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load image variants for action");
            if (RefPtr result = bestIconVariantForManifestKey(*actionObject, iconVariantsManifestKey, size, m_actionIconsCache, Error::InvalidActionIcon, localizedErrorDescription))
                return result;
            return icon(size);
        }
#endif

        String localizedErrorDescription;
        if (supportsManifestVersion(3))
            localizedErrorDescription = WEB_UI_STRING("Failed to load images in `default_icon` for the `action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load images for action only");
        else
            localizedErrorDescription = WEB_UI_STRING("Failed to load images in `default_icon` for the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load images for browser_action or page_action");

        if (RefPtr result = bestIconForManifestKey(*actionObject, defaultIconManifestKey, size, m_actionIconsCache, Error::InvalidActionIcon, localizedErrorDescription))
            return result;
        }

    return icon(size);
}

size_t WebExtension::bestIconSize(const JSON::Object& iconsObject, size_t idealPixelSize)
{
    if (!iconsObject.size())
        return 0;

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    // Check if the "any" size exists (typically a vector image), and prefer it.
    if (iconsObject.getValue(anyManifestKey)) {
        // Return max to ensure it takes precedence over all other sizes.
        return std::numeric_limits<size_t>::max();
    }
#endif

    // Check if the ideal size exists, if so return it.
    auto idealSizeString = String::number(idealPixelSize);
    if (iconsObject.getValue(idealSizeString))
        return idealPixelSize;

    Vector<size_t> sizeValues;
    for (auto key : iconsObject.keys()) {
        // Filter the values to only include numeric strings representing sizes. This will exclude non-numeric string
        // values such as "any", "color_schemes", and any other strings that cannot be converted to a positive integer.
        auto integerValue = parseInteger<size_t>(key);
        if (integerValue && integerValue > 0)
            sizeValues.append(*integerValue);
    }

    if (!sizeValues.size())
        return 0;

    // Sort the remaining keys and find the next largest size.
    std::sort(sizeValues.begin(), sizeValues.end());

    size_t bestSize = 0;
    for (auto size : sizeValues) {
        bestSize = size;
        if (bestSize >= idealPixelSize)
            break;
    }

    return bestSize;
}

String WebExtension::pathForBestImage(const JSON::Object& iconsObject, size_t idealPixelSize)
{
    auto bestSize = bestIconSize(iconsObject, idealPixelSize);
    if (!bestSize)
        return nullString();

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    if (bestSize == std::numeric_limits<size_t>::max())
        return iconsObject.getString(anyManifestKey);
#endif

    return iconsObject.getString(String::number(bestSize));
}

RefPtr<WebCore::Icon> WebExtension::bestIconForManifestKey(const JSON::Object& object, const String& manifestKey, WebCore::FloatSize idealSize, IconsCache& cacheLocation, Error error, const String& customLocalizedDescription)
{
    auto currentScales = availableScreenScales();
    if (auto cachedScales = cacheLocation.getOptional("scales"_s)) {
        auto scales = std::get_if<Vector<double>>(&*cachedScales);
        if (!scales || *scales != currentScales)
            cacheLocation.set("scales"_s, IconCacheEntry(currentScales));
    } else
        cacheLocation.set("scales"_s, IconCacheEntry(currentScales));

    auto cacheKey = idealSize.toJSONString();
    if (auto cacheResult = cacheLocation.getOptional(cacheKey))
        return *std::get_if<RefPtr<WebCore::Icon>>(&*cacheResult);

    RefPtr iconObject = object.getObject(manifestKey);
    if (!iconObject)
        return nullptr;

    RefPtr result = bestIcon(iconObject, idealSize, [&](Ref<API::Error> error) {
        recordError(error);
    });

    if (!result) {
        if (iconObject->size()) {
            // Record an error if the object had values, meaning the likely failure is the images were missing on disk or bad format.
            recordError(createError(error, customLocalizedDescription));
        } else if (!iconObject->size() || object.getValue(manifestKey)) {
            // Record an error if the key had object that was empty, or the key had a value of the wrong type.
            recordError(createError(error));
        }

        return nullptr;
    }

    cacheLocation.set(cacheKey, IconCacheEntry(result));

    return result;
}

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
static OptionSet<WebExtension::ColorScheme> toColorSchemes(RefPtr<JSON::Value> value)
{
    using ColorScheme = WebExtension::ColorScheme;

    if (!value) {
        // A null or invalid value counts as all color schemes.
        return { ColorScheme::Light, ColorScheme::Dark };
    }

    OptionSet<ColorScheme> result;

    RefPtr array = value->asArray();
    if (!array)
        return { ColorScheme::Light, ColorScheme::Dark };

    for (Ref value : *array) {
        if (value->asString() == lightManifestKey)
            result.add(ColorScheme::Light);
        if (value->asString() == darkManifestKey)
            result.add(ColorScheme::Dark);
    }

    return result;
}

RefPtr<JSON::Object> WebExtension::bestIconVariantJSONObject(RefPtr<JSON::Array> variants, size_t idealPixelSize, ColorScheme idealColorScheme)
{
    if (!variants || !variants->length())
        return nullptr;

    if (variants->length() == 1)
        return variants->get(0)->asObject();

    RefPtr<JSON::Object> bestVariant;
    RefPtr<JSON::Object> fallbackVariant;
    bool foundIdealFallbackVariant = false;

    size_t bestSize = 0;
    size_t fallbackSize = 0;

    // Pick the first variant matching color scheme and/or size.
    for (Ref variant : *variants) {
        if (!variant)
            continue;

        RefPtr variantObject = variant->asObject();
        auto colorSchemes = toColorSchemes(variantObject ? variantObject->getValue(colorSchemesManifestKey) : nullptr);
        auto currentBestSize = bestIconSize(*variantObject, idealPixelSize);

        if (colorSchemes.contains(idealColorScheme)) {
            if (currentBestSize >= idealPixelSize) {
                // Found the best variant, return it.
                return variantObject;
            }

            if (currentBestSize > bestSize) {
                // Found a larger ideal variant.
                bestSize = currentBestSize;
                bestVariant = variantObject;
            }
        } else if (!foundIdealFallbackVariant && currentBestSize >= idealPixelSize) {
            // Found an ideal fallback variant, based only on size.
            fallbackSize = currentBestSize;
            fallbackVariant = variantObject;
            foundIdealFallbackVariant = true;
        } else if (!foundIdealFallbackVariant && currentBestSize > fallbackSize) {
            // Found a smaller fallback variant.
            fallbackSize = currentBestSize;
            fallbackVariant = variantObject;
        }
    }

    return bestVariant ?: fallbackVariant;
}

RefPtr<WebCore::Icon> WebExtension::bestIconVariantForManifestKey(const JSON::Object& object, const String& manifestKey, WebCore::FloatSize idealSize, IconsCache& cacheLocation, Error error, const String& customLocalizedDescription)
{
    auto currentScales = availableScreenScales();
    if (auto cachedScales = cacheLocation.getOptional("scales"_s)) {
        auto scales = std::get_if<Vector<double>>(&*cachedScales);
        if (!scales || *scales != currentScales)
            cacheLocation.set("scales"_s, IconCacheEntry(currentScales));
    } else
        cacheLocation.set("scales"_s, IconCacheEntry(currentScales));

    auto cacheKey = idealSize.toJSONString();
    if (auto cacheResult = cacheLocation.getOptional(cacheKey))
        return *std::get_if<RefPtr<WebCore::Icon>>(&*cacheResult);

    RefPtr iconArray = object.getArray(manifestKey);
    RefPtr result = bestIconVariant(iconArray, idealSize, [&](Ref<API::Error> error) {
        recordError(error);
    });

    if (!result) {
        if (iconArray->length()) {
            // Record an error if the array had values, meaning the likely failure is the images were missing on disk or bad format.
            recordError(createError(error, customLocalizedDescription));
        } else if (!iconArray->length() || object.getValue(manifestKey)) {
            // Record an error if the key had an array that was empty, or the key had a value of the wrong type.
            recordError(createError(error));
        }

        return nullptr;
    }

    cacheLocation.set(cacheKey, IconCacheEntry(result));

    return result;
}
#endif // ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)

const WebExtension::CommandsVector& WebExtension::commands()
{
    populateCommandsIfNeeded();
    return m_commands;
}

bool WebExtension::hasCommands()
{
    populateCommandsIfNeeded();
    return !m_commands.isEmpty();
}

using ModifierFlags = WebExtension::ModifierFlags;

static bool parseCommandShortcut(const String& shortcut, OptionSet<ModifierFlags>& modifierFlags, String& key)
{
    modifierFlags = { };
    key = emptyString();

    // An empty shortcut is allowed.
    if (shortcut.isEmpty())
        return true;

    static NeverDestroyed<HashMap<String, ModifierFlags>> modifierMap = HashMap<String, ModifierFlags> {
        { "Ctrl"_s, ModifierFlags::Command },
        { "Command"_s, ModifierFlags::Command },
        { "Alt"_s, ModifierFlags::Option },
        { "MacCtrl"_s, ModifierFlags::Control },
        { "Shift"_s, ModifierFlags::Shift }
    };

    static NeverDestroyed<HashMap<String, String>> specialKeyMap = HashMap<String, String> {
        { "Comma"_s, ","_s },
        { "Period"_s, "."_s },
        { "Space"_s, " "_s },
        { "F1"_s, String::fromUTF8("\uF704") },
        { "F2"_s, String::fromUTF8("\uF705") },
        { "F3"_s, String::fromUTF8("\uF706") },
        { "F4"_s, String::fromUTF8("\uF707") },
        { "F5"_s, String::fromUTF8("\uF708") },
        { "F6"_s, String::fromUTF8("\uF709") },
        { "F7"_s, String::fromUTF8("\uF70A") },
        { "F8"_s, String::fromUTF8("\uF70B") },
        { "F9"_s, String::fromUTF8("\uF70C") },
        { "F10"_s, String::fromUTF8("\uF70D") },
        { "F11"_s, String::fromUTF8("\uF70E") },
        { "F12"_s, String::fromUTF8("\uF70F") },
        { "Insert"_s, String::fromUTF8("\uF727") },
        { "Delete"_s, String::fromUTF8("\uF728") },
        { "Home"_s, String::fromUTF8("\uF729") },
        { "End"_s, String::fromUTF8("\uF72B") },
        { "PageUp"_s, String::fromUTF8("\uF72C") },
        { "PageDown"_s, String::fromUTF8("\uF72D") },
        { "Up"_s, String::fromUTF8("\uF700") },
        { "Down"_s, String::fromUTF8("\uF701") },
        { "Left"_s, String::fromUTF8("\uF702") },
        { "Right"_s, String::fromUTF8("\uF703") }
    };

    auto parts = shortcut.split('+');

    // Reject shortcuts with fewer than two or more than three components.
    if (parts.size() < 2 || parts.size() > 3)
        return false;

    key = parts.takeLast();

    // Keys should not be present in the modifier map.
    if (modifierMap.get().contains(key))
        return false;

    if (key.length() == 1) {
        // Single-character keys must be alphanumeric.
        if (!isASCIIAlphanumeric(key[0]))
            return false;

        key = key.convertToASCIILowercase();
    } else {
        auto entry = specialKeyMap.get().find(key);

        // Non-alphanumeric keys must be in the special key map.
        if (entry == specialKeyMap.get().end())
            return false;

        key = entry->value;
    }

    for (auto& part : parts) {
        // Modifiers must exist in the modifier map.
        if (!modifierMap.get().contains(part))
            return false;

        modifierFlags.add(modifierMap.get().get(part));
    }

    // At least one valid modifier is required.
    if (!modifierFlags)
        return false;

    return true;
}

void WebExtension::populateCommandsIfNeeded()
{
    if (m_parsedManifestCommands)
        return;

    m_parsedManifestCommands = true;

    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/commands

    RefPtr commandsObject = manifestObject->getObject(commandsManifestKey);
    if (!commandsObject && manifestObject->getValue(commandsManifestKey)) {
        recordError(createError(Error::InvalidCommands));
        return;
    }

    bool hasActionCommand = false;

    if (commandsObject) {
        size_t commandsWithShortcuts = 0;
        std::optional<String> error;

        for (auto commandIdentifier : commandsObject->keys()) {
            if (commandIdentifier.isEmpty()) {
                error = WEB_UI_STRING("Empty or invalid identifier in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command identifier");
                continue;
            }

            RefPtr commandObject = commandsObject->getObject(commandIdentifier);
            if (!commandObject || !commandObject->size()) {
                error = WEB_UI_STRING("Empty or invalid command in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command");
                continue;
            }

            CommandData commandData;
            commandData.identifier = commandIdentifier;
            commandData.activationKey = emptyString();
            commandData.modifierFlags = { };

            bool isActionCommand = false;
            if (supportsManifestVersion(3) && commandData.identifier == "_execute_action"_s)
                isActionCommand = true;
            else if (!supportsManifestVersion(3) && (commandData.identifier == "_execute_browser_action"_s || commandData.identifier == "_execute_page_action"_s))
                isActionCommand = true;

            if (isActionCommand && !hasActionCommand)
                hasActionCommand = true;

            // Descriptions are required for standard commands, but are optional for action commands.
            auto description = commandObject->getString(commandsDescriptionKeyManifestKey);
            if (description.isEmpty() && !isActionCommand) {
                error = WEB_UI_STRING("Empty or invalid `description` in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command description");
                continue;
            }

            if (isActionCommand && description.isEmpty()) {
                description = displayActionLabel();
                if (description.isEmpty())
                    description = displayShortName();
            }

            commandData.description = description;

            if (RefPtr suggestedKeyObject = commandObject->getObject(commandsSuggestedKeyManifestKey)) {
#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
                const auto macPlatform = "mac"_s;
                const auto iosPlatform = "ios"_s;
#elif PLATFORM(GTK) || PLATFORM(WPE)
                const auto linuxPlatform = "linux"_s;
#endif
                const auto defaultPlatform = "default"_s;

                String platformShortcut;
#if PLATFORM(MAC)
                platformShortcut = !suggestedKeyObject->getString(macPlatform).isEmpty() ? suggestedKeyObject->getString(macPlatform) : suggestedKeyObject->getString(iosPlatform);
#elif PLATFORM(IOS_FAMILY)
                platformShortcut = !suggestedKeyObject->getString(iosPlatform).isEmpty() ? suggestedKeyObject->getString(iosPlatform) : suggestedKeyObject->getString(macPlatform);
#elif PLATFORM(GTK) || PLATFORM(WPE)
                platformShortcut = suggestedKeyObject->getString(linuxPlatform);
#endif
                if (platformShortcut.isEmpty())
                    platformShortcut = suggestedKeyObject->getString(defaultPlatform);

                if (!parseCommandShortcut(platformShortcut, commandData.modifierFlags, commandData.activationKey)) {
                    error = WEB_UI_STRING("Invalid `suggested_key` in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command shortcut");
                    continue;
                }

                if (!commandData.activationKey.isEmpty() && ++commandsWithShortcuts > maximumNumberOfShortcutCommands) {
                    error = WEB_UI_STRING("Too many shortcuts specified for `commands`, only 4 shortcuts are allowed.", "WKWebExtensionErrorInvalidManifestEntry description for too many command shortcuts");
                    commandData.activationKey = emptyString();
                    commandData.modifierFlags = { };
                }
            }

            m_commands.append(WTFMove(commandData));
        }

        if (error)
            recordError(createError(Error::InvalidCommands, error.value()));
    }

    if (!hasActionCommand) {
        String commandIdentifier;
        if (hasAction())
            commandIdentifier = "_execute_action"_s;
        else if (hasBrowserAction())
            commandIdentifier = "_execute_browser_action"_s;
        else if (hasPageAction())
            commandIdentifier = "_execute_page_action"_s;

        if (!commandIdentifier.isEmpty())
            m_commands.append({ commandIdentifier, displayActionLabel(), emptyString(), { } });
    }
}

std::optional<WebExtension::DeclarativeNetRequestRulesetData> WebExtension::parseDeclarativeNetRequestRulesetObject(const JSON::Object& rulesetObject, RefPtr<API::Error>& error)
{
    auto rulesetID = rulesetObject.getString(declarativeNetRequestRulesetIDManifestKey);
    if (rulesetID.isEmpty()) {
        error = createError(Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Empty or invalid `id` in `declarative_net_request` manifest entry.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for empty or invalid id in declarative_net_request manifest entry"));
        return { };
    }

    auto jsonPath = rulesetObject.getString(declarativeNetRequestRulePathManifestKey);
    if (jsonPath.isEmpty()) {
        error = createError(WebExtension::Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Empty or invalid `path` in `declarative_net_request` manifest entry.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for empty or invalid path in declarative_net_request manifest entry"));
        return { };
    }

    auto enabledBool = rulesetObject.getBoolean(declarativeNetRequestRuleEnabledManifestKey);
    if (!enabledBool) {
        error = createError(WebExtension::Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Missing or invalid `enabled` boolean for the `declarative_net_request` manifest entry.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for missing enabled boolean"));
        return { };
    }

    DeclarativeNetRequestRulesetData rulesetData = {
        rulesetID,
        *enabledBool,
        jsonPath
    };

    return std::optional { WTFMove(rulesetData) };
}

void WebExtension::populateDeclarativeNetRequestPropertiesIfNeeded()
{
    if (m_parsedManifestDeclarativeNetRequestRulesets)
        return;

    m_parsedManifestDeclarativeNetRequestRulesets = true;

    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/declarative_net_request

    if (!supportedPermissions().contains(WebExtensionPermission::declarativeNetRequest()) && !supportedPermissions().contains(WebExtensionPermission::declarativeNetRequestWithHostAccess())) {
        recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Manifest has no `declarativeNetRequest` permission.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for missing declarativeNetRequest permission")));
        return;
    }

    RefPtr declarativeNetRequestManifestObject = manifestObject->getObject(declarativeNetRequestManifestKey);
    if (!declarativeNetRequestManifestObject) {
        if (manifestObject->getValue(declarativeNetRequestManifestKey))
            recordError(createError(Error::InvalidDeclarativeNetRequest));
        return;
    }

    RefPtr declarativeNetRequestRulesets = declarativeNetRequestManifestObject->getArray(declarativeNetRequestRulesManifestKey);
    if (!declarativeNetRequestRulesets) {
        if (manifestObject->getValue(declarativeNetRequestManifestKey))
            recordError(createError(Error::InvalidDeclarativeNetRequest));
        return;
    }

    if (declarativeNetRequestRulesets->length() > webExtensionDeclarativeNetRequestMaximumNumberOfStaticRulesets)
        recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Exceeded maximum number of `declarative_net_request` rulesets. Ignoring extra rulesets.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for too many rulesets")));

    size_t rulesetCount = 0, enabledRulesetCount = 0;
    bool recordedTooManyRulesetsManifestError = false;
    HashSet<String> seenRulesetIDs;
    for (Ref value : *declarativeNetRequestRulesets) {
        if (rulesetCount >= webExtensionDeclarativeNetRequestMaximumNumberOfStaticRulesets)
            continue;

        RefPtr object = value->asObject();
        if (!object)
            continue;

        RefPtr<API::Error> error;
        auto optionalRuleset = parseDeclarativeNetRequestRulesetObject(*object, error);
        if (!optionalRuleset) {
            if (error)
                recordError(createError(Error::InvalidDeclarativeNetRequest, { }, error));
            continue;
        }

        auto& ruleset = optionalRuleset.value();
        if (seenRulesetIDs.contains(ruleset.rulesetID)) {
            recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_FORMAT_STRING("`declarative_net_request` ruleset with id \"%s\" is invalid. Ruleset id must be unique.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for duplicate ruleset id", ruleset.rulesetID.utf8().data())));
            continue;
        }

        if (ruleset.enabled && ++enabledRulesetCount > webExtensionDeclarativeNetRequestMaximumNumberOfEnabledRulesets && !recordedTooManyRulesetsManifestError) {
            recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_FORMAT_STRING("Exceeded maximum number of enabled `declarative_net_request` static rulesets. The first %lu will be applied, the remaining will be ignored.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for too many enabled static rulesets", webExtensionDeclarativeNetRequestMaximumNumberOfEnabledRulesets)));
            recordedTooManyRulesetsManifestError = true;
            continue;
        }

        seenRulesetIDs.add(ruleset.rulesetID);
        ++rulesetCount;

        m_declarativeNetRequestRulesets.append(WTFMove(ruleset));
    }
}

const WebExtension::DeclarativeNetRequestRulesetVector& WebExtension::declarativeNetRequestRulesets()
{
    populateDeclarativeNetRequestPropertiesIfNeeded();
    return m_declarativeNetRequestRulesets;
}

std::optional<WebExtension::DeclarativeNetRequestRulesetData> WebExtension::declarativeNetRequestRuleset(const String& identifier)
{
    for (auto& ruleset : declarativeNetRequestRulesets()) {
        if (ruleset.rulesetID == identifier)
            return ruleset;
    }

    return std::nullopt;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
