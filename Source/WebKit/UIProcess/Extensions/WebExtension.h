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
#include "CocoaImage.h"
#include "WebExtensionMatchPattern.h"
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSBundle;
OBJC_CLASS NSData;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSError;
OBJC_CLASS NSLocale;
OBJC_CLASS NSMutableArray;
OBJC_CLASS NSMutableDictionary;
OBJC_CLASS NSString;
OBJC_CLASS NSURL;
OBJC_CLASS UTType;
OBJC_CLASS _WKWebExtension;
OBJC_CLASS _WKWebExtensionLocalization;
OBJC_CLASS _WKWebExtensionMatchPattern;

#if PLATFORM(MAC)
#include <Security/CSCommon.h>
#endif

#ifdef __OBJC__
#include "_WKWebExtensionPermission.h"
#endif

namespace API {
class Data;
}

namespace WebKit {

class WebExtension : public API::ObjectImpl<API::Object::Type::WebExtension>, public CanMakeWeakPtr<WebExtension> {
    WTF_MAKE_NONCOPYABLE(WebExtension);

public:
    template<typename... Args>
    static Ref<WebExtension> create(Args&&... args)
    {
        return adoptRef(*new WebExtension(std::forward<Args>(args)...));
    }

    explicit WebExtension(NSBundle *appExtensionBundle, NSError ** = nullptr);
    explicit WebExtension(NSURL *resourceBaseURL, NSError ** = nullptr);
    explicit WebExtension(NSDictionary *manifest, NSDictionary *resources);
    explicit WebExtension(NSDictionary *resources);

    ~WebExtension() { }

    enum class CacheResult : bool { No, Yes };
    enum class SuppressNotification : bool { No, Yes };
    enum class SuppressNotFoundErrors : bool { No, Yes };

    enum class Error : uint8_t {
        Unknown = 1,
        ResourceNotFound,
        InvalidResourceCodeSignature,
        InvalidManifest,
        UnsupportedManifestVersion,
        InvalidAction,
        InvalidActionIcon,
        InvalidBackgroundContent,
        InvalidBackgroundPersistence,
        InvalidCommands,
        InvalidContentScripts,
        InvalidContentSecurityPolicy,
        InvalidDeclarativeNetRequest,
        InvalidDescription,
        InvalidExternallyConnectable,
        InvalidIcon,
        InvalidName,
        InvalidOptionsPage,
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

    using PermissionsSet = HashSet<String>;
    using MatchPatternSet = HashSet<Ref<WebExtensionMatchPattern>>;

    // Needs to match UIKeyModifierFlags and NSEventModifierFlags.
    enum class ModifierFlags : uint32_t {
        Shift   = 1 << 17,
        Control = 1 << 18,
        Option  = 1 << 19,
        Command = 1 << 20
    };

    static constexpr OptionSet<ModifierFlags> allModifierFlags()
    {
        return {
            ModifierFlags::Shift,
            ModifierFlags::Control,
            ModifierFlags::Option,
            ModifierFlags::Command
        };
    }

    struct CommandData {
        String identifier;
        String description;
        String activationKey;
        OptionSet<ModifierFlags> modifierFlags;
    };

    struct InjectedContentData {
        MatchPatternSet includeMatchPatterns;
        MatchPatternSet excludeMatchPatterns;

        InjectionTime injectionTime = InjectionTime::DocumentIdle;

        String identifier { ""_s };

        bool matchesAboutBlank { false };
        bool injectsIntoAllFrames { false };
        bool forMainWorld { false };

        RetainPtr<NSArray> scriptPaths;
        RetainPtr<NSArray> styleSheetPaths;

        RetainPtr<NSArray> includeGlobPatternStrings;
        RetainPtr<NSArray> excludeGlobPatternStrings;

        NSArray *expandedIncludeMatchPatternStrings() const;
        NSArray *expandedExcludeMatchPatternStrings() const;
    };

    struct WebAccessibleResourceData {
        MatchPatternSet matchPatterns;
        Vector<String> resourcePathPatterns;
    };

