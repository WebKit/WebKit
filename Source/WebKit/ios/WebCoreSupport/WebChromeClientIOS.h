/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)
#import "WebChromeClient.h"

class WebChromeClientIOS : public WebChromeClient {
public:
    WebChromeClientIOS(WebView *webView)
        : WebChromeClient(webView)
        , m_formNotificationSuppressions(0) { }
    
    void setWindowRect(const WebCore::FloatRect&) override;
    WebCore::FloatRect windowRect() override;
    void setStatusbarText(const WTF::String&) override { }

    void focus() override;
    void takeFocus(WebCore::FocusDirection) override { }

    void runJavaScriptAlert(WebCore::Frame*, const WTF::String&) override;
    bool runJavaScriptConfirm(WebCore::Frame*, const WTF::String&) override;
    bool runJavaScriptPrompt(WebCore::Frame*, const WTF::String& message, const WTF::String& defaultValue, WTF::String& result) override;

    void runOpenPanel(WebCore::Frame*, PassRefPtr<WebCore::FileChooser>) override;

#if ENABLE(TOUCH_EVENTS)
    void didPreventDefaultForEvent() override;
#endif
    void didReceiveMobileDocType(bool) override;
    void setNeedsScrollNotifications(WebCore::Frame*, bool) override;
    void observedContentChange(WebCore::Frame*) override;
    void clearContentChangeObservers(WebCore::Frame*) override;
    WebCore::FloatSize screenSize() const override;
    WebCore::FloatSize availableScreenSize() const override;
    void dispatchViewportPropertiesDidChange(const WebCore::ViewportArguments&) const override;
    void notifyRevealedSelectionByScrollingFrame(WebCore::Frame*) override;
    bool isStopping() override;
    void didLayout(LayoutType) override;
    void didStartOverflowScroll() override;
    void didEndOverflowScroll() override;

    void suppressFormNotifications() override;
    void restoreFormNotifications() override;
    
    void elementDidFocus(const WebCore::Node*) override;
    void elementDidBlur(const WebCore::Node*) override;

    void attachRootGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override;

    void didFlushCompositingLayers() override;

    void updateViewportConstrainedLayers(HashMap<PlatformLayer*, std::unique_ptr<WebCore::ViewportConstraints>>&, HashMap<PlatformLayer*, PlatformLayer*>&) override;

    bool fetchCustomFixedPositionLayoutRect(WebCore::IntRect&) override;
    void addOrUpdateScrollingLayer(WebCore::Node*, PlatformLayer*, PlatformLayer*, const WebCore::IntSize&, bool allowHorizontalScrollbar, bool allowVerticalScrollbar) override;
    void removeScrollingLayer(WebCore::Node*, PlatformLayer*, PlatformLayer*) override;

    bool selectItemWritingDirectionIsNatural() override;
    bool selectItemAlignmentFollowsMenuWritingDirection() override;
    RefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient*) const override;
    RefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient*) const override;

    void webAppOrientationsUpdated() override;
    void focusedElementChanged(WebCore::Element*) override;
    void showPlaybackTargetPicker(bool hasVideo) override;

#if ENABLE(ORIENTATION_EVENTS)
    int deviceOrientation() const override;
#endif

private:
    int m_formNotificationSuppressions;
};
#endif
