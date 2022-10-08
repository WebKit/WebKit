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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "APIObject.h"
#include "WebExtensionMatchPattern.h"
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#import "CocoaImage.h"

OBJC_CLASS NSArray;
OBJC_CLASS NSBundle;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSError;
OBJC_CLASS NSMutableArray;
OBJC_CLASS NSMutableDictionary;
OBJC_CLASS NSString;
OBJC_CLASS NSURL;
#endif

#if PLATFORM(MAC)
#include <Security/CSCommon.h>
#endif

namespace WebKit {

class WebExtension : public API::ObjectImpl<API::Object::Type::WebExtension> {
    WTF_MAKE_NONCOPYABLE(WebExtension);

public:
    template<typename... Args>
    static Ref<WebExtension> create(Args&&... args)
    {
        return adoptRef(*new WebExtension(std::forward<Args>(args)...));
    }

#if PLATFORM(COCOA)
    explicit WebExtension(NSBundle *appExtensionBundle);
    explicit WebExtension(NSURL *resourceBaseURL);
    explicit WebExtension(NSDictionary *manifest, NSDictionary *resources);
    explicit WebExtension(NSDictionary *resources);
#endif

    ~WebExtension() { }

    enum class CacheResult : bool { No, Yes };
    enum class SuppressNotification : bool { No, Yes };

    enum class Error : uint8_t {
        Unknown,
        ResourceNotFound,
        InvalidResourceCodeSignature,
        InvalidManifest,
        UnsupportedManifestVersion,
        InvalidAction,
        InvalidActionIcon,
        InvalidBackgroundContent,
        InvalidBackgroundPersistence,
        InvalidContentScripts,
        InvalidDeclarativeNetRequest,
        InvalidDescription,
        InvalidExternallyConnectable,
        InvalidIcon,
        InvalidName,
        InvalidURLOverrides,
        InvalidVersion,
        InvalidWebAccessibleResources,
        BackgroundContentFailedToLoad,
    };

    enum class InjectionTime : uint8_t {
        DocumentIdle,
        DocumentStart,
        DocumentEnd,
    };

    struct InjectedContentData {
        HashSet<Ref<WebExtensionMatchPattern>> includeMatchPatterns;
        HashSet<Ref<WebExtensionMatchPattern>> excludeMatchPatterns;

        InjectionTime injectionTime = InjectionTime::DocumentIdle;

        bool matchesAboutBlank = false;
        bool injectsIntoAllFrames = false;

#if PLATFORM(COCOA)
        RetainPtr<NSArray> scriptPaths;
        RetainPtr<NSArray> styleSheetPaths;

        RetainPtr<NSArray> includeGlobPatternStrings;
        RetainPtr<NSArray> excludeGlobPatternStrings;

        NSArray *expandedIncludeMatchPatternStrings() const;
        NSArray *expandedExcludeMatchPatternStrings() const;
#endif
    };

#if PLATFORM(COCOA)
    static NSSet *supportedPermissions();

    bool manifestParsedSuccessfully();
    NSDictionary *manifest();

    double manifestVersion();
    bool usesManifestVersion(double version) { return manifestVersion() >= version; }

#if PLATFORM(MAC)
    SecStaticCodeRef bundleStaticCode();
    bool validateResourceData(NSURL *, NSData *, NSError **);
#endif

    NSURL *resourceFileURLForPath(NSString *);
    NSData *resourceDataForPath(NSString *, CacheResult = CacheResult::No);

    NSString *webProcessDisplayName();

    NSString *displayName();
    NSString *displayShortName();
    NSString *displayVersion();
    NSString *displayDescription();
    NSString *version();

    CocoaImage *icon(CGSize idealSize);

    CocoaImage *actionIcon(CGSize idealSize);
    NSString *displayActionLabel();
    NSString *actionPopupPath();

    CocoaImage *imageForPath(NSString *);

