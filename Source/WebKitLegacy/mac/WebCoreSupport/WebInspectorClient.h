/*
 * Copyright (C) 2006, 2007, 2015 Apple Inc.  All rights reserved.
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

#import <JavaScriptCore/InspectorFrontendChannel.h>
#import <WebCore/FloatRect.h>
#import <WebCore/InspectorClient.h>
#import <WebCore/InspectorFrontendClientLocal.h>
#import <wtf/Forward.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

OBJC_CLASS NSURL;
OBJC_CLASS WebInspectorRemoteChannel;
OBJC_CLASS WebInspectorWindowController;
OBJC_CLASS WebNodeHighlighter;
OBJC_CLASS WebView;

namespace WebCore {
class CertificateInfo;
class Frame;
class Page;
}

class WebInspectorFrontendClient;

class WebInspectorClient final : public WebCore::InspectorClient, public Inspector::FrontendChannel {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WebInspectorClient(WebView *inspectedWebView);

    void inspectedPageDestroyed() override;

    Inspector::FrontendChannel* openLocalFrontend(WebCore::InspectorController*) override;
    void bringFrontendToFront() override;
    void didResizeMainFrame(WebCore::Frame*) override;

    void highlight() override;
    void hideHighlight() override;

#if ENABLE(REMOTE_INSPECTOR)
    bool allowRemoteInspectionToPageDirectly() const override { return true; }
#endif

#if PLATFORM(IOS_FAMILY)
    void showInspectorIndication() override;
    void hideInspectorIndication() override;

    bool overridesShowPaintRects() const override { return true; }
    void setShowPaintRects(bool) override;
    void showPaintRect(const WebCore::FloatRect&) override;
#endif

    void didSetSearchingForNode(bool) override;

    void sendMessageToFrontend(const String&) override;
    ConnectionType connectionType() const override { return ConnectionType::Local; }

    bool inspectorStartsAttached();
    void setInspectorStartsAttached(bool);
    void deleteInspectorStartsAttached();

    bool inspectorAttachDisabled();
    void setInspectorAttachDisabled(bool);
    void deleteInspectorAttachDisabled();

    void windowFullScreenDidChange();

    bool canAttach();

    void releaseFrontend();

private:
    std::unique_ptr<WebCore::InspectorFrontendClientLocal::Settings> createFrontendSettings();

    WebView *m_inspectedWebView { nullptr };
    RetainPtr<WebNodeHighlighter> m_highlighter;
    WebCore::Page* m_frontendPage { nullptr };
    std::unique_ptr<WebInspectorFrontendClient> m_frontendClient;
};


class WebInspectorFrontendClient : public WebCore::InspectorFrontendClientLocal {
public:
    WebInspectorFrontendClient(WebView*, WebInspectorWindowController*, WebCore::InspectorController*, WebCore::Page*, std::unique_ptr<Settings>);

    void attachAvailabilityChanged(bool);
    bool canAttach();

    void frontendLoaded() override;

    void startWindowDrag() override;

    String localizedStringsURL() override;

    void bringToFront() override;
    void closeWindow() override;
    void reopen() override;
    void resetState() override;

    void attachWindow(DockSide) override;
    void detachWindow() override;

    void setAttachedWindowHeight(unsigned height) override;
    void setAttachedWindowWidth(unsigned height) override;

#if !PLATFORM(IOS_FAMILY)
    const WebCore::FloatRect& sheetRect() const { return m_sheetRect; }
#endif
    void setSheetRect(const WebCore::FloatRect&) override;

    void inspectedURLChanged(const String& newURL) override;
    void showCertificate(const WebCore::CertificateInfo&) override;

private:
    void updateWindowTitle() const;

    bool canSave() override { return true; }
    void save(const String& url, const String& content, bool forceSaveAs, bool base64Encoded) override;
    void append(const String& url, const String& content) override;

#if !PLATFORM(IOS_FAMILY)
    WebView *m_inspectedWebView;
    RetainPtr<WebInspectorWindowController> m_frontendWindowController;
    String m_inspectedURL;
    HashMap<String, RetainPtr<NSURL>> m_suggestedToActualURLMap;
    WebCore::FloatRect m_sheetRect;
#endif
};
