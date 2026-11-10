/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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
#include <ranges>
#include <wtf/Borrow.h>
#include <wtf/FileHandle.h>
#include <wtf/FileSystem.h>
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

using namespace WebCore;

static constexpr auto defaultLocaleManifestKey = "default_locale"_s;

static constexpr auto messageSerializationManifestKey = "message_serialization"_s;

static constexpr auto iconsManifestKey = "icons"_s;

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
static constexpr auto iconVariantsManifestKey = "icon_variants"_s;
static constexpr auto colorSchemesManifestKey = "color_schemes"_s;
static constexpr auto colorSchemesAPIKey = "colorSchemes"_s;
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
static constexpr auto contentScriptsMatchAboutBlankManifestKey = "match_about_blank"_s;
static constexpr auto contentScriptsMatchOriginAsFallbackManifestKey = "match_origin_as_fallback"_s;
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

WebExtension::DataResources WebExtension::toDataResources(const WebExtension::Resources& resources)
{
    DataResources result;
    for (auto& [key, value] : resources) {
        if (auto* data = std::get_if<Ref<API::Data>>(&value))
            result.set(key, *data);
    }
    return result;
}

WebExtension::StringResources WebExtension::toStringResources(const WebExtension::Resources& resources)
{
    StringResources result;
    for (auto& [key, value] : resources) {
        if (auto* string = std::get_if<RefPtr<StringImpl>>(&value))
            result.set(key, *string);
    }
    return result;
}

WebExtension::WebExtension(const JSON::Value& manifest, Resources&& resources)
    : m_manifestJSON(manifest)
    , m_dataResources(toDataResources(resources))
    , m_stringResources(toStringResources(resources))
{
    auto manifestString = manifest.toJSONString();
    RELEASE_ASSERT(manifestString);

    m_stringResources.set("manifest.json"_si, manifestString);
}

WebExtension::WebExtension(Resources&& resources)
    : m_manifestJSON(JSON::Value::null())
    , m_dataResources(toDataResources(resources))
    , m_stringResources(toStringResources(resources))
{
}

WebExtension::~WebExtension()
{
    if (m_resourcesAreTemporary && !m_resourceBaseURL.isEmpty())
        FileSystem::deleteNonEmptyDirectory(m_resourceBaseURL.fileSystemPath());
}

static RefPtr<StringImpl> convertChromeExtensionToTemporaryZipFile(StringImpl* inputFilePath)
{
    // Converts a Chrome extension file to a temporary ZIP file by checking for a valid Chrome extension signature ('Cr24')
    // and copying the contents starting from the ZIP signature ('PK\x03\x04'). Returns a null string if the signatures
    // are not found or any file operations fail.

    auto inputFileHandle = FileSystem::openFile(inputFilePath, FileSystem::FileOpenMode::Read);
    if (!inputFileHandle)
        return nullString();

    // Read the magic signature.
    std::array<uint8_t, 4> signature;
    auto bytesRead = inputFileHandle.read(signature);
    if (bytesRead != signature.size())
        return nullString();

    // Verify Chrome extension magic signature.
    static std::array<uint8_t, 4> expectedSignature = { 'C', 'r', '2', '4' };
    if (signature != expectedSignature)
        return nullString();

    // Create a temporary ZIP file.
    auto [temporaryFilePath, temporaryFileHandle] = FileSystem::openTemporaryFile("WebKitExtension-"_s, ".zip"_s);
    if (!temporaryFileHandle)
        return nullString();

    std::array<uint8_t, 4096> buffer;
    bool signatureFound = false;

    while (true) {
        bytesRead = inputFileHandle.read(buffer);

        // Error reading file.
        if (!bytesRead)
            return nullString();

        // Done reading file.
        if (!*bytesRead)
            break;

        size_t bufferOffset = 0;
        if (!signatureFound) {
            // Not enough bytes for the signature.
            if (*bytesRead < 4)
                return nullString();

            // Search for the ZIP file magic signature in the buffer.
            for (size_t i = 0; i < *bytesRead - 3; ++i) {
                if (buffer[i] == 'P' && buffer[i + 1] == 'K' && buffer[i + 2] == 0x03 && buffer[i + 3] == 0x04) {
                    signatureFound = true;
                    bufferOffset = i;
                    break;
                }
            }

            // Continue until the start of the ZIP file is found.
            if (!signatureFound)
                continue;
        }

        auto bytesToWrite = std::span(buffer).subspan(bufferOffset, *bytesRead - bufferOffset);
        auto bytesWritten = temporaryFileHandle.write(bytesToWrite);
        if (bytesWritten != bytesToWrite.size())
            return nullString();
    }

    return temporaryFilePath;
}

RefPtr<StringImpl> WebExtension::processFileAndExtractZipArchive(StringImpl* path)
{
    // Check if the file is a Chrome extension archive and extract it.
    auto temporaryZipFilePath = convertChromeExtensionToTemporaryZipFile(path);
    if (temporaryZipFilePath) {
        auto temporaryDirectory = FileSystem::extractTemporaryZipArchive(temporaryZipFilePath);
        FileSystem::deleteFile(temporaryZipFilePath);
        return temporaryDirectory;
    }

    // Assume the file is already a ZIP archive and try to extract it.
    return FileSystem::extractTemporaryZipArchive(path);
}