    NSString *pathForBestImageInIconsDictionary(NSDictionary *, size_t idealPixelSize);
    CocoaImage *bestImageInIconsDictionary(NSDictionary *, size_t idealPointSize);
    CocoaImage *bestImageForIconsDictionaryManifestKey(NSDictionary *, NSString *manifestKey, CGSize idealSize, RetainPtr<CocoaImage>& cacheLocation, Error, NSString *customLocalizedDescription);

    bool hasBackgroundContent();
    bool backgroundContentIsPersistent();
    bool backgroundContentIsServiceWorker();

    NSString *generatedBackgroundContent();

    const Vector<InjectedContentData>& injectedContents();
    bool hasInjectedContentForURL(NSURL *);

    // Permissions requested by the extension in their manifest.
    // These are not the currently allowed permissions.
    NSSet *requestedPermissions();
    NSSet *optionalPermissions();

    bool hasRequestedPermission(NSString *) const;

    // Permission origins requested by the extension in their manifest.
    // These are not the currently allowed permission origins.
    const HashSet<Ref<WebExtensionMatchPattern>>& requestedPermissionOrigins();
    const HashSet<Ref<WebExtensionMatchPattern>>& optionalPermissionOrigins();

    // Combined origin set that includes permissions origins and injected content patterns from the manifest.
    const HashSet<Ref<WebExtensionMatchPattern>>&& allRequestedOrigins();

    NSError *createError(Error, NSString *customLocalizedDescription = nil, NSError *underlyingError = nil);

    // If an error can't be synchronously determined by one of the populate methods in the errors() getter,
    // then the caller of recordError() should pass SuppressNotification::No.
    void recordError(NSError *, SuppressNotification = SuppressNotification::Yes);

    NSArray *errors();
#endif

private:
#if PLATFORM(COCOA)
    bool parseManifest(NSData *);

    void populateDisplayStringsIfNeeded();
    void populateActionPropertiesIfNeeded();
    void populateBackgroundPropertiesIfNeeded();
    void populateContentScriptPropertiesIfNeeded();
    void populatePermissionsPropertiesIfNeeded();
#endif

    Vector<InjectedContentData> m_injectedContents;
    HashSet<Ref<WebExtensionMatchPattern>> m_permissionOrigins;
    HashSet<Ref<WebExtensionMatchPattern>> m_optionalPermissionOrigins;

#if PLATFORM(MAC)
    RetainPtr<SecStaticCodeRef> m_bundleStaticCode;
#endif

#if PLATFORM(COCOA)
    RetainPtr<NSBundle> m_bundle;
    RetainPtr<NSURL> m_resourceBaseURL;
    RetainPtr<NSDictionary> m_manifest;
    RetainPtr<NSMutableDictionary> m_resources;

    RetainPtr<NSMutableArray> m_errors;

    RetainPtr<NSString> m_displayName;
    RetainPtr<NSString> m_displayShortName;
    RetainPtr<NSString> m_displayVersion;
    RetainPtr<NSString> m_displayDescription;
    RetainPtr<NSString> m_version;

    RetainPtr<CocoaImage> m_icon;

    RetainPtr<NSDictionary> m_actionDictionary;
    RetainPtr<CocoaImage> m_actionIcon;
    RetainPtr<NSString> m_displayActionLabel;
    RetainPtr<NSString> m_actionPopupPath;

    RetainPtr<NSArray> m_backgroundScriptPaths;
    RetainPtr<NSString> m_backgroundPagePath;
    RetainPtr<NSString> m_backgroundServiceWorkerPath;
    RetainPtr<NSString> m_generatedBackgroundContent;
    bool m_backgroundContentIsPersistent = false;

    RetainPtr<NSSet> m_permissions;
    RetainPtr<NSSet> m_optionalPermissions;

    bool m_parsedManifest = false;
    bool m_parsedManifestDisplayStrings = false;
    bool m_parsedManifestActionProperties = false;
    bool m_parsedManifestBackgroundProperties = false;
    bool m_parsedManifestContentScriptProperties = false;
    bool m_parsedManifestPermissionProperties = false;
#endif
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
