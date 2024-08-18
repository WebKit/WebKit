/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include "Site.h"
#include "WebPreferencesDefaultValues.h"
#include "WebURLSchemeHandler.h"
#include <WebCore/ContentSecurityPolicy.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/ShouldRelaxThirdPartyCookieBlocking.h>
#include <WebCore/WritingToolsTypes.h>
#include <wtf/Forward.h>
#include <wtf/GetPtr.h>
#include <wtf/HashMap.h>
#include <wtf/Markable.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
OBJC_PROTOCOL(_UIClickInteractionDriving);
#include <pal/system/ios/UserInterfaceIdiom.h>
#include <wtf/RetainPtr.h>
#else
#include <WebCore/UserInterfaceDirectionPolicy.h>
#endif

#if PLATFORM(VISION) && ENABLE(GAMEPAD)
#include <WebCore/ShouldRequireExplicitConsentForGamepadAccess.h>
#endif

namespace WebKit {
class BrowsingContextGroup;
class VisitedLinkStore;
class WebPageGroup;
class WebPageProxy;
class WebPreferences;
class WebProcessPool;
class WebUserContentControllerProxy;
class WebsiteDataStore;

#if ENABLE(WK_WEB_EXTENSIONS)
class WebExtensionController;
#endif

#if PLATFORM(IOS_FAMILY)
enum class AttributionOverrideTesting : uint8_t {
    NoOverride,
    UserInitiated,
    AppInitiated
};
enum class DragLiftDelay : uint8_t {
    Short,
    Medium,
    Long
};
enum class SelectionGranularity : bool {
    Dynamic,
    Character
};
#endif

}

namespace API {

class ApplicationManifest;
class WebsitePolicies;

class PageConfiguration : public ObjectImpl<Object::Type::PageConfiguration> {
public:
    static Ref<PageConfiguration> create();

    explicit PageConfiguration();
    virtual ~PageConfiguration();

    Ref<PageConfiguration> copy() const;
    void copyDataFrom(const PageConfiguration&);

    WebKit::BrowsingContextGroup& browsingContextGroup() const;
    void setBrowsingContextGroup(RefPtr<WebKit::BrowsingContextGroup>&&);

    struct OpenerInfo {
        Ref<WebKit::WebProcessProxy> process;
        WebKit::Site site;
        WebCore::FrameIdentifier frameID;
        bool operator==(const OpenerInfo&) const;
    };
    const std::optional<OpenerInfo>& openerInfo() const;
    void setOpenerInfo(std::optional<OpenerInfo>&&);

    WebKit::WebProcessPool& processPool() const;
    void setProcessPool(RefPtr<WebKit::WebProcessPool>&&);

    WebKit::WebUserContentControllerProxy& userContentController() const;
    void setUserContentController(RefPtr<WebKit::WebUserContentControllerProxy>&&);

#if ENABLE(WK_WEB_EXTENSIONS)
    const WTF::URL& requiredWebExtensionBaseURL() const;
    void setRequiredWebExtensionBaseURL(WTF::URL&&);

    WebKit::WebExtensionController* webExtensionController() const;
    void setWebExtensionController(RefPtr<WebKit::WebExtensionController>&&);

    WebKit::WebExtensionController* weakWebExtensionController() const;
    void setWeakWebExtensionController(WebKit::WebExtensionController*);
#endif

    WebKit::WebPageGroup* pageGroup();
    void setPageGroup(RefPtr<WebKit::WebPageGroup>&&);

    WebKit::WebPreferences& preferences() const;
    void setPreferences(RefPtr<WebKit::WebPreferences>&&);

    WebKit::WebPageProxy* relatedPage() const;
    void setRelatedPage(WeakPtr<WebKit::WebPageProxy>&&);

    WebKit::WebPageProxy* pageToCloneSessionStorageFrom() const;
    void setPageToCloneSessionStorageFrom(WeakPtr<WebKit::WebPageProxy>&&);

    WebKit::WebPageProxy* alternateWebViewForNavigationGestures() const;
    void setAlternateWebViewForNavigationGestures(WeakPtr<WebKit::WebPageProxy>&&);

    WebKit::VisitedLinkStore& visitedLinkStore() const;
    void setVisitedLinkStore(RefPtr<WebKit::VisitedLinkStore>&&);

    WebKit::WebsiteDataStore& websiteDataStore() const;
    WebKit::WebsiteDataStore* websiteDataStoreIfExists() const;
    Ref<WebKit::WebsiteDataStore> protectedWebsiteDataStore() const;
    void setWebsiteDataStore(RefPtr<WebKit::WebsiteDataStore>&&);

