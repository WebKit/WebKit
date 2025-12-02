/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "WebChromeClient.h"
#import <wtf/TZoneMalloc.h>

namespace WebCore {
enum class BroadcastFocusedElement : bool;
class Frame;
struct FocusOptions;
}

class WebChromeClientIOS final : public WebChromeClient {
    WTF_MAKE_TZONE_ALLOCATED(WebChromeClientIOS);
public:
    WebChromeClientIOS(WebView* webView)
        : WebChromeClient(webView)
    {
    }

private:
    void setWindowRect(const WebCore::FloatRect&) final;
    WebCore::FloatRect windowRect() const final;

    void focus() final;
    void takeFocus(WebCore::FocusDirection) final { }

    void runJavaScriptAlert(WebCore::LocalFrame&, const WTF::String&) final;
    bool runJavaScriptConfirm(WebCore::LocalFrame&, const WTF::String&) final;
    bool runJavaScriptPrompt(WebCore::LocalFrame&, const WTF::String& message, const WTF::String& defaultValue, WTF::String& result) final;

    void runOpenPanel(WebCore::LocalFrame&, WebCore::FileChooser&) final;
    void showShareSheet(WebCore::ShareDataWithParsedURL&&, CompletionHandler<void(bool)>&&) final;

    bool hasAccessoryMousePointingDevice() const final { return false; }
    bool hoverSupportedByPrimaryPointingDevice() const final { return false; }
    bool hoverSupportedByAnyAvailablePointingDevice() const final { return false; }
    std::optional<WebCore::PointerCharacteristics> pointerCharacteristicsOfPrimaryPointingDevice() const final { return WebCore::PointerCharacteristics::Coarse; }
    OptionSet<WebCore::PointerCharacteristics> pointerCharacteristicsOfAllAvailablePointingDevices() const final { return WebCore::PointerCharacteristics::Coarse; }

    void setCursor(const WebCore::Cursor&) final { }
    void setCursorHiddenUntilMouseMoves(bool) final { }

#if ENABLE(TOUCH_EVENTS)
    void didPreventDefaultForEvent() final;
#endif

    void didReceiveMobileDocType(bool) final;
    void setNeedsScrollNotifications(WebCore::LocalFrame&, bool) final;
    void didFinishContentChangeObserving(WebCore::LocalFrame&, WKContentChange) final;
    WebCore::FloatSize screenSize() const final;
    WebCore::FloatSize availableScreenSize() const final;
    WebCore::FloatSize overrideScreenSize() const final;
    WebCore::FloatSize overrideAvailableScreenSize() const final;
    void dispatchDisabledAdaptationsDidChange(const OptionSet<WebCore::DisabledAdaptations>&) const final;
    void dispatchViewportPropertiesDidChange(const WebCore::ViewportArguments&) const final;
    void notifyRevealedSelectionByScrollingFrame(WebCore::LocalFrame&) final;
    bool isStopping() final;
    void didLayout(LayoutType) final;
    void didStartOverflowScroll() final;
    void didEndOverflowScroll() final;

    void suppressFormNotifications() final;
    void restoreFormNotifications() final;

    void elementDidFocus(WebCore::Element&, const WebCore::FocusOptions&) final;
    void elementDidBlur(WebCore::Element&) final;

    void attachRootGraphicsLayer(WebCore::LocalFrame&, WebCore::GraphicsLayer*) final;

    void didFlushCompositingLayers() final;

    void updateViewportConstrainedLayers(HashMap<PlatformLayer*, std::unique_ptr<WebCore::ViewportConstraints>>&, const HashMap<PlatformLayer*, PlatformLayer*>&) final;

    bool fetchCustomFixedPositionLayoutRect(WebCore::IntRect&) final;
    void addOrUpdateScrollingLayer(WebCore::Node*, PlatformLayer*, PlatformLayer*, const WebCore::IntSize&, bool allowHorizontalScrollbar, bool allowVerticalScrollbar) final;
    void removeScrollingLayer(WebCore::Node*, PlatformLayer*, PlatformLayer*) final;

    bool selectItemWritingDirectionIsNatural() final;
    bool selectItemAlignmentFollowsMenuWritingDirection() final;
    RefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient&) const final;
    RefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient&) const final;
    void relayAccessibilityNotification(String&&, RetainPtr<NSData>&&) const final { }
    void relayAriaNotifyNotification(WebCore::AriaNotifyData&&) const final { }
    void relayLiveRegionNotification(WebCore::LiveRegionAnnouncementData&&) const final { }
    void webAppOrientationsUpdated() final;
    void focusedElementChanged(WebCore::Element*, WebCore::LocalFrame*, WebCore::FocusOptions, WebCore::BroadcastFocusedElement) final;
    void showPlaybackTargetPicker(bool hasVideo, WebCore::RouteSharingPolicy, const String&) final;
    RefPtr<WebCore::Icon> createIconForFiles(const Vector<String>& filenames) final;

    bool showDataDetectorsUIForElement(const WebCore::Element&, const WebCore::Event&) final { return false; }

#if ENABLE(ORIENTATION_EVENTS)
    WebCore::IntDegrees deviceOrientation() const final;
#endif

    int m_formNotificationSuppressions { 0 };
};

#endif
