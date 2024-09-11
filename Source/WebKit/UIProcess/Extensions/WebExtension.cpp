/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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

#include "Logging.h"
#include "WebExtensionConstants.h"
#include "WebExtensionPermission.h"
#include "WebExtensionUtilities.h"

#include <wtf/FileSystem.h>
#include <wtf/Function.h>
#include <wtf/StdSet.h>
#include <wtf/text/StringToIntegerConversion.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

static constexpr auto manifestVersionManifestKey = "manifest_version"_s;

static constexpr auto nameManifestKey = "name"_s;
static constexpr auto shortNameManifestKey = "short_name"_s;
static constexpr auto versionManifestKey = "version"_s;
static constexpr auto versionNameManifestKey = "version_name"_s;
static constexpr auto descriptionManifestKey = "description"_s;

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
static constexpr auto defaultLocaleManifestKey = "default_locale"_s;
static constexpr auto defaultTitleManifestKey = "default_title"_s;
static constexpr auto defaultPopupManifestKey = "default_popup"_s;

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

static constexpr auto devtoolsPageManifestKey = "devtools_page"_s;

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

static constexpr auto permissionsManifestKey = "permissions"_s;
static constexpr auto optionalPermissionsManifestKey = "optional_permissions"_s;
static constexpr auto hostPermissionsManifestKey = "host_permissions"_s;
static constexpr auto optionalHostPermissionsManifestKey = "optional_host_permissions"_s;

static constexpr auto optionsUIManifestKey = "options_ui"_s;
static constexpr auto optionsUIPageManifestKey = "page"_s;
static constexpr auto optionsPageManifestKey = "options_page"_s;
static constexpr auto chromeURLOverridesManifestKey = "chrome_url_overrides"_s;
static constexpr auto browserURLOverridesManifestKey = "browser_url_overrides"_s;
static constexpr auto newTabManifestKey = "newtab"_s;

static constexpr auto contentSecurityPolicyManifestKey = "content_security_policy"_s;
static constexpr auto contentSecurityPolicyExtensionPagesManifestKey = "extension_pages"_s;

static constexpr auto commandsManifestKey = "commands"_s;
static constexpr auto commandsSuggestedKeyManifestKey = "suggested_key"_s;
static constexpr auto commandsDescriptionKeyManifestKey = "description"_s;

static constexpr auto webAccessibleResourcesManifestKey = "web_accessible_resources"_s;
static constexpr auto webAccessibleResourcesResourcesManifestKey = "resources"_s;
static constexpr auto webAccessibleResourcesMatchesManifestKey = "matches"_s;

static constexpr auto declarativeNetRequestManifestKey = "declarative_net_request"_s;
static constexpr auto declarativeNetRequestRulesManifestKey = "rule_resources"_s;
static constexpr auto declarativeNetRequestRulesetIDManifestKey = "id"_s;
static constexpr auto declarativeNetRequestRuleEnabledManifestKey = "enabled"_s;
static constexpr auto declarativeNetRequestRulePathManifestKey = "path"_s;

static constexpr auto externallyConnectableManifestKey = "externally_connectable"_s;
static constexpr auto externallyConnectableMatchesManifestKey = "matches"_s;
static constexpr auto externallyConnectableIDsManifestKey = "ids"_s;

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
static constexpr auto sidebarActionManifestKey = "sidebar_action"_s;
static constexpr auto sidePanelManifestKey = "side_panel"_s;
static constexpr auto sidebarActionTitleManifestKey = "default_title"_s;
static constexpr auto sidebarActionPathManifestKey = "default_panel"_s;
static constexpr auto sidePanelPathManifestKey = "default_path"_s;
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

static const size_t maximumNumberOfShortcutCommands = 4;

WebExtension::WebExtension(const JSON::Value& manifest, const Resources& resources)
    : m_manifest(manifest), m_resources(resources)
{
    ASSERT(manifest);

    String manifestData = manifest.toJSONString();
    RELEASE_ASSERT(manifestData);

    m_resources.set("manifest.json"_s, manifestData.span8());
}

WebExtension::WebExtension(const Resources& resources)
    : m_manifest(JSON::Object::create())
    , m_resources(resources)
{
    RefPtr<API::Error> error;
    String manifestData = resourceStringForPath("manifest.json"_s, &error);
    if (manifestData.isEmpty()) {
        createError(Error::InvalidManifest, ""_s, &*error);
        return;
    }

    auto manifestJSON = JSON::Value::parseJSON(manifestData);
    if (!manifestJSON) {
        createError(Error::InvalidManifest);
        return;
    }

    m_manifest = manifestJSON.releaseNonNull();
}

WebExtension::WebExtension(const JSON::Value& manifest, const Resources& resources, const URL& resourceBaseURL)
    : m_resourceBaseURL(resourceBaseURL)
    , m_manifest(manifest)
    , m_resources(resources)
{
    ASSERT(manifest);

    String manifestData = manifest.toJSONString();
    RELEASE_ASSERT(manifestData);

    m_resources.set("manifest.json"_s, manifestData.span8());
}

RefPtr<WebExtension> WebExtension::createWithURL(const URL& resourceBaseURL)
{
    ASSERT(resourceBaseURL.isEmpty());
    ASSERT(resourceBaseURL.protocolIsFile());
    ASSERT(resourceBaseURL.hasPath());

    auto resourceURL = URL(resourceBaseURL, "manifest.json"_s);

    auto data = FileSystem::readEntireFile(resourceURL.fileSystemPath());
    if (!data.has_value())
        return nullptr;

    auto manifestData = String::fromUTF8((*data).span());
    if (manifestData.isEmpty())
        return nullptr;

    auto manifestJSON = JSON::Value::parseJSON(manifestData);
    if (!manifestJSON)
        return nullptr;

    return adoptRef(*new WebExtension(*manifestJSON, Resources(), resourceBaseURL));
}

bool WebExtension::manifestParsedSuccessfully()
{
    if (m_parsedManifest)
        return true;

    // Validate that the manifest exists as an Object.
    if (!manifest().asObject())
        createError(Error::InvalidManifest);

    return !!manifest().asObject();
}

const JSON::Value& WebExtension::manifest()
{
    RefPtr<JSON::Object> manifestObject;

    if (m_parsedManifest)
        return m_manifest.get();

    m_parsedManifest = true;

    // m_defaultLocale = manifestObject->getString(defaultLocaleManifestKey);
    // m_localization = [[_WKWebExtensionLocalization alloc] initWithWebExtension:*this];
    // m_manifest = [m_localization.get() localizedDictionaryForDictionary:m_manifest.get()];
    ASSERT(m_manifest);

    // FIXME: Handle Safari version compatibility check.
    // Likely do this version checking when the extension is added to the WKWebExtensionController,
    // since that will need delegated to the app.

    return m_manifest.get();
}

Ref<API::Data> WebExtension::serializeManifest()
{
    return API::Data::create(m_manifest->toJSONString().span8());
}

