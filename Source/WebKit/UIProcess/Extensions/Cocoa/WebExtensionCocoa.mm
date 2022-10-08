/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "CocoaHelpers.h"
#import "_WKWebExtensionInternal.h"
#import "_WKWebExtensionPermission.h"
#import <WebCore/LocalizedStrings.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

static NSString * const manifestVersionManifestKey = @"manifest_version";

static NSString * const nameManifestKey = @"name";
static NSString * const shortNameManifestKey = @"short_name";
static NSString * const versionManifestKey = @"version";
static NSString * const versionNameManifestKey = @"version_name";
static NSString * const descriptionManifestKey = @"description";

static NSString * const backgroundManifestKey = @"background";
static NSString * const backgroundPageManifestKey = @"page";
static NSString * const backgroundServiceWorkerManifestKey = @"service_worker";
static NSString * const backgroundScriptsManifestKey = @"scripts";
static NSString * const backgroundPersistentManifestKey = @"persistent";

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

WebExtension::WebExtension(NSBundle *appExtensionBundle)
    : m_bundle(appExtensionBundle)
    , m_resourceBaseURL(appExtensionBundle.resourceURL.absoluteURL)
{
    ASSERT(appExtensionBundle);
}

WebExtension::WebExtension(NSURL *resourceBaseURL)
    : m_resourceBaseURL(resourceBaseURL)
{
    ASSERT(resourceBaseURL);
    ASSERT([resourceBaseURL isFileURL]);
    ASSERT([resourceBaseURL hasDirectoryPath]);
}

WebExtension::WebExtension(NSDictionary *manifest)
    : m_manifest([manifest copy])
    , m_parsedManifest(true)
{
    ASSERT(manifest);
}

WebExtension::WebExtension(NSData *manifestData)
    : m_parsedManifest(true)
{
    ASSERT(manifestData);

    parseManifest(manifestData);
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
    m_manifest = dynamic_objc_cast<NSDictionary>([NSJSONSerialization JSONObjectWithData:manifestData options:0 error:&parseError]);
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

    // FIXME: This needs to check the bundle code signature (if a bundle is used).
    NSURL *manifestURL = [NSURL fileURLWithPath:@"manifest.json" relativeToURL:m_resourceBaseURL.get()];
    NSData *manifestData = [NSData dataWithContentsOfURL:manifestURL];
    if (!manifestData) {
        recordError(createError(Error::ManifestNotFound));
        return nil;
    }

    if (!parseManifest(manifestData))
        return nil;

    // FIXME: Handle manifest localization.
    // FIXME: Handle Safari version compatibility check.
    // Likely do this version checking when the extension is added to the WKWebExtensionController,
    // since that will need delegated to the app.

    return m_manifest.get();
}

double WebExtension::manifestVersion()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/manifest_version

    return objectForKey<NSNumber>(manifest(), manifestVersionManifestKey).doubleValue;
}

bool WebExtension::hasRequestedPermission(NSString *permission) const
{
    return [m_permissions containsObject:permission];
}

