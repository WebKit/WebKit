/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include <WebCore/ChromeClient.h>

namespace WebCore {
class HTMLImageElement;
class RegistrableDomain;
enum class CookieConsentDecisionResult : uint8_t;
enum class StorageAccessPromptWasShown : bool;
enum class StorageAccessWasGranted : bool;
struct TextRecognitionOptions;
}

namespace WebKit {

class WebFrame;
class WebPage;

class WebChromeClient final : public WebCore::ChromeClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebChromeClient(WebPage&);

    WebPage& page() const { return m_page; }

private:
    ~WebChromeClient();

    void didInsertMenuElement(WebCore::HTMLMenuElement&) final;
    void didRemoveMenuElement(WebCore::HTMLMenuElement&) final;
    void didInsertMenuItemElement(WebCore::HTMLMenuItemElement&) final;
    void didRemoveMenuItemElement(WebCore::HTMLMenuItemElement&) final;

    void chromeDestroyed() final;
    
    void setWindowRect(const WebCore::FloatRect&) final;
    WebCore::FloatRect windowRect() final;
    
    WebCore::FloatRect pageRect() final;
    
    void focus() final;
    void unfocus() final;
    
    bool canTakeFocus(WebCore::FocusDirection) final;
    void takeFocus(WebCore::FocusDirection) final;

    void focusedElementChanged(WebCore::Element*) final;
    void focusedFrameChanged(WebCore::Frame*) final;

    // The Frame pointer provides the ChromeClient with context about which
    // Frame wants to create the new Page.  Also, the newly created window
    // should not be shown to the user until the ChromeClient of the newly
    // created Page has its show method called.
    WebCore::Page* createWindow(WebCore::Frame&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&) final;
    void show() final;
    
    bool canRunModal() final;
    void runModal() final;

    void reportProcessCPUTime(Seconds, WebCore::ActivityStateForCPUSampling) final;
    
    void setToolbarsVisible(bool) final;
    bool toolbarsVisible() final;
    
    void setStatusbarVisible(bool) final;
    bool statusbarVisible() final;
    
    void setScrollbarsVisible(bool) final;
    bool scrollbarsVisible() final;
    
    void setMenubarVisible(bool) final;
    bool menubarVisible() final;
    
    void setResizable(bool) final;
    
    void addMessageToConsole(JSC::MessageSource, JSC::MessageLevel, const String& message, unsigned lineNumber, unsigned columnNumber, const String& sourceID) final;
    void addMessageWithArgumentsToConsole(JSC::MessageSource, JSC::MessageLevel, const String& message, Span<const String> messageArguments, unsigned lineNumber, unsigned columnNumber, const String& sourceID) final;
    
    bool canRunBeforeUnloadConfirmPanel() final;
    bool runBeforeUnloadConfirmPanel(const String& message, WebCore::Frame&) final;
    
    void closeWindow() final;
    
    void runJavaScriptAlert(WebCore::Frame&, const String&) final;
    bool runJavaScriptConfirm(WebCore::Frame&, const String&) final;
    bool runJavaScriptPrompt(WebCore::Frame&, const String& message, const String& defaultValue, String& result) final;
    void setStatusbarText(const String&) final;

    WebCore::KeyboardUIMode keyboardUIMode() final;

    bool hoverSupportedByPrimaryPointingDevice() const final;
    bool hoverSupportedByAnyAvailablePointingDevice() const final;
    std::optional<WebCore::PointerCharacteristics> pointerCharacteristicsOfPrimaryPointingDevice() const final;
    OptionSet<WebCore::PointerCharacteristics> pointerCharacteristicsOfAllAvailablePointingDevices() const final;

    // HostWindow member function finals.
    void invalidateRootView(const WebCore::IntRect&) final;
    void invalidateContentsAndRootView(const WebCore::IntRect&) final;
    void invalidateContentsForSlowScroll(const WebCore::IntRect&) final;
    void scroll(const WebCore::IntSize& scrollDelta, const WebCore::IntRect& scrollRect, const WebCore::IntRect& clipRect) final;

    WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) const final;
    WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) const final;

    WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&) const final;
    WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&) const final;

    void didFinishLoadingImageForElement(WebCore::HTMLImageElement&) final;

    PlatformPageClient platformPageClient() const final;
    void contentsSizeChanged(WebCore::Frame&, const WebCore::IntSize&) const final;
    void intrinsicContentsSizeChanged(const WebCore::IntSize&) const final;

    void scrollContainingScrollViewsToRevealRect(const WebCore::IntRect&) const final; // Currently only Mac has a non empty implementation.
    void scrollMainFrameToRevealRect(const WebCore::IntRect&) const final;

    bool shouldUnavailablePluginMessageBeButton(WebCore::RenderEmbeddedObject::PluginUnavailabilityReason) const final;
    void unavailablePluginButtonClicked(WebCore::Element&, WebCore::RenderEmbeddedObject::PluginUnavailabilityReason) const final;

    void mouseDidMoveOverElement(const WebCore::HitTestResult&, unsigned modifierFlags, const String& toolTip, WebCore::TextDirection) final;

    void print(WebCore::Frame&, const WebCore::StringWithDirection&) final;

    void exceededDatabaseQuota(WebCore::Frame&, const String& databaseName, WebCore::DatabaseDetails) final { }

    void reachedMaxAppCacheSize(int64_t spaceNeeded) final;
    void reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin&, int64_t spaceNeeded) final;
    
#if ENABLE(INPUT_TYPE_COLOR)
    std::unique_ptr<WebCore::ColorChooser> createColorChooser(WebCore::ColorChooserClient&, const WebCore::Color&) final;
#endif

#if ENABLE(DATALIST_ELEMENT)
    std::unique_ptr<WebCore::DataListSuggestionPicker> createDataListSuggestionPicker(WebCore::DataListSuggestionsClient&) final;
    bool canShowDataListSuggestionLabels() const final;
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    std::unique_ptr<WebCore::DateTimeChooser> createDateTimeChooser(WebCore::DateTimeChooserClient&) final;
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    void didPreventDefaultForEvent() final;
#endif

#if PLATFORM(IOS_FAMILY)
    void didReceiveMobileDocType(bool) final;
    void setNeedsScrollNotifications(WebCore::Frame&, bool) final;
    void didFinishContentChangeObserving(WebCore::Frame&, WKContentChange) final;
    void notifyRevealedSelectionByScrollingFrame(WebCore::Frame&) final;
    bool isStopping() final;

    void didLayout(LayoutType = NormalLayout) final;
    void didStartOverflowScroll() final;
    void didEndOverflowScroll() final;
    bool hasStablePageScaleFactor() const final;

    // FIXME: See <rdar://problem/5975559>
    void suppressFormNotifications() final;
    void restoreFormNotifications() final;

    void addOrUpdateScrollingLayer(WebCore::Node*, PlatformLayer* scrollingLayer, PlatformLayer* contentsLayer, const WebCore::IntSize& scrollSize, bool allowHorizontalScrollbar, bool allowVerticalScrollbar) final;
    void removeScrollingLayer(WebCore::Node*, PlatformLayer* scrollingLayer, PlatformLayer* contentsLayer) final;

    void webAppOrientationsUpdated() final;
    void showPlaybackTargetPicker(bool hasVideo, WebCore::RouteSharingPolicy, const String&) final;

    Seconds eventThrottlingDelay() final;

    bool shouldUseMouseEventForSelection(const WebCore::PlatformMouseEvent&) final;

    bool showDataDetectorsUIForElement(const WebCore::Element&, const WebCore::Event&) final;
#endif

#if ENABLE(ORIENTATION_EVENTS)
    int deviceOrientation() const final;
#endif

    void runOpenPanel(WebCore::Frame&, WebCore::FileChooser&) final;
    void showShareSheet(WebCore::ShareDataWithParsedURL&, WTF::CompletionHandler<void(bool)>&&) final;
    void showContactPicker(const WebCore::ContactsRequestData&, WTF::CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&&) final;
    void loadIconForFiles(const Vector<String>&, WebCore::FileIconLoader&) final;

    void setCursor(const WebCore::Cursor&) final;
    void setCursorHiddenUntilMouseMoves(bool) final;
#if !HAVE(NSCURSOR) && !PLATFORM(GTK)
    bool supportsSettingCursor() final { return false; }
#endif

#if ENABLE(POINTER_LOCK)
    bool requestPointerLock() final;
    void requestPointerUnlock() final;
