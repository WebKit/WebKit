/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtension.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "APIData.h"
#import "CocoaHelpers.h"
#import "FoundationSPI.h"
#import "Logging.h"
#import "WKWebExtensionInternal.h"
#import "WKWebExtensionPermissionPrivate.h"
#import "WebExtensionConstants.h"
#import "WebExtensionUtilities.h"
#import "_WKWebExtensionLocalization.h"
#import <CoreFoundation/CFBundle.h>
#import <UniformTypeIdentifiers/UTCoreTypes.h>
#import <UniformTypeIdentifiers/UTType.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/BlockPtr.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(MAC)
#import <pal/spi/mac/NSImageSPI.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import <UIKit/UIKit.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(CoreSVG)
SOFT_LINK(CoreSVG, CGSVGDocumentCreateFromData, CGSVGDocumentRef, (CFDataRef data, CFDictionaryRef options), (data, options))
SOFT_LINK(CoreSVG, CGSVGDocumentRelease, void, (CGSVGDocumentRef document), (document))
#endif

namespace WebKit {

static NSString * const iconsManifestKey = @"icons";

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
static NSString * const iconVariantsManifestKey = @"icon_variants";
static NSString * const colorSchemesManifestKey = @"color_schemes";
static NSString * const lightManifestKey = @"light";
static NSString * const darkManifestKey = @"dark";
static NSString * const anyManifestKey = @"any";
#endif

static NSString * const actionManifestKey = @"action";
static NSString * const browserActionManifestKey = @"browser_action";
static NSString * const pageActionManifestKey = @"page_action";

static NSString * const defaultIconManifestKey = @"default_icon";
static NSString * const defaultLocaleManifestKey = @"default_locale";
static NSString * const defaultTitleManifestKey = @"default_title";
static NSString * const defaultPopupManifestKey = @"default_popup";

static NSString * const optionsUIManifestKey = @"options_ui";
static NSString * const browserURLOverridesManifestKey = @"browser_url_overrides";

static NSString * const generatedBackgroundPageFilename = @"_generated_background_page.html";
static NSString * const generatedBackgroundServiceWorkerFilename = @"_generated_service_worker.js";

static NSString * const permissionsManifestKey = @"permissions";
static NSString * const optionalPermissionsManifestKey = @"optional_permissions";
static NSString * const hostPermissionsManifestKey = @"host_permissions";
static NSString * const optionalHostPermissionsManifestKey = @"optional_host_permissions";

static NSString * const commandsManifestKey = @"commands";
static NSString * const commandsSuggestedKeyManifestKey = @"suggested_key";
static NSString * const commandsDescriptionKeyManifestKey = @"description";

static NSString * const declarativeNetRequestManifestKey = @"declarative_net_request";
static NSString * const declarativeNetRequestRulesManifestKey = @"rule_resources";
static NSString * const declarativeNetRequestRulesetIDManifestKey = @"id";
static NSString * const declarativeNetRequestRuleEnabledManifestKey = @"enabled";
static NSString * const declarativeNetRequestRulePathManifestKey = @"path";

static NSString * const externallyConnectableManifestKey = @"externally_connectable";
static NSString * const externallyConnectableMatchesManifestKey = @"matches";
static NSString * const externallyConnectableIDsManifestKey = @"ids";

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
static NSString * const sidebarActionManifestKey = @"sidebar_action";
static NSString * const sidePanelManifestKey = @"side_panel";
static NSString * const sidebarActionTitleManifestKey = @"default_title";
static NSString * const sidebarActionPathManifestKey = @"default_panel";
static NSString * const sidePanelPathManifestKey = @"default_path";
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

static const size_t maximumNumberOfShortcutCommands = 4;

WebExtension::WebExtension(NSBundle *appExtensionBundle, NSError **outError)
    : m_bundle(appExtensionBundle)
    , m_resourceBaseURL(appExtensionBundle.resourceURL.URLByStandardizingPath.absoluteURL)
    , m_manifestJSON(JSON::Value::null())
{
    ASSERT(appExtensionBundle);

    if (outError)
        *outError = nil;

    if (!manifestParsedSuccessfully()) {
        ASSERT(m_errors.get().count);
        *outError = m_errors.get().lastObject;
    }
}

WebExtension::WebExtension(NSURL *resourceBaseURL, NSError **outError)
    : m_resourceBaseURL(resourceBaseURL.URLByStandardizingPath.absoluteURL)
    , m_manifestJSON(JSON::Value::null())
{
    ASSERT(resourceBaseURL);
    ASSERT([resourceBaseURL isFileURL]);
    ASSERT([resourceBaseURL hasDirectoryPath]);

    if (outError)
        *outError = nil;

    if (!manifestParsedSuccessfully()) {
        ASSERT(m_errors.get().count);
        *outError = m_errors.get().lastObject;
    }
}

WebExtension::WebExtension(NSDictionary *manifest, NSDictionary *resources)
    : m_manifestJSON(JSON::Value::null())
    , m_resources([resources mutableCopy] ?: [NSMutableDictionary dictionary])
{
    ASSERT(manifest);

    NSData *manifestData = encodeJSONData(manifest);
    RELEASE_ASSERT(manifestData);

    [m_resources setObject:manifestData forKey:@"manifest.json"];
}

WebExtension::WebExtension(NSDictionary *resources)
    : m_manifestJSON(JSON::Value::null())
    , m_resources([resources mutableCopy] ?: [NSMutableDictionary dictionary])
{
}

bool WebExtension::parseManifest(NSData *manifestData)
{
    NSError *parseError;
    m_manifest = parseJSON(manifestData, { }, &parseError);
    if (!m_manifest) {
        recordError(createError(Error::InvalidManifest, { }, parseError));
        return false;
    }

    // Set to an empty object for now so calls to manifestParsedSuccessfully() during this will be true.
    // This is needed for localization to properly get the defaultLocale() while we are mid-parse.
    m_manifestJSON = JSON::Object::create();

    auto *defaultLocale = objectForKey<NSString>(m_manifest, defaultLocaleManifestKey);
    m_defaultLocale = [NSLocale localeWithLocaleIdentifier:defaultLocale];

    m_localization = [[_WKWebExtensionLocalization alloc] initWithWebExtension:*this];

    m_manifest = [m_localization.get() localizedDictionaryForDictionary:m_manifest.get()];
    if (!m_manifest) {
        m_manifestJSON = JSON::Value::null();
        recordError(createError(Error::InvalidManifest));
        return false;
    }

    // Parse m_manifestJSON after localization so it gets the localized version.
    RefPtr manifestJSON = JSON::Value::parseJSON(String(encodeJSONString(m_manifest.get())));
    if (!manifestJSON || !manifestJSON->asObject()) {
        m_manifestJSON = JSON::Value::null();
        recordError(createError(Error::InvalidManifest));
        return false;
    }

    m_manifestJSON = *manifestJSON;

    return true;
}

NSDictionary *WebExtension::manifest()
{
    if (m_parsedManifest)
        return m_manifest.get();

    m_parsedManifest = true;

    NSError *error;
    NSData *manifestData = resourceDataForPath(@"manifest.json", &error);
    if (!manifestData) {
        recordError(error);
        return nil;
    }

    if (!parseManifest(manifestData))
        return nil;

    return m_manifest.get();
}

Ref<API::Data> WebExtension::serializeManifest()
{
    return API::Data::createWithoutCopying(encodeJSONData(manifest()));
}

Ref<API::Data> WebExtension::serializeLocalization()
{
    return API::Data::createWithoutCopying(encodeJSONData(m_localization.get().localizationDictionary));
}

SecStaticCodeRef WebExtension::bundleStaticCode() const
{
    if (!m_bundle)
        return nullptr;

    if (m_bundleStaticCode)
        return m_bundleStaticCode.get();

    SecStaticCodeRef staticCodeRef;
    OSStatus error = SecStaticCodeCreateWithPath(bridge_cast(m_bundle.get().bundleURL), kSecCSDefaultFlags, &staticCodeRef);
    if (error != noErr || !staticCodeRef) {
        if (staticCodeRef)
            CFRelease(staticCodeRef);
        return nullptr;
    }

    m_bundleStaticCode = adoptCF(staticCodeRef);

    return m_bundleStaticCode.get();
}

NSData *WebExtension::bundleHash() const
{
    auto staticCode = bundleStaticCode();
    if (!staticCode)
        return nil;

    CFDictionaryRef codeSigningDictionary = nil;
    OSStatus error = SecCodeCopySigningInformation(staticCode, kSecCSDefaultFlags, &codeSigningDictionary);
    if (error != noErr || !codeSigningDictionary) {
        if (codeSigningDictionary)
            CFRelease(codeSigningDictionary);
        return nil;
    }

    auto *result = bridge_cast(checked_cf_cast<CFDataRef>(CFDictionaryGetValue(codeSigningDictionary, kSecCodeInfoUnique)));
    CFRelease(codeSigningDictionary);

    return result;
}

#if PLATFORM(MAC)
bool WebExtension::validateResourceData(NSURL *resourceURL, NSData *resourceData, NSError **outError)
{
    ASSERT([resourceURL isFileURL]);
    ASSERT(resourceData);

    if (!m_bundle)
        return true;

    auto staticCode = bundleStaticCode();
    if (!staticCode)
        return false;

    NSURL *bundleSupportFilesURL = CFBridgingRelease(CFBundleCopySupportFilesDirectoryURL(m_bundle.get()._cfBundle));
    NSString *bundleSupportFilesURLString = bundleSupportFilesURL.absoluteString;
    NSString *resourceURLString = resourceURL.absoluteString;
    ASSERT([resourceURLString hasPrefix:bundleSupportFilesURLString]);

    NSString *relativePathToResource = [resourceURLString substringFromIndex:bundleSupportFilesURLString.length].stringByRemovingPercentEncoding;
    OSStatus result = SecCodeValidateFileResource(staticCode, bridge_cast(relativePathToResource), bridge_cast(resourceData), kSecCSDefaultFlags);

    if (outError && result != noErr)
        *outError = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];

