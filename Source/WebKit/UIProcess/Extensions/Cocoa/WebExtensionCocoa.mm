/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
#import "WebExtensionDeclarativeNetRequestConstants.h"
#import "WebExtensionUtilities.h"
#import "_WKWebExtensionInternal.h"
#import "_WKWebExtensionLocalization.h"
#import "_WKWebExtensionPermission.h"
#import <CoreFoundation/CFBundle.h>
#import <UniformTypeIdentifiers/UTCoreTypes.h>
#import <UniformTypeIdentifiers/UTType.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/BlockPtr.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(MAC)
#import <pal/spi/mac/NSImageSPI.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
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

static NSString * const manifestVersionManifestKey = @"manifest_version";

static NSString * const nameManifestKey = @"name";
static NSString * const shortNameManifestKey = @"short_name";
static NSString * const versionManifestKey = @"version";
static NSString * const versionNameManifestKey = @"version_name";
static NSString * const descriptionManifestKey = @"description";

static NSString * const iconsManifestKey = @"icons";

static NSString * const actionManifestKey = @"action";
static NSString * const browserActionManifestKey = @"browser_action";
static NSString * const pageActionManifestKey = @"page_action";

static NSString * const defaultIconManifestKey = @"default_icon";
static NSString * const defaultLocaleManifestKey = @"default_locale";
static NSString * const defaultTitleManifestKey = @"default_title";
static NSString * const defaultPopupManifestKey = @"default_popup";

static NSString * const backgroundManifestKey = @"background";
static NSString * const backgroundPageManifestKey = @"page";
static NSString * const backgroundServiceWorkerManifestKey = @"service_worker";
static NSString * const backgroundScriptsManifestKey = @"scripts";
static NSString * const backgroundPersistentManifestKey = @"persistent";
static NSString * const backgroundPageTypeKey = @"type";
static NSString * const backgroundPageTypeModuleValue = @"module";

static NSString * const generatedBackgroundPageFilename = @"_generated_background_page.html";

static NSString * const contentScriptsManifestKey = @"content_scripts";
static NSString * const contentScriptsMatchesManifestKey = @"matches";
static NSString * const contentScriptsExcludeMatchesManifestKey = @"exclude_matches";
static NSString * const contentScriptsIncludeGlobsManifestKey = @"include_globs";
static NSString * const contentScriptsExcludeGlobsManifestKey = @"exclude_globs";
static NSString * const contentScriptsMatchesAboutBlankManifestKey = @"match_about_blank";
static NSString * const contentScriptsRunAtManifestKey = @"run_at";
static NSString * const contentScriptsDocumentIdleManifestKey = @"document_idle";
static NSString * const contentScriptsDocumentStartManifestKey = @"document_start";
static NSString * const contentScriptsDocumentEndManifestKey = @"document_end";
static NSString * const contentScriptsAllFramesManifestKey = @"all_frames";
static NSString * const contentScriptsJSManifestKey = @"js";
static NSString * const contentScriptsCSSManifestKey = @"css";

static NSString * const permissionsManifestKey = @"permissions";
static NSString * const optionalPermissionsManifestKey = @"optional_permissions";
static NSString * const hostPermissionsManifestKey = @"host_permissions";
static NSString * const optionalHostPermissionsManifestKey = @"optional_host_permissions";

static NSString * const optionsUIManifestKey = @"options_ui";
static NSString * const optionsUIPageManifestKey = @"page";
static NSString * const optionsPageManifestKey = @"options_page";
static NSString * const chromeURLOverridesManifestKey = @"chrome_url_overrides";
static NSString * const browserURLOverridesManifestKey = @"browser_url_overrides";
static NSString * const newTabManifestKey = @"newtab";

static NSString * const contentSecurityPolicyManifestKey = @"content_security_policy";
static NSString * const contentSecurityPolicyExtensionPagesManifestKey = @"extension_pages";

static NSString * const commandsManifestKey = @"commands";
static NSString * const commandsSuggestedKeyManifestKey = @"suggested_key";
static NSString * const commandsDescriptionKeyManifestKey = @"description";

static NSString * const webAccessibleResourcesManifestKey = @"web_accessible_resources";
static NSString * const webAccessibleResourcesResourcesManifestKey = @"resources";
static NSString * const webAccessibleResourcesMatchesManifestKey = @"matches";

static NSString * const declarativeNetRequestManifestKey = @"declarative_net_request";
static NSString * const declarativeNetRequestRulesManifestKey = @"rule_resources";
static NSString * const declarativeNetRequestRulesetIDManifestKey = @"id";
static NSString * const declarativeNetRequestRuleEnabledManifestKey = @"enabled";
static NSString * const declarativeNetRequestRulePathManifestKey = @"path";

static const size_t maximumNumberOfShortcutCommands = 4;

WebExtension::WebExtension(NSBundle *appExtensionBundle, NSError **outError)
    : m_bundle(appExtensionBundle)
    , m_resourceBaseURL(appExtensionBundle.resourceURL.URLByStandardizingPath.absoluteURL)
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
    : m_resources([resources mutableCopy] ?: [NSMutableDictionary dictionary])
{
    ASSERT(manifest);

    NSData *manifestData = encodeJSONData(manifest);
    RELEASE_ASSERT(manifestData);

    [m_resources setObject:manifestData forKey:@"manifest.json"];
}

WebExtension::WebExtension(NSDictionary *resources)
    : m_resources([resources mutableCopy] ?: [NSMutableDictionary dictionary])
{
}

bool WebExtension::manifestParsedSuccessfully()
{
    if (m_parsedManifest)
        return !!m_manifest;
    // If we haven't parsed yet, trigger a parse by calling the getter.
    return !!manifest();
}

bool WebExtension::parseManifest(NSData *manifestData)
{
    NSError *parseError;
    m_manifest = parseJSON(manifestData, { }, &parseError);
    if (!m_manifest) {
        recordError(createError(Error::InvalidManifest, nil, parseError));
        return false;
    }

    return true;
}

NSDictionary *WebExtension::manifest()
{
    if (m_parsedManifest)
        return m_manifest.get();

    m_parsedManifest = true;

    NSData *manifestData = resourceDataForPath(@"manifest.json");
    if (!manifestData)
        return nil;

    if (!parseManifest(manifestData))
        return nil;

    NSString *defaultLocale = [m_manifest objectForKey:defaultLocaleManifestKey];
    m_defaultLocale = [NSLocale localeWithLocaleIdentifier:defaultLocale];

    m_localization = [[_WKWebExtensionLocalization alloc] initWithWebExtension:*this];

    m_manifest = [m_localization.get() localizedDictionaryForDictionary:m_manifest.get()];
    ASSERT(m_manifest);

    // FIXME: Handle Safari version compatibility check.
    // Likely do this version checking when the extension is added to the WKWebExtensionController,
    // since that will need delegated to the app.

    return m_manifest.get();
}