double WebExtension::manifestVersion()
{
    auto manifestObject = m_manifest->asObject();
    if (!manifestObject)
        return 0; // 0 is not a valid manifest version

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/manifest_version
    auto value = manifestObject->getDouble(manifestVersionManifestKey);

    if (value)
        return *value;

    return 0;
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
        for (auto& matchPattern : data.matchPatterns) {
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
    auto manifestObject = m_manifest->asObject();
    ASSERT(manifestObject);

    RefPtr<JSON::Array> resourcesArray = manifestObject->getArray(webAccessibleResourcesManifestKey);

    if (resourcesArray) {
        bool errorOccured = false;
        for (auto i : *resourcesArray) {
            auto obj = i->asObject();
            if (!obj)
                continue;

            auto pathsArray = obj->getArray(webAccessibleResourcesResourcesManifestKey);
            auto matchesArray = obj->getArray(webAccessibleResourcesMatchesManifestKey);

            pathsArray = filterObjects(*pathsArray, [] (auto& value) -> bool {
                return !value.asString().isEmpty();
            });
            matchesArray = filterObjects(*matchesArray, [] (auto& value) -> bool {
                return !value.asString().isEmpty();
            });

            if (!pathsArray || !matchesArray) {
                errorOccured = true;
                continue;
            }

            MatchPatternSet matchPatterns;
            for (auto i : *matchesArray) {
                if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(i->asString())) {
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

        if (errorOccured)
            recordError(createError(Error::InvalidWebAccessibleResources));
        else
            recordError(createError(Error::InvalidWebAccessibleResources));
    }
}

void WebExtension::parseWebAccessibleResourcesVersion2()
{
    auto manifestObject = m_manifest->asObject();
    ASSERT(manifestObject);

    RefPtr<JSON::Array> resourcesArray = manifestObject->getArray(webAccessibleResourcesManifestKey);

    if (resourcesArray) {
        resourcesArray = filterObjects(*resourcesArray, [] (auto& value) -> bool {
            return !value.asString().isEmpty();
        });

        m_webAccessibleResources.append({ { }, makeStringVector(*resourcesArray) });
    } else
        recordError(createError(Error::InvalidWebAccessibleResources));
}

void WebExtension::populateWebAccessibleResourcesIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestWebAccessibleResources)
        return;

    m_parsedManifestWebAccessibleResources = true;

    auto manifestObject = m_manifest->asObject();
    ASSERT(manifestObject);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/web_accessible_resources
    if (supportsManifestVersion(3))
        parseWebAccessibleResourcesVersion3();
    else
        parseWebAccessibleResourcesVersion2();
}

std::optional<URL> WebExtension::resourceFileURLForPath(StringView path)
{
    ASSERT(path);

    if (path.startsWith('/'))
        path = path.substring(1);

    if (!path.length() || m_resourceBaseURL.isNull())
        return { };

    auto resourceURL = URL(m_resourceBaseURL, path.toString());

    // Don't allow escaping the base URL with "../".
    if (!resourceURL.string().startsWith(m_resourceBaseURL.string())) {
        RELEASE_LOG_ERROR(Extensions, "Resource URL path escape attempt: %s", resourceURL.string().utf8().data());
        return { };
    }

    return resourceURL;
}

Expected<String, Ref<API::Error>> WebExtension::resourceStringForPath(const String& originalPath, CacheResult cacheResult, SuppressNotFoundErrors suppressErrors)
{
    ASSERT(originalPath);

    String path = originalPath;

    // Remove leading slash to normalize the path for lookup/storage in the cache dictionary.
    if (path.startsWith('/'))
        path = path.substring(1);

    if (auto cachedData = m_resources.getOptional(path)) {
        if (auto cachedString = String::fromUTF8(*cachedData); !cachedString.isEmpty())
            return cachedString;
    }

    bool isServiceWorker = backgroundContentIsServiceWorker();
    if (!isServiceWorker && (path == generatedBackgroundPageFilename))
        return generatedBackgroundContent();

    if (isServiceWorker && (path == generatedBackgroundServiceWorkerFilename))
        return generatedBackgroundContent();

    auto data = resourceDataForPath(path, CacheResult::No, suppressErrors);
    if (!data)
        return makeUnexpected(result.error());

    auto resourceString = emptyString();
    if (!data.empty())
        resourceString = String::fromUTF8(data);

    if (!resourceString)
        return nullString(); // FIXME: Return an API Error here.

    if (cacheResult == CacheResult::Yes)
        m_resources.set(path, resourceString.span8());

    return resourceString;
}

#if PLATFORM(COCOA)
static WKWebExtensionError toAPI(WebExtension::Error error)
{
    switch (error) {
    case WebExtension::Error::Unknown:
        return WKWebExtensionErrorUnknown;
    case WebExtension::Error::ResourceNotFound:
        return WKWebExtensionErrorResourceNotFound;
    case WebExtension::Error::InvalidManifest:
        return WKWebExtensionErrorInvalidManifest;
    case WebExtension::Error::UnsupportedManifestVersion:
        return WKWebExtensionErrorUnsupportedManifestVersion;
    case WebExtension::Error::InvalidAction:
        return WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidActionIcon:
        return WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidBackgroundContent:
        return WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidCommands:
        return WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidContentScripts:
        return WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidContentSecurityPolicy:
        return WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidDeclarativeNetRequest:
        return WKWebExtensionErrorInvalidDeclarativeNetRequestEntry;
    case WebExtension::Error::InvalidDescription:
        return WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidExternallyConnectable:
        return WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidIcon:
        return WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidName:
        return WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidOptionsPage:
        return WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidURLOverrides:
        return WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidVersion:
        return WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidWebAccessibleResources:
        return WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidBackgroundPersistence:
        return WKWebExtensionErrorInvalidBackgroundPersistence;
    case WebExtension::Error::InvalidResourceCodeSignature:
        return WKWebExtensionErrorInvalidResourceCodeSignature;
    }
}
#endif

Ref<API::Error> WebExtension::createError(Error error, const String& customLocalizedDescription, API::Error* underlyingError)
{
#if PLATFORM(COCOA)
    auto errorCode = toAPI(error)
#else
    auto errorCode = (int)error
#endif

    String localizedDescription;

    switch (error) {
    case Error::Unknown:
        localizedDescription = WEB_UI_STRING("An unknown error has occurred.", "WKWebExtensionErrorUnknown description");
        break;

    case Error::ResourceNotFound:
        ASSERT(customLocalizedDescription);
        break;

    case Error::InvalidManifest:
        if (!underlyingError->localizedDescription().isEmpty())
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

    case Error::InvalidActionIcon:
        if (supportsManifestVersion(3))
            localizedDescription = WEB_UI_STRING("Empty or invalid `default_icon` for the `action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for default_icon in action only");
        else
            localizedDescription = WEB_UI_STRING("Empty or invalid `default_icon` for the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for default_icon in browser_action or page_action");
        break;

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
        if (!underlyingError->localizedDescription().isEmpty())
            localizedDescription = WEB_UI_FORMAT_STRING("Unable to parse `declarativeNetRequest` rules: %s", "WKWebExtensionErrorInvalidDeclarativeNetRequest description, because of a JSON error", underlyingError->localizedDescription().utf8().data());
        else
            localizedDescription = WEB_UI_STRING("Unable to parse `declarativeNetRequest` rules because of an unexpected error.", "WKWebExtensionErrorInvalidDeclarativeNetRequest description");
        break;

    case Error::InvalidDescription:
        localizedDescription = WEB_UI_STRING("Missing or empty `description` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for description");
        break;

    case Error::InvalidExternallyConnectable:
        localizedDescription = WEB_UI_STRING("Empty or invalid `externally_connectable` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for externally_connectable");
        break;

    case Error::InvalidIcon:
        localizedDescription = WEB_UI_STRING("Missing or empty `icons` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for icons");
        break;

    case Error::InvalidName:
        localizedDescription = WEB_UI_STRING("Missing or empty `name` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for name");
        break;

    case Error::InvalidOptionsPage:
        if (manifest().asObject()->getValue(optionsUIManifestKey))
            localizedDescription = WEB_UI_STRING("Empty or invalid `options_ui` manifest entry", "WKWebExtensionErrorInvalidManifestEntry description for options UI");
        else
            localizedDescription = WEB_UI_STRING("Empty or invalid `options_page` manifest entry", "WKWebExtensionErrorInvalidManifestEntry description for options page");
        break;

    case Error::InvalidURLOverrides:
        if (manifest().asObject()->getValue(browserURLOverridesManifestKey))
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

    case Error::InvalidResourceCodeSignature:
        ASSERT(customLocalizedDescription);
        break;
    }

    if (customLocalizedDescription.length())
        localizedDescription = customLocalizedDescription;

    const WebCore::ResourceError *resError = new WebCore::ResourceError("WebKitWebExtensionError"_s, errorCode, URL(), localizedDescription);

    return API::Error::create(*resError);
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

bool WebExtension::hasRequestedPermission(const String& permission) const
{
    return m_permissions.contains(permission);
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

void WebExtension::populateExternallyConnectableIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedExternallyConnectable)
        return;

    m_parsedExternallyConnectable = true;

    auto manifestObject = m_manifest->asObject();
    ASSERT(manifestObject);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/externally_connectable

    auto externallyConnectableObj = manifestObject->getObject(externallyConnectableManifestKey);
    if (!externallyConnectableObj)
        return;

    if (!externallyConnectableObj->size())
        return;

    bool errorOccured = false;
    MatchPatternSet matchPatterns;

    auto matchPatternStrings = externallyConnectableObj->getArray(externallyConnectableMatchesManifestKey);
    if (matchPatternStrings) {
        for (auto i : *matchPatternStrings) {
            auto matchPatternString = i->asString();
            if (!matchPatternString || matchPatternString.isNull())
                continue;

            if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(matchPatternString)) {
                if (matchPattern->matchesAllURLs() || !matchPattern->isSupported()) {
                    errorOccured = true;
                    continue;
                }

                // URL patterns must contain at least a second-level domain. Top level domains and wildcards are not standalone patterns.
                if (matchPattern->hostIsPublicSuffix()) {
                    errorOccured = true;
                    continue;
                }

                matchPatterns.add(matchPattern.releaseNonNull());
            }
        }
    }

    m_externallyConnectableMatchPatterns = matchPatterns;

    auto extensionIDs = externallyConnectableObj->getArray(externallyConnectableIDsManifestKey);
    extensionIDs = filterObjects(*extensionIDs, [] (auto& value) {
        return !value.asString().isEmpty();
    });

    if (errorOccured || (matchPatterns.isEmpty() && !extensionIDs->length()))
        recordError(createError(Error::InvalidExternallyConnectable));
}

