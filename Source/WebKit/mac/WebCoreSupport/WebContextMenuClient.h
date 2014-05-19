/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebCore/ContextMenuClient.h>
#import <WebCore/IntRect.h>

@class WebSharingServicePickerController;
@class WebView;

namespace WebCore {
class Node;
}

class WebContextMenuClient : public WebCore::ContextMenuClient {
public:
    WebContextMenuClient(WebView *webView);
    ~WebContextMenuClient();

    virtual void contextMenuDestroyed() override;
    
    virtual NSMutableArray* getCustomMenuFromDefaultItems(WebCore::ContextMenu*) override;
    virtual void contextMenuItemSelected(WebCore::ContextMenuItem*, const WebCore::ContextMenu*) override;
    
    virtual void downloadURL(const WebCore::URL&) override;
    virtual void searchWithGoogle(const WebCore::Frame*) override;
    virtual void lookUpInDictionary(WebCore::Frame*) override;
    virtual bool isSpeaking() override;
    virtual void speak(const WTF::String&) override;
    virtual void stopSpeaking() override;
    virtual void searchWithSpotlight() override;
    virtual void showContextMenu() override;

    NSRect screenRectForHitTestNode() const;

#if ENABLE(SERVICE_CONTROLS)
    void clearSharingServicePickerController();
    NSImage *renderedImageForControlledImage() const;
#endif

    WebView *webView() { return m_webView; }
        
private:
    NSMenu *contextMenuForEvent(NSEvent *, NSView *, bool& isServicesMenu);

    bool clientFloatRectForNode(WebCore::Node&, WebCore::FloatRect&) const;

    WebView *m_webView;
#if ENABLE(SERVICE_CONTROLS)
    RetainPtr<WebSharingServicePickerController> m_sharingServicePickerController;
#endif
};