    return result == noErr;
}
#endif // PLATFORM(MAC)

NSURL *WebExtension::resourceFileURLForPath(NSString *path)
{
    ASSERT(path);

    if ([path hasPrefix:@"/"])
        path = [path substringFromIndex:1];

    if (!path.length || !m_resourceBaseURL)
        return nil;

    NSURL *resourceURL = [NSURL fileURLWithPath:path.stringByRemovingPercentEncoding isDirectory:NO relativeToURL:m_resourceBaseURL.get()].URLByStandardizingPath;

    // Don't allow escaping the base URL with "../".
    if (![resourceURL.absoluteString hasPrefix:m_resourceBaseURL.get().absoluteString]) {
        RELEASE_LOG_ERROR(Extensions, "Resource URL path escape attempt: %{private}@", resourceURL);
        return nil;
    }

    return resourceURL;
}

UTType *WebExtension::resourceTypeForPath(NSString *path)
{
    UTType *result;

    if ([path hasPrefix:@"data:"]) {
        auto mimeTypeRange = [path rangeOfString:@";"];
        if (mimeTypeRange.location != NSNotFound) {
            auto *mimeType = [path substringWithRange:NSMakeRange(5, mimeTypeRange.location - 5)];
            result = [UTType typeWithMIMEType:mimeType];
        }
    } else if (auto *fileExtension = path.pathExtension; fileExtension.length)
        result = [UTType typeWithFilenameExtension:fileExtension];
    else if (auto *fileURL = resourceFileURLForPath(path))
        [fileURL getResourceValue:&result forKey:NSURLContentTypeKey error:nil];

    return result;
}

NSString *WebExtension::resourceStringForPath(NSString *path, NSError **outError, CacheResult cacheResult, SuppressNotFoundErrors suppressErrors)
{
    ASSERT(path);

    // Remove leading slash to normalize the path for lookup/storage in the cache dictionary.
    if ([path hasPrefix:@"/"])
        path = [path substringFromIndex:1];

    if (NSString *cachedString = objectForKey<NSString>(m_resources, path))
        return cachedString;

    if ([path isEqualToString:generatedBackgroundPageFilename] || [path isEqualToString:generatedBackgroundServiceWorkerFilename])
        return generatedBackgroundContent();

    NSData *data = resourceDataForPath(path, outError, CacheResult::No, suppressErrors);
    if (!data)
        return nil;

    NSString *string;
    [NSString stringEncodingForData:data encodingOptions:nil convertedString:&string usedLossyConversion:nil];
    if (!string)
        return nil;

    if (cacheResult == CacheResult::Yes) {
        if (!m_resources)
            m_resources = [NSMutableDictionary dictionary];
        [m_resources setObject:string forKey:path];
    }

    return string;
}