void WebExtension::populateDisplayStringsIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestDisplayStrings)
        return;

    m_parsedManifestDisplayStrings = true;

    auto manifestObject = m_manifest->asObject();
    ASSERT(manifestObject);

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
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestContentSecurityPolicyStrings)
        return;

    m_parsedManifestContentSecurityPolicyStrings = true;

    auto manifestObject = m_manifest->asObject();
    ASSERT(manifestObject);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/content_security_policy

    if (supportsManifestVersion(3)) {
        if (auto policyObject = manifestObject->getObject(contentSecurityPolicyManifestKey)) {
            m_contentSecurityPolicy = policyObject->getString(contentSecurityPolicyExtensionPagesManifestKey);
            if (!m_contentSecurityPolicy || (!policyObject->size() || policyObject->getValue(contentSecurityPolicyExtensionPagesManifestKey)->type() != JSON::Value::Type::String))
                recordError(createError(Error::InvalidContentSecurityPolicy));
        }
    } else {
        m_contentSecurityPolicy = manifestObject->getString(contentSecurityPolicyManifestKey);
        if (!m_contentSecurityPolicy && !manifestObject->getValue(contentSecurityPolicyManifestKey)->isNull())
            recordError(createError(Error::InvalidContentSecurityPolicy));
    }

    if (!m_contentSecurityPolicy)
        m_contentSecurityPolicy = "script-src 'self'"_s;
}

RefPtr<WebCore::Icon> WebExtension::icon(WebCore::FloatSize size)
{
    if (!manifestParsedSuccessfully())
        return nullptr;

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    if (m_manifest->asObject()->getValue(iconVariantsManifestKey)) {
        String localizedErrorDescription = WEB_UI_STRING("Failed to load images in `icon_variants` manifest entry.", "WKWebExtensionErrorInvalidIcon description for failing to load image variants");
        return bestImageForIconVariantsManifestKey(m_manifest.get(), iconsManifestKey, size, m_iconsCache, Error::InvalidIcon, localizedErrorDescription);
    }
#endif

    String localizedErrorDescription = WEB_UI_STRING("Failed to load images in `icons` manifest entry.", "WKWebExtensionErrorInvalidIcon description for failing to load images");
    return bestImageForIconsDictionaryManifestKey(m_manifest.get(), iconsManifestKey, size, m_iconsCache, Error::InvalidIcon, localizedErrorDescription);
}

RefPtr<WebCore::Icon> WebExtension::actionIcon(WebCore::FloatSize size)
{
    if (!manifestParsedSuccessfully())
        return nullptr;

    populateActionPropertiesIfNeeded();

    if (m_defaultActionIcon)
        return m_defaultActionIcon.get();

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    if (m_actionDictionary.get()->asObject()->getValue(iconVariantsManifestKey)) {
        String localizedErrorDescription = WEB_UI_STRING("Failed to load images in `icon_variants` for the `action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load image variants for action");
        if (auto result = bestImageForIconVariantsManifestKey(*m_actionDictionary, iconVariantsManifestKey, size, m_actionIconsCache, Error::InvalidActionIcon, localizedErrorDescription))
            return result;
        return icon(size);
    }
#endif

    String localizedErrorDescription;
    if (supportsManifestVersion(3))
        localizedErrorDescription = WEB_UI_STRING("Failed to load images in `default_icon` for the `action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load images for action only");
    else
        localizedErrorDescription = WEB_UI_STRING("Failed to load images in `default_icon` for the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load images for browser_action or page_action");

    if (auto result = bestImageForIconsDictionaryManifestKey(*m_actionDictionary, defaultIconManifestKey, size, m_actionIconsCache, Error::InvalidIcon, localizedErrorDescription))
        return result;
    return icon(size);
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
    auto manifestObject = m_manifest->asObject();
    if (!manifestObject)
        return false;

    auto action = manifestObject->getValue(actionManifestKey);
    return supportsManifestVersion(3) && action && action->type() == JSON::Value::Type::Object;
}