    WebsitePolicies& defaultWebsitePolicies() const;
    void setDefaultWebsitePolicies(RefPtr<WebsitePolicies>&&);

#if PLATFORM(IOS_FAMILY)
    bool canShowWhileLocked() const { return m_data.canShowWhileLocked; }
    void setCanShowWhileLocked(bool canShowWhileLocked) { m_data.canShowWhileLocked = canShowWhileLocked; }

    const RetainPtr<_UIClickInteractionDriving>& clickInteractionDriverForTesting() const { return m_data.clickInteractionDriverForTesting; }
    void setClickInteractionDriverForTesting(RetainPtr<_UIClickInteractionDriving>&& driver) { m_data.clickInteractionDriverForTesting = WTFMove(driver); }

    bool inlineMediaPlaybackRequiresPlaysInlineAttribute() const { return m_data.inlineMediaPlaybackRequiresPlaysInlineAttribute; }
    void setInlineMediaPlaybackRequiresPlaysInlineAttribute(bool requiresAttribute) { m_data.inlineMediaPlaybackRequiresPlaysInlineAttribute = requiresAttribute; }

    bool allowsInlineMediaPlayback() const { return m_data.allowsInlineMediaPlayback; }
    void setAllowsInlineMediaPlayback(bool allows) { m_data.allowsInlineMediaPlayback = allows; }

    bool allowsInlineMediaPlaybackAfterFullscreen() const { return m_data.allowsInlineMediaPlaybackAfterFullscreen; }
    void setAllowsInlineMediaPlaybackAfterFullscreen(bool allows) { m_data.allowsInlineMediaPlaybackAfterFullscreen = allows; }

    WebKit::AttributionOverrideTesting appInitiatedOverrideValueForTesting() const { return m_data.appInitiatedOverrideValueForTesting; }
    void setAppInitiatedOverrideValueForTesting(WebKit::AttributionOverrideTesting appInitiatedOverrideValueForTesting) { m_data.appInitiatedOverrideValueForTesting = appInitiatedOverrideValueForTesting; }

    WebKit::DragLiftDelay dragLiftDelay() const { return m_data.dragLiftDelay; }
    void setDragLiftDelay(WebKit::DragLiftDelay delay) { m_data.dragLiftDelay = delay; }

#if ENABLE(DATA_DETECTION)
    OptionSet<WebCore::DataDetectorType> dataDetectorTypes() const { return m_data.dataDetectorTypes; }
    void setDataDetectorTypes(OptionSet<WebCore::DataDetectorType> types) { m_data.dataDetectorTypes = types; }
#endif

    WebKit::SelectionGranularity selectionGranularity() const { return m_data.selectionGranularity; }
    void setSelectionGranularity(WebKit::SelectionGranularity granularity) { m_data.selectionGranularity = granularity; }

    bool textInteractionGesturesEnabled() const { return m_data.textInteractionGesturesEnabled; }
    void setTextInteractionGesturesEnabled(bool enabled) { m_data.textInteractionGesturesEnabled = enabled; }

    bool allowsPictureInPictureMediaPlayback() const { return m_data.allowsPictureInPictureMediaPlayback; }
    void setAllowsPictureInPictureMediaPlayback(bool allows) { m_data.allowsPictureInPictureMediaPlayback = allows; }

    bool longPressActionsEnabled() const { return m_data.longPressActionsEnabled; }
    void setLongPressActionsEnabled(bool enabled) { m_data.longPressActionsEnabled = enabled; }

    bool systemPreviewEnabled() const { return m_data.systemPreviewEnabled; }
    void setSystemPreviewEnabled(bool enabled) { m_data.systemPreviewEnabled = enabled; }

    bool shouldDecidePolicyBeforeLoadingQuickLookPreview() const { return m_data.shouldDecidePolicyBeforeLoadingQuickLookPreview; }
    void setShouldDecidePolicyBeforeLoadingQuickLookPreview(bool shouldDecide) { m_data.shouldDecidePolicyBeforeLoadingQuickLookPreview = shouldDecide; }
#endif

    bool mediaDataLoadsAutomatically() const { return m_data.mediaDataLoadsAutomatically; }
    void setMediaDataLoadsAutomatically(bool loads) { m_data.mediaDataLoadsAutomatically = loads; }

    bool initialCapitalizationEnabled() { return m_data.initialCapitalizationEnabled; }
    void setInitialCapitalizationEnabled(bool initialCapitalizationEnabled) { m_data.initialCapitalizationEnabled = initialCapitalizationEnabled; }