NSData *WebExtension::resourceDataForPath(NSString *path, NSError **outError, CacheResult cacheResult, SuppressNotFoundErrors suppressErrors)
{
    ASSERT(path);

    if (outError)
        *outError = nil;

    if ([path hasPrefix:@"data:"]) {
        if (auto base64Range = [path rangeOfString:@";base64,"]; base64Range.location != NSNotFound) {
            auto *base64String = [path substringFromIndex:NSMaxRange(base64Range)];
            return [[NSData alloc] initWithBase64EncodedString:base64String options:0];
        }

        if (auto commaRange = [path rangeOfString:@","]; commaRange.location != NSNotFound) {
            auto *urlEncodedString = [path substringFromIndex:NSMaxRange(commaRange)];
            auto *decodedString = [urlEncodedString stringByRemovingPercentEncoding];
            return [decodedString dataUsingEncoding:NSUTF8StringEncoding];
        }

        ASSERT([path isEqualToString:@"data:"]);
        return [NSData data];
    }

    // Remove leading slash to normalize the path for lookup/storage in the cache dictionary.
    if ([path hasPrefix:@"/"])
        path = [path substringFromIndex:1];

    if (id cachedObject = [m_resources objectForKey:path]) {
        if (auto *cachedData = dynamic_objc_cast<NSData>(cachedObject))
            return cachedData;

        if (auto *cachedString = dynamic_objc_cast<NSString>(cachedObject))
            return [cachedString dataUsingEncoding:NSUTF8StringEncoding];

        ASSERT(isValidJSONObject(cachedObject, JSONOptions::FragmentsAllowed));

        auto *result = encodeJSONData(cachedObject, JSONOptions::FragmentsAllowed);
        RELEASE_ASSERT(result);

        // Cache the JSON data, so it can be fetched quicker next time.
        [m_resources setObject:result forKey:path];

        return result;
    }

    if ([path isEqualToString:generatedBackgroundPageFilename] || [path isEqualToString:generatedBackgroundServiceWorkerFilename])
        return [static_cast<NSString *>(generatedBackgroundContent()) dataUsingEncoding:NSUTF8StringEncoding];

    NSURL *resourceURL = resourceFileURLForPath(path);
    if (!resourceURL) {
        if (suppressErrors == SuppressNotFoundErrors::No && outError)
            *outError = createError(Error::ResourceNotFound, WEB_UI_FORMAT_CFSTRING("Unable to find \"%@\" in the extension’s resources. It is an invalid path.", "WKWebExtensionErrorResourceNotFound description with invalid file path", (__bridge CFStringRef)path));
        return nil;
    }

    NSError *fileReadError;
    NSData *resultData = [NSData dataWithContentsOfURL:resourceURL options:NSDataReadingMappedIfSafe error:&fileReadError];
    if (!resultData) {
        if (suppressErrors == SuppressNotFoundErrors::No && outError)
            *outError = createError(Error::ResourceNotFound, WEB_UI_FORMAT_CFSTRING("Unable to find \"%@\" in the extension’s resources.", "WKWebExtensionErrorResourceNotFound description with file name", (__bridge CFStringRef)path), fileReadError);
        return nil;
    }

#if PLATFORM(MAC)
    NSError *validationError;
    if (!validateResourceData(resourceURL, resultData, &validationError)) {
        if (outError)
            *outError = createError(Error::InvalidResourceCodeSignature, WEB_UI_FORMAT_CFSTRING("Unable to validate \"%@\" with the extension’s code signature. It likely has been modified since the extension was built.", "WKWebExtensionErrorInvalidResourceCodeSignature description with file name", (__bridge CFStringRef)path), validationError);
        return nil;
    }
#endif

    if (cacheResult == CacheResult::Yes) {
        if (!m_resources)
            m_resources = [NSMutableDictionary dictionary];
        [m_resources setObject:resultData forKey:path];
    }

    return resultData;
}

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

NSError *WebExtension::createError(Error error, String customLocalizedDescription, NSError *underlyingError)
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
ALLOW_NONLITERAL_FORMAT_BEGIN
        if (NSString *debugDescription = underlyingError.userInfo[NSDebugDescriptionErrorKey])
            localizedDescription = [NSString stringWithFormat:WEB_UI_STRING("Unable to parse manifest: %@", "WKWebExtensionErrorInvalidManifest description, because of a JSON error"), debugDescription];
        else
            localizedDescription = WEB_UI_STRING("Unable to parse manifest because of an unexpected format.", "WKWebExtensionErrorInvalidManifest description");
ALLOW_NONLITERAL_FORMAT_END
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
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
        if (m_actionDictionary.get()[iconVariantsManifestKey]) {
            if (supportsManifestVersion(3))
                localizedDescription = WEB_UI_STRING("Empty or invalid `icon_variants` for the `action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for icon_variants in action only");
            else
                localizedDescription = WEB_UI_STRING("Empty or invalid `icon_variants` for the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for icon_variants in browser_action or page_action");
        } else
#endif
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
ALLOW_NONLITERAL_FORMAT_BEGIN
        if (NSString *debugDescription = underlyingError.userInfo[NSDebugDescriptionErrorKey])
            localizedDescription = [NSString stringWithFormat:WEB_UI_STRING("Unable to parse `declarativeNetRequest` rules: %@", "WKWebExtensionErrorInvalidDeclarativeNetRequest description, because of a JSON error"), debugDescription];
        else
            localizedDescription = WEB_UI_STRING("Unable to parse `declarativeNetRequest` rules because of an unexpected error.", "WKWebExtensionErrorInvalidDeclarativeNetRequest description");
ALLOW_NONLITERAL_FORMAT_END
        break;

    case Error::InvalidDescription:
        localizedDescription = WEB_UI_STRING("Missing or empty `description` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for description");
        break;

    case Error::InvalidExternallyConnectable:
        localizedDescription = WEB_UI_STRING("Empty or invalid `externally_connectable` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for externally_connectable");
        break;

    case Error::InvalidIcon:
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
        if ([manifest() objectForKey:iconVariantsManifestKey])
            localizedDescription = WEB_UI_STRING("Empty or invalid `icon_variants` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for icon_variants");
        else
#endif
            localizedDescription = WEB_UI_STRING("Missing or empty `icons` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for icons");
        break;

    case Error::InvalidName:
        localizedDescription = WEB_UI_STRING("Missing or empty `name` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for name");
        break;

    case Error::InvalidOptionsPage:
        if ([manifest() objectForKey:optionsUIManifestKey])
            localizedDescription = WEB_UI_STRING("Empty or invalid `options_ui` manifest entry", "WKWebExtensionErrorInvalidManifestEntry description for options UI");
        else
            localizedDescription = WEB_UI_STRING("Empty or invalid `options_page` manifest entry", "WKWebExtensionErrorInvalidManifestEntry description for options page");
        break;

    case Error::InvalidURLOverrides:
        if ([manifest() objectForKey:browserURLOverridesManifestKey])
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

    if (!customLocalizedDescription.isEmpty())
        localizedDescription = customLocalizedDescription;

    NSDictionary *userInfo;
    if (underlyingError)
        userInfo = @{ NSLocalizedDescriptionKey: localizedDescription, NSUnderlyingErrorKey: underlyingError };
    else
        userInfo = @{ NSLocalizedDescriptionKey: localizedDescription };

    return [[NSError alloc] initWithDomain:WKWebExtensionErrorDomain code:errorCode userInfo:userInfo];
}

void WebExtension::recordError(NSError *error)
{
    ASSERT(error);

    if (!m_errors)
        m_errors = [NSMutableArray array];

    RELEASE_LOG_ERROR(Extensions, "Error recorded: %{public}@", privacyPreservingDescription(error));

    // Only the first occurrence of each error is recorded in the array. This prevents duplicate errors,
    // such as repeated "resource not found" errors, from being included multiple times.
    if ([m_errors containsObject:error])
        return;

    [wrapper() willChangeValueForKey:@"errors"];
    [m_errors addObject:error];
    [wrapper() didChangeValueForKey:@"errors"];
}