bool WebExtension::hasBrowserAction()
{
    auto manifestObject = m_manifest->asObject();
    if (!manifestObject)
        return false;

    auto action = manifestObject->getValue(browserActionManifestKey);
    return !supportsManifestVersion(3) && action && action->type() == JSON::Value::Type::Object;
}

bool WebExtension::hasPageAction()
{
    auto manifestObject = m_manifest->asObject();
    if (!manifestObject)
        return false;

    auto action = manifestObject->getValue(browserActionManifestKey);
    return !supportsManifestVersion(3) && action && action->type() == JSON::Value::Type::Object;
}

void WebExtension::populateActionPropertiesIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestActionProperties)
        return;

    m_parsedManifestActionProperties = true;

    auto manifestObject = m_manifest->asObject();
    ASSERT(manifestObject);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/action
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/browser_action
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/page_action

    if (supportsManifestVersion(3))
        m_actionDictionary = manifestObject->getObject(actionManifestKey);
    else {
        auto dictionary = manifestObject->getValue(browserActionManifestKey);
        if (m_actionDictionary->isNull())
            m_actionDictionary = manifestObject->getObject(pageActionManifestKey);
        else
            m_actionDictionary = dictionary->asObject();
    }

    if (!m_actionDictionary)
        return;

    // Look for the "default_icon" as a string, which is useful for SVG icons. Only supported by Firefox currently.
    String defaultIconPath = m_actionDictionary->asObject()->getString(defaultIconManifestKey);
    if (!defaultIconPath.isEmpty()) {
        m_defaultActionIcon = imageForPath(defaultIconPath);

        if (!m_defaultActionIcon) {
            recordError(m_defaultActionIcon.error());

            String localizedErrorDescription;
            if (supportsManifestVersion(3))
                localizedErrorDescription = WEB_UI_STRING("Failed to load image for `default_icon` in the `action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load single image for action");
            else
                localizedErrorDescription = WEB_UI_STRING("Failed to load image for `default_icon` in the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load single image for browser_action or page_action");

            recordError(createError(Error::InvalidActionIcon, localizedErrorDescription));
        }
    }

    m_displayActionLabel = m_actionDictionary->asObject()->getString(defaultTitleManifestKey);
    m_actionPopupPath = m_actionDictionary->asObject()->getString(defaultPopupManifestKey);
}

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
bool WebExtension::hasSidebar()
{
    return m_manifest->getValue(sidebarActionManifestKey) || WebExtensionPermission::sidePanel();
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
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestSidebarProperties)
        return;

    m_parsedManifestSidebarProperties = true;

    auto manifestObject = m_manifest->asObject();
    ASSERT(manifestObject);

    // sidePanel documentation: https://developer.chrome.com/docs/extensions/reference/manifest#side-panel
    // see "Examples" header -> "Side Panel" tab (doesn't mention `default_path` key elsewhere)
    // sidebarAction documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/sidebar_action

    auto sidebarActionDictionary = manifestObject->getObject(sidebarActionManifestKey);
    if (sidebarActionDictionary) {
        populateSidebarActionProperties(sidebarActionDictionary);
        return;
    }

    auto sidePanelDictionary = manifestObject->getObject(sidePanelManifestKey);
    if (sidePanelDictionary)
        populateSidePanelProperties(sidePanelDictionary);
}

void WebExtension::populateSidebarActionProperties(RefPtr<JSON::Object> sidebarActionDictionary)
{
    // FIXME: <https://webkit.org/b/276833> implement sidebar icon parsing
    m_sidebarIconsCache = nullptr;
    m_sidebarTitle = sidebarActionDictionary->getString(sidebarActionTitleManifestKey);
    m_sidebarDocumentPath = sidebarActionDictionary->getString(sidebarActionPathManifestKey);
}

void WebExtension::populateSidePanelProperties(RefPtr<JSON::Object> sidePanelDictionary)
{
    // Since sidePanel cannot set a default title or icon from the manifest, setting these nil here is intentional.
    m_sidebarIconsCache = nullptr;
    m_sidebarTitle = String();
    m_sidebarDocumentPath = sidePanelDictionary->getString(sidePanelPathManifestKey);
}
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

std::optional<String> WebExtension::pathForBestImageInIconsDictionary(const JSON::Value& value, size_t idealPixelSize)
{
    size_t bestSize = bestSizeInIconsDictionary(value, idealPixelSize);
    if (!bestSize)
        return { };

    auto object = value.asObject();
    if (!object)
        return { };

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    if (bestSize == std::numeric_limits<size_t>::max())
        return object->getString(anyManifestKey);
#endif

    return object->getString(String::number(bestSize));
}

size_t WebExtension::bestSizeInIconsDictionary(const JSON::Value& value, size_t idealPixelSize)
{
    auto iconsDictionary = value.asObject();
    if (!iconsDictionary || !iconsDictionary->size())
        return 0;

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    // Check if the "any" size exists (typically a vector image), and prefer it.
    if (iconsDictionary->getValue(anyManifestKey)) {
        // Return max to ensure it takes precedence over all other sizes.
        return std::numeric_limits<size_t>::max();
    }
#endif

    // Check if the ideal size exists, if so return it.
    auto iconSizeString = iconsDictionary->getString(String::number(idealPixelSize));
    if (!iconSizeString.isEmpty()) {
        if (auto integer = parseInteger<size_t>(iconSizeString))
            return *integer;
    }

    // Sort the remaining keys and find the next largest size.
    auto sizeKeys = JSON::Array::create();
    for (auto key : iconsDictionary->keys()) {
        // Filter the values to only include numeric strings representing sizes. This will exclude non-numeric string
        // values such as "any", "color_schemes", and any other strings that cannot be converted to a positive integer.
        auto integer = parseInteger<size_t>(key);
        if (integer && integer > 0)
            sizeKeys->pushString(key);
    }

    if (!sizeKeys->length())
        return 0;

    std::sort(sizeKeys->begin(), sizeKeys->end(), [](Ref<JSON::Value> a, Ref<JSON::Value> b) {
        auto aInt = a->asInteger();
        auto bInt = b->asInteger();

        if (aInt && bInt)
            return aInt > bInt;

        return false;
    });

    size_t bestSize = 0;
    for (size_t i = 0; i < sizeKeys->length(); ++i) {
        auto sizeInt = sizeKeys->get(i)->asInteger();
        if (sizeInt) {
            bestSize = *sizeInt;
            if (bestSize >= idealPixelSize)
                break;
        }
    }

    ASSERT(bestSize);

    return bestSize;
}

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
static OptionSet<WebExtension::ColorScheme> toColorSchemes(RefPtr<JSON::Value> value)
{
    using ColorScheme = WebExtension::ColorScheme;

    if (!value) {
        // A nil value counts as all color schemes.
        return { ColorScheme::Light, ColorScheme::Dark };
    }

    OptionSet<ColorScheme> result;

    auto array = value->asArray();

    if (!array)
        return { ColorScheme::Light, ColorScheme::Dark };

    for (auto i : *array) {
        if (i->asString() == lightManifestKey)
            result.add(ColorScheme::Light);
        else if (i->asString() == darkManifestKey)
            result.add(ColorScheme::Dark);
    }

    return result;
}