    std::optional<double> cpuLimit() const { return m_data.cpuLimit; }
    void setCPULimit(double cpuLimit) { m_data.cpuLimit = cpuLimit; }

    bool waitsForPaintAfterViewDidMoveToWindow() const { return m_data.waitsForPaintAfterViewDidMoveToWindow; }
    void setWaitsForPaintAfterViewDidMoveToWindow(bool shouldSynchronize) { m_data.waitsForPaintAfterViewDidMoveToWindow = shouldSynchronize; }

    bool drawsBackground() const { return m_data.drawsBackground; }
    void setDrawsBackground(bool drawsBackground) { m_data.drawsBackground = drawsBackground; }

    bool isControlledByAutomation() const { return m_data.controlledByAutomation; }
    void setControlledByAutomation(bool controlledByAutomation) { m_data.controlledByAutomation = controlledByAutomation; }

    const WTF::String& overrideContentSecurityPolicy() const { return m_data.overrideContentSecurityPolicy; }
    void setOverrideContentSecurityPolicy(const WTF::String& overrideContentSecurityPolicy) { m_data.overrideContentSecurityPolicy = overrideContentSecurityPolicy; }

#if PLATFORM(COCOA)
    ClassStructPtr attachmentFileWrapperClass() const { return m_data.attachmentFileWrapperClass.get(); }
    void setAttachmentFileWrapperClass(ClassStructPtr c) { m_data.attachmentFileWrapperClass = c; }

    const std::optional<Vector<WTF::String>>& additionalSupportedImageTypes() const { return m_data.additionalSupportedImageTypes; }
    void setAdditionalSupportedImageTypes(std::optional<Vector<WTF::String>>&& additionalSupportedImageTypes) { m_data.additionalSupportedImageTypes = WTFMove(additionalSupportedImageTypes); }

    bool clientNavigationsRunAtForegroundPriority() const { return m_data.clientNavigationsRunAtForegroundPriority; }
    void setClientNavigationsRunAtForegroundPriority(bool value) { m_data.clientNavigationsRunAtForegroundPriority = value; }

    uintptr_t mediaTypesRequiringUserActionForPlayback() const { return m_data.mediaTypesRequiringUserActionForPlayback; }
    void setMediaTypesRequiringUserActionForPlayback(uintptr_t types) { m_data.mediaTypesRequiringUserActionForPlayback = types; }
#endif

#if ENABLE(APPLICATION_MANIFEST)
    ApplicationManifest* applicationManifest() const;
    void setApplicationManifest(RefPtr<ApplicationManifest>&&);
#endif

    RefPtr<WebKit::WebURLSchemeHandler> urlSchemeHandlerForURLScheme(const WTF::String&);
    void setURLSchemeHandlerForURLScheme(Ref<WebKit::WebURLSchemeHandler>&&, const WTF::String&);
    const HashMap<WTF::String, Ref<WebKit::WebURLSchemeHandler>>& urlSchemeHandlers() { return m_data.urlSchemeHandlers; }

    const Vector<WTF::String>& corsDisablingPatterns() const { return m_data.corsDisablingPatterns; }
    void setCORSDisablingPatterns(Vector<WTF::String>&& patterns) { m_data.corsDisablingPatterns = WTFMove(patterns); }

    HashSet<WTF::String> maskedURLSchemes() const;
    void setMaskedURLSchemes(HashSet<WTF::String>&& schemes) { m_data.maskedURLSchemesWasSet = true; m_data.maskedURLSchemes = WTFMove(schemes); }

    bool userScriptsShouldWaitUntilNotification() const { return m_data.userScriptsShouldWaitUntilNotification; }
    void setUserScriptsShouldWaitUntilNotification(bool value) { m_data.userScriptsShouldWaitUntilNotification = value; }

    bool crossOriginAccessControlCheckEnabled() const { return m_data.crossOriginAccessControlCheckEnabled; }
    void setCrossOriginAccessControlCheckEnabled(bool enabled) { m_data.crossOriginAccessControlCheckEnabled = enabled; }

    const WTF::String& processDisplayName() const { return m_data.processDisplayName; }
    void setProcessDisplayName(const WTF::String& name) { m_data.processDisplayName = name; }

    bool loadsSubresources() const { return m_data.loadsSubresources; }
    void setLoadsSubresources(bool loads) { m_data.loadsSubresources = loads; }