NSArray *WebExtension::errors()
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

    return [m_errors copy] ?: @[ ];
}

_WKWebExtensionLocalization *WebExtension::localization()
{
    if (!manifestParsedSuccessfully())
        return nil;

    return m_localization.get();
}

NSLocale *WebExtension::defaultLocale()
{
    if (!manifestParsedSuccessfully())
        return nil;

    return m_defaultLocale.get();
}

void WebExtension::populateExternallyConnectableIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedExternallyConnectable)
        return;

    m_parsedExternallyConnectable = true;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/externally_connectable

    auto *externallyConnectableDictionary = objectForKey<NSDictionary>(m_manifest, externallyConnectableManifestKey, false);

    if (!externallyConnectableDictionary)
        return;

    if (!externallyConnectableDictionary.count) {
        recordError(createError(Error::InvalidExternallyConnectable));
        return;
    }

    bool shouldReportError = false;
    MatchPatternSet matchPatterns;

    auto *matchPatternStrings = objectForKey<NSArray>(externallyConnectableDictionary, externallyConnectableMatchesManifestKey, true, NSString.class);
    for (NSString *matchPatternString in matchPatternStrings) {
        if (!matchPatternString.length)
            continue;

        if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(matchPatternString)) {
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

    m_externallyConnectableMatchPatterns = matchPatterns;

    auto *extensionIDs = objectForKey<NSArray>(externallyConnectableDictionary, externallyConnectableIDsManifestKey, true, NSString.class);
    extensionIDs = filterObjects(extensionIDs, ^bool(id key, NSString *extensionID) {
        return !!extensionID.length;
    });

    if (shouldReportError || (matchPatterns.isEmpty() && !extensionIDs.count))
        recordError(createError(Error::InvalidExternallyConnectable));
}

CocoaImage *WebExtension::icon(CGSize size)
{
    if (!manifestParsedSuccessfully())
        return nil;

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    if (m_manifest.get()[iconVariantsManifestKey]) {
        NSString *localizedErrorDescription = WEB_UI_STRING("Failed to load images in `icon_variants` manifest entry.", "WKWebExtensionErrorInvalidIcon description for failing to load image variants");
        return bestImageForIconVariantsManifestKey(m_manifest.get(), iconVariantsManifestKey, size, m_iconsCache, Error::InvalidIcon, localizedErrorDescription);
    }
#endif

    NSString *localizedErrorDescription = WEB_UI_STRING("Failed to load images in `icons` manifest entry.", "WKWebExtensionErrorInvalidIcon description for failing to load images");
    return bestImageForIconsDictionaryManifestKey(m_manifest.get(), iconsManifestKey, size, m_iconsCache, Error::InvalidIcon, localizedErrorDescription);
}

CocoaImage *WebExtension::actionIcon(CGSize size)
{
    if (!manifestParsedSuccessfully())
        return nil;

    populateActionPropertiesIfNeeded();

    if (m_defaultActionIcon)
        return m_defaultActionIcon.get();

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    if (m_actionDictionary.get()[iconVariantsManifestKey]) {
        NSString *localizedErrorDescription = WEB_UI_STRING("Failed to load images in `icon_variants` for the `action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load image variants for action");
        if (auto *result = bestImageForIconVariantsManifestKey(m_actionDictionary.get(), iconVariantsManifestKey, size, m_actionIconsCache, Error::InvalidActionIcon, localizedErrorDescription))
            return result;
        return icon(size);
    }
#endif

    NSString *localizedErrorDescription;
    if (supportsManifestVersion(3))
        localizedErrorDescription = WEB_UI_STRING("Failed to load images in `default_icon` for the `action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load images for action only");
    else
        localizedErrorDescription = WEB_UI_STRING("Failed to load images in `default_icon` for the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load images for browser_action or page_action");

    if (auto *result = bestImageForIconsDictionaryManifestKey(m_actionDictionary.get(), defaultIconManifestKey, size, m_actionIconsCache, Error::InvalidActionIcon, localizedErrorDescription))
        return result;
    return icon(size);
}

NSString *WebExtension::displayActionLabel()
{
    populateActionPropertiesIfNeeded();
    return m_displayActionLabel.get();
}

NSString *WebExtension::actionPopupPath()
{
    populateActionPropertiesIfNeeded();
    return m_actionPopupPath.get();
}

bool WebExtension::hasAction()
{
    return supportsManifestVersion(3) && objectForKey<NSDictionary>(m_manifest, actionManifestKey, false);
}

bool WebExtension::hasBrowserAction()
{
    return !supportsManifestVersion(3) && objectForKey<NSDictionary>(m_manifest, browserActionManifestKey, false);
}

bool WebExtension::hasPageAction()
{
    return !supportsManifestVersion(3) && objectForKey<NSDictionary>(m_manifest, pageActionManifestKey, false);
}

void WebExtension::populateActionPropertiesIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestActionProperties)
        return;

    m_parsedManifestActionProperties = true;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/action
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/browser_action
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/page_action

    if (supportsManifestVersion(3))
        m_actionDictionary = objectForKey<NSDictionary>(m_manifest, actionManifestKey, false);
    else {
        m_actionDictionary = objectForKey<NSDictionary>(m_manifest, browserActionManifestKey, false);
        if (!m_actionDictionary)
            m_actionDictionary = objectForKey<NSDictionary>(m_manifest, pageActionManifestKey, false);
    }

    if (!m_actionDictionary)
        return;

    // Look for the "default_icon" as a string, which is useful for SVG icons. Only supported by Firefox currently.
    if (auto *defaultIconPath = objectForKey<NSString>(m_actionDictionary, defaultIconManifestKey)) {
        NSError *resourceError;
        m_defaultActionIcon = imageForPath(defaultIconPath, &resourceError);

        if (!m_defaultActionIcon) {
            recordError(resourceError);

            NSString *localizedErrorDescription;
            if (supportsManifestVersion(3))
                localizedErrorDescription = WEB_UI_STRING("Failed to load image for `default_icon` in the `action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load single image for action");
            else
                localizedErrorDescription = WEB_UI_STRING("Failed to load image for `default_icon` in the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load single image for browser_action or page_action");

            recordError(createError(Error::InvalidActionIcon, localizedErrorDescription));
        }
    }

    m_displayActionLabel = objectForKey<NSString>(m_actionDictionary, defaultTitleManifestKey);
    m_actionPopupPath = objectForKey<NSString>(m_actionDictionary, defaultPopupManifestKey);
}

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
bool WebExtension::hasSidebarAction()
{
    return objectForKey<NSDictionary>(m_manifest, sidebarActionManifestKey);
}

bool WebExtension::hasSidePanel()
{
    return hasRequestedPermission(WKWebExtensionPermissionSidePanel);
}

bool WebExtension::hasAnySidebar()
{
    return hasSidebarAction() || hasSidePanel();
}

