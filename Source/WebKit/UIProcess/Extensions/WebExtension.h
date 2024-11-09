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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "APIData.h"
#include "APIObject.h"
#include "WebExtensionContentWorldType.h"
#include "WebExtensionMatchPattern.h"
#include <WebCore/FloatSize.h>
#include <WebCore/Icon.h>
#include <WebCore/UserStyleSheetTypes.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/JSONValues.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/spi/cocoa/SecuritySPI.h>

OBJC_CLASS NSBundle;
OBJC_CLASS NSData;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSError;
OBJC_CLASS NSURL;
OBJC_CLASS WKWebExtension;
OBJC_CLASS _WKWebExtensionLocalization;

namespace API {
class Data;
}

namespace WebKit {

class WebExtension : public API::ObjectImpl<API::Object::Type::WebExtension>, public CanMakeWeakPtr<WebExtension> {
    WTF_MAKE_NONCOPYABLE(WebExtension);

public:
    using IconCacheEntry = std::variant<RefPtr<WebCore::Icon>, Vector<double>>;
    using IconsCache = HashMap<String, IconCacheEntry>;
    using Resources = HashMap<String, Ref<API::Data>>;

    template<typename... Args>
    static Ref<WebExtension> create(Args&&... args)
    {
        return adoptRef(*new WebExtension(std::forward<Args>(args)...));
    }

    explicit WebExtension(NSBundle *appExtensionBundle, NSURL *resourceURL, RefPtr<API::Error>&);
    explicit WebExtension(NSDictionary *manifest, Resources&& = { });
    explicit WebExtension(Resources&& = { });

    ~WebExtension() { }

    enum class CacheResult : bool { No, Yes };
    enum class SuppressNotFoundErrors : bool { No, Yes };

    enum class Error : uint8_t {
        Unknown = 1,
        ResourceNotFound,
        InvalidArchive,
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
        InvalidDefaultLocale,
        InvalidDescription,
        InvalidExternallyConnectable,
        InvalidIcon,
        InvalidName,
        InvalidOptionsPage,
        InvalidURLOverrides,
        InvalidVersion,
        InvalidWebAccessibleResources,
    };

    // Keep in sync with WKWebExtensionError values.
    enum class APIError : uint8_t {
        Unknown = 1,
        ResourceNotFound,
        InvalidResourceCodeSignature,
        InvalidManifest,
        UnsupportedManifestVersion,
        InvalidManifestEntry,
        InvalidDeclarativeNetRequestEntry,
        InvalidBackgroundPersistence,
        InvalidArchive,
    };

    enum class InjectionTime : uint8_t {
        DocumentIdle,
        DocumentStart,
        DocumentEnd,
    };

    enum class Environment : bool {
        Document,
        ServiceWorker,
    };

    enum class ColorScheme : uint8_t {
        Light = 1 << 0,
        Dark  = 1 << 1
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
        WebExtensionContentWorldType contentWorldType { WebExtensionContentWorldType::ContentScript };
        WebCore::UserStyleLevel styleLevel { WebCore::UserStyleLevel::Author };

        Vector<String> scriptPaths;
        Vector<String> styleSheetPaths;

        Vector<String> includeGlobPatternStrings;
        Vector<String> excludeGlobPatternStrings;

        Vector<String> expandedIncludeMatchPatternStrings() const;
        Vector<String> expandedExcludeMatchPatternStrings() const;
    };

    struct WebAccessibleResourceData {
        MatchPatternSet matchPatterns;
        Vector<String> resourcePathPatterns;
    };

    struct DeclarativeNetRequestRulesetData {
        String rulesetID;
        bool enabled { false };
        String jsonPath;
    };

    struct LocaleComponents {
        String languageCode;
        String scriptCode;
        String countryCode;
    };

    using CommandsVector = Vector<CommandData>;
    using InjectedContentVector = Vector<InjectedContentData>;
    using WebAccessibleResourcesVector = Vector<WebAccessibleResourceData>;
    using DeclarativeNetRequestRulesetVector = Vector<DeclarativeNetRequestRulesetData>;

    static const PermissionsSet& supportedPermissions();

    bool operator==(const WebExtension& other) const { return (this == &other); }

    bool manifestParsedSuccessfully();
    NSDictionary *manifest();
    RefPtr<const JSON::Object> manifestObject() { return manifestParsedSuccessfully() ? m_manifestJSON->asObject() : nullptr; }
    Ref<API::Data> serializeManifest();

    double manifestVersion();
    bool supportsManifestVersion(double version) { ASSERT(version > 2); return manifestVersion() >= version; }

    Ref<API::Data> serializeLocalization();

    NSBundle *bundle() const { return m_bundle.get(); }
    SecStaticCodeRef bundleStaticCode() const;
    NSData *bundleHash() const;

#if PLATFORM(MAC)
    bool validateResourceData(NSURL *, NSData *, NSError **);
#endif