RefPtr<JSON::Value> WebExtension::iconsDictionaryForBestIconVariant(RefPtr<JSON::Value> variants, size_t idealPixelSize, ColorScheme idealColorScheme)
{
    if (!variants)
        return JSON::Value::null();

    if (!variants->asArray())
        return JSON::Value::null();

    if (variants->asArray()->length() == 1)
        return variants->asArray()->get(0);

    auto bestVariant = JSON::Value::null();
    auto fallbackVariant = JSON::Value::null();
    bool foundIdealFallbackVariant = false;

    size_t bestSize = 0;
    size_t fallbackSize = 0;

    // Pick the first variant matching color scheme and/or size.
    for (auto variant : *variants->asArray()) {
        if (!variant->asObject())
            return;
        if (!variant->asObject()->getArray(colorSchemesManifestKey))
            return;
        auto colorSchemes = toColorSchemes(variant->asObject()->getArray(colorSchemesManifestKey)->asValue());
        auto currentBestSize = bestSizeInIconsDictionary(variant, idealPixelSize);

        if (colorSchemes.contains(idealColorScheme)) {
            if (currentBestSize >= idealPixelSize) {
                // Found the best variant, return it.
                return variant;
            }

            if (currentBestSize > bestSize) {
                // Found a larger ideal variant.
                bestSize = currentBestSize;
                bestVariant = variant;
            }
        } else if (!foundIdealFallbackVariant && currentBestSize >= idealPixelSize) {
            // Found an ideal fallback variant, based only on size.
            fallbackSize = currentBestSize;
            fallbackVariant = variant;
            foundIdealFallbackVariant = true;
        } else if (!foundIdealFallbackVariant && currentBestSize > fallbackSize) {
            // Found a smaller fallback variant.
            fallbackSize = currentBestSize;
            fallbackVariant = variant;
        }
    }

    if (!!bestVariant)
        return bestVariant;

    return fallbackVariant;
}
#endif

bool WebExtension::hasBackgroundContent()
{
    populateBackgroundPropertiesIfNeeded();
    return m_backgroundScriptPaths.size() || !m_backgroundPagePath.isEmpty() || !m_backgroundServiceWorkerPath.isEmpty();
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

    if (m_backgroundScriptPaths.size()) {
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

    if (m_backgroundScriptPaths.size())
        return nullString();

    bool isServiceWorker = backgroundContentIsServiceWorker();
    bool usesModules = backgroundContentUsesModules();

    auto scriptsArray = JSON::Array::create();
    for (auto scriptPath : m_backgroundScriptPaths) {
        if (!scriptPath)
            continue;

        StringBuilder format;
        // check this: break is likely wrong
        if (isServiceWorker) {
            if (usesModules) {
                format.append("import \""_s, scriptPath, "\";"_s);
                scriptsArray->pushString(format.toString());
                break;
            }

            format.append("importScripts(\""_s, scriptPath, "\");"_s);
            scriptsArray->pushString(format.toString());
            break;
        }

        format.append("<script"_s);
        if (usesModules)
            format.append(" type=\"module\" "_s);
        format.append("src=\""_s, scriptPath, "\"></script>"_s);
        scriptsArray->pushString(format.toString());
    }

    StringBuilder generatedBackgroundContent;
    if (!isServiceWorker)
        generatedBackgroundContent.append("<!DOCTYPE html>\n<body>\n"_s);

    for (auto scriptPath = scriptsArray->begin(); scriptPath != scriptsArray->end(); ++scriptPath) {
        if (!scriptPath)
            continue;

        generatedBackgroundContent.append(scriptPath->get().asString(), "\n"_s);
    }

    if (!isServiceWorker)
        generatedBackgroundContent.append("\n</body>"_s);

    m_generatedBackgroundContent = generatedBackgroundContent.toString();
    return m_generatedBackgroundContent;
}

void WebExtension::populateBackgroundPropertiesIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestBackgroundProperties)
        return;

    m_parsedManifestBackgroundProperties = true;

    auto manifestObject = m_manifest->asObject();
    ASSERT(manifestObject);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/background

    auto backgroundManifestDictionary = manifestObject->getObject(backgroundManifestKey);
    if (!backgroundManifestDictionary->size()) {
        if (!manifestObject->getValue(backgroundManifestKey)->isNull())
            recordError(createError(Error::InvalidBackgroundContent));
        return;
    }

    m_backgroundPagePath = backgroundManifestDictionary->getString(backgroundPageManifestKey);
    m_backgroundServiceWorkerPath = backgroundManifestDictionary->getString(backgroundServiceWorkerManifestKey);
    m_backgroundContentUsesModules = (backgroundManifestDictionary->getString(backgroundPageTypeKey) == backgroundPageTypeModuleValue);

    if (auto backgroundScriptPaths = backgroundManifestDictionary->getArray(backgroundScriptsManifestKey)) {
        backgroundScriptPaths = filterObjects(*backgroundScriptPaths, [] (auto& value) {
            return !value.asString().isEmpty();
        });

        m_backgroundScriptPaths = makeStringVector(*backgroundScriptPaths);
    }

    auto supportedEnvironments = StdSet<WTF::CString>({ String(backgroundDocumentManifestKey).utf8(), String(backgroundServiceWorkerManifestKey).utf8() });

    // only CString has operator<.
    StdSet<WTF::CString> preferredEnvironments;
    auto environment = backgroundManifestDictionary->getString(backgroundPreferredEnvironmentManifestKey);
    auto environments = backgroundManifestDictionary->getArray(backgroundPreferredEnvironmentManifestKey);
    if (!environment.isEmpty()) {
        if (supportedEnvironments.contains(environment.utf8()))
            preferredEnvironments.insert(environment.utf8());
    } else if (environments) {
        auto filteredEnvironments = filterObjects(*environments, [supportedEnvironments] (auto& value) {
            return supportedEnvironments.contains(value.asString().utf8());
        });

        for (auto i = filteredEnvironments->begin(); i != filteredEnvironments->end(); ++i)
            preferredEnvironments.insert(i->get().asString().utf8());
    } else if (!backgroundManifestDictionary->getValue(backgroundPreferredEnvironmentManifestKey)->isNull())
        recordError(createError(Error::InvalidBackgroundContent, WEB_UI_STRING("Manifest `background` entry has an empty or invalid `preferred_environment` key.", "WKWebExtensionErrorInvalidBackgroundContent description for empty or invalid preferred environment key")));

    for (auto environment : preferredEnvironments) {
        if (String::fromUTF8(environment.data()) == backgroundDocumentManifestKey) {
            m_backgroundContentEnvironment = Environment::Document;
            m_backgroundServiceWorkerPath = String();

            if (!m_backgroundPagePath.isEmpty()) {
                // Page takes precedence over scripts and service worker.
                m_backgroundScriptPaths = Vector<String>();
                break;
            }

            if (m_backgroundScriptPaths.size()) {
                // Scripts takes precedence over service worker.
                break;
            }

            recordError(createError(Error::InvalidBackgroundContent, WEB_UI_STRING("Manifest `background` entry has missing or empty required `page` or `scripts` key for `preferred_environment` of `document`.", "WKWebExtensionErrorInvalidBackgroundContent description for missing background page or scripts keys")));
            break;
        }

        if (String::fromUTF8(environment.data()) == backgroundServiceWorkerManifestKey) {
            m_backgroundContentEnvironment = Environment::ServiceWorker;
            m_backgroundPagePath = String();

            if (!m_backgroundServiceWorkerPath.isEmpty()) {
                // Page takes precedence over scripts and service worker.
                m_backgroundScriptPaths = Vector<String>();
                break;
            }

            if (m_backgroundScriptPaths.size()) {
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
            m_backgroundServiceWorkerPath = String();

        // Scripts takes precedence over page and service worker.
        if (m_backgroundScriptPaths.size()) {
            m_backgroundServiceWorkerPath = String();
            m_backgroundPagePath = String();
        }

        m_backgroundContentEnvironment = !m_backgroundServiceWorkerPath.isEmpty() ? Environment::ServiceWorker : Environment::Document;

        if (!m_backgroundScriptPaths.size() && m_backgroundPagePath.isEmpty() && m_backgroundServiceWorkerPath.isEmpty())
            recordError(createError(Error::InvalidBackgroundContent, WEB_UI_STRING("Manifest `background` entry has missing or empty required `scripts`, `page`, or `service_worker` key.", "WKWebExtensionErrorInvalidBackgroundContent description for missing background required keys")));
    }

    auto persistentBoolean = backgroundManifestDictionary->getBoolean(backgroundPersistentManifestKey);
    m_backgroundContentIsPersistent = persistentBoolean ? *persistentBoolean : !(supportsManifestVersion(3) || !m_backgroundServiceWorkerPath.isEmpty());

    if (m_backgroundContentIsPersistent && supportsManifestVersion(3)) {
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Invalid `persistent` manifest entry. A `manifest_version` greater-than or equal to `3` must be non-persistent.", "WKWebExtensionErrorInvalidBackgroundPersistence description for manifest v3")));
        m_backgroundContentIsPersistent = false;
    }

    if (m_backgroundContentIsPersistent && !m_backgroundServiceWorkerPath.isEmpty()) {
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Invalid `persistent` manifest entry. A `service_worker` must be non-persistent.", "WKWebExtensionErrorInvalidBackgroundPersistence description for service worker")));
        m_backgroundContentIsPersistent = false;
    }

    if (!m_backgroundContentIsPersistent && hasRequestedPermission(WebExtensionPermission::webRequest()))
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
    return !!inspectorBackgroundPagePath();
}

const String& WebExtension::inspectorBackgroundPagePath()
{
    populateInspectorPropertiesIfNeeded();
    return m_inspectorBackgroundPagePath;
}

void WebExtension::populateInspectorPropertiesIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestInspectorProperties)
        return;

    m_parsedManifestInspectorProperties = true;

    auto manifestObject = m_manifest->asObject();
    ASSERT(manifestObject);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/devtools_page

    m_inspectorBackgroundPagePath = manifestObject->getString(devtoolsPageManifestKey);
}