    const std::optional<MemoryCompactLookupOnlyRobinHoodHashSet<WTF::String>>& allowedNetworkHosts() const { return m_data.allowedNetworkHosts; }
    void setAllowedNetworkHosts(std::optional<MemoryCompactLookupOnlyRobinHoodHashSet<WTF::String>>&& hosts) { m_data.allowedNetworkHosts = WTFMove(hosts); }

#if ENABLE(APP_BOUND_DOMAINS)
    bool ignoresAppBoundDomains() const { return m_data.ignoresAppBoundDomains; }
    void setIgnoresAppBoundDomains(bool shouldIgnore) { m_data.ignoresAppBoundDomains = shouldIgnore; }
    
    bool limitsNavigationsToAppBoundDomains() const { return m_data.limitsNavigationsToAppBoundDomains; }
    void setLimitsNavigationsToAppBoundDomains(bool limits) { m_data.limitsNavigationsToAppBoundDomains = limits; }
#endif

    void setMediaCaptureEnabled(bool value) { m_data.mediaCaptureEnabled = value; }
    bool mediaCaptureEnabled() const { return m_data.mediaCaptureEnabled; }

    void setHTTPSUpgradeEnabled(bool enabled) { m_data.httpsUpgradeEnabled = enabled; }
    bool httpsUpgradeEnabled() const { return m_data.httpsUpgradeEnabled; }

    bool suppressesIncrementalRendering() const { return m_data.suppressesIncrementalRendering; }
    void setSuppressesIncrementalRendering(bool suppresses) { m_data.suppressesIncrementalRendering = suppresses; }

    bool allowsAirPlayForMediaPlayback() const { return m_data.allowsAirPlayForMediaPlayback; }
    void setAllowsAirPlayForMediaPlayback(bool allows) { m_data.allowsAirPlayForMediaPlayback = allows; }

    bool ignoresViewportScaleLimits() const { return m_data.ignoresViewportScaleLimits; }
    void setIgnoresViewportScaleLimits(bool ignores) { m_data.ignoresViewportScaleLimits = ignores; }

#if PLATFORM(MAC)
    bool showsURLsInToolTips() const { return m_data.showsURLsInToolTips; }
    void setShowsURLsInToolTips(bool shows) { m_data.showsURLsInToolTips = shows; }

    bool serviceControlsEnabled() const { return m_data.serviceControlsEnabled; }
    void setServiceControlsEnabled(bool enabled) { m_data.serviceControlsEnabled = enabled; }

    bool imageControlsEnabled() const { return m_data.imageControlsEnabled; }
    void setImageControlsEnabled(bool enabled) { m_data.imageControlsEnabled = enabled; }

    bool contextMenuQRCodeDetectionEnabled() const { return m_data.contextMenuQRCodeDetectionEnabled; }
    void setContextMenuQRCodeDetectionEnabled(bool enabled) { m_data.contextMenuQRCodeDetectionEnabled = enabled; }

    WebCore::UserInterfaceDirectionPolicy userInterfaceDirectionPolicy() const { return m_data.userInterfaceDirectionPolicy; }
    void setUserInterfaceDirectionPolicy(WebCore::UserInterfaceDirectionPolicy policy) { m_data.userInterfaceDirectionPolicy = policy; }
#endif

#if ENABLE(APPLE_PAY)
    bool applePayEnabled() const { return m_data.applePayEnabled; }
    void setApplePayEnabled(bool enabled) { m_data.applePayEnabled = enabled; }
#endif

#if ENABLE(APP_HIGHLIGHTS)
    bool appHighlightsEnabled() const { return m_data.appHighlightsEnabled; }
    void setAppHighlightsEnabled(bool enabled) { m_data.appHighlightsEnabled = enabled; }
#endif

#if ENABLE(MULTI_REPRESENTATION_HEIC)
    bool multiRepresentationHEICInsertionEnabled() const { return m_data.multiRepresentationHEICInsertionEnabled; }
    void setMultiRepresentationHEICInsertionEnabled(bool enabled) { m_data.multiRepresentationHEICInsertionEnabled = enabled; }
#endif

    const WTF::String& groupIdentifier() const { return m_data.groupIdentifier; }
    void setGroupIdentifier(WTF::String&& identifier) { m_data.groupIdentifier = WTFMove(identifier); }

    const WTF::String& mediaContentTypesRequiringHardwareSupport() const { return m_data.mediaContentTypesRequiringHardwareSupport; }
    void setMediaContentTypesRequiringHardwareSupport(WTF::String&& types) { m_data.mediaContentTypesRequiringHardwareSupport = WTFMove(types); }

    const std::optional<WTF::String>& applicationNameForUserAgent() const { return m_data.applicationNameForUserAgent; }
    void setApplicationNameForUserAgent(std::optional<WTF::String>&& name) { m_data.applicationNameForUserAgent = WTFMove(name); }

