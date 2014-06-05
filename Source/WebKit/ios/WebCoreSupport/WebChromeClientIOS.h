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
    
    virtual void setWindowRect(const WebCore::FloatRect&) override;
    virtual WebCore::FloatRect windowRect() override;
    virtual void setStatusbarText(const WTF::String&) override { }

    virtual void focus() override;
    virtual void takeFocus(WebCore::FocusDirection) override { }

    virtual void runJavaScriptAlert(WebCore::Frame*, const WTF::String&) override;
    virtual bool runJavaScriptConfirm(WebCore::Frame*, const WTF::String&) override;
    virtual bool runJavaScriptPrompt(WebCore::Frame*, const WTF::String& message, const WTF::String& defaultValue, WTF::String& result) override;

    virtual void runOpenPanel(WebCore::Frame*, PassRefPtr<WebCore::FileChooser>) override;

#if ENABLE(TOUCH_EVENTS)
    virtual void didPreventDefaultForEvent() override;
#endif
    virtual void didReceiveMobileDocType(bool) override;
    virtual void setNeedsScrollNotifications(WebCore::Frame*, bool) override;
    virtual void observedContentChange(WebCore::Frame*) override;
    virtual void clearContentChangeObservers(WebCore::Frame*) override;
    virtual WebCore::FloatSize screenSize() const override;
    virtual WebCore::FloatSize availableScreenSize() const override;
    virtual void dispatchViewportPropertiesDidChange(const WebCore::ViewportArguments&) const override;
    virtual void notifyRevealedSelectionByScrollingFrame(WebCore::Frame*) override;
    virtual bool isStopping() override;
    virtual void didLayout(LayoutType) override;
    virtual void didStartOverflowScroll() override;
    virtual void didEndOverflowScroll() override;

    virtual void suppressFormNotifications() override;
    virtual void restoreFormNotifications() override;
    
    virtual void elementDidFocus(const WebCore::Node*) override;
    virtual void elementDidBlur(const WebCore::Node*) override;

    virtual void attachRootGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override;

    virtual void didFlushCompositingLayers() override;

    virtual void updateViewportConstrainedLayers(HashMap<PlatformLayer*, std::unique_ptr<WebCore::ViewportConstraints>>&, HashMap<PlatformLayer*, PlatformLayer*>&) override;

    virtual bool fetchCustomFixedPositionLayoutRect(WebCore::IntRect&) override;
    virtual void addOrUpdateScrollingLayer(WebCore::Node*, PlatformLayer*, PlatformLayer*, const WebCore::IntSize&, bool allowHorizontalScrollbar, bool allowVerticalScrollbar) override;
    virtual void removeScrollingLayer(WebCore::Node*, PlatformLayer*, PlatformLayer*) override;

    virtual bool selectItemWritingDirectionIsNatural() override;
    virtual bool selectItemAlignmentFollowsMenuWritingDirection() override;
    virtual PassRefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient*) const override;
    virtual PassRefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient*) const override;

    virtual void webAppOrientationsUpdated() override;
    virtual void focusedElementChanged(WebCore::Element*) override;
    virtual void showPlaybackTargetPicker(bool hasVideo) override;

#if ENABLE(ORIENTATION_EVENTS)
    virtual int deviceOrientation() const override;
#endif

private:
    int m_formNotificationSuppressions;
};
#endif