#endif

    void didAssociateFormControls(const Vector<RefPtr<WebCore::Element>>&, WebCore::Frame&) final;
    bool shouldNotifyOnFormChanges() final;

    bool selectItemWritingDirectionIsNatural() final;
    bool selectItemAlignmentFollowsMenuWritingDirection() final;
    RefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient&) const final;
    RefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient&) const final;

    WebCore::GraphicsLayerFactory* graphicsLayerFactory() const final;
    void attachRootGraphicsLayer(WebCore::Frame&, WebCore::GraphicsLayer*) final;
    void attachViewOverlayGraphicsLayer(WebCore::GraphicsLayer*) final;
    void setNeedsOneShotDrawingSynchronization() final;
    bool shouldTriggerRenderingUpdate(unsigned rescheduledRenderingUpdateCount) const final;
    void triggerRenderingUpdate() final;
    unsigned remoteImagesCountForTesting() const final; 

    void contentRuleListNotification(const URL&, const WebCore::ContentRuleListResults&) final;

    bool testProcessIncomingSyncMessagesWhenWaitingForSyncReply() final;

#if PLATFORM(WIN)
    void setLastSetCursorToCurrentCursor() final { }
    void AXStartFrameLoad() final { }
    void AXFinishFrameLoad() final { }
#endif

    void animationDidFinishForElement(const WebCore::Element&) final;

    WebCore::DisplayRefreshMonitorFactory* displayRefreshMonitorFactory() const final;

#if ENABLE(GPU_PROCESS)
    RefPtr<WebCore::ImageBuffer> createImageBuffer(const WebCore::FloatSize&, WebCore::RenderingMode, WebCore::RenderingPurpose, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::PixelFormat, bool avoidBackendSizeCheck = false) const final;
    RefPtr<WebCore::ImageBuffer> sinkIntoImageBuffer(std::unique_ptr<WebCore::SerializedImageBuffer>) final;
#endif
    std::unique_ptr<WebCore::WorkerClient> createWorkerClient(SerialFunctionDispatcher&) final;

#if ENABLE(WEBGL)
    RefPtr<WebCore::GraphicsContextGL> createGraphicsContextGL(const WebCore::GraphicsContextGLAttributes&) const final;
#endif

    RefPtr<PAL::WebGPU::GPU> createGPUForWebGPU() const final;

    CompositingTriggerFlags allowedCompositingTriggers() const final
    {
        return static_cast<CompositingTriggerFlags>(
            ThreeDTransformTrigger |
            VideoTrigger |
            PluginTrigger|
            CanvasTrigger |
#if PLATFORM(COCOA) || USE(NICOSIA)
            ScrollableNonMainFrameTrigger |
#endif
#if PLATFORM(IOS_FAMILY)
            AnimatedOpacityTrigger | // Allow opacity animations to trigger compositing mode for iPhone: <rdar://problem/7830677>
#endif
            AnimationTrigger);
    }

    bool layerTreeStateIsFrozen() const final;

#if ENABLE(ASYNC_SCROLLING)
    RefPtr<WebCore::ScrollingCoordinator> createScrollingCoordinator(WebCore::Page&) const final;
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    void prepareForVideoFullscreen() final;
    bool canEnterVideoFullscreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode) const final;
    bool supportsVideoFullscreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode) final;
    bool supportsVideoFullscreenStandby() final;
    void setMockVideoPresentationModeEnabled(bool) final;
    void enterVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode, bool standby) final;
    void exitVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&, WTF::CompletionHandler<void(bool)>&& = [](bool) { }) final;
    void setUpPlaybackControlsManager(WebCore::HTMLMediaElement&) final;
    void clearPlaybackControlsManager() final;
    void playbackControlsMediaEngineChanged() final;
#endif

#if ENABLE(MEDIA_USAGE)
    void addMediaUsageManagerSession(WebCore::MediaSessionIdentifier, const String&, const URL&) final;
    void updateMediaUsageManagerSessionState(WebCore::MediaSessionIdentifier, const WebCore::MediaUsageInfo&) final;
    void removeMediaUsageManagerSession(WebCore::MediaSessionIdentifier) final;
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    void exitVideoFullscreenToModeWithoutAnimation(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode) final;
#endif

#if ENABLE(FULLSCREEN_API)
    bool supportsFullScreenForElement(const WebCore::Element&, bool withKeyboard) final;
    void enterFullScreenForElement(WebCore::Element&) final;
    void exitFullScreenForElement(WebCore::Element*) final;
#endif