    double sampledPageTopColorMaxDifference() const { return m_data.sampledPageTopColorMaxDifference; }
    void setSampledPageTopColorMaxDifference(double difference) { m_data.sampledPageTopColorMaxDifference = difference; }

    double sampledPageTopColorMinHeight() const { return m_data.sampledPageTopColorMinHeight; }
    void setSampledPageTopColorMinHeight(double height) { m_data.sampledPageTopColorMinHeight = height; }

    double incrementalRenderingSuppressionTimeout() const { return m_data.incrementalRenderingSuppressionTimeout; }
    void setIncrementalRenderingSuppressionTimeout(double timeout) { m_data.incrementalRenderingSuppressionTimeout = timeout; }

    bool allowsJavaScriptMarkup() const { return m_data.allowsJavaScriptMarkup; }
    void setAllowsJavaScriptMarkup(bool allows) { m_data.allowsJavaScriptMarkup = allows; }

    bool convertsPositionStyleOnCopy() const { return m_data.convertsPositionStyleOnCopy; }
    void setConvertsPositionStyleOnCopy(bool converts) { m_data.convertsPositionStyleOnCopy = converts; }

    bool allowsMetaRefresh() const { return m_data.allowsMetaRefresh; }
    void setAllowsMetaRefresh(bool allows) { m_data.allowsMetaRefresh = allows; }

    bool allowUniversalAccessFromFileURLs() const { return m_data.allowUniversalAccessFromFileURLs; }
    void setAllowUniversalAccessFromFileURLs(bool allow) { m_data.allowUniversalAccessFromFileURLs = allow; }

    bool allowTopNavigationToDataURLs() const { return m_data.allowTopNavigationToDataURLs; }
    void setAllowTopNavigationToDataURLs(bool allow) { m_data.allowTopNavigationToDataURLs = allow; }

    bool needsStorageAccessFromFileURLsQuirk() const { return m_data.needsStorageAccessFromFileURLsQuirk; }
    void setNeedsStorageAccessFromFileURLsQuirk(bool needs) { m_data.needsStorageAccessFromFileURLsQuirk = needs; }

    bool legacyEncryptedMediaAPIEnabled() const { return m_data.legacyEncryptedMediaAPIEnabled; }
    void setLegacyEncryptedMediaAPIEnabled(bool enabled) { m_data.legacyEncryptedMediaAPIEnabled = enabled; }

    bool allowMediaContentTypesRequiringHardwareSupportAsFallback() const { return m_data.allowMediaContentTypesRequiringHardwareSupportAsFallback; }
    void setAllowMediaContentTypesRequiringHardwareSupportAsFallback(bool allow) { m_data.allowMediaContentTypesRequiringHardwareSupportAsFallback = allow; }

    bool colorFilterEnabled() const { return m_data.colorFilterEnabled; }
    void setColorFilterEnabled(bool enabled) { m_data.colorFilterEnabled = enabled; }

    bool incompleteImageBorderEnabled() const { return m_data.incompleteImageBorderEnabled; }
    void setIncompleteImageBorderEnabled(bool enabled) { m_data.incompleteImageBorderEnabled = enabled; }

    bool shouldDeferAsynchronousScriptsUntilAfterDocumentLoad() const { return m_data.shouldDeferAsynchronousScriptsUntilAfterDocumentLoad; }
    void setShouldDeferAsynchronousScriptsUntilAfterDocumentLoad(bool defer) { m_data.shouldDeferAsynchronousScriptsUntilAfterDocumentLoad = defer; }

    bool undoManagerAPIEnabled() const { return m_data.undoManagerAPIEnabled; }
    void setUndoManagerAPIEnabled(bool enabled) { m_data.undoManagerAPIEnabled = enabled; }

    bool mainContentUserGestureOverrideEnabled() const { return m_data.mainContentUserGestureOverrideEnabled; }
    void setMainContentUserGestureOverrideEnabled(bool enabled) { m_data.mainContentUserGestureOverrideEnabled = enabled; }

    bool invisibleAutoplayForbidden() const { return m_data.invisibleAutoplayForbidden; }
    void setInvisibleAutoplayForbidden(bool forbidden) { m_data.invisibleAutoplayForbidden = forbidden; }

    bool attachmentElementEnabled() const { return m_data.attachmentElementEnabled; }
    void setAttachmentElementEnabled(bool enabled) { m_data.attachmentElementEnabled = enabled; }

    bool attachmentWideLayoutEnabled() const { return m_data.attachmentWideLayoutEnabled; }
    void setAttachmentWideLayoutEnabled(bool enabled) { m_data.attachmentWideLayoutEnabled = enabled; }

