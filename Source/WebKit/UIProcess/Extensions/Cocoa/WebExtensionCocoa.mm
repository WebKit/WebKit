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
#import "WKNSError.h"
#import "WKWebExtensionInternal.h"
#import "WebExtensionConstants.h"
#import "WebExtensionPermission.h"
#import "WebExtensionUtilities.h"
#import "_WKWebExtensionLocalization.h"
#import <CoreFoundation/CFBundle.h>
#import <UniformTypeIdentifiers/UTCoreTypes.h>
#import <UniformTypeIdentifiers/UTType.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/BlockPtr.h>
#import <wtf/FileSystem.h>
#import <wtf/HashSet.h>
#import <wtf/Language.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/Scope.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/MakeString.h>
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

static NSString * const commandsManifestKey = @"commands";
static NSString * const commandsSuggestedKeyManifestKey = @"suggested_key";
static NSString * const commandsDescriptionKeyManifestKey = @"description";

static NSString * const declarativeNetRequestManifestKey = @"declarative_net_request";
static NSString * const declarativeNetRequestRulesManifestKey = @"rule_resources";
static NSString * const declarativeNetRequestRulesetIDManifestKey = @"id";
static NSString * const declarativeNetRequestRuleEnabledManifestKey = @"enabled";
static NSString * const declarativeNetRequestRulePathManifestKey = @"path";

static const size_t maximumNumberOfShortcutCommands = 4;