#if PLATFORM(COCOA)
    void elementDidFocus(WebCore::Element&, const WebCore::FocusOptions&) final;
    void elementDidBlur(WebCore::Element&) final;
    void elementDidRefocus(WebCore::Element&, const WebCore::FocusOptions&) final;
    void focusedElementDidChangeInputMode(WebCore::Element&, WebCore::InputMode) final;

    void makeFirstResponder() final;
    void assistiveTechnologyMakeFirstResponder() final;
#endif

    void enableSuddenTermination() final;
    void disableSuddenTermination() final;

#if PLATFORM(IOS_FAMILY)
    WebCore::FloatSize screenSize() const final;
    WebCore::FloatSize availableScreenSize() const final;
    WebCore::FloatSize overrideScreenSize() const final;
#endif

    void dispatchDisabledAdaptationsDidChange(const OptionSet<WebCore::DisabledAdaptations>&) const final;
    void dispatchViewportPropertiesDidChange(const WebCore::ViewportArguments&) const final;

    void notifyScrollerThumbIsVisibleInRect(const WebCore::IntRect&) final;
    void recommendedScrollbarStyleDidChange(WebCore::ScrollbarStyle newStyle) final;

    std::optional<WebCore::ScrollbarOverlayStyle> preferredScrollbarOverlayStyle() final;

    WebCore::Color underlayColor() const final;

    void themeColorChanged() const final;
    void pageExtendedBackgroundColorDidChange() const final;
    void sampledPageTopColorChanged() const final;
    
#if ENABLE(APP_HIGHLIGHTS)
    WebCore::HighlightVisibility appHighlightsVisiblility() const final;
#endif
    
    void wheelEventHandlersChanged(bool) final;

    String plugInStartLabelTitle(const String& mimeType) const final;
    String plugInStartLabelSubtitle(const String& mimeType) const final;
    String plugInExtraStyleSheet() const final;
    String plugInExtraScript() const final;

    void didAddHeaderLayer(WebCore::GraphicsLayer&) final;
    void didAddFooterLayer(WebCore::GraphicsLayer&) final;

    bool shouldUseTiledBackingForFrameView(const WebCore::FrameView&) const final;

    void isPlayingMediaDidChange(WebCore::MediaProducerMediaStateFlags) final;
    void handleAutoplayEvent(WebCore::AutoplayEvent, OptionSet<WebCore::AutoplayEventFlags>) final;

#if ENABLE(APP_HIGHLIGHTS)
    void storeAppHighlight(WebCore::AppHighlight&&) const final;
#endif

    void setTextIndicator(const WebCore::TextIndicatorData&) const final;

#if ENABLE(WEB_CRYPTO)
    bool wrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) const final;
    bool unwrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) const final;
#endif

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)
    void handleTelephoneNumberClick(const String& number, const WebCore::IntPoint&, const WebCore::IntRect&) final;
#endif

#if ENABLE(DATA_DETECTION)
    void handleClickForDataDetectionResult(const WebCore::DataDetectorElementInfo&, const WebCore::IntPoint&) final;
#endif

#if ENABLE(SERVICE_CONTROLS)
    void handleSelectionServiceClick(WebCore::FrameSelection&, const Vector<String>& telephoneNumbers, const WebCore::IntPoint&) final;
    bool hasRelevantSelectionServices(bool isTextOnly) const final;
    void handleImageServiceClick(const WebCore::IntPoint&, WebCore::Image&, WebCore::HTMLImageElement&) final;
    void handlePDFServiceClick(const WebCore::IntPoint&, WebCore::HTMLAttachmentElement&);
#endif

    bool shouldDispatchFakeMouseMoveEvents() const final;

    void handleAutoFillButtonClick(WebCore::HTMLInputElement&) final;

    void inputElementDidResignStrongPasswordAppearance(WebCore::HTMLInputElement&) final;

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
    void addPlaybackTargetPickerClient(WebCore::PlaybackTargetClientContextIdentifier) final;
    void removePlaybackTargetPickerClient(WebCore::PlaybackTargetClientContextIdentifier) final;
    void showPlaybackTargetPicker(WebCore::PlaybackTargetClientContextIdentifier, const WebCore::IntPoint&, bool) final;
    void playbackTargetPickerClientStateDidChange(WebCore::PlaybackTargetClientContextIdentifier, WebCore::MediaProducerMediaStateFlags) final;
    void setMockMediaPlaybackTargetPickerEnabled(bool) final;
    void setMockMediaPlaybackTargetPickerState(const String&, WebCore::MediaPlaybackTargetContext::MockState) final;
    void mockMediaPlaybackTargetPickerDismissPopup() final;
