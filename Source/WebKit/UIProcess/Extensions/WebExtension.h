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

#include "APIError.h"
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
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/spi/cocoa/SecuritySPI.h>
#endif

OBJC_CLASS NSArray;
OBJC_CLASS NSBundle;
OBJC_CLASS NSData;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSError;
OBJC_CLASS NSLocale;
OBJC_CLASS NSMutableArray;
OBJC_CLASS NSMutableDictionary;
OBJC_CLASS UTType;
OBJC_CLASS WKWebExtension;
OBJC_CLASS WKWebExtensionMatchPattern;
OBJC_CLASS _WKWebExtensionLocalization;

#ifdef __OBJC__
#include "WKWebExtensionPermission.h"
#endif

namespace API {
class Data;
}

namespace WebKit {

class WebExtension : public API::ObjectImpl<API::Object::Type::WebExtension>, public CanMakeWeakPtr<WebExtension> {
    WTF_MAKE_NONCOPYABLE(WebExtension);

public:
    using Resources = HashMap<String, std::span<const uint8_t>>;
    using IconsCacheVariant = std::variant<RefPtr<WebCore::Icon>, HashSet<String>>;
    using IconsCache = HashMap<String, IconsCacheVariant>; // Either store image, or list of scales

    template<typename... Args>
    static Ref<WebExtension> create(Args&&... args)
    {
        return adoptRef(*new WebExtension(std::forward<Args>(args)...));
    }

#if PLATFORM(COCOA)
    explicit WebExtension(NSBundle *appExtensionBundle);
#endif
    explicit WebExtension(const JSON::Value& manifest, const Resources&&);
    explicit WebExtension(const Resources&&);

    static RefPtr<WebExtension> createWithURL(const URL& resourceBaseURL); // Parsing with a URL can fail, but parsing without a URL should never fail.

    ~WebExtension() { }

    enum class CacheResult : bool { No, Yes };
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

    using CommandsVector = Vector<CommandData>;
    using InjectedContentVector = Vector<InjectedContentData>;
    using WebAccessibleResourcesVector = Vector<WebAccessibleResourceData>;
    using DeclarativeNetRequestRulesetVector = Vector<DeclarativeNetRequestRulesetData>;

    static const PermissionsSet& supportedPermissions();

    bool operator==(const WebExtension& other) const { return (this == &other); }

    bool manifestParsedSuccessfully();
    const JSON::Value& manifest();
    Ref<API::Data> serializeManifest();

    double manifestVersion();
    bool supportsManifestVersion(double version) { ASSERT(version > 2); return manifestVersion() >= version; }

    Ref<API::Data> serializeLocalization();

#if PLATFORM(COCOA)
    NSBundle *bundle() const { return m_bundle.get(); }
    SecStaticCodeRef bundleStaticCode() const;
    NSData *bundleHash() const;

    bool validateResourceData(NSURL *, NSData *, NSError **);

    UTType *resourceTypeForPath(StringView);
    NSLocale *defaultLocale();
#endif

    bool isWebAccessibleResource(const URL& resourceURL, const URL& pageURL);

    Expected<String, Ref<API::Error>> resourceStringForPath(const String&, CacheResult = CacheResult::No, SuppressNotFoundErrors = SuppressNotFoundErrors::No);
    Expected<std::span<const uint8_t>, Ref<API::Error>> resourceDataForPath(String, CacheResult = CacheResult::No, SuppressNotFoundErrors = SuppressNotFoundErrors::No);

    _WKWebExtensionLocalization *localization();
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
    bool hasSidebar();
    RefPtr<WebCore::Icon> sidebarIcon(WebCore::FloatSize idealSize);
    const String& sidebarDocumentPath();
    const String& sidebarTitle();
#endif

    Expected<RefPtr<WebCore::Icon>, Ref<API::Error>> imageForPath(String, WebCore::FloatSize sizeForResizing = WebCore::FloatSize());

    size_t bestSizeInIconsDictionary(const JSON::Value&, size_t idealPixelSize);
    std::optional<String> pathForBestImageInIconsDictionary(const JSON::Value&, size_t idealPixelSize);