    bool allowsInlinePredictions() const { return m_data.allowsInlinePredictions; }
    void setAllowsInlinePredictions(bool allows) { m_data.allowsInlinePredictions = allows; }

#if ENABLE(WRITING_TOOLS)
    WebCore::WritingTools::Behavior writingToolsBehavior() const { return m_data.writingToolsBehavior; }
    void setWritingToolsBehavior(WebCore::WritingTools::Behavior behavior) { m_data.writingToolsBehavior = behavior; }
#endif

    void setShouldRelaxThirdPartyCookieBlocking(WebCore::ShouldRelaxThirdPartyCookieBlocking value) { m_data.shouldRelaxThirdPartyCookieBlocking = value; }
    WebCore::ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking() const { return m_data.shouldRelaxThirdPartyCookieBlocking; }

    void setAttributedBundleIdentifier(WTF::String&& identifier) { m_data.attributedBundleIdentifier = WTFMove(identifier); }
    const WTF::String& attributedBundleIdentifier() const { return m_data.attributedBundleIdentifier; }

#if HAVE(TOUCH_BAR)
    bool requiresUserActionForEditingControlsManager() const { return m_data.requiresUserActionForEditingControlsManager; }
    void setRequiresUserActionForEditingControlsManager(bool value) { m_data.requiresUserActionForEditingControlsManager = value; }
#endif

    bool isLockdownModeExplicitlySet() const;
    bool lockdownModeEnabled() const;
    
    void setAllowTestOnlyIPC(bool enabled) { m_data.allowTestOnlyIPC = enabled; }
    bool allowTestOnlyIPC() const { return m_data.allowTestOnlyIPC; }

    bool scrollToTextFragmentIndicatorEnabled () const { return m_data.scrollToTextFragmentIndicatorEnabled; }
    void setScrollToTextFragmentIndicatorEnabled(bool enabled) { m_data.scrollToTextFragmentIndicatorEnabled = enabled; }

    bool scrollToTextFragmentMarkingEnabled() const { return m_data.scrollToTextFragmentMarkingEnabled; }
    void setScrollToTextFragmentMarkingEnabled(bool enabled) { m_data.scrollToTextFragmentMarkingEnabled = enabled; }

    void setPortsForUpgradingInsecureSchemeForTesting(uint16_t upgradeFromInsecurePort, uint16_t upgradeToSecurePort) { m_data.portsForUpgradingInsecureSchemeForTesting = { upgradeFromInsecurePort, upgradeToSecurePort }; }
    std::optional<std::pair<uint16_t, uint16_t>> portsForUpgradingInsecureSchemeForTesting() const { return m_data.portsForUpgradingInsecureSchemeForTesting; }

    void setDelaysWebProcessLaunchUntilFirstLoad(bool);
    bool delaysWebProcessLaunchUntilFirstLoad() const;

    void setContentSecurityPolicyModeForExtension(WebCore::ContentSecurityPolicyModeForExtension mode) { m_data.contentSecurityPolicyModeForExtension = mode; }
    WebCore::ContentSecurityPolicyModeForExtension contentSecurityPolicyModeForExtension() const { return m_data.contentSecurityPolicyModeForExtension; }

#if PLATFORM(VISION) && ENABLE(GAMEPAD)
    WebCore::ShouldRequireExplicitConsentForGamepadAccess gamepadAccessRequiresExplicitConsent() const { return m_data.gamepadAccessRequiresExplicitConsent; }
    void setGamepadAccessRequiresExplicitConsent(WebCore::ShouldRequireExplicitConsentForGamepadAccess value) { m_data.gamepadAccessRequiresExplicitConsent = value; }
#endif

private:
    struct Data {
        template<typename T, Ref<T>(*initializer)()> class LazyInitializedRef {
        public:
            LazyInitializedRef() = default;
            void operator=(const LazyInitializedRef& other) { m_value = &other.get(); }
            void operator=(RefPtr<T>&& t) { m_value = WTFMove(t); }
            T& get() const
            {
                if (!m_value)
                    m_value = initializer();
                return *m_value;
            }
            T* getIfExists() const { return m_value.get(); }
        private:
            mutable RefPtr<T> m_value;
        };
        static Ref<WebKit::BrowsingContextGroup> createBrowsingContextGroup();
        static Ref<WebKit::WebProcessPool> createWebProcessPool();
        static Ref<WebKit::WebUserContentControllerProxy> createWebUserContentControllerProxy();
        static Ref<WebKit::WebPreferences> createWebPreferences();
        static Ref<WebKit::VisitedLinkStore> createVisitedLinkStore();
        static Ref<WebsitePolicies> createWebsitePolicies();
#if PLATFORM(IOS_FAMILY)
        static bool defaultShouldDecidePolicyBeforeLoadingQuickLookPreview();
        static WebKit::DragLiftDelay defaultDragLiftDelay();
#endif
#if PLATFORM(COCOA)
        uintptr_t defaultMediaTypesRequiringUserActionForPlayback();
#endif