Ref<API::Data> WebExtension::serializeManifest()
{
    return API::Data::createWithoutCopying(encodeJSONData(manifest()));
}

double WebExtension::manifestVersion()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/manifest_version

    return objectForKey<NSNumber>(manifest(), manifestVersionManifestKey).doubleValue;
}

Ref<API::Data> WebExtension::serializeLocalization()
{
    return API::Data::createWithoutCopying(encodeJSONData(m_localization.get().localizationDictionary));
}

#if PLATFORM(MAC)

SecStaticCodeRef WebExtension::bundleStaticCode()
{
#if USE(APPLE_INTERNAL_SDK)
    if (!m_bundle)
        return nullptr;

    if (m_bundleStaticCode)
        return m_bundleStaticCode.get();

    SecStaticCodeRef staticCodeRef;
    OSStatus error = SecStaticCodeCreateWithPath((__bridge CFURLRef)m_bundle.get().bundleURL, kSecCSDefaultFlags, &staticCodeRef);
    if (error != noErr || !staticCodeRef) {
        if (staticCodeRef)
            CFRelease(staticCodeRef);
        return nullptr;
    }

    m_bundleStaticCode = adoptCF(staticCodeRef);

    return m_bundleStaticCode.get();
#else
    return nullptr;
#endif
}

bool WebExtension::validateResourceData(NSURL *resourceURL, NSData *resourceData, NSError **outError)
{
    ASSERT([resourceURL isFileURL]);
    ASSERT(resourceData);

#if USE(APPLE_INTERNAL_SDK)
    if (!m_bundle)
        return true;

    if (!bundleStaticCode())
        return false;

    NSURL *bundleSupportFilesURL = CFBridgingRelease(CFBundleCopySupportFilesDirectoryURL(m_bundle.get()._cfBundle));
    NSString *bundleSupportFilesURLString = bundleSupportFilesURL.absoluteString;
    NSString *resourceURLString = resourceURL.absoluteString;
    ASSERT([resourceURLString hasPrefix:bundleSupportFilesURLString]);

    NSString *relativePathToResource = [resourceURLString substringFromIndex:bundleSupportFilesURLString.length].stringByRemovingPercentEncoding;
    OSStatus result = SecCodeValidateFileResource(bundleStaticCode(), (__bridge CFStringRef)relativePathToResource, (__bridge CFDataRef)resourceData, kSecCSDefaultFlags);

    if (outError && result != noErr)
        *outError = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];

    return result == noErr;
#else
    return true;
#endif
}

#endif // PLATFORM(MAC)

bool WebExtension::isWebAccessibleResource(const URL& resourceURL, const URL& pageURL)
{
    populateWebAccessibleResourcesIfNeeded();

    auto resourcePath = resourceURL.path().toString();

    // The path is expected to match without the prefix slash.
    ASSERT(resourcePath.startsWith('/'));
    resourcePath = resourcePath.substring(1);

    for (auto& data : m_webAccessibleResources) {
        bool allowed = false;
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

void WebExtension::populateWebAccessibleResourcesIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestWebAccessibleResources)
        return;

    m_parsedManifestWebAccessibleResources = true;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/web_accessible_resources

    if (supportsManifestVersion(3)) {
        if (auto *resourcesArray = objectForKey<NSArray>(m_manifest, webAccessibleResourcesManifestKey, false, NSDictionary.class)) {
            bool errorOccurred = false;
            for (NSDictionary *resourcesDictionary in resourcesArray) {
                auto *pathsArray = objectForKey<NSArray>(resourcesDictionary, webAccessibleResourcesResourcesManifestKey, false, NSString.class);
                auto *matchesArray = objectForKey<NSArray>(resourcesDictionary, webAccessibleResourcesMatchesManifestKey, false, NSString.class);

                pathsArray = filterObjects(pathsArray, ^(id key, NSString *string) {
                    return !!string.length;
                });

                matchesArray = filterObjects(matchesArray, ^(id key, NSString *string) {
                    return !!string.length;
                });

                if (!pathsArray || !matchesArray) {
                    errorOccurred = true;
                    continue;
                }

                if (!pathsArray.count || !matchesArray.count)
                    continue;

                MatchPatternSet matchPatterns;
                for (NSString *matchPatternString in matchesArray) {
                    if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(matchPatternString)) {
                        if (matchPattern->isSupported())
                            matchPatterns.add(matchPattern.releaseNonNull());
                        else
                            errorOccurred = true;
                    }
                }

                if (matchPatterns.isEmpty()) {
                    errorOccurred = true;
                    continue;
                }

                m_webAccessibleResources.append({ WTFMove(matchPatterns), makeVector<String>(pathsArray) });
            }

            if (errorOccurred)
                recordError(createError(Error::InvalidWebAccessibleResources));
        } else if ([m_manifest objectForKey:webAccessibleResourcesManifestKey])
            recordError(createError(Error::InvalidWebAccessibleResources));
    } else {
        if (auto *resourcesArray = objectForKey<NSArray>(m_manifest, webAccessibleResourcesManifestKey, false, NSString.class)) {
            resourcesArray = filterObjects(resourcesArray, ^(id key, NSString *string) {
                return !!string.length;
            });

            if (resourcesArray.count) {
                MatchPatternSet matchPatterns { WebExtensionMatchPattern::allHostsAndSchemesMatchPattern() };
                m_webAccessibleResources.append({ WTFMove(matchPatterns), makeVector<String>(resourcesArray) });
            }
        } else if ([m_manifest objectForKey:webAccessibleResourcesManifestKey])
            recordError(createError(Error::InvalidWebAccessibleResources));
    }
}

NSURL *WebExtension::resourceFileURLForPath(NSString *path)
{
    ASSERT(path);

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
    } else {
        auto *fileURL = resourceFileURLForPath(path);
        [fileURL getResourceValue:&result forKey:NSURLContentTypeKey error:nil];
    }

    return result;
}