    struct DeclarativeNetRequestRulesetData {
        String rulesetID;
        bool enabled;
        String jsonPath;
    };

    using CommandsVector = Vector<CommandData>;
    using InjectedContentVector = Vector<InjectedContentData>;
    using WebAccessibleResourcesVector = Vector<WebAccessibleResourceData>;
    using DeclarativeNetRequestRulesetVector = Vector<DeclarativeNetRequestRulesetData>;

    static const PermissionsSet& supportedPermissions();

    bool operator==(const WebExtension& other) const { return (this == &other); }

    bool manifestParsedSuccessfully();
    NSDictionary *manifest();
    Ref<API::Data> serializeManifest();

    double manifestVersion();
    bool supportsManifestVersion(double version) { ASSERT(version > 2); return manifestVersion() >= version; }

    Ref<API::Data> serializeLocalization();

#if PLATFORM(MAC)
    SecStaticCodeRef bundleStaticCode();
    bool validateResourceData(NSURL *, NSData *, NSError **);
#endif

    bool isWebAccessibleResource(const URL& resourceURL, const URL& pageURL);

    NSURL *resourceFileURLForPath(NSString *);

    UTType *resourceTypeForPath(NSString *);

    NSString *resourceStringForPath(NSString *, CacheResult = CacheResult::No, SuppressNotFoundErrors = SuppressNotFoundErrors::No);
    NSData *resourceDataForPath(NSString *, CacheResult = CacheResult::No, SuppressNotFoundErrors = SuppressNotFoundErrors::No);

    _WKWebExtensionLocalization *localization();
    NSLocale *defaultLocale();

    NSString *displayName();
    NSString *displayShortName();
    NSString *displayVersion();
    NSString *displayDescription();
    NSString *version();

    NSString *contentSecurityPolicy();

    CocoaImage *icon(CGSize idealSize);

    CocoaImage *actionIcon(CGSize idealSize);
    NSString *displayActionLabel();
    NSString *actionPopupPath();

    bool hasAction();
    bool hasBrowserAction();
    bool hasPageAction();

    CocoaImage *imageForPath(NSString *);

    NSString *pathForBestImageInIconsDictionary(NSDictionary *, size_t idealPixelSize);
    CocoaImage *bestImageInIconsDictionary(NSDictionary *, size_t idealPointSize);
    CocoaImage *bestImageForIconsDictionaryManifestKey(NSDictionary *, NSString *manifestKey, CGSize idealSize, RetainPtr<CocoaImage>& cacheLocation, Error, NSString *customLocalizedDescription);

    bool hasBackgroundContent();
    bool backgroundContentIsPersistent();
    bool backgroundContentUsesModules();
    bool backgroundContentIsServiceWorker();

    NSString *backgroundContentPath();
    NSString *generatedBackgroundContent();

    bool hasOptionsPage();
    bool hasOverrideNewTabPage();

    NSString *optionsPagePath();
    NSString *overrideNewTabPagePath();

    const CommandsVector& commands();
    bool hasCommands();

    DeclarativeNetRequestRulesetVector& declarativeNetRequestRulesets();
    bool hasContentModificationRules() { return !declarativeNetRequestRulesets().isEmpty(); }

    const InjectedContentVector& staticInjectedContents();
    bool hasStaticInjectedContentForURL(NSURL *);
    bool hasStaticInjectedContent();

    // Permissions requested by the extension in their manifest.
    // These are not the currently allowed permissions.
    const PermissionsSet& requestedPermissions();
    const PermissionsSet& optionalPermissions();

    bool hasRequestedPermission(NSString *) const;

    // Permission patterns requested by the extension in their manifest.
    // These are not the currently allowed permission patterns.
    const MatchPatternSet& requestedPermissionMatchPatterns();
    const MatchPatternSet& optionalPermissionMatchPatterns();

    // Combined pattern set that includes permission patterns and injected content patterns from the manifest.
    MatchPatternSet allRequestedMatchPatterns();

    NSError *createError(Error, NSString *customLocalizedDescription = nil, NSError *underlyingError = nil);