NSError *WebExtension::createError(Error error, NSString *customLocalizedDescription, NSError *underlyingError)
{
    _WKWebExtensionError errorCode;
    NSString *localizedDescription;

    switch (error) {
    case Error::Unknown:
        errorCode = _WKWebExtensionErrorUnknown;
        localizedDescription = WEB_UI_STRING("An unknown error has occurred.", "WKWebExtensionErrorUnknown description");
        break;

    case Error::ManifestNotFound:
        errorCode = _WKWebExtensionErrorManifestNotFound;
        localizedDescription = WEB_UI_STRING("Unable to find `manifest.json` in the extension bundle.", "WKWebExtensionErrorManifestNotFound description");
        break;

    case Error::InvalidManifest:
        errorCode = _WKWebExtensionErrorInvalidManifest;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
        if (NSString *debugDescription = underlyingError.userInfo[NSDebugDescriptionErrorKey])
            localizedDescription = [NSString stringWithFormat:WEB_UI_STRING("Unable to parse manifest: %@", "WKWebExtensionErrorInvalidManifest description, because of a JSON error"), debugDescription];
        else
            localizedDescription = WEB_UI_STRING("Unable to parse manifest because of an unexpected format.", "WKWebExtensionErrorInvalidManifest description");
#pragma clang diagnostic pop
        break;

    case Error::UnsupportedManifestVersion:
        errorCode = _WKWebExtensionErrorUnsupportedManifestVersion;
        localizedDescription = WEB_UI_STRING("An unsupported `manifest_version` was specified.", "WKWebExtensionErrorUnsupportedManifestVersion description");
        break;

    case Error::InvalidActionIcon:
        errorCode = _WKWebExtensionErrorInvalidActionIcon;
        localizedDescription = WEB_UI_STRING("Empty or invalid `default_icon` for the `action`, `browser_action`, or `page_action` manifest entry.", "WKWebExtensionErrorInvalidActionIcon description");
        break;

    case Error::InvalidBackgroundContent:
        errorCode = _WKWebExtensionErrorInvalidBackgroundContent;
        localizedDescription = WEB_UI_STRING("Empty or invalid `background` manifest entry.", "WKWebExtensionErrorInvalidBackgroundContent description");
        break;

    case Error::InvalidContentScripts:
        errorCode = _WKWebExtensionErrorInvalidContentScripts;
        localizedDescription = WEB_UI_STRING("Empty or invalid `content_scripts` manifest entry.", "WKWebExtensionErrorInvalidContentScripts description");
        break;

    case Error::InvalidDeclarativeNetRequest:
        errorCode = _WKWebExtensionErrorInvalidDeclarativeNetRequest;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
        if (NSString *debugDescription = underlyingError.userInfo[NSDebugDescriptionErrorKey])
            localizedDescription = [NSString stringWithFormat:WEB_UI_STRING("Unable to parse `declarativeNetRequest` rules: %@", "WKWebExtensionErrorInvalidDeclarativeNetRequest description, because of a JSON error"), debugDescription];
        else
            localizedDescription = WEB_UI_STRING("Unable to parse `declarativeNetRequest` rules because of an unexpected error.", "WKWebExtensionErrorInvalidDeclarativeNetRequest description");
#pragma clang diagnostic pop
        break;

    case Error::InvalidDescription:
        errorCode = _WKWebExtensionErrorInvalidDescription;
        localizedDescription = WEB_UI_STRING("Missing or empty `description` manifest entry.", "WKWebExtensionErrorInvalidDescription description");
        break;

    case Error::InvalidExternallyConnectable:
        errorCode = _WKWebExtensionErrorInvalidExternallyConnectable;
        localizedDescription = WEB_UI_STRING("Empty or invalid `externally_connectable` manifest entry.", "WKWebExtensionErrorInvalidExternallyConnectable description");
        break;

    case Error::InvalidIcon:
        errorCode = _WKWebExtensionErrorInvalidIcon;
        localizedDescription = WEB_UI_STRING("Missing or empty `icons` manifest entry.", "WKWebExtensionErrorInvalidIcon description");
        break;

    case Error::InvalidName:
        errorCode = _WKWebExtensionErrorInvalidName;
        localizedDescription = WEB_UI_STRING("Missing or empty `name` manifest entry.", "WKWebExtensionErrorInvalidName description");
        break;

    case Error::InvalidURLOverrides:
        errorCode = _WKWebExtensionErrorInvalidURLOverrides;
        localizedDescription = WEB_UI_STRING("Empty or invalid url overrides manifest entry", "WKWebExtensionErrorInvalidURLOverrides description");
        break;

    case Error::InvalidVersion:
        errorCode = _WKWebExtensionErrorInvalidVersion;
        localizedDescription = WEB_UI_STRING("Missing or empty `version` manifest entry.", "WKWebExtensionErrorInvalidVersion description");
        break;

    case Error::InvalidWebAccessibleResources:
        errorCode = _WKWebExtensionErrorInvalidWebAccessibleResources;
        localizedDescription = WEB_UI_STRING("Invalid `web_accessible_resources` manifest entry.", "WKWebExtensionErrorInvalidWebAccessibleResources description");
        break;

    case Error::InvalidBackgroundPersistence:
        errorCode = _WKWebExtensionErrorInvalidBackgroundPersistence;
        localizedDescription = WEB_UI_STRING("Invalid `persistent` manifest entry.", "WKWebExtensionErrorInvalidBackgroundPersistence description");
        break;

    case Error::BackgroundContentFailedToLoad:
        errorCode = _WKWebExtensionErrorBackgroundContentFailedToLoad;
        localizedDescription = WEB_UI_STRING("The background content failed to load due to an error.", "WKWebExtensionErrorBackgroundContentFailedToLoad description");
        break;
    }

    if (customLocalizedDescription.length)
        localizedDescription = customLocalizedDescription;

    NSDictionary *userInfo = @{ NSLocalizedDescriptionKey: localizedDescription };
    if (underlyingError)
        userInfo = @{ NSLocalizedDescriptionKey: localizedDescription, NSUnderlyingErrorKey: underlyingError };

    return [[NSError alloc] initWithDomain:_WKWebExtensionErrorDomain code:errorCode userInfo:userInfo];
}