CocoaImage *WebExtension::sidebarIcon(CGSize idealSize)
{
    // FIXME: <https://webkit.org/b/276833> implement this
    return nil;
}

NSString *WebExtension::sidebarDocumentPath()
{
    populateSidebarPropertiesIfNeeded();
    return m_sidebarDocumentPath.get();
}

NSString *WebExtension::sidebarTitle()
{
    populateSidebarPropertiesIfNeeded();
    return m_sidebarTitle.get();
}

void WebExtension::populateSidebarPropertiesIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestSidebarProperties)
        return;

    // sidePanel documentation: https://developer.chrome.com/docs/extensions/reference/manifest#side-panel
    // see "Examples" header -> "Side Panel" tab (doesn't mention `default_path` key elsewhere)
    // sidebarAction documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/sidebar_action

    auto sidebarActionDictionary = objectForKey<NSDictionary>(m_manifest, sidebarActionManifestKey);
    if (sidebarActionDictionary) {
        populateSidebarActionProperties(sidebarActionDictionary);
        return;
    }

    auto sidePanelDictionary = objectForKey<NSDictionary>(m_manifest, sidePanelManifestKey);
    if (sidePanelDictionary)
        populateSidePanelProperties(sidePanelDictionary);
}

void WebExtension::populateSidebarActionProperties(RetainPtr<NSDictionary> sidebarActionDictionary)
{
    // FIXME: <https://webkit.org/b/276833> implement sidebar icon parsing
    m_sidebarIconsCache = nil;
    m_sidebarTitle = objectForKey<NSString>(sidebarActionDictionary, sidebarActionTitleManifestKey);
    m_sidebarDocumentPath = objectForKey<NSString>(sidebarActionDictionary, sidebarActionPathManifestKey);
}

void WebExtension::populateSidePanelProperties(RetainPtr<NSDictionary> sidePanelDictionary)
{
    // Since sidePanel cannot set a default title or icon from the manifest, setting these nil here is intentional.
    m_sidebarIconsCache = nil;
    m_sidebarTitle = nil;
    m_sidebarDocumentPath = objectForKey<NSString>(sidePanelDictionary, sidePanelPathManifestKey);
}
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

CocoaImage *WebExtension::imageForPath(NSString *imagePath, NSError **outError, CGSize sizeForResizing)
{
    ASSERT(imagePath);

    NSData *imageData = resourceDataForPath(imagePath, outError);
    if (!imageData)
        return nil;

    CocoaImage *result;

#if !USE(NSIMAGE_FOR_SVG_SUPPORT)
    UTType *imageType = resourceTypeForPath(imagePath);
    if ([imageType.identifier isEqualToString:UTTypeSVG.identifier]) {
#if USE(APPKIT)
        static Class svgImageRep = NSClassFromString(@"_NSSVGImageRep");
        RELEASE_ASSERT(svgImageRep);

        _NSSVGImageRep *imageRep = [[svgImageRep alloc] initWithData:imageData];
        if (!imageRep)
            return nil;

        result = [[NSImage alloc] init];
        [result addRepresentation:imageRep];
        result.size = imageRep.size;
#else
        CGSVGDocumentRef document = CGSVGDocumentCreateFromData(bridge_cast(imageData), nullptr);
        if (!document)
            return nil;

        // Since we need to rasterize, scale the image for the densest display, so it will have enough pixels to be sharp.
        result = [UIImage _imageWithCGSVGDocument:document scale:largestDisplayScale() orientation:UIImageOrientationUp];
        CGSVGDocumentRelease(document);
#endif // not USE(APPKIT)
    }
#endif // !USE(NSIMAGE_FOR_SVG_SUPPORT)

    if (!result)
        result = [[CocoaImage alloc] initWithData:imageData];

#if USE(APPKIT)
    if (!CGSizeEqualToSize(sizeForResizing, CGSizeZero)) {
        // Proportionally scale the size.
        auto originalSize = result.size;
        auto aspectWidth = sizeForResizing.width / originalSize.width;
        auto aspectHeight = sizeForResizing.height / originalSize.height;
        auto aspectRatio = std::min(aspectWidth, aspectHeight);

        result.size = CGSizeMake(originalSize.width * aspectRatio, originalSize.height * aspectRatio);
    }

    return result;
#else
    // Rasterization is needed because UIImageAsset will not register the image unless it is a CGImage.
    // If the image is already a CGImage bitmap, this operation is a no-op.
    result = result._rasterizedImage;

    if (!CGSizeEqualToSize(sizeForResizing, CGSizeZero) && !CGSizeEqualToSize(result.size, sizeForResizing))
        result = [result imageByPreparingThumbnailOfSize:sizeForResizing];

    return result;
#endif // not USE(APPKIT)
}

size_t WebExtension::bestSizeInIconsDictionary(NSDictionary *iconsDictionary, size_t idealPixelSize)
{
    if (!iconsDictionary.count)
        return 0;

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    // Check if the "any" size exists (typically a vector image), and prefer it.
    if (iconsDictionary[anyManifestKey]) {
        // Return max to ensure it takes precedence over all other sizes.
        return std::numeric_limits<size_t>::max();
    }
#endif

    // Check if the ideal size exists, if so return it.
    NSString *idealSizeString = @(idealPixelSize).stringValue;
    if (iconsDictionary[idealSizeString])
        return idealPixelSize;

    // Sort the remaining keys and find the next largest size.
    NSArray<NSString *> *sizeKeys = filterObjects(iconsDictionary.allKeys, ^bool(id, id value) {
        // Filter the values to only include numeric strings representing sizes. This will exclude non-numeric string
        // values such as "any", "color_schemes", and any other strings that cannot be converted to a positive integer.
        return dynamic_objc_cast<NSString>(value).integerValue > 0;
    });

    if (!sizeKeys.count)
        return 0;

    NSSortDescriptor *sortDescriptor = [[NSSortDescriptor alloc] initWithKey:@"self" ascending:YES selector:@selector(localizedStandardCompare:)];
    NSArray<NSString *> *sortedKeys = [sizeKeys sortedArrayUsingDescriptors:@[ sortDescriptor ]];

    size_t bestSize = 0;
    for (NSString *size in sortedKeys) {
        bestSize = size.integerValue;
        if (bestSize >= idealPixelSize)
            break;
    }

    return bestSize;
}

NSString *WebExtension::pathForBestImageInIconsDictionary(NSDictionary *iconsDictionary, size_t idealPixelSize)
{
    size_t bestSize = bestSizeInIconsDictionary(iconsDictionary, idealPixelSize);
    if (!bestSize)
        return nil;

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    if (bestSize == std::numeric_limits<size_t>::max())
        return iconsDictionary[anyManifestKey];
#endif

    return iconsDictionary[@(bestSize).stringValue];
}

