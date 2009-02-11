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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#import <WebCore/PlatformString.h>

#import <wtf/RetainPtr.h>

#ifdef __OBJC__
@class WebInspectorWindowController;
@class WebView;
#else
class WebInspectorWindowController;
class WebView;
#endif

class WebInspectorClient : public WebCore::InspectorClient {
public:
    WebInspectorClient(WebView *);

    virtual void inspectorDestroyed();

    virtual WebCore::Page* createPage();
    virtual WebCore::String localizedStringsURL();

    virtual WebCore::String hiddenPanels();

    virtual void showWindow();
    virtual void closeWindow();

    virtual void attachWindow();
    virtual void detachWindow();

    virtual void setAttachedWindowHeight(unsigned height);

    virtual void highlight(WebCore::Node*);
    virtual void hideHighlight();
    virtual void inspectedURLChanged(const WebCore::String& newURL);

    virtual void populateSetting(const WebCore::String& key, WebCore::InspectorController::Setting&);
    virtual void storeSetting(const WebCore::String& key, const WebCore::InspectorController::Setting&);
    virtual void removeSetting(const WebCore::String& key);

private:
    void updateWindowTitle() const;

    WebView *m_webView;
    RetainPtr<WebInspectorWindowController> m_windowController;
    WebCore::String m_inspectedURL;
};