NSString *WebExtension::resourceStringForPath(NSString *path, CacheResult cacheResult, SuppressNotFoundErrors suppressErrors)
{
    ASSERT(path);

    // Remove leading slash to normalize the path for lookup/storage in the cache dictionary.
    if ([path hasPrefix:@"/"])
        path = [path substringFromIndex:1];

    if (NSString *cachedString = objectForKey<NSString>(m_resources, path))
        return cachedString;

    if ([path isEqualToString:generatedBackgroundPageFilename])
        return generatedBackgroundContent();

    NSData *data = resourceDataForPath(path, CacheResult::No, suppressErrors);

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

NSData *WebExtension::resourceDataForPath(NSString *path, CacheResult cacheResult, SuppressNotFoundErrors suppressErrors)
{
    ASSERT(path);

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

    if (NSData *cachedData = objectForKey<NSData>(m_resources, path))
        return cachedData;

    if (NSString *cachedString = objectForKey<NSString>(m_resources, path))
        return [cachedString dataUsingEncoding:NSUTF8StringEncoding];

    if (NSDictionary *cachedDictionary = objectForKey<NSDictionary>(m_resources, path))
        return encodeJSONData(cachedDictionary);

    if ([path isEqualToString:generatedBackgroundPageFilename])
        return [generatedBackgroundContent() dataUsingEncoding:NSUTF8StringEncoding];

    NSURL *resourceURL = resourceFileURLForPath(path);
    if (!resourceURL) {
        if (suppressErrors == SuppressNotFoundErrors::No)
            recordError(createError(Error::ResourceNotFound, WEB_UI_FORMAT_CFSTRING("Unable to find \"%@\" in the extension’s resources. It is an invalid path.", "WKWebExtensionErrorResourceNotFound description with invalid file path", (__bridge CFStringRef)path)));
        return nil;
    }

    NSError *fileReadError;
    NSData *resultData = [NSData dataWithContentsOfURL:resourceURL options:NSDataReadingMappedIfSafe error:&fileReadError];
    if (!resultData) {
        if (suppressErrors == SuppressNotFoundErrors::No)
            recordError(createError(Error::ResourceNotFound, WEB_UI_FORMAT_CFSTRING("Unable to find \"%@\" in the extension’s resources.", "WKWebExtensionErrorResourceNotFound description with file name", (__bridge CFStringRef)path), fileReadError));
        return nil;
    }

#if PLATFORM(MAC)
    NSError *validationError;
    if (!validateResourceData(resourceURL, resultData, &validationError)) {
        recordError(createError(Error::InvalidResourceCodeSignature, WEB_UI_FORMAT_CFSTRING("Unable to validate \"%@\" with the extension’s code signature. It likely has been modified since the extension was built.", "WKWebExtensionErrorInvalidResourceCodeSignature description with file name", (__bridge CFStringRef)path), validationError));
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

bool WebExtension::hasRequestedPermission(NSString *permission) const
{
    return m_permissions.contains(permission);
}

static _WKWebExtensionError toAPI(WebExtension::Error error)
{
    switch (error) {
    case WebExtension::Error::Unknown:
        return _WKWebExtensionErrorUnknown;
    case WebExtension::Error::ResourceNotFound:
        return _WKWebExtensionErrorResourceNotFound;
    case WebExtension::Error::InvalidManifest:
        return _WKWebExtensionErrorInvalidManifest;
    case WebExtension::Error::UnsupportedManifestVersion:
        return _WKWebExtensionErrorUnsupportedManifestVersion;
    case WebExtension::Error::InvalidAction:
        return _WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidActionIcon:
        return _WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidBackgroundContent:
        return _WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidCommands:
        return _WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidContentScripts:
        return _WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidContentSecurityPolicy:
        return _WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidDeclarativeNetRequest:
        return _WKWebExtensionErrorInvalidDeclarativeNetRequestEntry;
    case WebExtension::Error::InvalidDescription:
        return _WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidExternallyConnectable:
        return _WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidIcon:
        return _WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidName:
        return _WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidOptionsPage:
        return _WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidURLOverrides:
        return _WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidVersion:
        return _WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidWebAccessibleResources:
        return _WKWebExtensionErrorInvalidManifestEntry;
    case WebExtension::Error::InvalidBackgroundPersistence:
        return _WKWebExtensionErrorInvalidBackgroundPersistence;
    case WebExtension::Error::BackgroundContentFailedToLoad:
        return _WKWebExtensionErrorBackgroundContentFailedToLoad;
    case WebExtension::Error::InvalidResourceCodeSignature:
        return _WKWebExtensionErrorInvalidResourceCodeSignature;
    }
}

NSError *WebExtension::createError(Error error, NSString *customLocalizedDescription, NSError *underlyingError)
{
    auto errorCode = toAPI(error);
    NSString *localizedDescription;

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

    case Error::BackgroundContentFailedToLoad:
        localizedDescription = WEB_UI_STRING("The background content failed to load due to an error.", "WKWebExtensionErrorBackgroundContentFailedToLoad description");
        break;

    case Error::InvalidResourceCodeSignature:
        ASSERT(customLocalizedDescription);
        break;
    }

    if (customLocalizedDescription.length)
        localizedDescription = customLocalizedDescription;

    NSDictionary *userInfo;
    if (underlyingError)
        userInfo = @{ NSLocalizedDescriptionKey: localizedDescription, NSUnderlyingErrorKey: underlyingError };
    else
        userInfo = @{ NSLocalizedDescriptionKey: localizedDescription };

    return [[NSError alloc] initWithDomain:_WKWebExtensionErrorDomain code:errorCode userInfo:userInfo];
}

void WebExtension::removeError(Error error, SuppressNotification suppressNotification)
{
    if (!m_errors)
        return;

    auto errorCode = toAPI(error);

    NSIndexSet *indexes = [m_errors indexesOfObjectsPassingTest:^BOOL(NSError *error, NSUInteger, BOOL *) {
        return error.code == errorCode;
    }];

    if (!indexes.count)
        return;

    [m_errors removeObjectsAtIndexes:indexes];

    if (suppressNotification == SuppressNotification::Yes)
        return;

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }]() {
        [NSNotificationCenter.defaultCenter postNotificationName:_WKWebExtensionErrorsWereUpdatedNotification object:wrapper() userInfo:nil];
    }).get());
}

void WebExtension::recordError(NSError *error, SuppressNotification suppressNotification)
{
    ASSERT(error);

    if (!m_errors)
        m_errors = [NSMutableArray array];

    RELEASE_LOG_ERROR(Extensions, "Error recorded: %{private}@", error);

    [m_errors addObject:error];

    if (suppressNotification == SuppressNotification::Yes)
        return;

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }]() {
        [NSNotificationCenter.defaultCenter postNotificationName:_WKWebExtensionErrorsWereUpdatedNotification object:wrapper() userInfo:nil];
    }).get());
}