bool WebExtension::parseManifest(StringView manifestString)
{
    RefPtr manifestValue = JSON::Value::parseJSON(manifestString, JSON::Value::ParsingMode::AllowTrailingCommas);
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

    if (auto defaultLocale = manifestObject->getString(StringImpl::create(defaultLocaleManifestKey).ptr()); defaultLocale) {
        auto parsedLocale = parseLocale(manifestObject->getString(StringImpl::create(defaultLocaleManifestKey).ptr()));
        if (!isEmpty(parsedLocale.languageCode)) {
            if (supportedLocales().containsIf([&](auto& item) { return equal(item, defaultLocale); }))
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
        return Ref { m_manifestJSON }->asObject();

    m_parsedManifest = true;

    auto manifestStringResult = resourceStringForPath("manifest.json"_si);
    if (!manifestStringResult) {
        recordErrorIfNeeded(manifestStringResult.error());
        return nullptr;
    }

    if (!parseManifest(manifestStringResult.value()))
        return nullptr;

    return Ref { m_manifestJSON }->asObject();
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
    if (auto value = manifestObject->getDouble(StringImpl::create(manifestVersionManifestKey).ptr()))
        return *value;

    return 0;
}

RefPtr<API::Data> WebExtension::serializeManifest()
{
    Ref manifestJSON = m_manifestJSON;
    if (!manifestJSON)
        return nullptr;

    return API::Data::create(utf8(manifestJSON->toJSONString()).span());
}

RefPtr<API::Data> WebExtension::serializeLocalization()
{
    if (!m_localization || !m_localization->localizationJSON())
        return nullptr;

    return API::Data::create(utf8(m_localization->localizationJSON()->toJSONString()).span());
}

RefPtr<WebExtensionLocalization> WebExtension::localization()
{
    if (!manifestParsedSuccessfully())
        return nullptr;

    return m_localization;
}

bool WebExtension::hasRequestedPermission(RefPtr<StringImpl> permission)
{
    populatePermissionsPropertiesIfNeeded();
    return m_permissions.contains(permission);
}

bool WebExtension::isWebAccessibleResource(const URL& resourceURL, const URL& pageURL)
{
    populateWebAccessibleResourcesIfNeeded();

    auto resourcePath = resourceURL.path().toString();

    // The path is expected to match without the prefix slash.
    ASSERT(startsWith(resourcePath, '/'));
    resourcePath = substring(resourcePath, 1);

    for (auto& data : borrow(m_webAccessibleResources)) {
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

        for (auto& pathPattern : borrow(data.resourcePathPatterns)) {
            // Because we remove the prefix slash from the resource path, we also have to remove it from the pattern path.
            if (startsWith(pathPattern, '/'))
                pathPattern = substring(pathPattern, 1);

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

    if (RefPtr resourcesArray = manifestObject->getArray(StringImpl::create(webAccessibleResourcesManifestKey).ptr())) {
        bool errorOccured = false;
        for (Ref resource : *resourcesArray) {
            if (RefPtr resourceObject = resource->asObject()) {
                RefPtr pathsArray = resourceObject->getArray(StringImpl::create(webAccessibleResourcesResourcesManifestKey).ptr());
                if (pathsArray) {
                    pathsArray = filterObjects(*pathsArray, [](auto& value) {
                        return !isEmpty(value.asString());
                    });
                } else {
                    errorOccured = true;
                    continue;
                }

                RefPtr matchesArray = resourceObject->getArray(StringImpl::create(webAccessibleResourcesMatchesManifestKey).ptr());
                if (matchesArray) {
                    matchesArray = filterObjects(*matchesArray, [](auto& value) {
                        return !isEmpty(value.asString());
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

                m_webAccessibleResources.append({ WTF::move(matchPatterns), makeStringVector(*pathsArray) });
            }
        }

        if (errorOccured)
            recordError(createError(Error::InvalidWebAccessibleResources));
    } else if (manifestObject->getValue(StringImpl::create(webAccessibleResourcesManifestKey).ptr()))
        recordError(createError(Error::InvalidWebAccessibleResources));
}

void WebExtension::parseWebAccessibleResourcesVersion2()
{
    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    if (RefPtr resourcesArray = manifestObject->getArray(StringImpl::create(webAccessibleResourcesManifestKey).ptr())) {
        resourcesArray = filterObjects(*resourcesArray, [](auto& value) {
            return !isEmpty(value.asString());
        });

        m_webAccessibleResources.append({ { }, makeStringVector(*resourcesArray) });
    } else if (manifestObject->getValue(StringImpl::create(webAccessibleResourcesManifestKey).ptr()))
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

URL WebExtension::resourceFileURLForPath(StringImpl* originalPath)
{
    ASSERT(originalPath);

    RefPtr<StringImpl> path = originalPath;
    if (startsWith(path, '/'))
        path = substring(path, 1);

    if (!length(path) || m_resourceBaseURL.isEmpty())
        return { };

    URL result { m_resourceBaseURL, path };
    if (!FileSystem::fileExists(result.fileSystemPath()))
        return { };

    // Don't allow escaping the base URL with "../".
    auto basePath = FileSystem::realPath(m_resourceBaseURL.fileSystemPath());
    auto resourcePath = FileSystem::realPath(result.fileSystemPath());
    if (!startsWith(resourcePath, basePath)) {
        RELEASE_LOG_ERROR(Extensions, "Resource URL path escape attempt: %s", utf8(resourcePath));
        return { };
    }

    return result;
}

RefPtr<StringImpl> WebExtension::resourceKeyForPath(StringImpl* originalPath)
{
    // Read the path as a URL, like resourceFileURLForPath(), so a query or fragment is dropped and percent-encoding is decoded.
    auto path = URL { URL { "file:///"_si }, originalPath }.fileSystemPath();
    if (startsWith(path, '/'))
        path = substring(path, 1);

    return path;
}

bool WebExtension::hasResourceForPath(StringImpl* originalPath)
{
    ASSERT(originalPath);

    RefPtr<StringImpl> path = originalPath;
    if (startsWith(path, '/'))
        path = substring(path, 1);

    if (equal(path, generatedBackgroundPageFilename) || equal(path, generatedBackgroundServiceWorkerFilename))
        return true;

    auto resourceKey = resourceKeyForPath(originalPath);
    if (m_stringResources.contains(resourceKey) || m_dataResources.contains(resourceKey))
        return true;

    return !resourceFileURLForPath(path).isEmpty();
}

RefPtr<StringImpl> WebExtension::resourceMIMETypeForPath(StringImpl* path)
{
    auto dataPrefix = "data:"_s;
    if (startsWith(path, dataPrefix)) {
        auto mimeTypePosition = find(path, ';');
        if (mimeTypePosition != notFound)
            return substring(path, dataPrefix.length(), mimeTypePosition - dataPrefix.length());
        return defaultMIMEType();
    }

#if PLATFORM(COCOA)
    if (startsWith(path, "symbol:"_s))
        return defaultMIMEType();
#endif

    return MIMETypeRegistry::mimeTypeForPath(path);
}

std::expected<RefPtr<StringImpl>, RefPtr<API::Error>> WebExtension::resourceStringForPath(StringImpl* originalPath, CacheResult cacheResult, SuppressNotFoundErrors suppressErrors)
{
    ASSERT(originalPath);

    RefPtr<StringImpl> path = originalPath;

    // Remove leading slash to normalize the path for lookup/storage in the cache dictionary.
    if (startsWith(path, '/'))
        path = substring(path, 1);

    if (equal(path, generatedBackgroundPageFilename) || equal(path, generatedBackgroundServiceWorkerFilename))
        return generatedBackgroundContent();

    auto resourceKey = resourceKeyForPath(originalPath);

    if (auto maybeString = m_stringResources.getOptional(resourceKey))
        return *maybeString;

    if (auto maybeData = m_dataResources.getOptional(resourceKey)) {
        auto string = StringImpl::fromUTF8(maybeData->get().span());
        m_stringResources.set(resourceKey, string);
        return string;
    }

    auto dataResult = resourceDataForPath(path, cacheResult, suppressErrors);
    if (!dataResult)
        return makeUnexpected(dataResult.error());

    Ref data = dataResult.value();

    if (!data->size())
        return emptyString();

    auto mimeType = MIMETypeRegistry::mimeTypeForPath(path);
    RefPtr decoder = TextResourceDecoder::create(TextResourceDecoder::contentTypeForMIMEType(mimeType), PAL::UTF8Encoding());

    auto result = decoder->decode(data->span());
    if (cacheResult == CacheResult::Yes)
        m_stringResources.set(resourceKey, result);

    return result;
}

NODELETE static int toAPI(WebExtension::Error error)
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
    case WebExtension::Error::InvalidMessageSerialization:
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

Ref<API::Error> WebExtension::createError(Error error, StringImpl* customLocalizedDescription, RefPtr<API::Error> underlyingError)
{
    auto errorCode = toAPI(error);
    RefPtr<StringImpl> localizedDescription;

    switch (error) {
    case Error::Unknown:
        localizedDescription = WEB_UI_STRING("An unknown error has occurred.", "WKWebExtensionErrorUnknown description");
        break;

    case Error::ResourceNotFound:
        ASSERT(customLocalizedDescription);
        break;

    case Error::InvalidManifest:
        if (underlyingError && !isEmpty(underlyingError->localizedDescription()))
            localizedDescription = WEB_UI_FORMAT_STRING("Unable to parse manifest: %s", "WKWebExtensionErrorInvalidManifest description, because of a JSON error", utf8(underlyingError->localizedDescription()));
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
            if (actionObject->getValue(StringImpl::create(iconVariantsManifestKey).ptr())) {
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
        if (underlyingError && !isEmpty(underlyingError->localizedDescription()))
            localizedDescription = WEB_UI_FORMAT_STRING("Unable to parse `declarativeNetRequest` rules: %s", "WKWebExtensionErrorInvalidDeclarativeNetRequest description, because of a JSON error", utf8(underlyingError->localizedDescription()));
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

    case Error::InvalidMessageSerialization:
        localizedDescription = WEB_UI_STRING("Invalid `message_serialization` manifest entry. The value must be `json` or `structured_clone`.", "WKWebExtensionErrorInvalidManifestEntry description for message_serialization");
        break;

    case Error::InvalidIcon:
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
        if (manifestObject()->getValue(StringImpl::create(iconVariantsManifestKey).ptr()))
            localizedDescription = WEB_UI_STRING("Empty or invalid `icon_variants` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for icon_variants");
        else
#endif
            localizedDescription = WEB_UI_STRING("Missing or empty `icons` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for icons");
        break;

    case Error::InvalidName:
        localizedDescription = WEB_UI_STRING("Missing or empty `name` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for name");
        break;

    case Error::InvalidOptionsPage:
        if (manifestObject()->getValue(StringImpl::create(optionsUIManifestKey).ptr()))
            localizedDescription = WEB_UI_STRING("Empty or invalid `options_ui` manifest entry", "WKWebExtensionErrorInvalidManifestEntry description for options UI");
        else
            localizedDescription = WEB_UI_STRING("Empty or invalid `options_page` manifest entry", "WKWebExtensionErrorInvalidManifestEntry description for options page");
        break;

    case Error::InvalidURLOverrides:
        if (manifestObject()->getValue(StringImpl::create(browserURLOverridesManifestKey).ptr()))
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

    if (!isEmpty(customLocalizedDescription))
        localizedDescription = customLocalizedDescription;

    return API::Error::create({ "WKWebExtensionErrorDomain"_si, errorCode, { }, localizedDescription }, underlyingError);
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
    populateMessageSerializationIfNeeded();
    populateWebAccessibleResourcesIfNeeded();
    populateCommandsIfNeeded();
    populateDeclarativeNetRequestPropertiesIfNeeded();
    populateExternallyConnectableIfNeeded();

    return m_errors;
}

const Vector<RefPtr<StringImpl>>& WebExtension::supportedLocales()
{
    if (!m_supportedLocales.isEmpty())
        return m_supportedLocales;

    auto localesString = "_locales/"_s;
    auto localeDirectoryPath = resourceFileURLForPath(StringImpl::create(localesString).ptr()).fileSystemPath();
    if (!isEmpty(localeDirectoryPath)) {
        m_supportedLocales = FileSystem::listDirectory(localeDirectoryPath);
        return m_supportedLocales;
    }

    // For tests that don't have a file system location, check the resource cache.
    auto prefixLength = localesString.length();
    auto pathFunctor = [&](StringImpl* path) {
        if (!startsWith(path, localesString))
            return;

        auto localeEnd = find(path, '/', prefixLength);
        if (localeEnd == notFound)
            return;

        auto locale = substring(path, prefixLength, localeEnd - prefixLength);
        if (!m_supportedLocales.containsIf([&](auto& item) { return equal(item, locale); }))
            m_supportedLocales.append(locale);
    };

    for (auto& path : m_dataResources.keys())
        pathFunctor(path);

    for (auto& path : m_stringResources.keys())
        pathFunctor(path);

    return m_supportedLocales;
}

const RefPtr<StringImpl>& WebExtension::defaultLocale()
{
    if (!manifestParsedSuccessfully())
        return nullString();

    return m_defaultLocale;
}

RefPtr<StringImpl> WebExtension::bestMatchLocale()
{
    const auto& supportedLocales = this->supportedLocales();
    if (supportedLocales.isEmpty())
        return nullString();

    if (supportedLocales.size() == 1)
        return supportedLocales.first();

    auto preferredLocale = defaultLanguage(ShouldMinimizeLanguages::No);

    bool exactMatch = false;
    auto bestMatchIndex = indexOfBestMatchingLanguageInList(preferredLocale, supportedLocales, exactMatch);
    if (bestMatchIndex != notFound)
        return supportedLocales[bestMatchIndex];

#if PLATFORM(COCOA)
    auto preferredLocaleComponents = parseLocale(preferredLocale);

    // On Apple platforms, the best match search uses Foundation, which skips "zh" when the preferred locale is "zh-Hant",
    // likely assuming "zh" refers to simplified Chinese. However, web extensions expect the base language to be selected
    // if it is supported, regardless of specific variants.
    auto matchingLanguageIndex = supportedLocales.findIf([&](const auto& locale) {
        return equalIgnoringASCIICase(locale, preferredLocaleComponents.languageCode);
    });

    if (matchingLanguageIndex != notFound)
        return supportedLocales[matchingLanguageIndex];
#endif

    return defaultLocale();
}

const RefPtr<StringImpl>& WebExtension::displayName()
{
    populateDisplayStringsIfNeeded();
    return m_displayName;
}

const RefPtr<StringImpl>& WebExtension::displayShortName()
{
    populateDisplayStringsIfNeeded();
    return m_displayShortName;
}

const RefPtr<StringImpl>& WebExtension::displayVersion()
{
    populateDisplayStringsIfNeeded();
    return m_displayVersion;
}

const RefPtr<StringImpl>& WebExtension::displayDescription()
{
    populateDisplayStringsIfNeeded();
    return m_displayDescription;
}

const RefPtr<StringImpl>& WebExtension::version()
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

    m_displayName = manifestObject->getString(StringImpl::create(nameManifestKey).ptr());
    m_displayShortName = manifestObject->getString(StringImpl::create(shortNameManifestKey).ptr());

    if (isEmpty(m_displayShortName))
        m_displayShortName = m_displayName;

    if (isEmpty(m_displayName))
        recordError(createError(Error::InvalidName));

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/version

    m_version = manifestObject->getString(StringImpl::create(versionManifestKey).ptr());

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/version_name

    m_displayVersion = manifestObject->getString(StringImpl::create(versionNameManifestKey).ptr());

    if (isEmpty(m_displayVersion))
        m_displayVersion = m_version;

    if (isEmpty(m_version))
        recordError(createError(Error::InvalidVersion));

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/description

    m_displayDescription = manifestObject->getString(StringImpl::create(descriptionManifestKey).ptr());
    if (isEmpty(m_displayDescription))
        recordError(createError(Error::InvalidDescription));
}

const RefPtr<StringImpl>& WebExtension::contentSecurityPolicy()
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
        if (RefPtr policyObject = manifestObject->getObject(StringImpl::create(contentSecurityPolicyManifestKey).ptr())) {
            m_contentSecurityPolicy = policyObject->getString(StringImpl::create(contentSecurityPolicyExtensionPagesManifestKey).ptr());
            if (!m_contentSecurityPolicy && (!policyObject->size() || policyObject->getValue(StringImpl::create(contentSecurityPolicyExtensionPagesManifestKey).ptr())))
                recordError(createError(Error::InvalidContentSecurityPolicy));
        }
    } else {
        m_contentSecurityPolicy = manifestObject->getString(StringImpl::create(contentSecurityPolicyManifestKey).ptr());
        if (!m_contentSecurityPolicy && manifestObject->getValue(StringImpl::create(contentSecurityPolicyManifestKey).ptr()))
            recordError(createError(Error::InvalidContentSecurityPolicy));
    }

    if (!m_contentSecurityPolicy)
        m_contentSecurityPolicy = "script-src 'self'"_si;
}

bool WebExtension::usesStructuredCloneMessageSerialization()
{
    populateMessageSerializationIfNeeded();
    return m_usesStructuredCloneMessageSerialization;
}

void WebExtension::populateMessageSerializationIfNeeded()
{
    if (m_parsedManifestMessageSerialization)
        return;

    m_parsedManifestMessageSerialization = true;

    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return;

    // Documentation: https://github.com/w3c/webextensions/issues/931

    if (!manifestObject->getValue(StringImpl::create(messageSerializationManifestKey).ptr()))
        return;

    auto messageSerialization = manifestObject->getString(StringImpl::create(messageSerializationManifestKey).ptr());
    if (equal(messageSerialization, "structured_clone"_s)) {
        m_usesStructuredCloneMessageSerialization = true;
        return;
    }

    if (!equal(messageSerialization, "json"_s))
        recordError(createError(Error::InvalidMessageSerialization));
}

bool WebExtension::hasBackgroundContent()
{
    populateBackgroundPropertiesIfNeeded();
    return !m_backgroundScriptPaths.isEmpty() || !isEmpty(m_backgroundPagePath) || !isEmpty(m_backgroundServiceWorkerPath);
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

const RefPtr<StringImpl>& WebExtension::backgroundContentPath()
{
    populateBackgroundPropertiesIfNeeded();

    if (!isEmpty(m_backgroundServiceWorkerPath))
        return m_backgroundServiceWorkerPath;

    if (!m_backgroundScriptPaths.isEmpty()) {
        if (backgroundContentIsServiceWorker()) {
            static const NeverDestroyed<RefPtr<StringImpl>> backgroundContentString = generatedBackgroundServiceWorkerFilename;
            return backgroundContentString;
        }

        static const NeverDestroyed<RefPtr<StringImpl>> backgroundContentString = generatedBackgroundPageFilename;
        return backgroundContentString;
    }

    if (!isEmpty(m_backgroundPagePath))
        return m_backgroundPagePath;

    ASSERT_NOT_REACHED();
    return nullString();
}

const RefPtr<StringImpl>& WebExtension::generatedBackgroundContent()
{
    if (!isEmpty(m_generatedBackgroundContent))
        return m_generatedBackgroundContent;

    populateBackgroundPropertiesIfNeeded();

    if (!isEmpty(m_backgroundServiceWorkerPath) || !isEmpty(m_backgroundPagePath))
        return nullString();

    if (m_backgroundScriptPaths.isEmpty())
        return nullString();

    bool isServiceWorker = backgroundContentIsServiceWorker();
    bool usesModules = backgroundContentUsesModules();

    const Vector scripts = [&] {
        Vector<RefPtr<StringImpl>> scripts;
        for (auto& scriptPath : borrow(m_backgroundScriptPaths)) {
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
        return scripts;
    }();

    StringBuilder generatedBackgroundContent;
    if (!isServiceWorker)
        generatedBackgroundContent.append("<!DOCTYPE html>\n<meta charset=\"utf-8\">\n<body>\n"_s);

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

    RefPtr backgroundManifestObject = manifestObject->getObject(StringImpl::create(backgroundManifestKey).ptr());
    if (!backgroundManifestObject || !backgroundManifestObject->size()) {
        if (manifestObject->getValue(StringImpl::create(backgroundManifestKey).ptr()))
            recordError(createError(Error::InvalidBackgroundContent));
        return;
    }

    m_backgroundPagePath = backgroundManifestObject->getString(StringImpl::create(backgroundPageManifestKey).ptr());
    m_backgroundServiceWorkerPath = backgroundManifestObject->getString(StringImpl::create(backgroundServiceWorkerManifestKey).ptr());
    m_backgroundContentUsesModules = (equal(backgroundManifestObject->getString(StringImpl::create(backgroundPageTypeKey).ptr()), backgroundPageTypeModuleValue));

    if (RefPtr backgroundScriptPaths = backgroundManifestObject->getArray(StringImpl::create(backgroundScriptsManifestKey).ptr())) {
        backgroundScriptPaths = filterObjects(*backgroundScriptPaths, [](auto& value) {
            return !isEmpty(value.asString());
        });

        m_backgroundScriptPaths = makeStringVector(*backgroundScriptPaths);
    }

    Vector<RefPtr<StringImpl>> supportedEnvironments = { backgroundDocumentManifestKey, backgroundServiceWorkerManifestKey };

    Vector<RefPtr<StringImpl>> preferredEnvironments;
    if (auto environment = backgroundManifestObject->getString(StringImpl::create(backgroundPreferredEnvironmentManifestKey).ptr()); !isEmpty(environment)) {
        if (supportedEnvironments.containsIf([&](auto& item) { return equal(item, environment); }))
            preferredEnvironments.append(environment);
    } else if (RefPtr environments = backgroundManifestObject->getArray(StringImpl::create(backgroundPreferredEnvironmentManifestKey).ptr()); environments && environments->length()) {
        Ref filteredEnvironments = filterObjects(*environments, [supportedEnvironments](auto& value) {
            return supportedEnvironments.containsIf([string = value.asString()](auto& item) {
                return equal(item, string);
            });
        });

        for (Ref environment : filteredEnvironments.get())
            preferredEnvironments.append(environment->asString());
    } else if (backgroundManifestObject->getValue(StringImpl::create(backgroundPreferredEnvironmentManifestKey).ptr()))
        recordError(createError(Error::InvalidBackgroundContent, WEB_UI_STRING("Manifest `background` entry has an empty or invalid `preferred_environment` key.", "WKWebExtensionErrorInvalidBackgroundContent description for empty or invalid preferred environment key")));

    for (auto& environment : borrow(preferredEnvironments)) {
        if (equal(environment, backgroundDocumentManifestKey)) {
            m_backgroundContentEnvironment = Environment::Document;
            m_backgroundServiceWorkerPath = nullString();

            if (!isEmpty(m_backgroundPagePath)) {
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

        if (equal(environment, backgroundServiceWorkerManifestKey)) {
            m_backgroundContentEnvironment = Environment::ServiceWorker;
            m_backgroundPagePath = nullString();

            if (!isEmpty(m_backgroundServiceWorkerPath)) {
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
        // Page takes precedence over scripts and service worker.
        if (!isEmpty(m_backgroundPagePath)) {
            if (!m_backgroundScriptPaths.isEmpty())
                recordError(createError(Error::InvalidBackgroundContent, WEB_UI_STRING("Manifest `background` entry has both `page` and `scripts` keys. The `page` key is used and `scripts` is ignored.", "WKWebExtensionErrorInvalidBackgroundContent description for both background page and scripts keys")));

            m_backgroundScriptPaths = { };
            m_backgroundServiceWorkerPath = nullString();
        }

        // Scripts takes precedence over service worker.
        if (!m_backgroundScriptPaths.isEmpty())
            m_backgroundServiceWorkerPath = nullString();

        m_backgroundContentEnvironment = !isEmpty(m_backgroundServiceWorkerPath) ? Environment::ServiceWorker : Environment::Document;

        if (m_backgroundScriptPaths.isEmpty() && isEmpty(m_backgroundPagePath) && isEmpty(m_backgroundServiceWorkerPath))
            recordError(createError(Error::InvalidBackgroundContent, WEB_UI_STRING("Manifest `background` entry has missing or empty required `scripts`, `page`, or `service_worker` key.", "WKWebExtensionErrorInvalidBackgroundContent description for missing background required keys")));
    }

    auto persistentBoolean = backgroundManifestObject->getBoolean(StringImpl::create(backgroundPersistentManifestKey).ptr());
    m_backgroundContentIsPersistent = persistentBoolean ? *persistentBoolean : !(supportsManifestVersion(3) || !isEmpty(m_backgroundServiceWorkerPath));

    if (m_backgroundContentIsPersistent && supportsManifestVersion(3)) {
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Invalid `persistent` manifest entry. A `manifest_version` greater-than or equal to `3` must be non-persistent.", "WKWebExtensionErrorInvalidBackgroundPersistence description for manifest v3")));
        m_backgroundContentIsPersistent = false;
    }

    if (m_backgroundContentIsPersistent && !isEmpty(m_backgroundServiceWorkerPath)) {
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Invalid `persistent` manifest entry. A `service_worker` must be non-persistent.", "WKWebExtensionErrorInvalidBackgroundPersistence description for service worker")));
        m_backgroundContentIsPersistent = false;
    }

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
    return !isEmpty(m_inspectorBackgroundPagePath);
}

const RefPtr<StringImpl>& WebExtension::inspectorBackgroundPagePath()
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

    m_inspectorBackgroundPagePath = manifestObject->getString(StringImpl::create(devtoolsPageManifestKey).ptr());
}

bool WebExtension::hasOptionsPage()
{
    populatePagePropertiesIfNeeded();
    return !isEmpty(m_optionsPagePath);
}

bool WebExtension::hasOverrideNewTabPage()
{
    populatePagePropertiesIfNeeded();
    return !isEmpty(m_overrideNewTabPagePath);
}

const RefPtr<StringImpl>& WebExtension::optionsPagePath()
{
    populatePagePropertiesIfNeeded();
    return m_optionsPagePath;
}

const RefPtr<StringImpl>& WebExtension::overrideNewTabPagePath()
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

    RefPtr optionsObject = manifestObject->getObject(StringImpl::create(optionsUIManifestKey).ptr());
    if (optionsObject) {
        m_optionsPagePath = optionsObject->getString(StringImpl::create(optionsUIPageManifestKey).ptr());
        if (isEmpty(m_optionsPagePath))
            recordError(createError(Error::InvalidOptionsPage));
    } else {
        m_optionsPagePath = manifestObject->getString(StringImpl::create(optionsPageManifestKey).ptr());
        if (isEmpty(m_optionsPagePath) && manifestObject->getValue(StringImpl::create(optionsPageManifestKey).ptr()))
            recordError(createError(Error::InvalidOptionsPage));
    }

    RefPtr overridesObject = manifestObject->getObject(StringImpl::create(browserURLOverridesManifestKey).ptr());
    if (!overridesObject)
        overridesObject = manifestObject->getObject(StringImpl::create(chromeURLOverridesManifestKey).ptr());

    if (overridesObject && overridesObject->size()) {
        m_overrideNewTabPagePath = overridesObject->getString(StringImpl::create(newTabManifestKey).ptr());
        if (isEmpty(m_overrideNewTabPagePath) && overridesObject->getValue(StringImpl::create(newTabManifestKey).ptr()))
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

    for (auto& injectedContent : borrow(m_staticInjectedContents)) {
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

Vector<RefPtr<StringImpl>> WebExtension::InjectedContentData::expandedIncludeMatchPatternStrings() const
{
    Vector<RefPtr<StringImpl>> result;

    for (auto& includeMatchPattern : includeMatchPatterns)
        result.appendVector(includeMatchPattern->expandedStrings());

    return result;
}

Vector<RefPtr<StringImpl>> WebExtension::InjectedContentData::expandedExcludeMatchPatternStrings() const
{
    Vector<RefPtr<StringImpl>> result;

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

    RefPtr contentScriptsManifestArray = manifestObject->getArray(StringImpl::create(contentScriptsManifestKey).ptr());
    if (!contentScriptsManifestArray || !contentScriptsManifestArray->length()) {
        if (manifestObject->getValue(StringImpl::create(contentScriptsManifestKey).ptr()))
            recordError(createError(Error::InvalidContentScripts));
        return;
    }

    auto addInjectedContentData = [this](auto& injectedContentObject) {
        HashSet<Ref<WebExtensionMatchPattern>> includeMatchPatterns;

        // Required. Specifies which pages the specified scripts and stylesheets will be injected into.
        RefPtr matchesArray = injectedContentObject->getArray(StringImpl::create(contentScriptsMatchesManifestKey).ptr());
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
        RefPtr scriptPaths = injectedContentObject->getArray(StringImpl::create(contentScriptsJSManifestKey).ptr());
        if (!scriptPaths)
            scriptPaths = JSON::Array::create();

        scriptPaths = filterObjects(*scriptPaths, [](auto& value) {
            return !isEmpty(value.asString());
        });

        // Optional. The list of CSS files to be injected into matching pages. These are injected in the order they appear in this array, before any DOM is constructed or displayed for the page.
        RefPtr styleSheetPaths = injectedContentObject->getArray(StringImpl::create(contentScriptsCSSManifestKey).ptr());
        if (!styleSheetPaths)
            styleSheetPaths = JSON::Array::create();

        styleSheetPaths = filterObjects(*styleSheetPaths, [](auto& value) {
            return !isEmpty(value.asString());
        });

        if (!scriptPaths->length() && !styleSheetPaths->length()) {
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has missing or empty `js` and `css` arrays.", "WKWebExtensionErrorInvalidContentScripts description for missing or empty `js` and `css` arrays")));
            return;
        }

        // Optional. Whether the script should inject into an about:blank frame where the parent or opener frame matches one of the patterns declared in matches. Defaults to false.
        bool matchAboutBlank = injectedContentObject->getBoolean(StringImpl::create(contentScriptsMatchAboutBlankManifestKey).ptr()).value_or(false);

        // Optional. Whether the script should inject in frames that were created by a matching origin, but whose URL or origin may not directly match the pattern.
        // These include frames with different schemes, such as about:, data:, and blob:. Defaults to false.
        bool matchOriginAsFallback = injectedContentObject->getBoolean(StringImpl::create(contentScriptsMatchOriginAsFallbackManifestKey).ptr()).value_or(false);

        // When both "match_origin_as_fallback" and "match_about_blank" are specified, "match_origin_as_fallback" takes priority.
        auto matchParentFrame = WebCore::UserContentMatchParentFrame::Never;
        if (matchOriginAsFallback)
            matchParentFrame = UserContentMatchParentFrame::ForOpaqueOrigins;
        else if (matchAboutBlank)
            matchParentFrame = UserContentMatchParentFrame::ForAboutBlank;

        HashSet<Ref<WebExtensionMatchPattern>> excludeMatchPatterns;

        // Optional. Excludes pages that this content script would otherwise be injected into.
        RefPtr excludeMatchesArray = injectedContentObject->getArray(StringImpl::create(contentScriptsExcludeMatchesManifestKey).ptr());
        if (!excludeMatchesArray)
            excludeMatchesArray = JSON::Array::create();

        for (Ref matchPatternStringValue : *excludeMatchesArray) {
            auto matchPatternString = matchPatternStringValue->asString();

            if (isEmpty(matchPatternString))
                continue;

            if (RefPtr matchPattern = WebExtensionMatchPattern::getOrCreate(matchPatternString)) {
                if (matchPattern->isSupported())
                    excludeMatchPatterns.add(matchPattern.releaseNonNull());
            }
        }

        // Optional. Applied after matches to include only those URLs that also match this glob.
        RefPtr includeGlobPatternStrings = injectedContentObject->getArray(StringImpl::create(contentScriptsIncludeGlobsManifestKey).ptr());
        if (!includeGlobPatternStrings)
            includeGlobPatternStrings = JSON::Array::create();

        includeGlobPatternStrings = filterObjects(*includeGlobPatternStrings, [](auto& value) {
            return !!isEmpty(value.asString());
        });

        // Optional. Applied after matches to exclude URLs that match this glob.
        RefPtr excludeGlobPatternStrings = injectedContentObject->getArray(StringImpl::create(contentScriptsExcludeGlobsManifestKey).ptr());
        if (!excludeGlobPatternStrings)
            excludeGlobPatternStrings = JSON::Array::create();

        excludeGlobPatternStrings = filterObjects(*excludeGlobPatternStrings, [](auto& value) -> bool {
            return !!isEmpty(value.asString());
        });

        // Optional. The "all_frames" field allows the extension to specify if JavaScript and CSS files should be injected into all frames matching the specified URL requirements or only into the
        // topmost frame in a tab. Defaults to false, meaning that only the top frame is matched. If specified true, it will inject into all frames, even if the frame is not the topmost frame in
        // the tab. Each frame is checked independently for URL requirements, it will not inject into child frames if the URL requirements are not met.
        auto injectsIntoAllFrames = injectedContentObject->getBoolean(StringImpl::create(contentScriptsAllFramesManifestKey).ptr()).value_or(false);

        auto injectionTime = InjectionTime::DocumentIdle;
        auto runsAtString = injectedContentObject->getString(StringImpl::create(contentScriptsRunAtManifestKey).ptr());
        if (!runsAtString || equal(runsAtString, contentScriptsDocumentIdleManifestKey))
            injectionTime = InjectionTime::DocumentIdle;
        else if (equal(runsAtString, contentScriptsDocumentStartManifestKey))
            injectionTime = InjectionTime::DocumentStart;
        else if (equal(runsAtString, contentScriptsDocumentEndManifestKey))
            injectionTime = InjectionTime::DocumentEnd;
        else
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has unknown `run_at` value.", "WKWebExtensionErrorInvalidContentScripts description for unknown 'run_at' value")));

        auto contentWorldType = WebExtensionContentWorldType::ContentScript;
        auto worldString = injectedContentObject->getString(StringImpl::create(contentScriptsWorldManifestKey).ptr());
        if (!worldString || equalIgnoringASCIICase(worldString, contentScriptsIsolatedManifestKey))
            contentWorldType = WebExtensionContentWorldType::ContentScript;
        else if (equalIgnoringASCIICase(worldString, contentScriptsMainManifestKey))
            contentWorldType = WebExtensionContentWorldType::Main;
        else
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has unknown `world` value.", "WKWebExtensionErrorInvalidContentScripts description for unknown 'world' value")));

        auto styleLevel = WebCore::UserStyleLevel::Author;
        auto cssOriginString = injectedContentObject->getString(StringImpl::create(contentScriptsCSSOriginManifestKey).ptr());
        if (!cssOriginString || equalIgnoringASCIICase(cssOriginString, contentScriptsAuthorManifestKey))
            styleLevel = WebCore::UserStyleLevel::Author;
        else if (equalIgnoringASCIICase(cssOriginString, contentScriptsUserManifestKey))
            styleLevel = WebCore::UserStyleLevel::User;
        else
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has unknown `css_origin` value.", "WKWebExtensionErrorInvalidContentScripts description for unknown 'css_origin' value")));

        InjectedContentData injectedContentData;
        injectedContentData.includeMatchPatterns = WTF::move(includeMatchPatterns);
        injectedContentData.excludeMatchPatterns = WTF::move(excludeMatchPatterns);
        injectedContentData.injectionTime = injectionTime;
        injectedContentData.matchParentFrame = matchParentFrame;
        injectedContentData.injectsIntoAllFrames = injectsIntoAllFrames;
        injectedContentData.contentWorldType = contentWorldType;
        injectedContentData.styleLevel = styleLevel;
        injectedContentData.scriptPaths = makeStringVector(*scriptPaths);
        injectedContentData.styleSheetPaths = makeStringVector(*styleSheetPaths);
        injectedContentData.includeGlobPatternStrings = makeStringVector(*includeGlobPatternStrings);
        injectedContentData.excludeGlobPatternStrings = makeStringVector(*excludeGlobPatternStrings);

        m_staticInjectedContents.append(WTF::move(injectedContentData));
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
        return manifestObject->getValue(StringImpl::create(sidebarActionManifestKey).ptr());

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

const std::optional<RefPtr<StringImpl>>& WebExtension::sidebarDocumentPath()
{
    populateSidebarPropertiesIfNeeded();
    return m_sidebarDocumentPath;
}

const std::optional<RefPtr<StringImpl>>& WebExtension::sidebarTitle()
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

    if (RefPtr sidebarActionObject = manifestObject->getObject(StringImpl::create(sidebarActionManifestKey).ptr())) {
        populateSidebarActionProperties(*sidebarActionObject);
        return;
    }

    if (RefPtr sidePanelObject = manifestObject->getObject(StringImpl::create(sidePanelManifestKey).ptr()))
        populateSidePanelProperties(*sidePanelObject);
}

static std::optional<RefPtr<StringImpl>> toOptionalString(RefPtr<StringImpl>&& string)
{
    if (!string)
        return std::nullopt;
    return WTF::move(string);
}

void WebExtension::populateSidebarActionProperties(const JSON::Object& sidebarActionObject)
{
    // FIXME: <https://webkit.org/b/276833> implement sidebar icon parsing
    m_sidebarIconsCache = std::nullopt;
    m_sidebarTitle = toOptionalString(sidebarActionObject.getString(StringImpl::create(sidebarActionTitleManifestKey).ptr()));
    m_sidebarDocumentPath = toOptionalString(sidebarActionObject.getString(StringImpl::create(sidebarActionPathManifestKey).ptr()));
}

void WebExtension::populateSidePanelProperties(const JSON::Object& sidePanelObject)
{
    // Since sidePanel cannot set a default title or icon from the manifest, setting these to nullopt here is intentional.
    m_sidebarIconsCache = std::nullopt;
    m_sidebarTitle = std::nullopt;
    m_sidebarDocumentPath = toOptionalString(sidePanelObject.getString(StringImpl::create(sidePanelPathManifestKey).ptr()));
}
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

const WebExtension::PermissionsSet& WebExtension::supportedPermissions()
{
    static MainRunLoopNeverDestroyed<PermissionsSet> permissions = std::initializer_list<RefPtr<StringImpl>> { WebExtensionPermission::activeTab(), WebExtensionPermission::alarms(), WebExtensionPermission::clipboardWrite(),
        WebExtensionPermission::contextMenus(), WebExtensionPermission::cookies(), WebExtensionPermission::declarativeNetRequest(), WebExtensionPermission::declarativeNetRequestFeedback(),
        WebExtensionPermission::declarativeNetRequestWithHostAccess(), WebExtensionPermission::menus(), WebExtensionPermission::nativeMessaging(), WebExtensionPermission::notifications(), WebExtensionPermission::scripting(),
        WebExtensionPermission::storage(), WebExtensionPermission::tabs(), WebExtensionPermission::unlimitedStorage(), WebExtensionPermission::webNavigation(), WebExtensionPermission::webRequest(),
#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
        WebExtensionPermission::sidePanel(),
#endif
#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)
        WebExtensionPermission::bookmarks(),
#endif
#if ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)
        WebExtensionPermission::offscreen(),
#endif
#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/WebExtensionSupportedPermissionsAdditions.h>)
#include <WebKitAdditions/WebExtensionSupportedPermissionsAdditions.h>
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

    for (auto& injectedContent : borrow(m_staticInjectedContents)) {
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

    RefPtr externallyConnectableObject = manifestObject->getObject(StringImpl::create(externallyConnectableManifestKey).ptr());
    if (!externallyConnectableObject)
        return;

    if (!externallyConnectableObject->size()) {
        recordError(createError(Error::InvalidExternallyConnectable));
        return;
    }

    bool shouldReportError = false;
    MatchPatternSet matchPatterns;

    if (RefPtr matchPatternStrings = externallyConnectableObject->getArray(StringImpl::create(externallyConnectableMatchesManifestKey).ptr())) {
        for (auto matchPatternStringValue : *matchPatternStrings) {
            auto matchPatternString = matchPatternStringValue->asString();
            if (isEmpty(matchPatternString))
                continue;

            if (RefPtr matchPattern = WebExtensionMatchPattern::getOrCreate(matchPatternString)) {
                if (!matchPattern || !matchPattern->isSupported()) {
                    shouldReportError = true;
                    continue;
                }

                matchPatterns.add(matchPattern.releaseNonNull());
            }
        }
    }

    m_externallyConnectableMatchPatterns = WTF::move(matchPatterns);

    RefPtr extensionIDs = externallyConnectableObject->getArray(StringImpl::create(externallyConnectableIDsManifestKey).ptr());
    if (extensionIDs) {
        extensionIDs = filterObjects(*extensionIDs, [](auto& value) {
            return !isEmpty(value.asString());
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
    if (RefPtr permissionsManifestArray = manifestObject->getArray(StringImpl::create(permissionsManifestKey).ptr())) {
        for (Ref permissionObject : *permissionsManifestArray) {
            auto permission = permissionObject->asString();
            if (isEmpty(permission))
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
        if (RefPtr hostPermissionsManifestArray = manifestObject->getArray(StringImpl::create(hostPermissionsManifestKey).ptr())) {
            for (Ref permissionObject : *hostPermissionsManifestArray) {
                auto permission = permissionObject->asString();
                if (isEmpty(permission))
                    continue;

                if (RefPtr matchPattern = WebExtensionMatchPattern::getOrCreate(permission)) {
                    if (matchPattern->isSupported())
                        m_permissionMatchPatterns.add(matchPattern.releaseNonNull());
                }
            }
        }
    }

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/optional_permissions

    if (RefPtr optionalPermissionsManifestArray = manifestObject->getArray(StringImpl::create(optionalPermissionsManifestKey).ptr())) {
        for (Ref permissionObject : *optionalPermissionsManifestArray) {
            auto permission = permissionObject->asString();
            if (isEmpty(permission))
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
        if (RefPtr hostPermissionsManifestArray = manifestObject->getArray(StringImpl::create(optionalHostPermissionsManifestKey).ptr())) {
            for (Ref permissionObject : *hostPermissionsManifestArray) {
                auto permission = permissionObject->asString();
                if (isEmpty(permission))
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
        actionObject = manifestObject->getObject(StringImpl::create(actionManifestKey).ptr());
    else {
        actionObject = manifestObject->getObject(StringImpl::create(browserActionManifestKey).ptr());
        if (!actionObject)
            actionObject = manifestObject->getObject(StringImpl::create(pageActionManifestKey).ptr());
    }

    if (!actionObject) {
        if (supportsManifestVersion(3)) {
            if (manifestObject->getValue(StringImpl::create(browserActionManifestKey).ptr()) || manifestObject->getValue(StringImpl::create(pageActionManifestKey).ptr()))
                recordError(createError(Error::InvalidAction, WEB_UI_STRING("The `browser_action` and `page_action` manifest entries require manifest version 2 or lower.", "WKWebExtensionErrorInvalidManifestEntry description for browser_action or page_action in manifest version 3")));
        } else if (manifestObject->getValue(StringImpl::create(actionManifestKey).ptr()))
            recordError(createError(Error::InvalidAction, WEB_UI_STRING("The `action` manifest entry requires manifest version 3 or higher.", "WKWebExtensionErrorInvalidManifestEntry description for action in manifest version 2")));

        return;
    }

    // Look for the "default_icon" as a string, which is useful for SVG icons. Only supported by Firefox currently.
    if (auto defaultIconPath = actionObject->getString(StringImpl::create(defaultIconManifestKey).ptr()); !isEmpty(defaultIconPath)) {
        auto defaultIconResult = iconForPath(defaultIconPath);
        if (!defaultIconResult) {
            recordErrorIfNeeded(defaultIconResult.error());

            RefPtr<StringImpl> localizedErrorDescription;
            if (supportsManifestVersion(3))
                localizedErrorDescription = WEB_UI_STRING("Failed to load image for `default_icon` in the `action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load single image for action");
            else
                localizedErrorDescription = WEB_UI_STRING("Failed to load image for `default_icon` in the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load single image for browser_action or page_action");

            recordError(createError(Error::InvalidActionIcon, localizedErrorDescription));
        } else
            m_defaultActionIcon = defaultIconResult.value().get();
    }

    m_displayActionLabel = actionObject->getString(StringImpl::create(defaultTitleManifestKey).ptr());
    m_actionPopupPath = actionObject->getString(StringImpl::create(defaultPopupManifestKey).ptr());

    m_actionObject = actionObject;
}

const RefPtr<StringImpl>& WebExtension::displayActionLabel()
{
    populateActionPropertiesIfNeeded();
    return m_displayActionLabel;
}

const RefPtr<StringImpl>& WebExtension::actionPopupPath()
{
    populateActionPropertiesIfNeeded();
    return m_actionPopupPath;
}

bool WebExtension::hasAction()
{
    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return false;

    return supportsManifestVersion(3) && manifestObject->getValue(StringImpl::create(actionManifestKey).ptr());
}

bool WebExtension::hasBrowserAction()
{
    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return false;

    return !supportsManifestVersion(3) && manifestObject->getValue(StringImpl::create(browserActionManifestKey).ptr());
}

bool WebExtension::hasPageAction()
{
    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return false;

    return !supportsManifestVersion(3) && manifestObject->getValue(StringImpl::create(pageActionManifestKey).ptr());
}

RefPtr<WebCore::Icon> WebExtension::icon(WebCore::FloatSize size)
{
    RefPtr manifestObject = this->manifestObject();
    if (!manifestObject)
        return nullptr;

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    if (manifestObject->getValue(StringImpl::create(iconVariantsManifestKey).ptr())) {
        RefPtr<StringImpl> localizedErrorDescription = WEB_UI_STRING("Failed to load images in `icon_variants` manifest entry.", "WKWebExtensionErrorInvalidIcon description for failing to load image variants");
        return bestIconVariantForManifestKey(*manifestObject, StringImpl::create(iconVariantsManifestKey).ptr(), size, m_iconsCache, Error::InvalidIcon, localizedErrorDescription);
    }
#endif

    RefPtr<StringImpl> localizedErrorDescription = WEB_UI_STRING("Failed to load images in `icons` manifest entry.", "WKWebExtensionErrorInvalidIcon description for failing to load images");
    return bestIconForManifestKey(*manifestObject, StringImpl::create(iconsManifestKey).ptr(), size, m_iconsCache, Error::InvalidIcon, localizedErrorDescription);
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
        if (actionObject->getValue(StringImpl::create(iconVariantsManifestKey).ptr())) {
            RefPtr<StringImpl> localizedErrorDescription = WEB_UI_STRING("Failed to load images in `icon_variants` for the `action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load image variants for action");
            if (RefPtr result = bestIconVariantForManifestKey(*actionObject, StringImpl::create(iconVariantsManifestKey).ptr(), size, m_actionIconsCache, Error::InvalidActionIcon, localizedErrorDescription))
                return result;
            return icon(size);
        }
#endif

        RefPtr<StringImpl> localizedErrorDescription;
        if (supportsManifestVersion(3))
            localizedErrorDescription = WEB_UI_STRING("Failed to load images in `default_icon` for the `action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load images for action only");
        else
            localizedErrorDescription = WEB_UI_STRING("Failed to load images in `default_icon` for the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load images for browser_action or page_action");

        if (RefPtr result = bestIconForManifestKey(*actionObject, StringImpl::create(defaultIconManifestKey).ptr(), size, m_actionIconsCache, Error::InvalidActionIcon, localizedErrorDescription))
            return result;
    }

    return icon(size);
}

size_t WebExtension::bestIconSize(const JSON::Object& iconsObject, size_t idealPixelSize)
{
    if (!iconsObject.size())
        return 0;

    // Check if the ideal size exists, if so return it.
    auto idealSizeString = makeString(idealPixelSize);
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

    // Sort the remaining keys and find the next largest size.
    std::ranges::sort(sizeValues);

    size_t bestSize = 0;
    for (auto size : sizeValues) {
        bestSize = size;
        if (bestSize >= idealPixelSize)
            return bestSize;
    }

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    // No explicit size is large enough, so use "any" (typically a vector image) before the largest smaller size.
    if (iconsObject.getValue(StringImpl::create(anyManifestKey).ptr())) {
        // Return max to ensure it takes precedence over the smaller sizes.
        return std::numeric_limits<size_t>::max();
    }
#endif

    return bestSize;
}

RefPtr<StringImpl> WebExtension::pathForBestImage(const JSON::Object& iconsObject, size_t idealPixelSize)
{
    auto bestSize = bestIconSize(iconsObject, idealPixelSize);
    if (!bestSize)
        return nullString();

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    if (bestSize == std::numeric_limits<size_t>::max())
        return iconsObject.getString(StringImpl::create(anyManifestKey).ptr());
#endif

    return iconsObject.getString(makeString(bestSize));
}

RefPtr<WebCore::Icon> WebExtension::bestIconForManifestKey(const JSON::Object& object, StringImpl* manifestKey, WebCore::FloatSize idealSize, IconsCache& cacheLocation, Error error, StringImpl* customLocalizedDescription)
{
    auto currentScales = availableScreenScales();
    if (auto cachedScales = cacheLocation.getOptional("scales"_si)) {
        auto scales = std::get_if<Vector<double>>(&*cachedScales);
        if (!scales || *scales != currentScales)
            cacheLocation.set("scales"_si, IconCacheEntry(currentScales));
    } else
        cacheLocation.set("scales"_si, IconCacheEntry(currentScales));

    auto cacheKey = idealSize.toJSONString();
    if (auto cacheResult = cacheLocation.getOptional(cacheKey))
        return *std::get_if<RefPtr<WebCore::Icon>>(&*cacheResult);

    RefPtr iconObject = object.getObject(manifestKey);
    if (!iconObject) {
        // A string `default_icon` is loaded and reported by populateActionPropertiesIfNeeded().
        RefPtr value = object.getValue(manifestKey);
        if (value && !(equal(manifestKey, defaultIconManifestKey) && value->type() == JSON::Value::Type::String))
            recordError(createError(error));

        return nullptr;
    }

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
        if (equal(value->asString(), lightManifestKey))
            result.add(ColorScheme::Light);
        if (equal(value->asString(), darkManifestKey))
            result.add(ColorScheme::Dark);
    }

    return result;
}

RefPtr<JSON::Object> WebExtension::bestIconVariantJSONObject(RefPtr<JSON::Array> variants, size_t idealPixelSize, ColorScheme idealColorScheme)
{
    if (!variants || !variants->length())
        return nullptr;

    // A variant with an unknown property is skipped entirely, so a later variant can match instead.
    auto hasOnlyKnownProperties = [](const JSON::Object& variantObject) {
        for (auto key : variantObject.keys()) {
            if (equal(key, anyManifestKey) || equal(key, colorSchemesManifestKey) || equal(key, colorSchemesAPIKey))
                continue;

            auto size = parseInteger<size_t>(key);
            if (!size || !*size)
                return false;
        }

        return true;
    };

    RefPtr<JSON::Object> bestVariant;
    RefPtr<JSON::Object> fallbackVariant;
    bool foundIdealFallbackVariant = false;

    size_t bestSize = 0;
    size_t fallbackSize = 0;

    // Pick the first variant matching color scheme and/or size.
    for (Ref variant : *variants) {
        RefPtr variantObject = variant->asObject();
        if (!variantObject || !hasOnlyKnownProperties(*variantObject))
            continue;

        auto colorSchemes = toColorSchemes(variantObject ? variantObject->getValue(StringImpl::create(colorSchemesManifestKey).ptr()) ?: variantObject->getValue(StringImpl::create(colorSchemesAPIKey).ptr()) : nullptr);
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

RefPtr<WebCore::Icon> WebExtension::bestIconVariantForManifestKey(const JSON::Object& object, StringImpl* manifestKey, WebCore::FloatSize idealSize, IconsCache& cacheLocation, Error error, StringImpl* customLocalizedDescription)
{
    auto currentScales = availableScreenScales();
    if (auto cachedScales = cacheLocation.getOptional("scales"_si)) {
        auto scales = std::get_if<Vector<double>>(&*cachedScales);
        if (!scales || *scales != currentScales)
            cacheLocation.set("scales"_si, IconCacheEntry(currentScales));
    } else
        cacheLocation.set("scales"_si, IconCacheEntry(currentScales));

    auto cacheKey = idealSize.toJSONString();
    if (auto cacheResult = cacheLocation.getOptional(cacheKey))
        return *std::get_if<RefPtr<WebCore::Icon>>(&*cacheResult);

    RefPtr iconArray = object.getArray(manifestKey);
    if (!iconArray) {
        recordError(createError(error));
        return nullptr;
    }

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

static bool parseCommandShortcut(StringImpl* shortcut, OptionSet<ModifierFlags>& modifierFlags, RefPtr<StringImpl>& key)
{
    modifierFlags = { };
    key = emptyString();

    // An empty shortcut is allowed.
    if (isEmpty(shortcut))
        return true;

    static NeverDestroyed<HashMap<RefPtr<StringImpl>, ModifierFlags>> modifierMap = HashMap<RefPtr<StringImpl>, ModifierFlags> {
        { "Ctrl"_s, ModifierFlags::Command },
        { "Command"_s, ModifierFlags::Command },
        { "Alt"_s, ModifierFlags::Option },
        { "MacCtrl"_s, ModifierFlags::Control },
        { "Shift"_s, ModifierFlags::Shift }
    };

    static NeverDestroyed<HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>>> specialKeyMap = HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> {
        { "Comma"_s, ","_s },
        { "Period"_s, "."_s },
        { "Space"_s, " "_s },
        { "F1"_s, StringImpl::fromUTF8("\uF704") },
        { "F2"_s, StringImpl::fromUTF8("\uF705") },
        { "F3"_s, StringImpl::fromUTF8("\uF706") },
        { "F4"_s, StringImpl::fromUTF8("\uF707") },
        { "F5"_s, StringImpl::fromUTF8("\uF708") },
        { "F6"_s, StringImpl::fromUTF8("\uF709") },
        { "F7"_s, StringImpl::fromUTF8("\uF70A") },
        { "F8"_s, StringImpl::fromUTF8("\uF70B") },
        { "F9"_s, StringImpl::fromUTF8("\uF70C") },
        { "F10"_s, StringImpl::fromUTF8("\uF70D") },
        { "F11"_s, StringImpl::fromUTF8("\uF70E") },
        { "F12"_s, StringImpl::fromUTF8("\uF70F") },
        { "Insert"_s, StringImpl::fromUTF8("\uF727") },
        { "Delete"_s, StringImpl::fromUTF8("\uF728") },
        { "Home"_s, StringImpl::fromUTF8("\uF729") },
        { "End"_s, StringImpl::fromUTF8("\uF72B") },
        { "PageUp"_s, StringImpl::fromUTF8("\uF72C") },
        { "PageDown"_s, StringImpl::fromUTF8("\uF72D") },
        { "Up"_s, StringImpl::fromUTF8("\uF700") },
        { "Down"_s, StringImpl::fromUTF8("\uF701") },
        { "Left"_s, StringImpl::fromUTF8("\uF702") },
        { "Right"_s, StringImpl::fromUTF8("\uF703") }
    };

    auto parts = split(shortcut, '+');

    // Reject shortcuts with fewer than two or more than three components.
    if (parts.size() < 2 || parts.size() > 3)
        return false;

    key = parts.takeLast();

    // Keys should not be present in the modifier map.
    if (modifierMap.get().contains(key))
        return false;

    if (length(key) == 1) {
        // Single-character keys must be alphanumeric.
        if (!isASCIIAlphanumeric(codeUnitAt(key, 0)))
            return false;

        key = convertToASCIILowercase(key);
    } else {
        auto entry = specialKeyMap.get().find(key);

        // Non-alphanumeric keys must be in the special key map.
        if (entry == specialKeyMap.get().end())
            return false;

        key = entry->value;
    }

    for (auto& part : borrow(parts)) {
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

    RefPtr commandsObject = manifestObject->getObject(StringImpl::create(commandsManifestKey).ptr());
    if (!commandsObject && manifestObject->getValue(StringImpl::create(commandsManifestKey).ptr())) {
        recordError(createError(Error::InvalidCommands));
        return;
    }

    bool hasActionCommand = false;

    if (commandsObject) {
        size_t commandsWithShortcuts = 0;
        std::optional<RefPtr<StringImpl>> error;

        for (auto commandIdentifier : commandsObject->keys()) {
            if (isEmpty(commandIdentifier)) {
                error = WEB_UI_STRING("Empty or invalid identifier in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command identifier");
                continue;
            }

            bool isActionCommand = false;
            if (supportsManifestVersion(3) && equal(commandIdentifier, "_execute_action"_s))
                isActionCommand = true;
            else if (!supportsManifestVersion(3) && (equal(commandIdentifier, "_execute_browser_action"_s) || equal(commandIdentifier, "_execute_page_action"_s)))
                isActionCommand = true;

            RefPtr commandObject = commandsObject->getObject(commandIdentifier);
            if (!commandObject || (!commandObject->size() && !isActionCommand)) {
                error = WEB_UI_STRING("Empty or invalid command in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command");
                continue;
            }

            CommandData commandData;
            commandData.identifier = commandIdentifier;
            commandData.activationKey = emptyString();
            commandData.modifierFlags = { };

            if (isActionCommand && !hasActionCommand)
                hasActionCommand = true;

            // Descriptions are required for standard commands, but are optional for action commands.
            auto description = commandObject->getString(StringImpl::create(commandsDescriptionKeyManifestKey).ptr());
            if (isEmpty(description) && !isActionCommand) {
                error = WEB_UI_STRING("Empty or invalid `description` in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command description");
                continue;
            }

            if (isActionCommand && isEmpty(description)) {
                description = displayActionLabel();
                if (isEmpty(description))
                    description = displayShortName();
            }

            commandData.description = description;

            if (RefPtr suggestedKeyObject = commandObject->getObject(StringImpl::create(commandsSuggestedKeyManifestKey).ptr())) {
#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
                const auto macPlatform = "mac"_s;
                const auto iosPlatform = "ios"_s;
#elif PLATFORM(GTK) || PLATFORM(WPE)
                const auto linuxPlatform = "linux"_s;
#endif
                const auto defaultPlatform = "default"_s;

                RefPtr<StringImpl> platformShortcut;
#if PLATFORM(MAC)
                platformShortcut = !isEmpty(suggestedKeyObject->getString(StringImpl::create(macPlatform).ptr())) ? suggestedKeyObject->getString(StringImpl::create(macPlatform).ptr()) : suggestedKeyObject->getString(StringImpl::create(iosPlatform).ptr());
#elif PLATFORM(IOS_FAMILY)
                platformShortcut = !isEmpty(suggestedKeyObject->getString(StringImpl::create(iosPlatform).ptr())) ? suggestedKeyObject->getString(StringImpl::create(iosPlatform).ptr()) : suggestedKeyObject->getString(StringImpl::create(macPlatform).ptr());
#elif PLATFORM(GTK) || PLATFORM(WPE)
                platformShortcut = suggestedKeyObject->getString(StringImpl::create(linuxPlatform).ptr());
#endif
                if (isEmpty(platformShortcut))
                    platformShortcut = suggestedKeyObject->getString(StringImpl::create(defaultPlatform).ptr());

                if (!parseCommandShortcut(platformShortcut, commandData.modifierFlags, commandData.activationKey)) {
                    error = WEB_UI_STRING("Invalid `suggested_key` in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command shortcut");
                    continue;
                }

                if (!isEmpty(commandData.activationKey) && ++commandsWithShortcuts > maximumNumberOfShortcutCommands) {
                    error = WEB_UI_STRING("Too many shortcuts specified for `commands`, only 4 shortcuts are allowed.", "WKWebExtensionErrorInvalidManifestEntry description for too many command shortcuts");
                    commandData.activationKey = emptyString();
                    commandData.modifierFlags = { };
                }
            }

            m_commands.append(WTF::move(commandData));
        }

        if (error)
            recordError(createError(Error::InvalidCommands, error.value()));
    }

    if (!hasActionCommand) {
        RefPtr<StringImpl> commandIdentifier;
        if (hasAction())
            commandIdentifier = "_execute_action"_si;
        else if (hasBrowserAction())
            commandIdentifier = "_execute_browser_action"_si;
        else if (hasPageAction())
            commandIdentifier = "_execute_page_action"_si;

        if (!isEmpty(commandIdentifier)) {
            auto description = displayActionLabel();
            if (isEmpty(description))
                description = displayShortName();
            m_commands.append({ commandIdentifier, description, emptyString(), { } });
        }
    }
}

std::expected<WebExtension::DeclarativeNetRequestRulesetData, Ref<API::Error>> WebExtension::parseDeclarativeNetRequestRulesetObject(const JSON::Object& rulesetObject)
{
    auto rulesetID = rulesetObject.getString(StringImpl::create(declarativeNetRequestRulesetIDManifestKey).ptr());
    if (isEmpty(rulesetID)) {
        return makeUnexpected(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Empty or invalid `id` in `declarative_net_request` manifest entry.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for empty or invalid id in declarative_net_request manifest entry")));
    }

    auto jsonPath = rulesetObject.getString(StringImpl::create(declarativeNetRequestRulePathManifestKey).ptr());
    if (isEmpty(jsonPath)) {
        return makeUnexpected(createError(WebExtension::Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Empty or invalid `path` in `declarative_net_request` manifest entry.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for empty or invalid path in declarative_net_request manifest entry")));
    }

    auto enabledBool = rulesetObject.getBoolean(StringImpl::create(declarativeNetRequestRuleEnabledManifestKey).ptr());
    if (!enabledBool) {
        return makeUnexpected(createError(WebExtension::Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Missing or invalid `enabled` boolean for the `declarative_net_request` manifest entry.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for missing enabled boolean")));
    }

    DeclarativeNetRequestRulesetData rulesetData = {
        rulesetID,
        *enabledBool,
        jsonPath
    };

    return rulesetData;
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

    RefPtr declarativeNetRequestManifestObject = manifestObject->getObject(StringImpl::create(declarativeNetRequestManifestKey).ptr());
    if (!declarativeNetRequestManifestObject) {
        if (manifestObject->getValue(StringImpl::create(declarativeNetRequestManifestKey).ptr()))
            recordError(createError(Error::InvalidDeclarativeNetRequest));
        return;
    }

    RefPtr declarativeNetRequestRulesets = declarativeNetRequestManifestObject->getArray(StringImpl::create(declarativeNetRequestRulesManifestKey).ptr());
    if (!declarativeNetRequestRulesets) {
        if (manifestObject->getValue(StringImpl::create(declarativeNetRequestManifestKey).ptr()))
            recordError(createError(Error::InvalidDeclarativeNetRequest));
        return;
    }

    if (declarativeNetRequestRulesets->length() > webExtensionDeclarativeNetRequestMaximumNumberOfStaticRulesets)
        recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Exceeded maximum number of `declarative_net_request` rulesets. Ignoring extra rulesets.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for too many rulesets")));

    size_t rulesetCount = 0, enabledRulesetCount = 0;
    bool recordedTooManyRulesetsManifestError = false;
    HashSet<RefPtr<StringImpl>> seenRulesetIDs;
    for (Ref value : *declarativeNetRequestRulesets) {
        if (rulesetCount >= webExtensionDeclarativeNetRequestMaximumNumberOfStaticRulesets)
            continue;

        RefPtr object = value->asObject();
        if (!object)
            continue;

        auto optionalRuleset = parseDeclarativeNetRequestRulesetObject(*object);
        if (!optionalRuleset) {
            recordError(createError(Error::InvalidDeclarativeNetRequest, nullptr, optionalRuleset.error().get()));
            continue;
        }

        auto& ruleset = optionalRuleset.value();
        if (seenRulesetIDs.contains(ruleset.rulesetID)) {
            recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_FORMAT_STRING("`declarative_net_request` ruleset with id \"%s\" is invalid. Ruleset id must be unique.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for duplicate ruleset id", utf8(ruleset.rulesetID))));
            continue;
        }

        if (ruleset.enabled && ++enabledRulesetCount > webExtensionDeclarativeNetRequestMaximumNumberOfEnabledRulesets && !recordedTooManyRulesetsManifestError) {
            recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_FORMAT_STRING("Exceeded maximum number of enabled `declarative_net_request` static rulesets. The first %zu will be applied, the remaining will be ignored.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for too many enabled static rulesets", webExtensionDeclarativeNetRequestMaximumNumberOfEnabledRulesets)));
            recordedTooManyRulesetsManifestError = true;
            continue;
        }

        seenRulesetIDs.add(ruleset.rulesetID);
        ++rulesetCount;

        m_declarativeNetRequestRulesets.append(WTF::move(ruleset));
    }
}

const WebExtension::DeclarativeNetRequestRulesetVector& WebExtension::declarativeNetRequestRulesets()
{
    populateDeclarativeNetRequestPropertiesIfNeeded();
    return m_declarativeNetRequestRulesets;
}

std::optional<WebExtension::DeclarativeNetRequestRulesetData> WebExtension::declarativeNetRequestRuleset(StringImpl* identifier)
{
    for (auto& ruleset : borrow(declarativeNetRequestRulesets())) {
        if (equal(ruleset.rulesetID, identifier))
            return ruleset;
    }

    return std::nullopt;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