bool WebExtension::hasOptionsPage()
{
    populatePagePropertiesIfNeeded();
    return !!m_optionsPagePath;
}

bool WebExtension::hasOverrideNewTabPage()
{
    populatePagePropertiesIfNeeded();
    return !!m_overrideNewTabPagePath;
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
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestPageProperties)
        return;

    m_parsedManifestPageProperties = true;

    auto manifestObject = m_manifest->asObject();
    ASSERT(manifestObject);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/options_ui
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/options_page
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/chrome_url_overrides

    auto optionsDictionary = manifestObject->getObject(optionsUIManifestKey);
    if (optionsDictionary) {
        m_optionsPagePath = optionsDictionary->getString(optionsUIPageManifestKey);
        if (m_optionsPagePath.isEmpty())
            recordError(createError(Error::InvalidOptionsPage));
    } else {
        m_optionsPagePath = manifestObject->getString(optionsPageManifestKey);
        if (m_optionsPagePath.isEmpty())
            recordError(createError(Error::InvalidOptionsPage));
    }

    auto overridesDictionary = manifestObject->getObject(browserURLOverridesManifestKey);
    if (!overridesDictionary)
        overridesDictionary = manifestObject->getObject(chromeURLOverridesManifestKey);

    if (overridesDictionary && overridesDictionary->size()) {
        m_overrideNewTabPagePath = overridesDictionary->getString(newTabManifestKey);
        if (m_optionsPagePath.isEmpty())
            recordError(createError(Error::InvalidURLOverrides, WEB_UI_STRING("Empty or invalid `newtab` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid new tab entry")));
    } else
        recordError(createError(Error::InvalidURLOverrides));
}

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
        { "F1"_s, "\\uF704"_s },
        { "F2"_s, "\\uF705"_s },
        { "F3"_s, "\\uF706"_s },
        { "F4"_s, "\\uF707"_s },
        { "F5"_s, "\\uF708"_s },
        { "F6"_s, "\\uF709"_s },
        { "F7"_s, "\\uF70A"_s },
        { "F8"_s, "\\uF70B"_s },
        { "F9"_s, "\\uF70C"_s },
        { "F10"_s, "\\uF70D"_s },
        { "F11"_s, "\\uF70E"_s },
        { "F12"_s, "\\uF70F"_s },
        { "Insert"_s, "\\uF727"_s },
        { "Delete"_s, "\\uF728"_s },
        { "Home"_s, "\\uF729"_s },
        { "End"_s, "\\uF72B"_s },
        { "PageUp"_s, "\\uF72C"_s },
        { "PageDown"_s, "\\uF72D"_s },
        { "Up"_s, "\\uF700"_s },
        { "Down"_s, "\\uF701"_s },
        { "Left"_s, "\\uF702"_s },
        { "Right"_s, "\\uF703"_s }
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
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestCommands)
        return;

    m_parsedManifestCommands = true;

    auto manifestObject = m_manifest->asObject();
    ASSERT(manifestObject);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/commands

    auto commandsDictionary = manifestObject->getObject(commandsManifestKey);
    if (!commandsDictionary) {
        recordError(createError(Error::InvalidCommands));
        return;
    }

    size_t commandsWithShortcuts = 0;
    std::optional<String> error;

    bool hasActionCommand = false;

    for (auto commandIdentifier : commandsDictionary->keys()) {
        if (commandIdentifier.isEmpty()) {
            error = WEB_UI_STRING("Empty or invalid identifier in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command identifier");
            continue;
        }

        auto commandDictionary = commandsDictionary->getObject(commandIdentifier);
        if (!commandDictionary->size()) {
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
        auto description = commandDictionary->getString(commandsDescriptionKeyManifestKey);
        if (description.isEmpty() && !isActionCommand) {
            error = WEB_UI_STRING("Empty or invalid `description` in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command description");
            continue;
        }

        if (isActionCommand && !description.isEmpty()) {
            description = displayActionLabel();
            if (!description.isEmpty())
                description = displayShortName();
        }

        commandData.description = description;

        if (auto suggestedKeyDictionary = commandDictionary->getObject(commandsSuggestedKeyManifestKey)) {
#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
            const auto macPlatform = "mac"_s;
            const auto iosPlatform = "ios"_s;
#elif PLATFORM(GTK)
            const auto linuxPlatform = "linux"_s;
#endif
            const auto defaultPlatform = "default"_s;

#if PLATFORM(MAC)
            auto platformShortcut = !suggestedKeyDictionary->getString(macPlatform).isEmpty() ? suggestedKeyDictionary->getString(macPlatform) : suggestedKeyDictionary->getString(iosPlatform);
#elif PLATFORM(IOS_FAMILY)
            auto platformShortcut = !suggestedKeyDictionary->getString(iosPlatform).isEmpty() ? suggestedKeyDictionary->getString(iosPlatform) : suggestedKeyDictionary->getString(macPlatform);
#elif PLATFORM(GTK)
            auto platformShortcut = suggestedKeyDictionary->getString(linuxPlatform);
#else
            auto platformShortcut = suggestedKeyDictionary->getString(defaultPlatform);
#endif
            if (platformShortcut.isEmpty())
                platformShortcut = suggestedKeyDictionary->getString(defaultPlatform);

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

    if (error)
        recordError(createError(Error::InvalidCommands, error.value()));
}

std::optional<WebExtension::DeclarativeNetRequestRulesetData> WebExtension::parseDeclarativeNetRequestRulesetDictionary(RefPtr<JSON::Value> rulesetDictionary, RefPtr<API::Error> *error)
{
    std::array<String, 3> requiredKeysInRulesetDictionary = {
        declarativeNetRequestRulesetIDManifestKey,
        declarativeNetRequestRuleEnabledManifestKey,
        declarativeNetRequestRulePathManifestKey
    };

    if (auto rulesetObject = rulesetDictionary->asObject()) {
        String rulesetID = rulesetDictionary->asObject()->getString(declarativeNetRequestRulesetIDManifestKey);
        if (rulesetID.isEmpty()) {
            *error = createError(WebExtension::Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Empty `declarative_net_request` ruleset id.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for empty ruleset id"));
            return { };
        }

        String jsonPath = rulesetDictionary->asObject()->getString(declarativeNetRequestRulePathManifestKey);
        if (jsonPath.isEmpty()) {
            *error = createError(WebExtension::Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Empty `declarative_net_request` JSON path.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for empty JSON path"));
            return { };
        }

        std::optional<bool> enabled = rulesetDictionary->asObject()->getBoolean(declarativeNetRequestRuleEnabledManifestKey);
        if (!enabled)
            *enabled = false;

        DeclarativeNetRequestRulesetData rulesetData = {
            rulesetID,
            *enabled,
            jsonPath
        };

        return rulesetData;
    }

    return { };
}

void WebExtension::populateDeclarativeNetRequestPropertiesIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestDeclarativeNetRequestRulesets)
        return;

    m_parsedManifestDeclarativeNetRequestRulesets = true;

    auto manifestObject = m_manifest->asObject();
    ASSERT(manifestObject);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/declarative_net_request

    if (!supportedPermissions().contains(WebExtensionPermission::declarativeNetRequest()) && !supportedPermissions().contains(WebExtensionPermission::declarativeNetRequestWithHostAccess())) {
        recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Manifest has no `declarativeNetRequest` permission.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for missing declarativeNetRequest permission")));
        return;
    }

    auto declarativeNetRequestManifestDictionary = manifestObject->getValue(declarativeNetRequestManifestKey);
    if (!declarativeNetRequestManifestDictionary || declarativeNetRequestManifestDictionary->type() != JSON::Value::Type::Object) {
        recordError(createError(Error::InvalidDeclarativeNetRequest));
        return;
    }

    auto declarativeNetRequestRulesets = declarativeNetRequestManifestDictionary->asObject()->getArray(declarativeNetRequestRulesManifestKey);
    if (!declarativeNetRequestRulesets->length()) {
        recordError(createError(Error::InvalidDeclarativeNetRequest));
        return;
    }

    if (declarativeNetRequestRulesets->length() > webExtensionDeclarativeNetRequestMaximumNumberOfStaticRulesets)
        recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Exceeded maximum number of `declarative_net_request` rulesets. Ignoring extra rulesets.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for too many rulesets")));

    unsigned rulesetCount = 0;
    unsigned enabledRulesetCount = 0;
    bool recordedTooManyRulesetsManifestError = false;
    HashSet<String> seenRulesetIDs;
    for (auto i : *declarativeNetRequestRulesets) {
        if (rulesetCount >= webExtensionDeclarativeNetRequestMaximumNumberOfStaticRulesets)
            continue;

        RefPtr<API::Error> *error = nullptr;
        auto optionalRuleset = parseDeclarativeNetRequestRulesetDictionary(adoptRef(i.get()), error);
        if (!optionalRuleset && error) {
            recordError(createError(Error::InvalidDeclarativeNetRequest, String(), error->get()));
            continue;
        }

        auto ruleset = optionalRuleset.value();
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

        m_declarativeNetRequestRulesets.append(ruleset);
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

    return { };
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
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestContentScriptProperties)
        return;

    m_parsedManifestContentScriptProperties = true;

    auto manifestObject = m_manifest->asObject();
    ASSERT(manifestObject);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/content_scripts

    auto contentScriptsManifestArray = manifestObject->getArray(contentScriptsManifestKey);
    if (!contentScriptsManifestArray || !contentScriptsManifestArray->length()) {
        recordError(createError(Error::InvalidContentScripts));
        return;
    }

    auto addInjectedContentData = [this](auto dictionary) {
        HashSet<Ref<WebExtensionMatchPattern>> includeMatchPatterns;

        // Required. Specifies which pages the specified scripts and stylesheets will be injected into.
        auto matchesArray = dictionary->getArray(contentScriptsMatchesManifestKey);
        if (!matchesArray) {
            recordError(createError(Error::InvalidContentScripts));
            return;
        }

        for (auto matchPatternStringValue : *matchesArray) {
            String matchPatternString = matchPatternStringValue->asString();

            if (!matchPatternString)
                continue;

            if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(matchPatternString)) {
                if (matchPattern->isSupported())
                    includeMatchPatterns.add(matchPattern.releaseNonNull());
            }
        }

        if (includeMatchPatterns.isEmpty()) {
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has no specified `matches` entry.", "WKWebExtensionErrorInvalidContentScripts description for missing matches entry")));
            return;
        }

        // Optional. The list of JavaScript files to be injected into matching pages. These are injected in the order they appear in this array.
        auto scriptPaths = dictionary->getArray(contentScriptsJSManifestKey);
        if (!scriptPaths)
            return;

        scriptPaths = filterObjects(*scriptPaths, [] (auto& value) {
            return !!value.asString().isEmpty();
        });

        // Optional. The list of CSS files to be injected into matching pages. These are injected in the order they appear in this array, before any DOM is constructed or displayed for the page.
        auto styleSheetPaths = dictionary->getArray(contentScriptsCSSManifestKey);
        if (!styleSheetPaths)
            return;

        styleSheetPaths = filterObjects(*styleSheetPaths, [] (auto& value) {
            return !!value.asString().isEmpty();
        });

        if (!scriptPaths->length() && !styleSheetPaths->length()) {
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has missing or empty 'js' and 'css' arrays.", "WKWebExtensionErrorInvalidContentScripts description for missing or empty 'js' and 'css' arrays")));
            return;
        }

        // Optional. Whether the script should inject into an about:blank frame where the parent or opener frame matches one of the patterns declared in matches. Defaults to false.
        bool matchesAboutBlank = *dictionary->getBoolean(contentScriptsMatchesAboutBlankManifestKey);

        HashSet<Ref<WebExtensionMatchPattern>> excludeMatchPatterns;

        // Optional. Excludes pages that this content script would otherwise be injected into.
        auto excludeMatchesArray = dictionary->getArray(contentScriptsExcludeMatchesManifestKey);
        if (!excludeMatchesArray)
            return;

        for (auto matchPatternStringValue : *excludeMatchesArray) {
            String matchPatternString = matchPatternStringValue->asString();

            if (matchPatternString.isEmpty())
                continue;

            if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(matchPatternString)) {
                if (matchPattern->isSupported())
                    excludeMatchPatterns.add(matchPattern.releaseNonNull());
            }
        }

        // Optional. Applied after matches to include only those URLs that also match this glob.
        auto includeGlobPatternStrings = dictionary->getArray(contentScriptsIncludeGlobsManifestKey);
        if (!includeGlobPatternStrings)
            return;

        includeGlobPatternStrings = filterObjects(*includeGlobPatternStrings, [] (auto& value) {
            return !!value.asString().isEmpty();
        });

        // Optional. Applied after matches to exclude URLs that match this glob.
        auto excludeGlobPatternStrings = dictionary->getArray(contentScriptsExcludeGlobsManifestKey);
        if (!excludeGlobPatternStrings)
            return;

        excludeGlobPatternStrings = filterObjects(*excludeGlobPatternStrings, [] (auto& value) -> bool {
            return !!value.asString().isEmpty();
        });

        // Optional. The "all_frames" field allows the extension to specify if JavaScript and CSS files should be injected into all frames matching the specified URL requirements or only into the
        // topmost frame in a tab. Defaults to false, meaning that only the top frame is matched. If specified true, it will inject into all frames, even if the frame is not the topmost frame in
        // the tab. Each frame is checked independently for URL requirements, it will not inject into child frames if the URL requirements are not met.
        bool injectsIntoAllFrames = *dictionary->getBoolean(contentScriptsAllFramesManifestKey);

        InjectionTime injectionTime = InjectionTime::DocumentIdle;
        String runsAtString = dictionary->getString(contentScriptsRunAtManifestKey);
        if (!runsAtString || (runsAtString == contentScriptsDocumentIdleManifestKey))
            injectionTime = InjectionTime::DocumentIdle;
        else if (runsAtString == contentScriptsDocumentStartManifestKey)
            injectionTime = InjectionTime::DocumentStart;
        else if (runsAtString == contentScriptsDocumentEndManifestKey)
            injectionTime = InjectionTime::DocumentEnd;
        else
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has unknown `run_at` value.", "WKWebExtensionErrorInvalidContentScripts description for unknown 'run_at' value")));

        WebExtensionContentWorldType contentWorldType = WebExtensionContentWorldType::ContentScript;
        String worldString = dictionary->getString(contentScriptsWorldManifestKey);
        if (!worldString || (runsAtString == contentScriptsIsolatedManifestKey))
            contentWorldType = WebExtensionContentWorldType::ContentScript;
        else if (worldString == contentScriptsMainManifestKey)
            contentWorldType = WebExtensionContentWorldType::Main;
        else
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has unknown `world` value.", "WKWebExtensionErrorInvalidContentScripts description for unknown 'world' value")));

        auto styleLevel = WebCore::UserStyleLevel::Author;
        String cssOriginString = dictionary->getString(contentScriptsCSSOriginManifestKey);
        if (!cssOriginString || (cssOriginString == contentScriptsAuthorManifestKey))
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

    for (auto contentScriptsManifestEntry : *contentScriptsManifestArray) {
        if (auto dictionary = contentScriptsManifestEntry->asObject())
            addInjectedContentData(dictionary);
    }
}

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

    for (auto& matchPattern : m_permissionMatchPatterns)
        result.add(matchPattern);

    for (auto& matchPattern : m_externallyConnectableMatchPatterns)
        result.add(matchPattern);

    for (auto& injectedContent : m_staticInjectedContents) {
        for (auto& matchPattern : injectedContent.includeMatchPatterns)
            result.add(matchPattern);
    }

    return result;
}

void WebExtension::populatePermissionsPropertiesIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestPermissionProperties)
        return;

    m_parsedManifestPermissionProperties = true;

    auto manifestObject = m_manifest->asObject();
    ASSERT(manifestObject);

    bool findMatchPatternsInPermissions = !supportsManifestVersion(3);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/permissions

    auto permissions = manifestObject->getArray(permissionsManifestKey);
    if (!permissions)
        return;

    for (auto i : *permissions) {
        auto permission = i->asString();
        if (!permission.isEmpty())
            continue;

        if (findMatchPatternsInPermissions) {
            if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(permission)) {
                if (matchPattern->isSupported())
                    m_permissionMatchPatterns.add(matchPattern.releaseNonNull());
                continue;
            }
        }

        if (supportedPermissions().contains(permission))
            m_permissions.add(permission);
    }

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/host_permissions

    if (!findMatchPatternsInPermissions) {
        auto hostPermissions = manifestObject->getArray(hostPermissionsManifestKey);
        if (!hostPermissions)
            return;

        for (auto i : *hostPermissions) {
            auto permission = i->asString();
            if (!permission.isEmpty())
                continue;

            if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(permission)) {
                if (matchPattern->isSupported())
                    m_permissionMatchPatterns.add(matchPattern.releaseNonNull());
                continue;
            }
        }
    }

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/optional_permissions

    auto optionalPermissions = manifestObject->getArray(optionalPermissionsManifestKey);
    if (!optionalPermissions)
        return;

    for (auto i : *optionalPermissions) {
        auto permission = i->asString();
        if (!permission.isEmpty())
            continue;

        if (findMatchPatternsInPermissions) {
            if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(permission)) {
                if (matchPattern->isSupported() && !m_permissionMatchPatterns.contains(*matchPattern))
                    m_optionalPermissionMatchPatterns.add(matchPattern.releaseNonNull());
            }
            continue;
        }

        if (!m_permissions.contains(permission) && supportedPermissions().contains(permission))
            m_optionalPermissions.add(permission);
    }

    // Documentation: https://github.com/w3c/webextensions/issues/119

    if (!findMatchPatternsInPermissions) {
        auto hostPermissions = manifestObject->getArray(optionalHostPermissionsManifestKey);
        if (!hostPermissions)
            return;

        for (auto i : *hostPermissions) {
            auto permission = i->asString();
            if (!permission.isEmpty())
                continue;

            if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(permission)) {
                if (matchPattern->isSupported() && !m_permissionMatchPatterns.contains(*matchPattern))
                    m_optionalPermissionMatchPatterns.add(matchPattern.releaseNonNull());
            }
        }
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