static String convertChromeExtensionToTemporaryZipFile(const String& inputFilePath)
{
    // Converts a Chrome extension file to a temporary ZIP file by checking for a valid Chrome extension signature ('Cr24')
    // and copying the contents starting from the ZIP signature ('PK\x03\x04'). Returns a null string if the signatures
    // are not found or any file operations fail.

    auto inputFileHandle = FileSystem::openFile(inputFilePath, FileSystem::FileOpenMode::Read);
    if (!FileSystem::isHandleValid(inputFileHandle))
        return nullString();

    auto closeFile = makeScopeExit([&] {
        FileSystem::unlockAndCloseFile(inputFileHandle);
    });

    static std::array<uint8_t, 4> expectedSignature = { 'C', 'r', '2', '4' };

    // Verify Chrome extension magic signature.
    std::array<uint8_t, 4> signature;
    auto bytesRead = FileSystem::readFromFile(inputFileHandle, signature);
    if (bytesRead != expectedSignature.size() || signature != expectedSignature)
        return nullString();

    // Create a temporary ZIP file.
    auto [temporaryFilePath, temporaryFileHandle] = FileSystem::openTemporaryFile("WebKitExtension-"_s, ".zip"_s);
    if (!FileSystem::isHandleValid(temporaryFileHandle))
        return nullString();

    auto closeTempFile = makeScopeExit([fileHandle = temporaryFileHandle] {
        FileSystem::unlockAndCloseFile(fileHandle);
    });

    std::array<uint8_t, 4096> buffer;
    bool signatureFound = false;

    while (true) {
        bytesRead = FileSystem::readFromFile(inputFileHandle, buffer);

        // Error reading file.
        if (bytesRead < 0)
            return nullString();

        // Done reading file.
        if (!bytesRead)
            break;

        size_t bufferOffset = 0;
        if (!signatureFound) {
            // Not enough bytes for the signature.
            if (bytesRead < 4)
                return nullString();

            // Search for the ZIP file magic signature in the buffer.
            for (ssize_t i = 0; i < bytesRead - 3; ++i) {
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

        auto bytesToWrite = std::span(buffer).subspan(bufferOffset, bytesRead - bufferOffset);
        auto bytesWritten = FileSystem::writeToFile(temporaryFileHandle, bytesToWrite);
        if (bytesWritten != static_cast<int64_t>(bytesToWrite.size()))
            return nullString();
    }

    return temporaryFilePath;
}

static String processFileAndExtractZipArchive(const String& path)
{
    // Check if the file is a Chrome extension archive and extract it.
    auto temporaryZipFilePath = convertChromeExtensionToTemporaryZipFile(path);
    if (!temporaryZipFilePath.isNull()) {
        auto temporaryDirectory = FileSystem::extractTemporaryZipArchive(temporaryZipFilePath);
        FileSystem::deleteFile(temporaryZipFilePath);
        return temporaryDirectory;
    }

    // Assume the file is already a ZIP archive and try to extract it.
    return FileSystem::extractTemporaryZipArchive(path);
}

WebExtension::WebExtension(NSBundle *appExtensionBundle, NSURL *resourceURL, RefPtr<API::Error>& outError)
    : m_bundle(appExtensionBundle)
    , m_resourceBaseURL(resourceURL)
    , m_manifestJSON(JSON::Value::null())
{
    RELEASE_ASSERT(m_bundle || m_resourceBaseURL.isValid());

    outError = nullptr;

    if (m_resourceBaseURL.isValid()) {
        BOOL isDirectory;
        if (![NSFileManager.defaultManager fileExistsAtPath:m_resourceBaseURL.fileSystemPath() isDirectory:&isDirectory]) {
            outError = createError(Error::Unknown);
            return;
        }

        if (!isDirectory) {
            auto temporaryDirectory = processFileAndExtractZipArchive(m_resourceBaseURL.fileSystemPath());
            if (!temporaryDirectory) {
                outError = createError(Error::InvalidArchive);
                return;
            }

            ASSERT(temporaryDirectory.right(1) != "/"_s);
            m_resourceBaseURL = URL::fileURLWithFileSystemPath(makeString(temporaryDirectory, '/'));
        }

#if PLATFORM(MAC)
        m_shouldValidateResourceData = false;
#endif
    }

    if (m_bundle) {
        auto *bundleResourceURL = m_bundle.get().resourceURL.URLByStandardizingPath.absoluteURL;
        if (m_resourceBaseURL.isEmpty())
            m_resourceBaseURL = bundleResourceURL;

#if PLATFORM(MAC)
        m_shouldValidateResourceData = m_resourceBaseURL == URL(bundleResourceURL);
#endif
    }

    RELEASE_ASSERT(m_resourceBaseURL.protocolIsFile());
    RELEASE_ASSERT(m_resourceBaseURL.hasPath());
    RELEASE_ASSERT(m_resourceBaseURL.path().right(1) == "/"_s);

    if (!manifestParsedSuccessfully()) {
        ASSERT(!m_errors.isEmpty());
        outError = m_errors.last().ptr();
    }
}

WebExtension::WebExtension(NSDictionary *manifest, Resources&& resources)
    : m_manifestJSON(JSON::Value::null())
    , m_resources(WTFMove(resources))
{
    RELEASE_ASSERT(manifest);

    auto *manifestData = encodeJSONData(manifest);
    RELEASE_ASSERT(manifestData);

    m_resources.set("manifest.json"_s, API::Data::createWithoutCopying(manifestData));
}

WebExtension::WebExtension(Resources&& resources)
    : m_manifestJSON(JSON::Value::null())
    , m_resources(WTFMove(resources))
{
}

bool WebExtension::parseManifest(NSData *manifestData)
{
    NSError *parseError;
    m_manifest = parseJSON(manifestData, { }, &parseError);
    if (!m_manifest) {
        if (parseError)
            recordError(createError(Error::InvalidManifest, { }, API::Error::create(parseError)));
        else
            recordError(createError(Error::InvalidManifest));
        return false;
    }

    // Set to an empty object for now so calls to manifestParsedSuccessfully() during this will be true.
    // This is needed for localization to properly get the defaultLocale() while we are mid-parse.
    m_manifestJSON = JSON::Object::create();

    if (id defaultLocaleValue = m_manifest.get()[defaultLocaleManifestKey]) {
        if (auto *defaultLocale = dynamic_objc_cast<NSString>(defaultLocaleValue)) {
            auto parsedLocale = parseLocale(defaultLocale);
            if (!parsedLocale.languageCode.isEmpty()) {
                if (supportedLocales().contains(String(defaultLocale)))
                    m_defaultLocale = defaultLocale;
                else
                    recordError(createError(Error::InvalidDefaultLocale, WEB_UI_STRING("Unable to find `default_locale` in “_locales” folder.", "WKWebExtensionErrorInvalidManifestEntry description for missing default_locale")));
            } else
                recordError(createError(Error::InvalidDefaultLocale));
        } else
            recordError(createError(Error::InvalidDefaultLocale));
    }

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

    RefPtr<API::Error> error;
    RefPtr manifestData = resourceDataForPath("manifest.json"_s, error);
    if (!manifestData || error) {
        recordErrorIfNeeded(error);
        return nil;
    }

    if (!parseManifest(static_cast<NSData *>(manifestData->wrapper())))
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

    if (!m_shouldValidateResourceData)
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
    else if (auto *fileURL = static_cast<NSURL *>(resourceFileURLForPath(path)))
        [fileURL getResourceValue:&result forKey:NSURLContentTypeKey error:nil];

    return result;
}

RefPtr<API::Data> WebExtension::resourceDataForPath(const String& originalPath, RefPtr<API::Error>& outError, CacheResult cacheResult, SuppressNotFoundErrors suppressErrors)
{
    ASSERT(originalPath);

    outError = nullptr;

    String path = originalPath;

    // Remove leading slash to normalize the path for lookup/storage in the cache dictionary.
    if (path.startsWith('/'))
        path = path.substring(1);

    auto *cocoaPath = static_cast<NSString *>(path);

    if ([cocoaPath hasPrefix:@"data:"]) {
        if (auto base64Range = [cocoaPath rangeOfString:@";base64,"]; base64Range.location != NSNotFound) {
            auto *base64String = [cocoaPath substringFromIndex:NSMaxRange(base64Range)];
            auto *data = [[NSData alloc] initWithBase64EncodedString:base64String options:0];
            return API::Data::createWithoutCopying(data);
        }

        if (auto commaRange = [cocoaPath rangeOfString:@","]; commaRange.location != NSNotFound) {
            auto *urlEncodedString = [cocoaPath substringFromIndex:NSMaxRange(commaRange)];
            auto *decodedString = [urlEncodedString stringByRemovingPercentEncoding];
            auto *data = [decodedString dataUsingEncoding:NSUTF8StringEncoding];
            return API::Data::createWithoutCopying(data);
        }

        ASSERT([cocoaPath isEqualToString:@"data:"]);
        return API::Data::create(std::span<const uint8_t> { });
    }

    // Remove leading slash to normalize the path for lookup/storage in the cache dictionary.
    if ([cocoaPath hasPrefix:@"/"])
        cocoaPath = [cocoaPath substringFromIndex:1];

    if (RefPtr cachedObject = m_resources.get(path))
        return cachedObject;

    if ([cocoaPath isEqualToString:generatedBackgroundPageFilename] || [cocoaPath isEqualToString:generatedBackgroundServiceWorkerFilename])
        return API::Data::create(generatedBackgroundContent().utf8().span());

    auto *resourceURL = static_cast<NSURL *>(resourceFileURLForPath(path));
    if (!resourceURL) {
        if (suppressErrors == SuppressNotFoundErrors::No)
            outError = createError(Error::ResourceNotFound, WEB_UI_FORMAT_CFSTRING("Unable to find \"%@\" in the extension’s resources. It is an invalid path.", "WKWebExtensionErrorResourceNotFound description with invalid file path", (__bridge CFStringRef)cocoaPath));
        return nullptr;
    }

    NSError *fileReadError;
    NSData *resultData = [NSData dataWithContentsOfURL:resourceURL options:NSDataReadingMappedIfSafe error:&fileReadError];
    if (!resultData) {
        if (suppressErrors == SuppressNotFoundErrors::No)
            outError = createError(Error::ResourceNotFound, WEB_UI_FORMAT_CFSTRING("Unable to find \"%@\" in the extension’s resources.", "WKWebExtensionErrorResourceNotFound description with file name", (__bridge CFStringRef)cocoaPath), API::Error::create(fileReadError));
        return nullptr;
    }

#if PLATFORM(MAC)
    NSError *validationError;
    if (!validateResourceData(resourceURL, resultData, &validationError)) {
        outError = createError(Error::InvalidResourceCodeSignature, WEB_UI_FORMAT_CFSTRING("Unable to validate \"%@\" with the extension’s code signature. It likely has been modified since the extension was built.", "WKWebExtensionErrorInvalidResourceCodeSignature description with file name", (__bridge CFStringRef)cocoaPath), API::Error::create(validationError));
        return nullptr;
    }
#endif

    Ref data = API::Data::createWithoutCopying(resultData);
    if (cacheResult == CacheResult::Yes)
        m_resources.set(path, data);

    return data;
}

void WebExtension::recordError(Ref<API::Error> error)
{
    RELEASE_LOG_ERROR(Extensions, "Error recorded: %{public}@", privacyPreservingDescription(error->platformError()));

    // Only the first occurrence of each error is recorded in the array. This prevents duplicate errors,
    // such as repeated "resource not found" errors, from being included multiple times.
    if (m_errors.contains(error))
        return;

    [wrapper() willChangeValueForKey:@"errors"];
    m_errors.append(error);
    [wrapper() didChangeValueForKey:@"errors"];
}

_WKWebExtensionLocalization *WebExtension::localization()
{
    if (!manifestParsedSuccessfully())
        return nil;

    return m_localization.get();
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
        RefPtr<API::Error> resourceError;
        m_defaultActionIcon = imageForPath(defaultIconPath, resourceError);

        if (!m_defaultActionIcon) {
            recordErrorIfNeeded(resourceError);

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

CocoaImage *WebExtension::imageForPath(NSString *imagePath, RefPtr<API::Error>& outError, CGSize sizeForResizing)
{
    ASSERT(imagePath);

    RefPtr data = resourceDataForPath(imagePath, outError);
    if (!data || outError)
        return nil;

    auto *imageData = static_cast<NSData *>(data->wrapper());

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

CocoaImage *WebExtension::bestImageInIconsDictionary(NSDictionary *iconsDictionary, CGSize idealSize, const Function<void(Ref<API::Error>)>& reportError)
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
        RefPtr<API::Error> resourceError;
        if (auto *image = imageForPath(iconPath, resourceError, idealSize)) {
            if (!resultImage)
                resultImage = image;
            else
                [resultImage addRepresentations:image.representations];
        } else if (reportError && resourceError)
            reportError(*resourceError);
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
        RefPtr<API::Error> resourceError;
        if (auto *image = imageForPath(path, resourceError, idealSize))
            return image;

        if (reportError && resourceError)
            reportError(*resourceError);

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
    auto *result = bestImageInIconsDictionary(iconDictionary, idealSize, [&](Ref<API::Error> error) {
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

CocoaImage *WebExtension::bestImageForIconVariants(NSArray *variants, CGSize idealSize, const Function<void(Ref<API::Error>)>& reportError)
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
    auto *result = bestImageForIconVariants(variants, idealSize, [&](Ref<API::Error> error) {
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

std::optional<WebExtension::DeclarativeNetRequestRulesetData> WebExtension::parseDeclarativeNetRequestRulesetDictionary(NSDictionary *rulesetDictionary, RefPtr<API::Error>& error)
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

    error = nullptr;

    NSString *exceptionString;
    bool isRulesetDictionaryValid = validateDictionary(rulesetDictionary, nil, requiredKeysInRulesetDictionary, keyToExpectedValueTypeInRulesetDictionary, &exceptionString);
    if (!isRulesetDictionaryValid) {
        error = createError(WebExtension::Error::InvalidDeclarativeNetRequest, exceptionString);
        return std::nullopt;
    }

    NSString *rulesetID = objectForKey<NSString>(rulesetDictionary, declarativeNetRequestRulesetIDManifestKey);
    if (!rulesetID.length) {
        error = createError(WebExtension::Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Empty `declarative_net_request` ruleset id.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for empty ruleset id"));
        return std::nullopt;
    }

    NSString *jsonPath = objectForKey<NSString>(rulesetDictionary, declarativeNetRequestRulePathManifestKey);
    if (!jsonPath.length) {
        error = createError(WebExtension::Error::InvalidDeclarativeNetRequest, WEB_UI_STRING("Empty `declarative_net_request` JSON path.", "WKWebExtensionErrorInvalidDeclarativeNetRequestEntry description for empty JSON path"));
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

    if (!supportedPermissions().contains(WebExtensionPermission::declarativeNetRequest()) && !supportedPermissions().contains(WebExtensionPermission::declarativeNetRequestWithHostAccess())) {
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

        RefPtr<API::Error> error;
        auto optionalRuleset = parseDeclarativeNetRequestRulesetDictionary(rulesetDictionary, error);
        if (!optionalRuleset) {
            if (error)
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

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