NSArray *WebExtension::errors()
{
    // FIXME: <https://webkit.org/b/246493> Include runtime errors.

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

NSString *WebExtension::displayName()
{
    populateDisplayStringsIfNeeded();
    return m_displayName.get();
}

NSString *WebExtension::displayShortName()
{
    populateDisplayStringsIfNeeded();
    return m_displayShortName.get();
}

NSString *WebExtension::displayVersion()
{
    populateDisplayStringsIfNeeded();
    return m_displayVersion.get();
}

NSString *WebExtension::displayDescription()
{
    populateDisplayStringsIfNeeded();
    return m_displayDescription.get();
}

NSString *WebExtension::version()
{
    populateDisplayStringsIfNeeded();
    return m_version.get();
}

void WebExtension::populateDisplayStringsIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestDisplayStrings)
        return;

    m_parsedManifestDisplayStrings = true;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/name

    m_displayName = objectForKey<NSString>(m_manifest, nameManifestKey);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/short_name

    m_displayShortName = objectForKey<NSString>(m_manifest, shortNameManifestKey);
    if (!m_displayShortName)
        m_displayShortName = m_displayName;

    if (!m_displayName)
        recordError(createError(Error::InvalidName));

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/version

    m_version = objectForKey<NSString>(m_manifest, versionManifestKey);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/version_name

    m_displayVersion = objectForKey<NSString>(m_manifest, versionNameManifestKey);
    if (!m_displayVersion)
        m_displayVersion = m_version;

    if (!m_version)
        recordError(createError(Error::InvalidVersion));

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/description

    m_displayDescription = objectForKey<NSString>(m_manifest, descriptionManifestKey);
    if (!m_displayDescription)
        recordError(createError(Error::InvalidDescription));
}

NSString *WebExtension::contentSecurityPolicy()
{
    populateContentSecurityPolicyStringsIfNeeded();
    return m_contentSecurityPolicy.get();
}

void WebExtension::populateContentSecurityPolicyStringsIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestContentSecurityPolicyStrings)
        return;

    m_parsedManifestContentSecurityPolicyStrings = true;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/content_security_policy

    if (supportsManifestVersion(3)) {
        if (auto *policyDictionary = objectForKey<NSDictionary>(m_manifest, contentSecurityPolicyManifestKey, false)) {
            m_contentSecurityPolicy = objectForKey<NSString>(policyDictionary, contentSecurityPolicyExtensionPagesManifestKey);
            if (!m_contentSecurityPolicy && (!policyDictionary.count || policyDictionary[contentSecurityPolicyExtensionPagesManifestKey]))
                recordError(createError(Error::InvalidContentSecurityPolicy));
        }
    } else {
        m_contentSecurityPolicy = objectForKey<NSString>(m_manifest, contentSecurityPolicyManifestKey);
        if (!m_contentSecurityPolicy && [m_manifest objectForKey:contentSecurityPolicyManifestKey])
            recordError(createError(Error::InvalidContentSecurityPolicy));
    }

    if (!m_contentSecurityPolicy)
        m_contentSecurityPolicy = @"script-src 'self'";
}

CocoaImage *WebExtension::icon(CGSize size)
{
    if (!manifestParsedSuccessfully())
        return nil;

    NSString *localizedErrorDescription = WEB_UI_STRING("Failed to load images in `icons` manifest entry.", "WKWebExtensionErrorInvalidIcon description for failing to load images");
    return bestImageForIconsDictionaryManifestKey(m_manifest.get(), iconsManifestKey, size, m_icon, Error::InvalidIcon, localizedErrorDescription);
}

CocoaImage *WebExtension::actionIcon(CGSize size)
{
    if (!manifestParsedSuccessfully())
        return nil;

    populateActionPropertiesIfNeeded();

    NSString *localizedErrorDescription;
    if (supportsManifestVersion(3))
        localizedErrorDescription = WEB_UI_STRING("Failed to load images in `default_icon` for the `action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load images for action only");
    else
        localizedErrorDescription = WEB_UI_STRING("Failed to load images in `default_icon` for the `browser_action` or `page_action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description for failing to load images for browser_action or page_action");

    if (auto *result = bestImageForIconsDictionaryManifestKey(m_actionDictionary.get(), defaultIconManifestKey, size, m_actionIcon, Error::InvalidActionIcon, localizedErrorDescription))
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
    return supportsManifestVersion(3) && objectForKey<NSDictionary>(m_manifest, actionManifestKey);
}

bool WebExtension::hasBrowserAction()
{
    return !supportsManifestVersion(3) && objectForKey<NSDictionary>(m_manifest, browserActionManifestKey);
}

