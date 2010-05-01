/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebPage_h
#define WebPage_h

#include "DrawingArea.h"
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/IntRect.h>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace CoreIPC {
    class ArgumentDecoder;
    class Connection;
    class MessageID;
}

namespace WebCore {
    class GraphicsContext;
    class KeyboardEvent;
    class Page;
    class PlatformKeyboardEvent;
    class PlatformMouseEvent;
    class PlatformWheelEvent;
    class String;
}

namespace WebKit {

class DrawingArea;
class WebFrame;
class WebPreferencesStore;

class WebPage : public RefCounted<WebPage> {
public:
    static PassRefPtr<WebPage> create(uint64_t pageID, const WebCore::IntSize& viewSize, const WebPreferencesStore&, DrawingArea::Type);
    ~WebPage();

    void close();

    WebCore::Page* corePage() const { return m_page; }
    uint64_t pageID() const { return m_pageID; }

    WebFrame* webFrame(uint64_t) const;
    void addWebFrame(uint64_t, WebFrame*);
    void removeWebFrame(uint64_t);

    void setSize(const WebCore::IntSize&);
    const WebCore::IntSize& size() const { return m_viewSize; }

    DrawingArea* drawingArea() const { return m_drawingArea; }

    // -- Called by the DrawingArea.
    // FIXME: We could genericize these into a DrawingArea client interface. Would that be beneficial?
    void drawRect(WebCore::GraphicsContext&, const WebCore::IntRect&);
    void layoutIfNeeded();

    // -- Called from WebCore clients.
    void backForwardListDidChange();
    bool handleEditingKeyboardEvent(WebCore::KeyboardEvent*);
    void show();

    // -- Called from WebProcess.
    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder&);

private:
    WebPage(uint64_t pageID, const WebCore::IntSize& viewSize, const WebPreferencesStore&, DrawingArea::Type);

    void platformInitialize();
    static const char* interpretKeyEvent(const WebCore::KeyboardEvent*);

    // Actions
    void tryClose();
    void loadURL(const WebCore::String&);
    void stopLoading();
    void reload();
    void goForward();
    void goBack();
    void setActive(bool);
    void setFocused(bool);
    void mouseEvent(const WebCore::PlatformMouseEvent&);
    void wheelEvent(WebCore::PlatformWheelEvent&);
    void keyEvent(const WebCore::PlatformKeyboardEvent&);
    void runJavaScriptInMainFrame(const WebCore::String&, uint64_t callbackID);
    void getRenderTreeExternalRepresentation(uint64_t callbackID);
    void preferencesDidChange(const WebPreferencesStore&);

    void didReceivePolicyDecision(WebFrame*, uint64_t listenerID, WebCore::PolicyAction policyAction);
    
    WebCore::Page* m_page;
    RefPtr<WebFrame> m_mainFrame;
    HashMap<uint64_t, WebFrame*> m_frameMap;

    WebCore::IntSize m_viewSize;
    DrawingArea* m_drawingArea;

    bool m_canGoBack;
    bool m_canGoForward;

    uint64_t m_pageID;
};

} // namespace WebKit

#endif // WebPage_h