        LazyInitializedRef<WebKit::BrowsingContextGroup, createBrowsingContextGroup> browsingContextGroup;
        LazyInitializedRef<WebKit::WebProcessPool, createWebProcessPool> processPool;
        LazyInitializedRef<WebKit::WebUserContentControllerProxy, createWebUserContentControllerProxy> userContentController;
        LazyInitializedRef<WebKit::WebPreferences, createWebPreferences> preferences;
        LazyInitializedRef<WebKit::VisitedLinkStore, createVisitedLinkStore> visitedLinkStore;
        LazyInitializedRef<WebsitePolicies, createWebsitePolicies> defaultWebsitePolicies;
        mutable RefPtr<WebKit::WebsiteDataStore> websiteDataStore;

#if ENABLE(WK_WEB_EXTENSIONS)
        WTF::URL requiredWebExtensionBaseURL;
        RefPtr<WebKit::WebExtensionController> webExtensionController;
        WeakPtr<WebKit::WebExtensionController> weakWebExtensionController;
#endif
        RefPtr<WebKit::WebPageGroup> pageGroup;
        WeakPtr<WebKit::WebPageProxy> relatedPage;
        std::optional<OpenerInfo> openerInfo;
        WeakPtr<WebKit::WebPageProxy> pageToCloneSessionStorageFrom;
        WeakPtr<WebKit::WebPageProxy> alternateWebViewForNavigationGestures;

#if PLATFORM(IOS_FAMILY)
        bool canShowWhileLocked { false };
        WebKit::AttributionOverrideTesting appInitiatedOverrideValueForTesting { WebKit::AttributionOverrideTesting::NoOverride };
        RetainPtr<_UIClickInteractionDriving> clickInteractionDriverForTesting;
        bool allowsInlineMediaPlayback { !PAL::currentUserInterfaceIdiomIsSmallScreen() };
        bool inlineMediaPlaybackRequiresPlaysInlineAttribute { !allowsInlineMediaPlayback };
        bool allowsInlineMediaPlaybackAfterFullscreen { !allowsInlineMediaPlayback };
        bool mediaDataLoadsAutomatically { allowsInlineMediaPlayback };
        WebKit::DragLiftDelay dragLiftDelay { defaultDragLiftDelay() };
#if ENABLE(DATA_DETECTION)
        OptionSet<WebCore::DataDetectorType> dataDetectorTypes;
#endif
        WebKit::SelectionGranularity selectionGranularity { WebKit::SelectionGranularity::Dynamic };
#if PLATFORM(WATCHOS)
        bool allowsPictureInPictureMediaPlayback { false };
        bool textInteractionGesturesEnabled { false };
        bool longPressActionsEnabled { false };
#else // PLATFORM(WATCHOS)
        bool allowsPictureInPictureMediaPlayback { true };
        bool textInteractionGesturesEnabled { true };
        bool longPressActionsEnabled { true };
#endif // PLATFORM(WATCHOS)
        bool systemPreviewEnabled { false };
        bool shouldDecidePolicyBeforeLoadingQuickLookPreview { defaultShouldDecidePolicyBeforeLoadingQuickLookPreview() };
#else // PLATFORM(IOS_FAMILY)
        bool mediaDataLoadsAutomatically { true };
#endif // PLATFORM(IOS_FAMILY)
        bool initialCapitalizationEnabled { true };
        bool waitsForPaintAfterViewDidMoveToWindow { true };
        bool drawsBackground { true };
        bool controlledByAutomation { false };
        bool allowTestOnlyIPC { false };
        std::optional<bool> delaysWebProcessLaunchUntilFirstLoad;
        std::optional<double> cpuLimit;
        std::optional<std::pair<uint16_t, uint16_t>> portsForUpgradingInsecureSchemeForTesting;

        WTF::String overrideContentSecurityPolicy;

#if PLATFORM(COCOA)
        RetainPtr<ClassStructPtr> attachmentFileWrapperClass;
        std::optional<WTF::Vector<WTF::String>> additionalSupportedImageTypes;
        bool clientNavigationsRunAtForegroundPriority { true };
        uintptr_t mediaTypesRequiringUserActionForPlayback { defaultMediaTypesRequiringUserActionForPlayback() };
#endif

#if ENABLE(APPLICATION_MANIFEST)
        RefPtr<ApplicationManifest> applicationManifest;
#endif