    // If an error can't be synchronously determined by one of the populate methods in the errors() getter,
    // then the caller of recordError() should pass SuppressNotification::No.
    void recordError(NSError *, SuppressNotification = SuppressNotification::Yes);
    void removeError(Error, SuppressNotification = SuppressNotification::No);

    NSArray *errors();

#ifdef __OBJC__
    _WKWebExtension *wrapper() const { return (_WKWebExtension *)API::ObjectImpl<API::Object::Type::WebExtension>::wrapper(); }
#endif

private:
    bool parseManifest(NSData *);

    void populateDisplayStringsIfNeeded();
    void populateActionPropertiesIfNeeded();
    void populateBackgroundPropertiesIfNeeded();
    void populateContentScriptPropertiesIfNeeded();
    void populatePermissionsPropertiesIfNeeded();
    void populatePagePropertiesIfNeeded();
    void populateContentSecurityPolicyStringsIfNeeded();
    void populateWebAccessibleResourcesIfNeeded();
    void populateCommandsIfNeeded();
    void populateDeclarativeNetRequestPropertiesIfNeeded();

    std::optional<WebExtension::DeclarativeNetRequestRulesetData> parseDeclarativeNetRequestRulesetDictionary(NSDictionary *, NSError **);

    InjectedContentVector m_staticInjectedContents;
    WebAccessibleResourcesVector m_webAccessibleResources;
    CommandsVector m_commands;
    DeclarativeNetRequestRulesetVector m_declarativeNetRequestRulesets;

    MatchPatternSet m_permissionMatchPatterns;
    MatchPatternSet m_optionalPermissionMatchPatterns;

    PermissionsSet m_permissions;
    PermissionsSet m_optionalPermissions;

#if PLATFORM(MAC)
    RetainPtr<SecStaticCodeRef> m_bundleStaticCode;
#endif

    RetainPtr<NSBundle> m_bundle;
    RetainPtr<NSURL> m_resourceBaseURL;
    RetainPtr<NSDictionary> m_manifest;
    RetainPtr<NSMutableDictionary> m_resources;

    RetainPtr<NSLocale> m_defaultLocale;
    RetainPtr<_WKWebExtensionLocalization> m_localization;

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

    RetainPtr<NSString> m_contentSecurityPolicy;

    RetainPtr<NSArray> m_backgroundScriptPaths;
    RetainPtr<NSString> m_backgroundPagePath;
    RetainPtr<NSString> m_backgroundServiceWorkerPath;
    RetainPtr<NSString> m_generatedBackgroundContent;

    RetainPtr<NSString> m_optionsPagePath;
    RetainPtr<NSString> m_overrideNewTabPagePath;

    bool m_backgroundContentIsPersistent : 1 { false };
    bool m_backgroundPageUsesModules : 1 { false };
    bool m_parsedManifest : 1 { false };
    bool m_parsedManifestDisplayStrings : 1 { false };
    bool m_parsedManifestContentSecurityPolicyStrings : 1 { false };
    bool m_parsedManifestActionProperties : 1 { false };
    bool m_parsedManifestBackgroundProperties : 1 { false };
    bool m_parsedManifestContentScriptProperties : 1 { false };
    bool m_parsedManifestPermissionProperties : 1 { false };
    bool m_parsedManifestPageProperties : 1 { false };
    bool m_parsedManifestWebAccessibleResources : 1 { false };
    bool m_parsedManifestCommands : 1 { false };
    bool m_parsedManifestDeclarativeNetRequestRulesets : 1 { false };
};

#ifdef __OBJC__

NSSet<_WKWebExtensionPermission> *toAPI(const WebExtension::PermissionsSet&);
NSSet<_WKWebExtensionMatchPattern *> *toAPI(const WebExtension::MatchPatternSet&);

#endif

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::WebExtension::ModifierFlags> {
    using values = EnumValues<
        WebKit::WebExtension::ModifierFlags,
        WebKit::WebExtension::ModifierFlags::Shift,
        WebKit::WebExtension::ModifierFlags::Control,
        WebKit::WebExtension::ModifierFlags::Option,
        WebKit::WebExtension::ModifierFlags::Command
    >;
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
