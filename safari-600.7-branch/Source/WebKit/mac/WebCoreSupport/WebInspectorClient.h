/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#import <WebCore/InspectorClient.h>
#import <WebCore/InspectorForwarding.h>
#import <WebCore/InspectorFrontendClientLocal.h>

#import <wtf/Forward.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

#ifdef __OBJC__
@class NSURL;
@class WebInspectorRemoteChannel;
@class WebInspectorWindowController;
@class WebNodeHighlighter;
@class WebView;
#else
class NSURL;
class WebInspectorRemoteChannel;
class WebInspectorWindowController;
class WebNodeHighlighter;
class WebView;
#endif

namespace WebCore {

class Frame;
class Page;

}

class WebInspectorFrontendClient;

class WebInspectorClient : public WebCore::InspectorClient, public WebCore::InspectorFrontendChannel {
public:
    explicit WebInspectorClient(WebView *);

    virtual void inspectorDestroyed() override;

    virtual WebCore::InspectorFrontendChannel* openInspectorFrontend(WebCore::InspectorController*) override;
    virtual void closeInspectorFrontend() override;
    virtual void bringFrontendToFront() override;
    virtual void didResizeMainFrame(WebCore::Frame*) override;

    virtual void highlight() override;
    virtual void hideHighlight() override;

#if PLATFORM(IOS)
    virtual void showInspectorIndication() override;
    virtual void hideInspectorIndication() override;
#endif

    virtual void didSetSearchingForNode(bool) override;

    virtual bool sendMessageToFrontend(const String&) override;

    bool inspectorStartsAttached();
    void setInspectorStartsAttached(bool);

    bool inspectorAttachDisabled();
    void setInspectorAttachDisabled(bool);

    void releaseFrontend();

private:
    std::unique_ptr<WebCore::InspectorFrontendClientLocal::Settings> createFrontendSettings();

    WebView *m_webView;
    RetainPtr<WebNodeHighlighter> m_highlighter;
    WebCore::Page* m_frontendPage;
    WebInspectorFrontendClient* m_frontendClient;
};


class WebInspectorFrontendClient : public WebCore::InspectorFrontendClientLocal {
public:
    WebInspectorFrontendClient(WebView*, WebInspectorWindowController*, WebCore::InspectorController*, WebCore::Page*, std::unique_ptr<Settings>);

    void attachAvailabilityChanged(bool);

    virtual void frontendLoaded();

    virtual String localizedStringsURL();

    virtual void bringToFront();
    virtual void closeWindow();
    virtual void disconnectFromBackend();

    virtual void attachWindow(DockSide);
    virtual void detachWindow();

    virtual void setAttachedWindowHeight(unsigned height);
    virtual void setAttachedWindowWidth(unsigned height);
    virtual void setToolbarHeight(unsigned) override;

    virtual void inspectedURLChanged(const String& newURL);

private:
    void updateWindowTitle() const;

    virtual bool canSave() override { return true; }
    virtual void save(const String& url, const String& content, bool forceSaveAs, bool base64Encoded) override;
    virtual void append(const String& url, const String& content) override;

#if !PLATFORM(IOS)
    WebView* m_inspectedWebView;
    RetainPtr<WebInspectorWindowController> m_windowController;
    String m_inspectedURL;
    HashMap<String, RetainPtr<NSURL>> m_suggestedToActualURLMap;
#endif
};
