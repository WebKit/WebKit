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

#ifndef PluginView_h
#define PluginView_h

#include "NPRuntimeObjectMap.h"
#include "Plugin.h"
#include "PluginController.h"
#include "RunLoop.h"
#include "WebFrame.h"

#include <WebCore/MediaCanStartListener.h>
#include <WebCore/PluginViewBase.h>
#include <wtf/Deque.h>

// FIXME: Eventually this should move to WebCore.

namespace WebCore {
    class Frame;
    class HTMLPlugInElement;
}

namespace WebKit {

class PluginView : public WebCore::PluginViewBase, WebCore::MediaCanStartListener, PluginController, WebFrame::LoadListener {
public:
    static PassRefPtr<PluginView> create(WebCore::HTMLPlugInElement* pluginElement, PassRefPtr<Plugin> plugin, const Plugin::Parameters& parameters)
    {
        return adoptRef(new PluginView(pluginElement, plugin, parameters));
    }

    WebCore::Frame* frame();

    bool isBeingDestroyed() const { return m_isBeingDestroyed; }

    void manualLoadDidReceiveResponse(const WebCore::ResourceResponse&);
    void manualLoadDidReceiveData(const char* bytes, int length);
    void manualLoadDidFinishLoading();
    void manualLoadDidFail(const WebCore::ResourceError&);

#if PLATFORM(MAC)
    void setWindowIsVisible(bool);
    void setWindowIsFocused(bool);
    void setWindowFrame(const WebCore::IntRect&);
#endif

private:
    PluginView(WebCore::HTMLPlugInElement*, PassRefPtr<Plugin>, const Plugin::Parameters& parameters);
    virtual ~PluginView();

    void initializePlugin();
    void destroyPlugin();

    void viewGeometryDidChange();
    WebCore::IntRect clipRectInWindowCoordinates() const;
    void focusPluginElement();
    
    void pendingURLRequestsTimerFired();
    class URLRequest;
    void performURLRequest(URLRequest*);

    // Perform a URL request where the frame target is not null.
    void performFrameLoadURLRequest(URLRequest*);

    // Perform a URL request where the URL protocol is "javascript:".
    void performJavaScriptURLRequest(URLRequest*);

    class Stream;
    void addStream(Stream*);
    void removeStream(Stream*);
    void cancelAllStreams();

    // WebCore::PluginViewBase
#if PLATFORM(MAC)
    virtual PlatformLayer* platformLayer() const;
#endif
    virtual JSC::JSObject* scriptObject(JSC::JSGlobalObject*);
    
    // WebCore::Widget
    virtual void setFrameRect(const WebCore::IntRect&);
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect&);
    virtual void invalidateRect(const WebCore::IntRect&);
    virtual void setFocus(bool);
    virtual void frameRectsChanged();
    virtual void setParent(WebCore::ScrollView*);
    virtual void handleEvent(WebCore::Event*);

    // WebCore::MediaCanStartListener
    virtual void mediaCanStart();

    // PluginController
    virtual void invalidate(const WebCore::IntRect&);
    virtual String userAgent(const WebCore::KURL&);
    virtual void loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, 
                         const WebCore::HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups);
    virtual void cancelStreamLoad(uint64_t streamID);
    virtual void cancelManualStreamLoad();
    virtual NPObject* windowScriptNPObject();
    virtual NPObject* pluginElementNPObject();
    virtual bool evaluate(NPObject*, const String&scriptString, NPVariant* result, bool allowPopups);
    virtual void setStatusbarText(const String&);
    virtual bool isAcceleratedCompositingEnabled();
    virtual void pluginProcessCrashed();
#if PLATFORM(WIN)
    virtual HWND nativeParentWindow();
#endif

    // WebFrame::LoadListener
    virtual void didFinishLoad(WebFrame*);
    virtual void didFailLoad(WebFrame*, bool wasCancelled);

    WebCore::HTMLPlugInElement* m_pluginElement;
    RefPtr<Plugin> m_plugin;
    WebPage* m_webPage;
    Plugin::Parameters m_parameters;
    
    bool m_isInitialized;
    bool m_isWaitingUntilMediaCanStart;
    bool m_isBeingDestroyed;

    // Pending URLRequests that the plug-in has made.
    Deque<RefPtr<URLRequest> > m_pendingURLRequests;
    RunLoop::Timer<PluginView> m_pendingURLRequestsTimer;

    // Pending frame loads that the plug-in has made.
    typedef HashMap<RefPtr<WebFrame>, RefPtr<URLRequest> > FrameLoadMap;
    FrameLoadMap m_pendingFrameLoads;

    // Streams that the plug-in has requested to load. 
    HashMap<uint64_t, RefPtr<Stream> > m_streams;

    // A map of all related NPObjects for this plug-in view.
    NPRuntimeObjectMap m_npRuntimeObjectMap;
};

} // namespace WebKit

#endif // PluginView_h
