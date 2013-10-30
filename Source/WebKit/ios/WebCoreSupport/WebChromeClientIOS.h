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
    
    virtual void setWindowRect(const WebCore::FloatRect&) OVERRIDE;
    virtual WebCore::FloatRect windowRect() OVERRIDE;
    virtual void setStatusbarText(const WTF::String&) OVERRIDE { }

    virtual void focus() OVERRIDE;
    virtual void takeFocus(WebCore::FocusDirection) OVERRIDE { }

    virtual void runJavaScriptAlert(WebCore::Frame*, const WTF::String&) OVERRIDE;
    virtual bool runJavaScriptConfirm(WebCore::Frame*, const WTF::String&) OVERRIDE;
    virtual bool runJavaScriptPrompt(WebCore::Frame*, const WTF::String& message, const WTF::String& defaultValue, WTF::String& result) OVERRIDE;

    virtual void runOpenPanel(WebCore::Frame*, PassRefPtr<WebCore::FileChooser>) OVERRIDE;

#if ENABLE(TOUCH_EVENTS)
    virtual void didPreventDefaultForEvent() OVERRIDE;
#endif
    virtual void didReceiveDocType(WebCore::Frame*) OVERRIDE;
    virtual void setNeedsScrollNotifications(WebCore::Frame*, bool) OVERRIDE;
    virtual void observedContentChange(WebCore::Frame*) OVERRIDE;
    virtual void clearContentChangeObservers(WebCore::Frame*) OVERRIDE;
    virtual void dispatchViewportPropertiesDidChange(const WebCore::ViewportArguments&) const OVERRIDE;
    virtual void notifyRevealedSelectionByScrollingFrame(WebCore::Frame*) OVERRIDE;
    virtual bool isStopping() OVERRIDE;
    virtual void didLayout(LayoutType) OVERRIDE;
    virtual void didStartOverflowScroll() OVERRIDE;
    virtual void didEndOverflowScroll() OVERRIDE;

    virtual void suppressFormNotifications() OVERRIDE;
    virtual void restoreFormNotifications() OVERRIDE;
    
    virtual void formStateDidChange(const WebCore::Node*) OVERRIDE { }

    virtual void elementDidFocus(const WebCore::Node*) OVERRIDE;
    virtual void elementDidBlur(const WebCore::Node*) OVERRIDE;

#if USE(ACCELERATED_COMPOSITING)
    virtual void attachRootGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) OVERRIDE;
#endif

    virtual void didFlushCompositingLayers() OVERRIDE;

    virtual void updateViewportConstrainedLayers(HashMap<PlatformLayer*, OwnPtr<WebCore::ViewportConstraints> >&, HashMap<PlatformLayer*, PlatformLayer*>&) OVERRIDE;

    virtual bool fetchCustomFixedPositionLayoutRect(WebCore::IntRect&) OVERRIDE;
    virtual void addOrUpdateScrollingLayer(WebCore::Node*, PlatformLayer*, PlatformLayer*, const WebCore::IntSize&, bool allowHorizontalScrollbar, bool allowVerticalScrollbar) OVERRIDE;
    virtual void removeScrollingLayer(WebCore::Node*, PlatformLayer*, PlatformLayer*) OVERRIDE;

    virtual bool selectItemWritingDirectionIsNatural() OVERRIDE;
    virtual bool selectItemAlignmentFollowsMenuWritingDirection() OVERRIDE;
    virtual PassRefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient*) const OVERRIDE;
    virtual PassRefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient*) const OVERRIDE;

    virtual void webAppOrientationsUpdated() OVERRIDE;
    virtual void focusedNodeChanged(WebCore::Node*) OVERRIDE;

private:
    int m_formNotificationSuppressions;
};
#endif