bool WebExtension::hasPageAction()
{
    return !supportsManifestVersion(3) && objectForKey<NSDictionary>(m_manifest, pageActionManifestKey);
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
        m_actionDictionary = objectForKey<NSDictionary>(m_manifest, actionManifestKey);
    else {
        m_actionDictionary = objectForKey<NSDictionary>(m_manifest, browserActionManifestKey);
        if (!m_actionDictionary)
            m_actionDictionary = objectForKey<NSDictionary>(m_manifest, pageActionManifestKey);
    }

    if (!m_actionDictionary)
        return;

    // Look for the "default_icon" as a string, which is useful for SVG icons. Only supported by Firefox currently.
    NSString *defaultIconPath = objectForKey<NSString>(m_actionDictionary, defaultIconManifestKey);
    if (defaultIconPath.length) {
        m_actionIcon = imageForPath(defaultIconPath);

        if (!m_actionIcon) {
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

CocoaImage *WebExtension::imageForPath(NSString *imagePath)
{
    ASSERT(imagePath);

    NSData *imageData = resourceDataForPath(imagePath, CacheResult::Yes);
    if (!imageData)
        return nil;

#if !USE(NSIMAGE_FOR_SVG_SUPPORT)
    UTType *imageType = resourceTypeForPath(imagePath);
    if ([imageType.identifier isEqualToString:UTTypeSVG.identifier]) {
#if USE(APPKIT)
        static Class svgImageRep = NSClassFromString(@"_NSSVGImageRep");
        RELEASE_ASSERT(svgImageRep);

        _NSSVGImageRep *imageRep = [[svgImageRep alloc] initWithData:imageData];
        if (!imageRep)
            return nil;

        NSImage *result = [[NSImage alloc] init];
        [result addRepresentation:imageRep];
        result.size = imageRep.size;

        return result;
#else
        CGSVGDocumentRef document = CGSVGDocumentCreateFromData((__bridge CFDataRef)imageData, nullptr);
        if (!document)
            return nil;

        UIImage *result = [UIImage _imageWithCGSVGDocument:document];
        CGSVGDocumentRelease(document);

        return result;
#endif // not USE(APPKIT)
    }
#endif // !USE(NSIMAGE_FOR_SVG_SUPPORT)

    return [[CocoaImage alloc] initWithData:imageData];
}

NSString *WebExtension::pathForBestImageInIconsDictionary(NSDictionary *iconsDictionary, size_t idealPixelSize)
{
    if (!iconsDictionary.count)
        return nil;

    // Check if the ideal size exists, if so return it.
    NSString *idealSizeString = @(idealPixelSize).stringValue;
    if (NSString *resultPath = iconsDictionary[idealSizeString])
        return resultPath;

    // Check if the ideal retina size exists, if so return it. This will cause downsampling to the ideal size,
    // but it is an even multiplier so it is better than a fractional scaling.
    idealSizeString = @(idealPixelSize * 2).stringValue;
    if (NSString *resultPath = iconsDictionary[idealSizeString])
        return resultPath;

#if PLATFORM(IOS) || PLATFORM(VISION)
    // Check if the ideal 3x retina size exists. This will usually be called on 2x iOS devices when a 3x image might exist.
    // Since the ideal size is likly already 2x, multiply by 1.5x to get the 3x pixel size.
    idealSizeString = @(idealPixelSize * 1.5).stringValue;
    if (NSString *resultPath = iconsDictionary[idealSizeString])
        return resultPath;
#endif

    // Since the ideal size does not exist, sort the keys and return the next largest size. This could cause
    // fractional scaling when the final image is displayed. Better than nothing.
    NSArray<NSString *> *sizeKeys = filterObjects(iconsDictionary.allKeys, ^bool(id, id value) {
        return [value isKindOfClass:NSString.class];
    });

    if (!sizeKeys.count)
        return nil;

    NSSortDescriptor *sortDescriptor = [[NSSortDescriptor alloc] initWithKey:@"self" ascending:YES selector:@selector(localizedStandardCompare:)];
    NSArray<NSString *> *sortedKeys = [sizeKeys sortedArrayUsingDescriptors:@[ sortDescriptor ]];

    size_t bestSize = 0;
    for (NSString *size in sortedKeys) {
        bestSize = size.integerValue;
        if (bestSize >= idealPixelSize)
            break;
    }

    ASSERT(bestSize);

    return iconsDictionary[@(bestSize).stringValue];
}

CocoaImage *WebExtension::bestImageInIconsDictionary(NSDictionary *iconsDictionary, size_t idealPointSize)
{
    if (!iconsDictionary.count)
        return nil;

    NSUInteger standardPixelSize = idealPointSize;
#if PLATFORM(IOS) || PLATFORM(VISION)
    standardPixelSize *= UIScreen.mainScreen.scale;
#endif

    NSString *standardIconPath = pathForBestImageInIconsDictionary(iconsDictionary, standardPixelSize);
    if (!standardIconPath.length)
        return nil;

    CocoaImage *resultImage = imageForPath(standardIconPath);
    if (!resultImage)
        return nil;

#if PLATFORM(MAC)
    static Class svgImageRep = NSClassFromString(@"_NSSVGImageRep");
    RELEASE_ASSERT(svgImageRep);

    BOOL isVectorImage = [resultImage.representations indexOfObjectPassingTest:^BOOL(NSImageRep *imageRep, NSUInteger, BOOL *) {
        return [imageRep isKindOfClass:svgImageRep];
    }] != NSNotFound;

    if (isVectorImage)
        return resultImage;

    NSUInteger retinaPixelSize = standardPixelSize * 2;
    NSString *retinaIconPath = pathForBestImageInIconsDictionary(iconsDictionary, retinaPixelSize);
    if (retinaIconPath.length && ![retinaIconPath isEqualToString:standardIconPath]) {
        if (CocoaImage *retinaImage = imageForPath(retinaIconPath))
            [resultImage addRepresentations:retinaImage.representations];
    }
#endif // PLATFORM(MAC)

    return resultImage;
}

CocoaImage *WebExtension::bestImageForIconsDictionaryManifestKey(NSDictionary *dictionary, NSString *manifestKey, CGSize idealSize, RetainPtr<CocoaImage>& cacheLocation, Error error, NSString *customLocalizedDescription)
{
    if (cacheLocation)
        return cacheLocation.get();

    CGFloat idealPointSize = idealSize.width > idealSize.height ? idealSize.width : idealSize.height;
    NSDictionary *iconDictionary = objectForKey<NSDictionary>(dictionary, manifestKey);
    CocoaImage *result = bestImageInIconsDictionary(iconDictionary, idealPointSize);
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

    // Cache the icon if there is only one size, usually this is a vector icon or a large bitmap.
    if (iconDictionary.count == 1)
        cacheLocation = result;

    return result;
}

bool WebExtension::hasBackgroundContent()
{
    populateBackgroundPropertiesIfNeeded();
    return m_backgroundScriptPaths.get().count || m_backgroundPagePath || m_backgroundServiceWorkerPath;
}

bool WebExtension::backgroundContentIsPersistent()
{
    populateBackgroundPropertiesIfNeeded();
    return hasBackgroundContent() && m_backgroundContentIsPersistent;
}

bool WebExtension::backgroundContentUsesModules()
{
    populateBackgroundPropertiesIfNeeded();
    return hasBackgroundContent() && m_backgroundPageUsesModules;
}

bool WebExtension::backgroundContentIsServiceWorker()
{
    populateBackgroundPropertiesIfNeeded();
    return !!m_backgroundServiceWorkerPath;
}

NSString *WebExtension::backgroundContentPath()
{
    populateBackgroundPropertiesIfNeeded();

    if (m_backgroundServiceWorkerPath)
        return m_backgroundServiceWorkerPath.get();

    if (m_backgroundScriptPaths.get().count)
        return generatedBackgroundPageFilename;

    if (m_backgroundPagePath)
        return m_backgroundPagePath.get();

    ASSERT_NOT_REACHED();
    return nil;
}

NSString *WebExtension::generatedBackgroundContent()
{
    if (m_generatedBackgroundContent)
        return m_generatedBackgroundContent.get();

    populateBackgroundPropertiesIfNeeded();

    if (m_backgroundServiceWorkerPath)
        return nil;

    if (!m_backgroundScriptPaths.get().count)
        return nil;

    NSArray<NSString *> *scriptTagsArray = mapObjects(m_backgroundScriptPaths, ^(NSNumber *index, NSString *scriptPath) {
        if (backgroundContentUsesModules())
            return [NSString stringWithFormat:@"<script type=\"module\" src=\"%@\"></script>", scriptPath];

        return [NSString stringWithFormat:@"<script src=\"%@\"></script>", scriptPath];
    });

    m_generatedBackgroundContent = [NSString stringWithFormat:@"<!DOCTYPE html>\n<body>\n%@\n</body>", [scriptTagsArray componentsJoinedByString:@"\n"]];

    return m_generatedBackgroundContent.get();
}

void WebExtension::populateBackgroundPropertiesIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestBackgroundProperties)
        return;

    m_parsedManifestBackgroundProperties = true;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/background

    NSDictionary<NSString *, id> *backgroundManifestDictionary = objectForKey<NSDictionary>(m_manifest, backgroundManifestKey);
    if (!backgroundManifestDictionary.count) {
        if ([m_manifest objectForKey:backgroundManifestKey])
            recordError(createError(Error::InvalidBackgroundContent));
        return;
    }

    m_backgroundScriptPaths = objectForKey<NSArray>(backgroundManifestDictionary, backgroundScriptsManifestKey, true, NSString.class);
    m_backgroundPagePath = objectForKey<NSString>(backgroundManifestDictionary, backgroundPageManifestKey);
    m_backgroundServiceWorkerPath = objectForKey<NSString>(backgroundManifestDictionary, backgroundServiceWorkerManifestKey);
    m_backgroundPageUsesModules = [objectForKey<NSString>(backgroundManifestDictionary, backgroundPageTypeKey) isEqualToString:backgroundPageTypeModuleValue];

    m_backgroundScriptPaths = filterObjects(m_backgroundScriptPaths, ^(NSNumber *index, NSString *scriptPath) {
        return !!scriptPath.length;
    });

    // Scripts takes precedence over page.
    if (m_backgroundScriptPaths.get().count)
        m_backgroundPagePath = nil;

    // Service Worker takes precedence over page and scripts.
    if (m_backgroundServiceWorkerPath) {
        m_backgroundScriptPaths = nil;
        m_backgroundPagePath = nil;
    }

    if (!m_backgroundScriptPaths.get().count && !m_backgroundPagePath && !m_backgroundServiceWorkerPath)
        recordError(createError(Error::InvalidBackgroundContent, WEB_UI_STRING("Manifest `background` entry has missing or empty required key `scripts`, `page`, or `service_worker`.", "WKWebExtensionErrorInvalidBackgroundContent description for missing background required keys")));

    NSNumber *persistentBoolean = objectForKey<NSNumber>(backgroundManifestDictionary, backgroundPersistentManifestKey);
    m_backgroundContentIsPersistent = persistentBoolean ? persistentBoolean.boolValue : !(supportsManifestVersion(3) || m_backgroundServiceWorkerPath);

    if (m_backgroundContentIsPersistent && supportsManifestVersion(3)) {
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Invalid `persistent` manifest entry. A `manifest_version` greater-than or equal to `3` must be non-persistent.", "WKWebExtensionErrorInvalidBackgroundPersistence description for manifest v3")));
        m_backgroundContentIsPersistent = false;
    }

    if (m_backgroundContentIsPersistent && m_backgroundServiceWorkerPath) {
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Invalid `persistent` manifest entry. A `service_worker` must be non-persistent.", "WKWebExtensionErrorInvalidBackgroundPersistence description for service worker")));
        m_backgroundContentIsPersistent = false;
    }

    if (!m_backgroundContentIsPersistent && hasRequestedPermission(_WKWebExtensionPermissionWebRequest))
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Non-persistent background content cannot listen to `webRequest` events.", "WKWebExtensionErrorInvalidBackgroundPersistence description for webRequest events")));

#if PLATFORM(IOS) || PLATFORM(VISION)
    if (m_backgroundContentIsPersistent)
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Invalid `persistent` manifest entry. A non-persistent background is required on iOS and iPadOS.", "WKWebExtensionErrorInvalidBackgroundPersistence description for iOS")));
#endif
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

NSString *WebExtension::optionsPagePath()
{
    populatePagePropertiesIfNeeded();
    return m_optionsPagePath.get();
}

NSString *WebExtension::overrideNewTabPagePath()
{
    populatePagePropertiesIfNeeded();
    return m_overrideNewTabPagePath.get();
}

void WebExtension::populatePagePropertiesIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestPageProperties)
        return;

    m_parsedManifestPageProperties = true;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/options_ui
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/options_page
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/chrome_url_overrides

    if (auto *optionsDictionary = objectForKey<NSDictionary>(m_manifest, optionsUIManifestKey, false)) {
        m_optionsPagePath = objectForKey<NSString>(optionsDictionary, optionsUIPageManifestKey);
        if (!m_optionsPagePath)
            recordError(createError(Error::InvalidOptionsPage));
    } else {
        m_optionsPagePath = objectForKey<NSString>(m_manifest, optionsPageManifestKey);
        if (!m_optionsPagePath && [m_manifest objectForKey:optionsPageManifestKey])
            recordError(createError(Error::InvalidOptionsPage));
    }

    auto *overridesDictionary = objectForKey<NSDictionary>(m_manifest, browserURLOverridesManifestKey, false);
    if (!overridesDictionary)
        overridesDictionary = objectForKey<NSDictionary>(m_manifest, chromeURLOverridesManifestKey, false);

    if (overridesDictionary && !overridesDictionary.count)
        recordError(createError(Error::InvalidURLOverrides));

    m_overrideNewTabPagePath = objectForKey<NSString>(overridesDictionary, newTabManifestKey);
    if (!m_overrideNewTabPagePath && overridesDictionary[newTabManifestKey])
        recordError(createError(Error::InvalidURLOverrides, WEB_UI_STRING("Empty or invalid `newtab` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid new tab entry")));
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
        if (id value = [m_manifest objectForKey:commandsManifestKey]; value && ![value isKindOfClass:NSDictionary.class])
            recordError(createError(Error::InvalidCommands));
        return;
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

        if (!hasActionCommand) {
            if (supportsManifestVersion(3) && commandData.identifier == "_execute_action"_s)
                hasActionCommand = true;
            else if (!supportsManifestVersion(3) && (commandData.identifier == "_execute_browser_action"_s || commandData.identifier == "_execute_page_action"_s))
                hasActionCommand = true;
        }

        auto *description = objectForKey<NSString>(commandDictionary, commandsDescriptionKeyManifestKey);
        if (!description.length) {
            error = WEB_UI_STRING("Empty or invalid `description` in the `commands` manifest entry.", "WKWebExtensionErrorInvalidManifestEntry description for invalid command description");
            continue;
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

const Vector<WebExtension::InjectedContentData>& WebExtension::staticInjectedContents()
{
    populateContentScriptPropertiesIfNeeded();
    return m_staticInjectedContents;
}

bool WebExtension::hasStaticInjectedContentForURL(NSURL *url)
{
    ASSERT(url);

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
        *error = createError(WebExtension::Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Empty `declarative_net_request` ruleset id.", "_WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for empty ruleset id"));
        return std::nullopt;
    }

    NSString *jsonPath = objectForKey<NSString>(rulesetDictionary, declarativeNetRequestRulePathManifestKey);
    if (!jsonPath.length) {
        *error = createError(WebExtension::Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Empty `declarative_net_request` JSON path.", "_WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for empty JSON path"));
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

    if (!supportedPermissions().contains(_WKWebExtensionPermissionDeclarativeNetRequest) && !supportedPermissions().contains(_WKWebExtensionPermissionDeclarativeNetRequestWithHostAccess)) {
        recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Manifest has no `declarativeNetRequest` permission.", "_WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for missing declarativeNetRequest permission")));
        return;
    }

    auto *declarativeNetRequestManifestDictionary = objectForKey<NSDictionary>(m_manifest, declarativeNetRequestManifestKey);
    if (!declarativeNetRequestManifestDictionary) {
        if ([m_manifest objectForKey:declarativeNetRequestManifestKey])
            recordError(createError(Error::InvalidDeclarativeNetRequest));
        return;
    }

    NSArray<NSDictionary *> *declarativeNetRequestRulesets = objectForKey<NSArray>(declarativeNetRequestManifestDictionary, declarativeNetRequestRulesManifestKey, true, NSDictionary.class);
    if (!declarativeNetRequestRulesets) {
        if ([m_manifest objectForKey:declarativeNetRequestManifestKey])
            recordError(createError(Error::InvalidDeclarativeNetRequest));
        return;
    }

    if (declarativeNetRequestRulesets.count > webExtensionDeclarativeNetRequestMaximumNumberOfStaticRulesets)
        recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Exceeded maximum number of `declarative_net_request` rulesets. Ignoring extra rulesets.", "_WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for too many rulesets")));

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
            recordError(createError(Error::InvalidDeclarativeNetRequest, nil, error));
            continue;
        }

        auto ruleset = optionalRuleset.value();
        if (seenRulesetIDs.contains(ruleset.rulesetID)) {
            recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_FORMAT_STRING("`declarative_net_request` ruleset with id \"%@\" is invalid. Ruleset id must be unique.", "_WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for duplicate ruleset id", (NSString *)ruleset.rulesetID)));
            continue;
        }

        if (ruleset.enabled && ++enabledRulesetCount > webExtensionDeclarativeNetRequestMaximumNumberOfEnabledRulesets && !recordedTooManyRulesetsManifestError) {
            recordError(createError(Error::InvalidDeclarativeNetRequest, WEB_UI_FORMAT_STRING("Exceeded maximum number of enabled `declarative_net_request` static rulesets. The first %lu will be applied, the remaining will be ignored.", "_WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for too many enabled static rulesets", webExtensionDeclarativeNetRequestMaximumNumberOfEnabledRulesets)));
            recordedTooManyRulesetsManifestError = true;
            continue;
        }

        seenRulesetIDs.add(ruleset.rulesetID);
        ++rulesetCount;

        m_declarativeNetRequestRulesets.append(ruleset);
    }
}