    bool isWebAccessibleResource(const URL& resourceURL, const URL& pageURL);

    String resourceMIMETypeForPath(const String&);

    String resourceStringForPath(const String&, RefPtr<API::Error>&, CacheResult = CacheResult::No, SuppressNotFoundErrors = SuppressNotFoundErrors::No);
    RefPtr<API::Data> resourceDataForPath(const String&, RefPtr<API::Error>&, CacheResult = CacheResult::No, SuppressNotFoundErrors = SuppressNotFoundErrors::No);

    _WKWebExtensionLocalization *localization();

    const Vector<String>& supportedLocales();
    const String& defaultLocale();
    String bestMatchLocale();

    const String& displayName();
    const String& displayShortName();
    const String& displayVersion();
    const String& displayDescription();
    const String& version();

    const String& contentSecurityPolicy();

    RefPtr<WebCore::Icon> icon(WebCore::FloatSize idealSize);

    RefPtr<WebCore::Icon> actionIcon(WebCore::FloatSize idealSize);
    const String& displayActionLabel();
    const String& actionPopupPath();

    bool hasAction();
    bool hasBrowserAction();
    bool hasPageAction();

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
    bool hasSidebarAction();
    bool hasSidePanel();
    bool hasAnySidebar();
    RefPtr<WebCore::Icon> sidebarIcon(WebCore::FloatSize idealSize);
    const Sring& sidebarDocumentPath();
    const String& sidebarTitle();
#endif

    RefPtr<WebCore::Icon> iconForPath(const String&, RefPtr<API::Error>&, WebCore::FloatSize sizeForResizing = { });

    size_t bestIconSize(const JSON::Object&, size_t idealPixelSize);
    String pathForBestImage(const JSON::Object&, size_t idealPixelSize);

    RefPtr<WebCore::Icon> bestIcon(RefPtr<JSON::Object>, WebCore::FloatSize idealSize, const Function<void(Ref<API::Error>)>&);
    RefPtr<WebCore::Icon> bestIconForManifestKey(const JSON::Object&, const String& manifestKey, WebCore::FloatSize idealSize, IconsCache& cacheLocation, Error, const String& customLocalizedDescription);

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    RefPtr<JSON::Object> bestIconVariantJSONObject(RefPtr<JSON::Array>, size_t idealPixelSize, ColorScheme);
    RefPtr<WebCore::Icon> bestIconVariant(RefPtr<JSON::Array>, WebCore::FloatSize idealSize, const Function<void(Ref<API::Error>)>&);
    RefPtr<WebCore::Icon> bestIconVariantForManifestKey(const JSON::Object&, const String& manifestKey, WebCore::FloatSize idealSize, IconsCache& cacheLocation, Error, const String& customLocalizedDescription);
#endif

    bool hasBackgroundContent();
    bool backgroundContentIsPersistent();
    bool backgroundContentUsesModules();
    bool backgroundContentIsServiceWorker();

    const String& backgroundContentPath();
    const String& generatedBackgroundContent();

    bool hasInspectorBackgroundPage();
    const String& inspectorBackgroundPagePath();

    bool hasOptionsPage();
    bool hasOverrideNewTabPage();

    const String& optionsPagePath();
    const String& overrideNewTabPagePath();

    const CommandsVector& commands();
    bool hasCommands();

    const DeclarativeNetRequestRulesetVector& declarativeNetRequestRulesets();
    std::optional<DeclarativeNetRequestRulesetData> declarativeNetRequestRuleset(const String&);
    bool hasContentModificationRules() { return !declarativeNetRequestRulesets().isEmpty(); }

    const InjectedContentVector& staticInjectedContents();
    bool hasStaticInjectedContentForURL(const URL&);
    bool hasStaticInjectedContent();

    // Permissions requested by the extension in their manifest.
    // These are not the currently allowed permissions.
    const PermissionsSet& requestedPermissions();
    const PermissionsSet& optionalPermissions();

    bool hasRequestedPermission(String);

    // Match patterns requested by the extension in their manifest.
    // These are not the currently allowed permission patterns.
    const MatchPatternSet& requestedPermissionMatchPatterns();
    const MatchPatternSet& optionalPermissionMatchPatterns();

    // Permission patterns requested by the extension in their manifest.
    // These determine which websites the extension can communicate with.
    const MatchPatternSet& externallyConnectableMatchPatterns();

    // Combined pattern set that includes permission patterns and injected content patterns from the manifest.
    MatchPatternSet allRequestedMatchPatterns();