CocoaImage *WebExtension::bestImageInIconsDictionary(NSDictionary *iconsDictionary, CGSize idealSize, const Function<void(NSError *)>& reportError)
{
    if (!iconsDictionary.count)
        return nil;

    auto idealPointSize = idealSize.width > idealSize.height ? idealSize.width : idealSize.height;
    auto *screenScales = availableScreenScales();
    auto *uniquePaths = [NSMutableSet set];
#if PLATFORM(IOS_FAMILY)
    auto *scalePaths = [NSMutableDictionary dictionary];
#endif

    for (NSNumber *scale in screenScales) {
        auto pixelSize = idealPointSize * scale.doubleValue;
        auto *iconPath = pathForBestImageInIconsDictionary(iconsDictionary, pixelSize);
        if (!iconPath)
            continue;

        [uniquePaths addObject:iconPath];

#if PLATFORM(IOS_FAMILY)
        scalePaths[scale] = iconPath;
#endif
    }

    if (!uniquePaths.count)
        return nil;

#if USE(APPKIT)
    // Return a combined image so the system can select the most appropriate representation based on the current screen scale.
    NSImage *resultImage;

    for (NSString *iconPath in uniquePaths) {
        NSError *resourceError;
        if (auto *image = imageForPath(iconPath, &resourceError, idealSize)) {
            if (!resultImage)
                resultImage = image;
            else
                [resultImage addRepresentations:image.representations];
        } else if (reportError && resourceError)
            reportError(resourceError);
    }

    return resultImage;
#else
    if (uniquePaths.count == 1) {
        [scalePaths removeAllObjects];

        // Add a single value back that has 0 for the scale, which is the
        // unspecified (universal) trait value for display scale.
        scalePaths[@0] = uniquePaths.anyObject;
    }

    auto *images = mapObjects<NSDictionary>(scalePaths, ^id(NSNumber *scale, NSString *path) {
        NSError *resourceError;
        if (auto *image = imageForPath(path, &resourceError, idealSize))
            return image;

        if (reportError && resourceError)
            reportError(resourceError);

        return nil;
    });

    if (images.count == 1)
        return images.allValues.firstObject;

    // Make a dynamic image asset that returns an image based on the trait collection.
    auto *imageAsset = [UIImageAsset _dynamicAssetNamed:NSUUID.UUID.UUIDString generator:^(UIImageAsset *, UIImageConfiguration *configuration, UIImage *) {
        return images[@(configuration.traitCollection.displayScale)] ?: images[@0];
    }];

    // The returned image retains its link to the image asset and adapts to trait changes,
    // automatically displaying the correct variant based on the current traits.
    return [imageAsset imageWithTraitCollection:UITraitCollection.currentTraitCollection];
#endif // not USE(APPKIT)
}

CocoaImage *WebExtension::bestImageForIconsDictionaryManifestKey(NSDictionary *dictionary, NSString *manifestKey, CGSize idealSize, RetainPtr<NSMutableDictionary>& cacheLocation, Error error, NSString *customLocalizedDescription)
{
    // Clear the cache if the display scales change (connecting display, etc.)
    auto *currentScales = availableScreenScales();
    auto *cachedScales = objectForKey<NSSet>(cacheLocation, @"scales");
    if (!cacheLocation || ![currentScales isEqualToSet:cachedScales])
        cacheLocation = [NSMutableDictionary dictionaryWithObject:currentScales forKey:@"scales"];

    auto *cacheKey = @(idealSize);
    if (id cachedResult = cacheLocation.get()[cacheKey])
        return dynamic_objc_cast<CocoaImage>(cachedResult);

    auto *iconDictionary = objectForKey<NSDictionary>(dictionary, manifestKey);
    auto *result = bestImageInIconsDictionary(iconDictionary, idealSize, [&](auto *error) {
        recordError(error);
    });

    cacheLocation.get()[cacheKey] = result ?: NSNull.null;

    if (!result) {
        if (iconDictionary.count) {
            // Record an error if the dictionary had values, meaning the likely failure is the images were missing on disk or bad format.
            recordError(createError(error, customLocalizedDescription));
        } else if ((iconDictionary && !iconDictionary.count) || dictionary[manifestKey]) {
            // Record an error if the key had dictionary that was empty, or the key had a value of the wrong type.
            recordError(createError(error));
        }

        return nil;
    }

    return result;
}

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
static OptionSet<WebExtension::ColorScheme> toColorSchemes(id value)
{
    using ColorScheme = WebExtension::ColorScheme;

    if (!value) {
        // A nil value counts as all color schemes.
        return { ColorScheme::Light, ColorScheme::Dark };
    }

    OptionSet<ColorScheme> result;

    auto *array = dynamic_objc_cast<NSArray>(value);
    if ([array containsObject:lightManifestKey])
        result.add(ColorScheme::Light);

    if ([array containsObject:darkManifestKey])
        result.add(ColorScheme::Dark);

    return result;
}