void WebExtension::recordError(NSError *error, SuppressNotification suppressNotification)
{
    ASSERT(error);

    if (!m_errors)
        m_errors = [NSMutableArray array];

    [m_errors addObject:error];

    if (suppressNotification == SuppressNotification::No)
        [NSNotificationCenter.defaultCenter postNotificationName:_WKWebExtensionErrorsWereUpdatedNotification object:WebKit::wrapper(this) userInfo:nil];
}

NSArray *WebExtension::errors()
{
    // FIXME: Include runtime and declarativeNetRequest errors.

    populateDisplayStringsIfNeeded();
    populateBackgroundPropertiesIfNeeded();
    populateContentScriptPropertiesIfNeeded();
    populatePermissionsPropertiesIfNeeded();

    return [m_errors copy];
}

NSString *WebExtension::webProcessDisplayName()
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    return [NSString stringWithFormat:WEB_UI_STRING("%@ Web Extension", "Extension's process name that appears in Activity Monitor where the parameter is the name of the extension"), displayShortName()];
#pragma clang diagnostic pop
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

bool WebExtension::backgroundContentIsServiceWorker()
{
    populateBackgroundPropertiesIfNeeded();
    return !!m_backgroundServiceWorkerPath;
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
    m_backgroundContentIsPersistent = persistentBoolean ? persistentBoolean.boolValue : !(usesManifestVersion(3) || m_backgroundServiceWorkerPath);

    if (m_backgroundContentIsPersistent && usesManifestVersion(3)) {
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Invalid `persistent` manifest entry. A `manifest_version` greater-than or equal to `3` must be non-persistent.", "WKWebExtensionErrorInvalidBackgroundPersistence description for manifest v3")));
        m_backgroundContentIsPersistent = false;
    }

    if (m_backgroundContentIsPersistent && m_backgroundServiceWorkerPath) {
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Invalid `persistent` manifest entry. A `service_worker` must be non-persistent.", "WKWebExtensionErrorInvalidBackgroundPersistence description for service worker")));
        m_backgroundContentIsPersistent = false;
    }

    if (!m_backgroundContentIsPersistent && hasRequestedPermission(_WKWebExtensionPermissionWebRequest))
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Non-persistent background content cannot listen to `webRequest` events.", "WKWebExtensionErrorInvalidBackgroundPersistence description for webRequest events")));

#if PLATFORM(IOS)
    if (m_backgroundContentIsPersistent)
        recordError(createError(Error::InvalidBackgroundPersistence, WEB_UI_STRING("Invalid `persistent` manifest entry. A non-persistent background is required on iOS and iPadOS.", "WKWebExtensionErrorInvalidBackgroundPersistence description for iOS")));
#endif
}

const Vector<WebExtension::InjectedContentData>& WebExtension::injectedContents()
{
    populateContentScriptPropertiesIfNeeded();
    return m_injectedContents;
}

bool WebExtension::hasInjectedContentForURL(NSURL *url)
{
    ASSERT(url);

    populateContentScriptPropertiesIfNeeded();

    for (auto& injectedContent : m_injectedContents) {
        // FIXME: rdar://problem/57375730 Add support for exclude globs.
        bool isExcluded = false;
        for (auto& excludeMatchPattern : injectedContent.excludeMatchPatterns) {
            if (excludeMatchPattern->matchesURL(url)) {
                isExcluded = true;
                break;
            }
        }

        if (isExcluded)
            continue;

        // FIXME: rdar://problem/57375730 Add support for include globs.
        for (auto& includeMatchPattern : injectedContent.includeMatchPatterns) {
            if (includeMatchPattern->matchesURL(url))
                return true;
        }
    }

    return false;
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

        m_injectedContents.append({ WTFMove(includeMatchPatterns), WTFMove(excludeMatchPatterns), injectionTime, matchesAboutBlank, injectsIntoAllFrames, scriptPaths, styleSheetPaths, includeGlobPatternStrings, excludeGlobPatternStrings });
    };

    for (NSDictionary<NSString *, id> *contentScriptsManifestEntry in contentScriptsManifestArray)
        addInjectedContentData(contentScriptsManifestEntry);
}

NSSet *WebExtension::supportedPermissions()
{
    static NSSet *permissions = [NSSet setWithObjects:_WKWebExtensionPermissionActiveTab, _WKWebExtensionPermissionAlarms, _WKWebExtensionPermissionClipboardWrite,
        _WKWebExtensionPermissionContextMenus, _WKWebExtensionPermissionCookies, _WKWebExtensionPermissionDeclarativeNetRequest, _WKWebExtensionPermissionDeclarativeNetRequestFeedback,
        _WKWebExtensionPermissionDeclarativeNetRequestWithHostAccess, _WKWebExtensionPermissionMenus, _WKWebExtensionPermissionNativeMessaging, _WKWebExtensionPermissionScripting,
        _WKWebExtensionPermissionStorage, _WKWebExtensionPermissionTabs, _WKWebExtensionPermissionUnlimitedStorage, _WKWebExtensionPermissionWebNavigation, _WKWebExtensionPermissionWebRequest, nil];
    return permissions;
}