WebExtension::DeclarativeNetRequestRulesetVector& WebExtension::declarativeNetRequestRulesets()
{
    populateDeclarativeNetRequestPropertiesIfNeeded();
    return m_declarativeNetRequestRulesets;
}

NSArray *WebExtension::InjectedContentData::expandedIncludeMatchPatternStrings() const
{
    NSMutableArray<NSString *> *result = [NSMutableArray array];

    for (auto& includeMatchPattern : includeMatchPatterns)
        [result addObjectsFromArray:includeMatchPattern->expandedStrings()];

    return [result copy];
}

NSArray *WebExtension::InjectedContentData::expandedExcludeMatchPatternStrings() const
{
    NSMutableArray<NSString *> *result = [NSMutableArray array];

    for (auto& excludeMatchPattern : excludeMatchPatterns)
        [result addObjectsFromArray:excludeMatchPattern->expandedStrings()];

    return [result copy];
}

void WebExtension::populateContentScriptPropertiesIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestContentScriptProperties)
        return;

    m_parsedManifestContentScriptProperties = true;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/content_scripts

    NSArray<NSDictionary *> *contentScriptsManifestArray = objectForKey<NSArray>(m_manifest, contentScriptsManifestKey, true, NSDictionary.class);
    if (!contentScriptsManifestArray.count) {
        if ([m_manifest objectForKey:contentScriptsManifestKey])
            recordError(createError(Error::InvalidContentScripts));
        return;
    }

    auto addInjectedContentData = ^(NSDictionary<NSString *, id> *dictionary) {
        HashSet<Ref<WebExtensionMatchPattern>> includeMatchPatterns;

        // Required. Specifies which pages the specified scripts and stylesheets will be injected into.
        NSArray<NSString *> *matchesArray = objectForKey<NSArray>(dictionary, contentScriptsMatchesManifestKey, true, NSString.class);
        for (NSString *matchPatternString in matchesArray) {
            if (!matchPatternString.length)
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
        NSArray *scriptPaths = objectForKey<NSArray>(dictionary, contentScriptsJSManifestKey, true, NSString.class);
        scriptPaths = filterObjects(scriptPaths, ^(id key, NSString *string) {
            return !!string.length;
        });

        // Optional. The list of CSS files to be injected into matching pages. These are injected in the order they appear in this array, before any DOM is constructed or displayed for the page.
        NSArray *styleSheetPaths = objectForKey<NSArray>(dictionary, contentScriptsCSSManifestKey, true, NSString.class);
        styleSheetPaths = filterObjects(styleSheetPaths, ^(id key, NSString *string) {
            return !!string.length;
        });

        if (!scriptPaths.count && !styleSheetPaths.count) {
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has missing or empty 'js' and 'css' arrays.", "WKWebExtensionErrorInvalidContentScripts description for missing or empty 'js' and 'css' arrays")));
            return;
        }

        // Optional. Whether the script should inject into an about:blank frame where the parent or opener frame matches one of the patterns declared in matches. Defaults to false.
        bool matchesAboutBlank = objectForKey<NSNumber>(dictionary, contentScriptsMatchesAboutBlankManifestKey).boolValue;

        HashSet<Ref<WebExtensionMatchPattern>> excludeMatchPatterns;

        // Optional. Excludes pages that this content script would otherwise be injected into.
        NSArray<NSString *> *excludeMatchesArray = objectForKey<NSArray>(dictionary, contentScriptsExcludeMatchesManifestKey, true, NSString.class);
        for (NSString *matchPatternString in excludeMatchesArray) {
            if (!matchPatternString.length)
                continue;

            if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(matchPatternString)) {
                if (matchPattern->isSupported())
                    excludeMatchPatterns.add(matchPattern.releaseNonNull());
            }
        }

        // Optional. Applied after matches to include only those URLs that also match this glob.
        NSArray *includeGlobPatternStrings = objectForKey<NSArray>(dictionary, contentScriptsIncludeGlobsManifestKey, true, NSString.class);
        includeGlobPatternStrings = filterObjects(includeGlobPatternStrings, ^(id key, NSString *string) {
            return !!string.length;
        });

        // Optional. Applied after matches to exclude URLs that match this glob.
        NSArray *excludeGlobPatternStrings = objectForKey<NSArray>(dictionary, contentScriptsExcludeGlobsManifestKey, true, NSString.class);
        excludeGlobPatternStrings = filterObjects(excludeGlobPatternStrings, ^(id key, NSString *string) {
            return !!string.length;
        });

        // Optional. The "all_frames" field allows the extension to specify if JavaScript and CSS files should be injected into all frames matching the specified URL requirements or only into the
        // topmost frame in a tab. Defaults to false, meaning that only the top frame is matched. If specified true, it will inject into all frames, even if the frame is not the topmost frame in
        // the tab. Each frame is checked independently for URL requirements, it will not inject into child frames if the URL requirements are not met.
        bool injectsIntoAllFrames = objectForKey<NSNumber>(dictionary, contentScriptsAllFramesManifestKey).boolValue;

        InjectionTime injectionTime = InjectionTime::DocumentIdle;
        NSString *runsAtString = objectForKey<NSString>(dictionary, contentScriptsRunAtManifestKey);
        if (!runsAtString || [runsAtString isEqualToString:contentScriptsDocumentIdleManifestKey])
            injectionTime = InjectionTime::DocumentIdle;
        else if ([runsAtString isEqualToString:contentScriptsDocumentStartManifestKey])
            injectionTime = InjectionTime::DocumentStart;
        else if ([runsAtString isEqualToString:contentScriptsDocumentEndManifestKey])
            injectionTime = InjectionTime::DocumentEnd;
        else
            recordError(createError(Error::InvalidContentScripts, WEB_UI_STRING("Manifest `content_scripts` entry has unknown `run_at` value.", "WKWebExtensionErrorInvalidContentScripts description for unknown 'run_at' value")));

        InjectedContentData injectedContentData;
        injectedContentData.includeMatchPatterns = WTFMove(includeMatchPatterns);
        injectedContentData.excludeMatchPatterns = WTFMove(excludeMatchPatterns);
        injectedContentData.injectionTime = injectionTime;
        injectedContentData.matchesAboutBlank = matchesAboutBlank;
        injectedContentData.injectsIntoAllFrames = injectsIntoAllFrames;
        injectedContentData.forMainWorld = false;
        injectedContentData.scriptPaths = scriptPaths;
        injectedContentData.styleSheetPaths = styleSheetPaths;
        injectedContentData.includeGlobPatternStrings = includeGlobPatternStrings;
        injectedContentData.excludeGlobPatternStrings = excludeGlobPatternStrings;

        m_staticInjectedContents.append(WTFMove(injectedContentData));
    };

    for (NSDictionary<NSString *, id> *contentScriptsManifestEntry in contentScriptsManifestArray)
        addInjectedContentData(contentScriptsManifestEntry);
}

