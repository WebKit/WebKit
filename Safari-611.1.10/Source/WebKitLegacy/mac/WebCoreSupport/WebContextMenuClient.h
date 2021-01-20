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

#if ENABLE(CONTEXT_MENUS)

#import "WebSharingServicePickerController.h"
#import <WebCore/ContextMenuClient.h>
#import <WebCore/IntRect.h>

@class WebSharingServicePickerController;
@class WebView;

namespace WebCore {
class Node;
}

class WebContextMenuClient : public WebCore::ContextMenuClient
#if ENABLE(SERVICE_CONTROLS)
    , public WebSharingServicePickerClient
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebContextMenuClient(WebView *webView);
    virtual ~WebContextMenuClient();

    void contextMenuDestroyed() override;

    void downloadURL(const URL&) override;
    void searchWithGoogle(const WebCore::Frame*) override;
    void lookUpInDictionary(WebCore::Frame*) override;
    bool isSpeaking() override;
    void speak(const WTF::String&) override;
    void stopSpeaking() override;
    void searchWithSpotlight() override;
    void showContextMenu() override;

#if ENABLE(SERVICE_CONTROLS)
    // WebSharingServicePickerClient
    void sharingServicePickerWillBeDestroyed(WebSharingServicePickerController &) override;
    WebCore::FloatRect screenRectForCurrentSharingServicePickerItem(WebSharingServicePickerController &) override;
    RetainPtr<NSImage> imageForCurrentSharingServicePickerItem(WebSharingServicePickerController &) override;
#endif

private:
    NSMenu *contextMenuForEvent(NSEvent *, NSView *, bool& isServicesMenu);

    bool clientFloatRectForNode(WebCore::Node&, WebCore::FloatRect&) const;

#if ENABLE(SERVICE_CONTROLS)
    RetainPtr<WebSharingServicePickerController> m_sharingServicePickerController;
#else
    WebView* m_webView;
#endif
};

#endif // ENABLE(CONTEXT_MENUS)