NSDictionary *WebExtension::iconsDictionaryForBestIconVariant(NSArray *variants, size_t idealPixelSize, ColorScheme idealColorScheme)
{
    if (!variants.count)
        return nil;

    if (variants.count == 1)
        return variants.firstObject;

    NSDictionary *bestVariant;
    NSDictionary *fallbackVariant;
    bool foundIdealFallbackVariant = false;

    size_t bestSize = 0;
    size_t fallbackSize = 0;

    // Pick the first variant matching color scheme and/or size.
    for (NSDictionary *variant in variants) {
        auto colorSchemes = toColorSchemes(variant[colorSchemesManifestKey]);
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

    return bestVariant ?: fallbackVariant;
}

CocoaImage *WebExtension::bestImageForIconVariants(NSArray *variants, CGSize idealSize, const Function<void(NSError *)>& reportError)
{
    auto idealPointSize = idealSize.width > idealSize.height ? idealSize.width : idealSize.height;
    auto *lightIconsDictionary = iconsDictionaryForBestIconVariant(variants, idealPointSize, ColorScheme::Light);
    auto *darkIconsDictionary = iconsDictionaryForBestIconVariant(variants, idealPointSize, ColorScheme::Dark);

    // If the light and dark icons dictionaries are the same, or if either is nil, return the available image directly.
    if (!lightIconsDictionary || !darkIconsDictionary || [lightIconsDictionary isEqualToDictionary:darkIconsDictionary])
        return bestImageInIconsDictionary(lightIconsDictionary ?: darkIconsDictionary, idealSize, reportError);

    auto *lightImage = bestImageInIconsDictionary(lightIconsDictionary, idealSize, reportError);
    auto *darkImage = bestImageInIconsDictionary(darkIconsDictionary, idealSize, reportError);

    // If either the light or dark icon is nil, return the available image directly.
    if (!lightImage || !darkImage)
        return lightImage ?: darkImage;

#if USE(APPKIT)
    // The images need to be the same size to draw correctly in the block.
    auto imageSize = lightImage.size.width >= darkImage.size.width ? lightImage.size : darkImage.size;
    lightImage.size = imageSize;
    darkImage.size = imageSize;

    // Make a dynamic image that draws the light or dark image based on the current appearance.
    return [NSImage imageWithSize:imageSize flipped:NO drawingHandler:^BOOL(NSRect rect) {
        static auto *darkAppearanceNames = @[
            NSAppearanceNameDarkAqua,
            NSAppearanceNameVibrantDark,
            NSAppearanceNameAccessibilityHighContrastDarkAqua,
            NSAppearanceNameAccessibilityHighContrastVibrantDark,
        ];

        if ([NSAppearance.currentDrawingAppearance bestMatchFromAppearancesWithNames:darkAppearanceNames])
            [darkImage drawInRect:rect];
        else
            [lightImage drawInRect:rect];

        return YES;
    }];
#else
    // Make a dynamic image asset that returns the light or dark image based on the trait collection.
    auto *imageAsset = [UIImageAsset _dynamicAssetNamed:NSUUID.UUID.UUIDString generator:^(UIImageAsset *, UIImageConfiguration *configuration, UIImage *) {
        return configuration.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark ? darkImage : lightImage;
    }];

    // The returned image retains its link to the image asset and adapts to trait changes,
    // automatically displaying the correct variant based on the current traits.
    return [imageAsset imageWithTraitCollection:UITraitCollection.currentTraitCollection];
#endif // not USE(APPKIT)
}

CocoaImage *WebExtension::bestImageForIconVariantsManifestKey(NSDictionary *dictionary, NSString *manifestKey, CGSize idealSize, RetainPtr<NSMutableDictionary>& cacheLocation, Error error, NSString *customLocalizedDescription)
{
    // Clear the cache if the display scales change (connecting display, etc.)
    auto *currentScales = availableScreenScales();
    auto *cachedScales = objectForKey<NSSet>(cacheLocation, @"scales");
    if (!cacheLocation || ![currentScales isEqualToSet:cachedScales])
        cacheLocation = [NSMutableDictionary dictionaryWithObject:currentScales forKey:@"scales"];

    auto *cacheKey = @(idealSize);
    if (id cachedResult = cacheLocation.get()[cacheKey])
        return dynamic_objc_cast<CocoaImage>(cachedResult);

    auto *variants = objectForKey<NSArray>(dictionary, manifestKey, false, NSDictionary.class);
    auto *result = bestImageForIconVariants(variants, idealSize, [&](auto *error) {
        recordError(error);
    });

    cacheLocation.get()[cacheKey] = result ?: NSNull.null;

    if (!result) {
        if (variants.count) {
            // Record an error if the array had values, meaning the likely failure is the images were missing on disk or bad format.
            recordError(createError(error, customLocalizedDescription));
        } else if ((variants && !variants.count) || dictionary[manifestKey]) {
            // Record an error if the key had an array that was empty, or the key had a value of the wrong type.
            recordError(createError(error));
        }

        return nil;
    }

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
        { "F1"_s, @"\uF704" },
        { "F2"_s, @"\uF705" },
        { "F3"_s, @"\uF706" },
        { "F4"_s, @"\uF707" },
        { "F5"_s, @"\uF708" },
        { "F6"_s, @"\uF709" },
        { "F7"_s, @"\uF70A" },
        { "F8"_s, @"\uF70B" },
        { "F9"_s, @"\uF70C" },
        { "F10"_s, @"\uF70D" },
        { "F11"_s, @"\uF70E" },
        { "F12"_s, @"\uF70F" },
        { "Insert"_s, @"\uF727" },
        { "Delete"_s, @"\uF728" },
        { "Home"_s, @"\uF729" },
        { "End"_s, @"\uF72B" },
        { "PageUp"_s, @"\uF72C" },
        { "PageDown"_s, @"\uF72D" },
        { "Up"_s, @"\uF700" },
        { "Down"_s, @"\uF701" },
        { "Left"_s, @"\uF702" },
        { "Right"_s, @"\uF703" }
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

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/commands

    auto *commandsDictionary = objectForKey<NSDictionary>(m_manifest, commandsManifestKey, false, NSDictionary.class);
    if (!commandsDictionary) {
        if (id value = [m_manifest objectForKey:commandsManifestKey]; value && ![value isKindOfClass:NSDictionary.class]) {
            recordError(createError(Error::InvalidCommands));
            return;
        }
    }

    if (id value = [m_manifest objectForKey:commandsManifestKey]; commandsDictionary.count != dynamic_objc_cast<NSDictionary>(value).count) {
        recordError(createError(Error::InvalidCommands));
        return;
    }

    size_t commandsWithShortcuts = 0;
    std::optional<String> error;

    bool hasActionCommand = false;

    for (NSString *commandIdentifier in commandsDictionary) {
        if (!commandIdentifier.length) {
            error = WEB_UI_STRING("Empty or invalid identifier in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command identifier");
            continue;
        }

        auto *commandDictionary = objectForKey<NSDictionary>(commandsDictionary, commandIdentifier);
        if (!commandDictionary.count) {
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
        auto *description = objectForKey<NSString>(commandDictionary, commandsDescriptionKeyManifestKey);
        if (!description.length && !isActionCommand) {
            error = WEB_UI_STRING("Empty or invalid `description` in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command description");
            continue;
        }

        if (isActionCommand && !description.length) {
            description = displayActionLabel();
            if (!description.length)
                description = displayShortName();
        }

        commandData.description = description;

        if (auto *suggestedKeyDictionary = objectForKey<NSDictionary>(commandDictionary, commandsSuggestedKeyManifestKey)) {
            static NSString * const macPlatform = @"mac";
            static NSString * const iosPlatform = @"ios";
            static NSString * const defaultPlatform = @"default";

#if PLATFORM(MAC)
            auto *platformShortcut = objectForKey<NSString>(suggestedKeyDictionary, macPlatform) ?: objectForKey<NSString>(suggestedKeyDictionary, iosPlatform);
#else
            auto *platformShortcut = objectForKey<NSString>(suggestedKeyDictionary, iosPlatform) ?: objectForKey<NSString>(suggestedKeyDictionary, macPlatform);
#endif
            if (!platformShortcut.length)
                platformShortcut = objectForKey<NSString>(suggestedKeyDictionary, defaultPlatform) ?: @"";

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

std::optional<WebExtension::DeclarativeNetRequestRulesetData> WebExtension::parseDeclarativeNetRequestRulesetDictionary(NSDictionary *rulesetDictionary, NSError **error)
{
    NSArray *requiredKeysInRulesetDictionary = @[
        declarativeNetRequestRulesetIDManifestKey,
        declarativeNetRequestRuleEnabledManifestKey,
        declarativeNetRequestRulePathManifestKey,
    ];

    NSDictionary *keyToExpectedValueTypeInRulesetDictionary = @{
        declarativeNetRequestRulesetIDManifestKey: NSString.class,
        declarativeNetRequestRuleEnabledManifestKey: @YES.class,
        declarativeNetRequestRulePathManifestKey: NSString.class,
    };

    NSString *exceptionString;
    bool isRulesetDictionaryValid = validateDictionary(rulesetDictionary, nil, requiredKeysInRulesetDictionary, keyToExpectedValueTypeInRulesetDictionary, &exceptionString);
    if (!isRulesetDictionaryValid) {
        *error = createError(WebExtension::Error::InvalidDeclarativeNetRequest, exceptionString);
        return std::nullopt;
    }

    NSString *rulesetID = objectForKey<NSString>(rulesetDictionary, declarativeNetRequestRulesetIDManifestKey);
    if (!rulesetID.length) {
        *error = createError(WebExtension::Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Empty `declarative_net_request` ruleset id.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for empty ruleset id"));
        return std::nullopt;
    }

    NSString *jsonPath = objectForKey<NSString>(rulesetDictionary, declarativeNetRequestRulePathManifestKey);
    if (!jsonPath.length) {
        *error = createError(WebExtension::Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Empty `declarative_net_request` JSON path.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for empty JSON path"));
        return std::nullopt;

    }

    DeclarativeNetRequestRulesetData rulesetData = {
        rulesetID,
        (bool)objectForKey<NSNumber>(rulesetDictionary, declarativeNetRequestRuleEnabledManifestKey).boolValue,
        jsonPath
    };

    return std::optional { WTFMove(rulesetData) };
}

void WebExtension::populateDeclarativeNetRequestPropertiesIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestDeclarativeNetRequestRulesets)
        return;

    m_parsedManifestDeclarativeNetRequestRulesets = true;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/declarative_net_request

    if (!supportedPermissions().contains(WKWebExtensionPermissionDeclarativeNetRequest) && !supportedPermissions().contains(WKWebExtensionPermissionDeclarativeNetRequestWithHostAccess)) {
        recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Manifest has no `declarativeNetRequest` permission.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for missing declarativeNetRequest permission")));
        return;
    }

    auto *declarativeNetRequestManifestDictionary = objectForKey<NSDictionary>(m_manifest, declarativeNetRequestManifestKey);
    if (!declarativeNetRequestManifestDictionary) {
        if ([m_manifest objectForKey:declarativeNetRequestManifestKey])
            recordError(createError(Error::InvalidDeclarativeNetRequest));
        return;
    }

    NSArray<NSDictionary *> *declarativeNetRequestRulesets = objectForKey<NSArray>(declarativeNetRequestManifestDictionary, declarativeNetRequestRulesManifestKey, false, NSDictionary.class);
    if (!declarativeNetRequestRulesets) {
        if ([m_manifest objectForKey:declarativeNetRequestManifestKey])
            recordError(createError(Error::InvalidDeclarativeNetRequest));
        return;
    }

    if (declarativeNetRequestRulesets.count > webExtensionDeclarativeNetRequestMaximumNumberOfStaticRulesets)
        recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Exceeded maximum number of `declarative_net_request` rulesets. Ignoring extra rulesets.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for too many rulesets")));

    NSUInteger rulesetCount = 0;
    NSUInteger enabledRulesetCount = 0;
    bool recordedTooManyRulesetsManifestError = false;
    HashSet<String> seenRulesetIDs;
    for (NSDictionary *rulesetDictionary in declarativeNetRequestRulesets) {
        if (rulesetCount >= webExtensionDeclarativeNetRequestMaximumNumberOfStaticRulesets)
            continue;

        NSError *error;
        auto optionalRuleset = parseDeclarativeNetRequestRulesetDictionary(rulesetDictionary, &error);
        if (!optionalRuleset) {
            recordError(createError(Error::InvalidDeclarativeNetRequest, { }, error));
            continue;
        }

        auto ruleset = optionalRuleset.value();
        if (seenRulesetIDs.contains(ruleset.rulesetID)) {
            recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_FORMAT_STRING("`declarative_net_request` ruleset with id \"%@\" is invalid. Ruleset id must be unique.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for duplicate ruleset id", (NSString *)ruleset.rulesetID)));
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

    return std::nullopt;
}

const WebExtension::PermissionsSet& WebExtension::supportedPermissions()
{
    static MainThreadNeverDestroyed<PermissionsSet> permissions = std::initializer_list<String> { WKWebExtensionPermissionActiveTab, WKWebExtensionPermissionAlarms, WKWebExtensionPermissionClipboardWrite,
        WKWebExtensionPermissionContextMenus, WKWebExtensionPermissionCookies, WKWebExtensionPermissionDeclarativeNetRequest, WKWebExtensionPermissionDeclarativeNetRequestFeedback,
        WKWebExtensionPermissionDeclarativeNetRequestWithHostAccess, WKWebExtensionPermissionMenus, WKWebExtensionPermissionNativeMessaging, WKWebExtensionPermissionNotifications, WKWebExtensionPermissionScripting,
        WKWebExtensionPermissionStorage, WKWebExtensionPermissionTabs, WKWebExtensionPermissionUnlimitedStorage, WKWebExtensionPermissionWebNavigation, WKWebExtensionPermissionWebRequest,
#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
        WKWebExtensionPermissionSidePanel,
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

    m_parsedManifestPermissionProperties = YES;

    bool findMatchPatternsInPermissions = !supportsManifestVersion(3);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/permissions

    NSArray<NSString *> *permissions = objectForKey<NSArray>(m_manifest, permissionsManifestKey, true, NSString.class);
    for (NSString *permission in permissions) {
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
        NSArray<NSString *> *hostPermissions = objectForKey<NSArray>(m_manifest, hostPermissionsManifestKey, true, NSString.class);

        for (NSString *hostPattern in hostPermissions) {
            if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(hostPattern)) {
                if (matchPattern->isSupported())
                    m_permissionMatchPatterns.add(matchPattern.releaseNonNull());
            }
        }
    }

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/optional_permissions

    NSArray<NSString *> *optionalPermissions = objectForKey<NSArray>(m_manifest, optionalPermissionsManifestKey, true, NSString.class);
    for (NSString *optionalPermission in optionalPermissions) {
        if (findMatchPatternsInPermissions) {
            if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(optionalPermission)) {
                if (matchPattern->isSupported() && !m_permissionMatchPatterns.contains(*matchPattern))
                    m_optionalPermissionMatchPatterns.add(matchPattern.releaseNonNull());
                continue;
            }
        }

        if (!m_permissions.contains(optionalPermission) && supportedPermissions().contains(optionalPermission))
            m_optionalPermissions.add(optionalPermission);
    }

    // Documentation: https://github.com/w3c/webextensions/issues/119

    if (!findMatchPatternsInPermissions) {
        NSArray<NSString *> *hostPermissions = objectForKey<NSArray>(m_manifest, optionalHostPermissionsManifestKey, true, NSString.class);

        for (NSString *hostPattern in hostPermissions) {
            if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(hostPattern)) {
                if (matchPattern->isSupported() && !m_permissionMatchPatterns.contains(*matchPattern))
                    m_optionalPermissionMatchPatterns.add(matchPattern.releaseNonNull());
            }
        }
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
