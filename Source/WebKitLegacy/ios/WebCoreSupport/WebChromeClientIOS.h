/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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

class WebChromeClientIOS final : public WebChromeClient {
public:
    WebChromeClientIOS(WebView* webView)
        : WebChromeClient(webView)
    {
    }

private:
    void setWindowRect(const WebCore::FloatRect&) final;
    WebCore::FloatRect windowRect() final;
    void setStatusbarText(const WTF::String&) final { }

    void focus() final;
    void takeFocus(WebCore::FocusDirection) final { }

    void runJavaScriptAlert(WebCore::Frame&, const WTF::String&) final;
    bool runJavaScriptConfirm(WebCore::Frame&, const WTF::String&) final;
    bool runJavaScriptPrompt(WebCore::Frame&, const WTF::String& message, const WTF::String& defaultValue, WTF::String& result) final;

    void runOpenPanel(WebCore::Frame&, WebCore::FileChooser&) final;
    void showShareSheet(WebCore::ShareDataWithParsedURL&, CompletionHandler<void(bool)>&&) final;

#if ENABLE(TOUCH_EVENTS)
    void didPreventDefaultForEvent() final;
#endif

    void didReceiveMobileDocType(bool) final;
    void setNeedsScrollNotifications(WebCore::Frame&, bool) final;
    void observedContentChange(WebCore::Frame&) final;
    void clearContentChangeObservers(WebCore::Frame&) final;
    WebCore::FloatSize screenSize() const final;
    WebCore::FloatSize availableScreenSize() const final;
    WebCore::FloatSize overrideScreenSize() const final;
    void dispatchDisabledAdaptationsDidChange(const OptionSet<WebCore::DisabledAdaptations>&) const final;
    void dispatchViewportPropertiesDidChange(const WebCore::ViewportArguments&) const final;
    void notifyRevealedSelectionByScrollingFrame(WebCore::Frame&) final;
    bool isStopping() final;
    void didLayout(LayoutType) final;
    void didStartOverflowScroll() final;
    void didEndOverflowScroll() final;

    void suppressFormNotifications() final;
    void restoreFormNotifications() final;

    void elementDidFocus(WebCore::Element&) final;
    void elementDidBlur(WebCore::Element&) final;

    void attachRootGraphicsLayer(WebCore::Frame&, WebCore::GraphicsLayer*) final;

    void didFlushCompositingLayers() final;

    void updateViewportConstrainedLayers(HashMap<PlatformLayer*, std::unique_ptr<WebCore::ViewportConstraints>>&, HashMap<PlatformLayer*, PlatformLayer*>&) final;

    bool fetchCustomFixedPositionLayoutRect(WebCore::IntRect&) final;
    void addOrUpdateScrollingLayer(WebCore::Node*, PlatformLayer*, PlatformLayer*, const WebCore::IntSize&, bool allowHorizontalScrollbar, bool allowVerticalScrollbar) final;
    void removeScrollingLayer(WebCore::Node*, PlatformLayer*, PlatformLayer*) final;

    bool selectItemWritingDirectionIsNatural() final;
    bool selectItemAlignmentFollowsMenuWritingDirection() final;
    RefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient&) const final;
    RefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient&) const final;

    void webAppOrientationsUpdated() final;
    void focusedElementChanged(WebCore::Element*) final;
    void showPlaybackTargetPicker(bool hasVideo, WebCore::RouteSharingPolicy, const String&) final;
    RefPtr<WebCore::Icon> createIconForFiles(const Vector<String>& filenames) final;

#if ENABLE(ORIENTATION_EVENTS)
    int deviceOrientation() const final;
#endif

    int m_formNotificationSuppressions { 0 };
};

#endif