    RefPtr<WebCore::Icon> bestImageInIconsDictionary(JSON::Value&, WebCore::FloatSize idealSize, const Function<void(RefPtr<API::Error> *)>&);
    RefPtr<WebCore::Icon> bestImageForIconsDictionaryManifestKey(const JSON::Value&, String manifestKey, WebCore::FloatSize idealSize, IconsCache& cacheLocation, Error, const String& customLocalizedDescription);

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    RefPtr<JSON::Value> iconsDictionaryForBestIconVariant(RefPtr<JSON::Value>, size_t idealPixelSize, ColorScheme);
    RefPtr<WebCore::Icon> bestImageForIconVariants(RefPtr<JSON::Value>, WebCore::FloatSize idealSize, const Function<void(RefPtr<API::Error> *)>&);
    RefPtr<WebCore::Icon> bestImageForIconVariantsManifestKey(const JSON::Value&, String manifestKey, WebCore::FloatSize idealSize, IconsCache& cacheLocation, Error, const String& customLocalizedDescription);
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

    bool hasRequestedPermission(const String&) const;

    // Match patterns requested by the extension in their manifest.
    // These are not the currently allowed permission patterns.
    const MatchPatternSet& requestedPermissionMatchPatterns();
    const MatchPatternSet& optionalPermissionMatchPatterns();

    // Permission patterns requested by the extension in their manifest.
    // These determine which websites the extension can communicate with.
    const MatchPatternSet& externallyConnectableMatchPatterns();

    // Combined pattern set that includes permission patterns and injected content patterns from the manifest.
    MatchPatternSet allRequestedMatchPatterns();

    Ref<API::Error> createError(Error, const String& customLocalizedDescription = String(), API::Error* underlyingError = nullptr);

    void recordErrorIfNeeded(RefPtr<API::Error> error) { if (error) recordError(*error); }
    void recordError(Ref<API::Error>);

    Vector<Ref<API::Error>> errors();

#ifdef __OBJC__
    WKWebExtension *wrapper() const { return (WKWebExtension *)API::ObjectImpl<API::Object::Type::WebExtension>::wrapper(); }
#endif

private:
    // This constructor should only be called with the createWithURL function, which parses the manifest before creating the extension.
    explicit WebExtension(const JSON::Value& manifest, const Resources&, const URL& resourceBaseURL);

    void parseWebAccessibleResourcesVersion2();
    void parseWebAccessibleResourcesVersion3();

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

    std::optional<URL> resourceFileURLForPath(StringView);

    std::optional<WebExtension::DeclarativeNetRequestRulesetData> parseDeclarativeNetRequestRulesetDictionary(RefPtr<JSON::Value>, RefPtr<API::Error> *);

    InjectedContentVector m_staticInjectedContents;
    WebAccessibleResourcesVector m_webAccessibleResources;
    CommandsVector m_commands;
    DeclarativeNetRequestRulesetVector m_declarativeNetRequestRulesets;

    MatchPatternSet m_permissionMatchPatterns;
    MatchPatternSet m_optionalPermissionMatchPatterns;

    PermissionsSet m_permissions;
    PermissionsSet m_optionalPermissions;

    MatchPatternSet m_externallyConnectableMatchPatterns;

#if PLATFORM(COCOA)
    RetainPtr<NSBundle> m_bundle;
    mutable RetainPtr<SecStaticCodeRef> m_bundleStaticCode;
#endif

    URL m_resourceBaseURL;
    Ref<const JSON::Value> m_manifest;
    Resources m_resources;

#if PLATFORM(COCOA)
    RetainPtr<NSLocale> m_defaultLocale;
    RetainPtr<_WKWebExtensionLocalization> m_localization;
#endif

    Vector<Ref<API::Error>> m_errors;

    String m_displayName;
    String m_displayShortName;
    String m_displayVersion;
    String m_displayDescription;
    String m_version;

    IconsCache m_iconsCache;

    RefPtr<JSON::Value> m_actionDictionary;
    IconsCache m_actionIconsCache;
    RefPtr<WebCore::Icon> m_defaultActionIcon;
    String m_displayActionLabel;
    String m_actionPopupPath;

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
    IconsCache m_sidebarIconsCache;
    String m_sidebarDocumentPath;
    String m_sidebarTitle;
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

    String m_contentSecurityPolicy;

    Vector<String> m_backgroundScriptPaths;
    String m_backgroundPagePath;
    String m_backgroundServiceWorkerPath;
    String m_generatedBackgroundContent;
    Environment m_backgroundContentEnvironment { Environment::Document };

    String m_inspectorBackgroundPagePath;

    String m_optionsPagePath;
    String m_overrideNewTabPagePath;

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