NSSet *WebExtension::requestedPermissions()
{
    populatePermissionsPropertiesIfNeeded();
    return m_permissions.get();
}

NSSet *WebExtension::optionalPermissions()
{
    populatePermissionsPropertiesIfNeeded();
    return m_optionalPermissions.get();
}

const HashSet<Ref<WebExtensionMatchPattern>>& WebExtension::requestedPermissionOrigins()
{
    populatePermissionsPropertiesIfNeeded();
    return m_permissionOrigins;
}

const HashSet<Ref<WebExtensionMatchPattern>>& WebExtension::optionalPermissionOrigins()
{
    populatePermissionsPropertiesIfNeeded();
    return m_optionalPermissionOrigins;
}

const HashSet<Ref<WebExtensionMatchPattern>>&& WebExtension::allRequestedOrigins()
{
    populatePermissionsPropertiesIfNeeded();
    populateContentScriptPropertiesIfNeeded();

    HashSet<Ref<WebExtensionMatchPattern>> result;

    for (auto& matchPattern : m_permissionOrigins)
        result.add(matchPattern);

    // FIXME: Add externally connectable match patterns.

    for (auto& injectedContent : m_injectedContents) {
        for (auto& matchPattern : injectedContent.includeMatchPatterns)
            result.add(matchPattern);
    }

    return WTFMove(result);
}

void WebExtension::populatePermissionsPropertiesIfNeeded()
{
    if (!manifestParsedSuccessfully())
        return;

    if (m_parsedManifestPermissionProperties)
        return;

    m_parsedManifestPermissionProperties = YES;

    bool findOriginsInPermissions = !usesManifestVersion(3);

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/permissions

    NSArray<NSString *> *permissions = objectForKey<NSArray>(m_manifest, permissionsManifestKey, true, NSString.class);
    NSMutableSet<NSString *> *filteredPermissions = [NSMutableSet set];

    for (NSString *permission in permissions) {
        if (findOriginsInPermissions) {
            if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(permission)) {
                if (matchPattern->isSupported())
                    m_permissionOrigins.add(matchPattern.releaseNonNull());
                continue;
            }
        }

        if ([supportedPermissions() containsObject:permission])
            [filteredPermissions addObject:permission];
    }

    m_permissions = filteredPermissions;

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/host_permissions

    if (!findOriginsInPermissions) {
        NSArray<NSString *> *hostPermissions = objectForKey<NSArray>(m_manifest, hostPermissionsManifestKey, true, NSString.class);

        for (NSString *hostPattern in hostPermissions) {
            if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(hostPattern)) {
                if (matchPattern->isSupported())
                    m_permissionOrigins.add(matchPattern.releaseNonNull());
            }
        }
    }

    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/optional_permissions

    NSArray<NSString *> *optionalPermissions = objectForKey<NSArray>(m_manifest, optionalPermissionsManifestKey, true, NSString.class);
    NSMutableSet<NSString *> *filteredOptionalPermissions = [NSMutableSet set];

    for (NSString *optionalPermission in optionalPermissions) {
        if (findOriginsInPermissions) {
            if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(optionalPermission)) {
                if (matchPattern->isSupported() && !m_permissionOrigins.contains(*matchPattern))
                    m_optionalPermissionOrigins.add(matchPattern.releaseNonNull());
                continue;
            }
        }

        if (![m_permissions containsObject:optionalPermission] && [supportedPermissions() containsObject:optionalPermission])
            [filteredOptionalPermissions addObject:optionalPermission];
    }

    m_optionalPermissions = filteredOptionalPermissions;

    // Documentation: https://github.com/w3c/webextensions/issues/119

    if (!findOriginsInPermissions) {
        NSArray<NSString *> *hostPermissions = objectForKey<NSArray>(m_manifest, optionalHostPermissionsManifestKey, true, NSString.class);

        for (NSString *hostPattern in hostPermissions) {
            if (auto matchPattern = WebExtensionMatchPattern::getOrCreate(hostPattern)) {
                if (matchPattern->isSupported() && !m_permissionOrigins.contains(*matchPattern))
                    m_optionalPermissionOrigins.add(matchPattern.releaseNonNull());
            }
        }
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