const WebExtension::PermissionsSet& WebExtension::supportedPermissions()
{
    static MainThreadNeverDestroyed<PermissionsSet> permissions = std::initializer_list<String> { _WKWebExtensionPermissionActiveTab, _WKWebExtensionPermissionAlarms, _WKWebExtensionPermissionClipboardWrite,
        _WKWebExtensionPermissionContextMenus, _WKWebExtensionPermissionCookies, _WKWebExtensionPermissionDeclarativeNetRequest, _WKWebExtensionPermissionDeclarativeNetRequestFeedback,
        _WKWebExtensionPermissionDeclarativeNetRequestWithHostAccess, _WKWebExtensionPermissionMenus, _WKWebExtensionPermissionNativeMessaging, _WKWebExtensionPermissionScripting,
        _WKWebExtensionPermissionStorage, _WKWebExtensionPermissionTabs, _WKWebExtensionPermissionUnlimitedStorage, _WKWebExtensionPermissionWebNavigation, _WKWebExtensionPermissionWebRequest };
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

WebExtension::MatchPatternSet WebExtension::allRequestedMatchPatterns()
{
    populatePermissionsPropertiesIfNeeded();
    populateContentScriptPropertiesIfNeeded();

    WebExtension::MatchPatternSet result;

    for (auto& matchPattern : m_permissionMatchPatterns)
        result.add(matchPattern);

    // FIXME: <https://webkit.org/b/246491> Add externally connectable match patterns.

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

NSSet<_WKWebExtensionPermission> *toAPI(const WebExtension::PermissionsSet& permissions)
{
    NSMutableSet<_WKWebExtensionPermission> *result = [NSMutableSet setWithCapacity:permissions.size()];

    for (auto& permission : permissions)
        [result addObject:(NSString *)permission];

    return [result copy];
}

NSSet<_WKWebExtensionMatchPattern *> *toAPI(const WebExtension::MatchPatternSet& patterns)
{
    NSMutableSet<_WKWebExtensionMatchPattern *> *result = [NSMutableSet setWithCapacity:patterns.size()];

    for (auto& matchPattern : patterns)
        [result addObject:matchPattern->wrapper()];

    return [result copy];
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