#endif

    void imageOrMediaDocumentSizeChanged(const WebCore::IntSize&) final;

    RefPtr<WebCore::Icon> createIconForFiles(const Vector<String>& filenames) final;

#if ENABLE(VIDEO) && USE(GSTREAMER)
    void requestInstallMissingMediaPlugins(const String& /*details*/, const String& /*description*/, WebCore::MediaPlayerRequestInstallMissingPluginsCallback&) final;
#endif

    void didInvalidateDocumentMarkerRects() final;

#if ENABLE(TRACKING_PREVENTION)
    void hasStorageAccess(WebCore::RegistrableDomain&& subFrameDomain, WebCore::RegistrableDomain&& topFrameDomain, WebCore::Frame&, WTF::CompletionHandler<void(bool)>&&) final;
    void requestStorageAccess(WebCore::RegistrableDomain&& subFrameDomain, WebCore::RegistrableDomain&& topFrameDomain, WebCore::Frame&, WebCore::StorageAccessScope, WTF::CompletionHandler<void(WebCore::RequestStorageAccessResult)>&&) final;
    bool hasPageLevelStorageAccess(const WebCore::RegistrableDomain& topLevelDomain, const WebCore::RegistrableDomain& resourceDomain) const final;
#endif

#if ENABLE(DEVICE_ORIENTATION)
    void shouldAllowDeviceOrientationAndMotionAccess(WebCore::Frame&, bool mayPrompt, CompletionHandler<void(WebCore::DeviceOrientationOrMotionPermissionState)>&&) final;
#endif

    void configureLoggingChannel(const String&, WTFLogChannelState, WTFLogLevel) final;

    bool userIsInteracting() const final;
    void setUserIsInteracting(bool) final;

#if ENABLE(WEB_AUTHN)
    void setMockWebAuthenticationConfiguration(const WebCore::MockWebAuthenticationConfiguration&) final;
#endif

#if PLATFORM(MAC)
    void changeUniversalAccessZoomFocus(const WebCore::IntRect&, const WebCore::IntRect&) final;
#endif

#if ENABLE(IMAGE_ANALYSIS)
    void requestTextRecognition(WebCore::Element&, WebCore::TextRecognitionOptions&&, CompletionHandler<void(RefPtr<WebCore::Element>&&)>&& = { }) final;
#endif

    bool needsImageOverlayControllerForSelectionPainting() const final
    {
#if USE(UIKIT_EDITING)
        return false;
#else
        return true;
#endif
    }

#if ENABLE(TEXT_AUTOSIZING)
    void textAutosizingUsesIdempotentModeChanged() final;
#endif

    URL sanitizeLookalikeCharacters(const URL&) const final;

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
    void showMediaControlsContextMenu(WebCore::FloatRect&&, Vector<WebCore::MediaControlsContextMenuItem>&&, CompletionHandler<void(WebCore::MediaControlsContextMenuItem::ID)>&&) final;
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if ENABLE(WEBXR) && !USE(OPENXR)
    void enumerateImmersiveXRDevices(CompletionHandler<void(const PlatformXR::Instance::DeviceList&)>&&) final;
    void requestPermissionOnXRSessionFeatures(const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList& /* granted */, const PlatformXR::Device::FeatureList& /* consentRequired */, const PlatformXR::Device::FeatureList& /* consentOptional */, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&&) final;
#endif

#if ENABLE(APPLE_PAY_AMS_UI)
    void startApplePayAMSUISession(const URL&, const WebCore::ApplePayAMSUIRequest&, CompletionHandler<void(std::optional<bool>&&)>&&) final;
    void abortApplePayAMSUISession() final;
#endif

    void requestCookieConsent(CompletionHandler<void(WebCore::CookieConsentDecisionResult)>&&) final;

    void classifyModalContainerControls(Vector<String>&&, CompletionHandler<void(Vector<WebCore::ModalContainerControlType>&&)>&&) final;

    void decidePolicyForModalContainer(OptionSet<WebCore::ModalContainerControlType>, CompletionHandler<void(WebCore::ModalContainerDecision)>&&) final;

    const AtomString& searchStringForModalContainerObserver() const final;

    mutable bool m_cachedMainFrameHasHorizontalScrollbar { false };
    mutable bool m_cachedMainFrameHasVerticalScrollbar { false };

    WebPage& m_page;
};

} // namespace WebKit