    Ref<API::Error> createError(Error error, const String& customLocalizedDescription = { }, RefPtr<API::Error> underlyingError = nullptr);
    void recordError(Ref<API::Error>);
    void recordErrorIfNeeded(RefPtr<API::Error> error)
    {
        if (error)
            recordError(*error);
    }

    Vector<Ref<API::Error>> errors();

#ifdef __OBJC__
    WKWebExtension *wrapper() const { return (WKWebExtension *)API::ObjectImpl<API::Object::Type::WebExtension>::wrapper(); }
#endif

private:
    bool parseManifest(NSData *);

    void parseWebAccessibleResourcesVersion3();
    void parseWebAccessibleResourcesVersion2();

    void populateDisplayStringsIfNeeded();
    void populateActionPropertiesIfNeeded();
    void populateBackgroundPropertiesIfNeeded();
    void populateInspectorPropertiesIfNeeded();
    void populateContentScriptPropertiesIfNeeded();
    void populatePermissionsPropertiesIfNeeded();
    void populatePagePropertiesIfNeeded();
    void populateContentSecurityPolicyStringsIfNeeded();
    void populateWebAccessibleResourcesIfNeeded();
    void populateCommandsIfNeeded();
    void populateDeclarativeNetRequestPropertiesIfNeeded();
    void populateExternallyConnectableIfNeeded();
#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
    void populateSidebarPropertiesIfNeeded();
    void populateSidebarActionProperties(RetainPtr<NSDictionary>);
    void populateSidePanelProperties(RetainPtr<NSDictionary>);
#endif

    URL resourceFileURLForPath(const String&);

    std::optional<WebExtension::DeclarativeNetRequestRulesetData> parseDeclarativeNetRequestRulesetDictionary(NSDictionary *, RefPtr<API::Error>&);

    InjectedContentVector m_staticInjectedContents;
    WebAccessibleResourcesVector m_webAccessibleResources;
    CommandsVector m_commands;
    DeclarativeNetRequestRulesetVector m_declarativeNetRequestRulesets;

    MatchPatternSet m_permissionMatchPatterns;
    MatchPatternSet m_optionalPermissionMatchPatterns;

    PermissionsSet m_permissions;
    PermissionsSet m_optionalPermissions;

    MatchPatternSet m_externallyConnectableMatchPatterns;

    RetainPtr<NSBundle> m_bundle;
    mutable RetainPtr<SecStaticCodeRef> m_bundleStaticCode;
    URL m_resourceBaseURL;
    RetainPtr<NSDictionary> m_manifest;
    Ref<const JSON::Value> m_manifestJSON;
    Resources m_resources;

    String m_defaultLocale;
    Vector<String> m_supportedLocales;
    RetainPtr<_WKWebExtensionLocalization> m_localization;

    Vector<Ref<API::Error>> m_errors;

    String m_displayName;
    String m_displayShortName;
    String m_displayVersion;
    String m_displayDescription;
    String m_version;

    IconsCache m_iconsCache;

    RefPtr<JSON::Object> m_actionObject;
    IconsCache m_actionIconsCache;
    RefPtr<WebCore::Icon> m_defaultActionIcon;
    String m_displayActionLabel;
    String m_actionPopupPath;

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
    IconsCache m_sidebarIconsCache;
    String m_sidebarDocumentPath;
    String m_sidebarTitle;
#endif

    String m_contentSecurityPolicy;

    Vector<String> m_backgroundScriptPaths;
    String m_backgroundPagePath;
    String m_backgroundServiceWorkerPath;
    String m_generatedBackgroundContent;
    Environment m_backgroundContentEnvironment { Environment::Document };

    String m_inspectorBackgroundPagePath;

    String m_optionsPagePath;
    String m_overrideNewTabPagePath;

#if PLATFORM(MAC)
    bool m_shouldValidateResourceData : 1 { true };
#endif
    bool m_backgroundContentIsPersistent : 1 { false };
    bool m_backgroundContentUsesModules : 1 { false };
    bool m_parsedManifest : 1 { false };
    bool m_parsedManifestDisplayStrings : 1 { false };
    bool m_parsedManifestContentSecurityPolicyStrings : 1 { false };
    bool m_parsedManifestActionProperties : 1 { false };
    bool m_parsedManifestBackgroundProperties : 1 { false };
    bool m_parsedManifestInspectorProperties : 1 { false };
    bool m_parsedManifestContentScriptProperties : 1 { false };
    bool m_parsedManifestPermissionProperties : 1 { false };
    bool m_parsedManifestPageProperties : 1 { false };
    bool m_parsedManifestWebAccessibleResources : 1 { false };
    bool m_parsedManifestCommands : 1 { false };
    bool m_parsedManifestDeclarativeNetRequestRulesets : 1 { false };
    bool m_parsedExternallyConnectable : 1 { false };
#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
    bool m_parsedManifestSidebarProperties : 1 { false };
#endif
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