        HashMap<WTF::String, Ref<WebKit::WebURLSchemeHandler>> urlSchemeHandlers;
        Vector<WTF::String> corsDisablingPatterns;
        HashSet<WTF::String> maskedURLSchemes;
        bool maskedURLSchemesWasSet { false };
        bool userScriptsShouldWaitUntilNotification { true };
        bool crossOriginAccessControlCheckEnabled { true };
        WTF::String processDisplayName;
        bool loadsSubresources { true };
        std::optional<MemoryCompactLookupOnlyRobinHoodHashSet<WTF::String>> allowedNetworkHosts;

#if ENABLE(APP_BOUND_DOMAINS)
        bool ignoresAppBoundDomains { false };
        bool limitsNavigationsToAppBoundDomains { false };
#endif

        bool mediaCaptureEnabled { false };
        bool httpsUpgradeEnabled { true };
        bool suppressesIncrementalRendering { false };
        bool ignoresViewportScaleLimits { false };
        bool allowsAirPlayForMediaPlayback { true };

#if PLATFORM(MAC)
        bool showsURLsInToolTips { false };
        bool serviceControlsEnabled { false };
        bool imageControlsEnabled { false };
        bool contextMenuQRCodeDetectionEnabled { false };
        WebCore::UserInterfaceDirectionPolicy userInterfaceDirectionPolicy { WebCore::UserInterfaceDirectionPolicy::Content };
#endif
#if ENABLE(APPLE_PAY)
        bool applePayEnabled { DEFAULT_VALUE_FOR_ApplePayEnabled };
#endif
#if ENABLE(APP_HIGHLIGHTS)
        bool appHighlightsEnabled { DEFAULT_VALUE_FOR_AppHighlightsEnabled };
#endif
#if ENABLE(MULTI_REPRESENTATION_HEIC)
        bool multiRepresentationHEICInsertionEnabled { false };
#endif
        WTF::String groupIdentifier;
        WTF::String mediaContentTypesRequiringHardwareSupport;
        std::optional<WTF::String> applicationNameForUserAgent;
        double sampledPageTopColorMaxDifference { DEFAULT_VALUE_FOR_SampledPageTopColorMaxDifference };
        double sampledPageTopColorMinHeight { DEFAULT_VALUE_FOR_SampledPageTopColorMinHeight };
        double incrementalRenderingSuppressionTimeout { 5 };
        bool allowsJavaScriptMarkup { true };
        bool convertsPositionStyleOnCopy { false };
        bool allowsMetaRefresh { true };
        bool allowUniversalAccessFromFileURLs { false };
        bool allowTopNavigationToDataURLs { false };
        bool needsStorageAccessFromFileURLsQuirk { true };
        bool legacyEncryptedMediaAPIEnabled { true };
        bool allowMediaContentTypesRequiringHardwareSupportAsFallback { true };
        bool colorFilterEnabled { false };
        bool incompleteImageBorderEnabled { false };
        bool shouldDeferAsynchronousScriptsUntilAfterDocumentLoad { true };
        bool undoManagerAPIEnabled { false };
        bool mainContentUserGestureOverrideEnabled { false };
        bool invisibleAutoplayForbidden { false };
        bool attachmentElementEnabled { false };
        bool attachmentWideLayoutEnabled { false };
        bool allowsInlinePredictions { false };
        bool scrollToTextFragmentIndicatorEnabled { true };
        bool scrollToTextFragmentMarkingEnabled { true };
#if PLATFORM(VISION) && ENABLE(GAMEPAD)
        WebCore::ShouldRequireExplicitConsentForGamepadAccess gamepadAccessRequiresExplicitConsent { WebCore::ShouldRequireExplicitConsentForGamepadAccess::No };
#endif

#if ENABLE(WRITING_TOOLS)
        WebCore::WritingTools::Behavior writingToolsBehavior { WebCore::WritingTools::Behavior::Default };
#endif

        WebCore::ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking { WebCore::ShouldRelaxThirdPartyCookieBlocking::No };
        WTF::String attributedBundleIdentifier;

#if HAVE(TOUCH_BAR)
        bool requiresUserActionForEditingControlsManager { false };
#endif

        WebCore::ContentSecurityPolicyModeForExtension contentSecurityPolicyModeForExtension { WebCore::ContentSecurityPolicyModeForExtension::None };
    };

    // All data members should be added to the Data structure to avoid breaking PageConfiguration::copy().
    Data m_data;
};

} // namespace API
