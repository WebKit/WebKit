/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#if USE(PLUGIN_HOST_PROCESS)

#ifndef NetscapePluginInstanceProxy_h
#define NetscapePluginInstanceProxy_h

#include <WebCore/Timer.h>
#include <WebKit/npapi.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>

@class WebHostedNetscapePluginView;

namespace WebKit {

class HostedNetscapePluginStream;
class NetscapePluginHostProxy;
    
class NetscapePluginInstanceProxy : public RefCounted<NetscapePluginInstanceProxy> {
public:
    static PassRefPtr<NetscapePluginInstanceProxy> create(NetscapePluginHostProxy* pluginHostProxy, WebHostedNetscapePluginView *pluginView, uint32_t pluginID, uint32_t renderContextID, boolean_t useSoftwareRenderer)
    {
        return adoptRef(new NetscapePluginInstanceProxy(pluginHostProxy, pluginView, pluginID, renderContextID, useSoftwareRenderer));
    }
    ~NetscapePluginInstanceProxy();
    
    uint32_t pluginID() const { return m_pluginID; }
    uint32_t renderContextID() const { return m_renderContextID; }
    bool useSoftwareRenderer() const { return m_useSoftwareRenderer; }
    WebHostedNetscapePluginView *pluginView() const { return m_pluginView; }
    NetscapePluginHostProxy* hostProxy() const { return m_pluginHostProxy; }
    
    HostedNetscapePluginStream *pluginStream(uint32_t streamID);
    void disconnectStream(HostedNetscapePluginStream*);
    
    void pluginHostDied();
    
    void resize(NSRect size, NSRect clipRect);
    void destroy();
    void focusChanged(bool hasFocus);
    void windowFocusChanged(bool hasFocus);
    void windowFrameChanged(NSRect frame);
    
    void mouseEvent(NSView *pluginView, NSEvent *, NPCocoaEventType);
    void startTimers(bool throttleTimers);
    void stopTimers();
    
    void status(const char* message);
    NPError loadURL(const char* url, const char* target, bool post, const char* postData, uint32_t postDataLength, bool postDataIsFile, bool currentEventIsUserGesture, uint32_t& requestID);
    
private:
    NPError loadRequest(NSURLRequest *, const char* cTarget, bool currentEventIsUserGesture, uint32_t& streamID);
    
    class PluginRequest;
    void performRequest(PluginRequest*);
    void evaluateJavaScript(PluginRequest*);
    
    NetscapePluginInstanceProxy(NetscapePluginHostProxy*, WebHostedNetscapePluginView *, uint32_t pluginID, uint32_t renderContextID, boolean_t useSoftwareRenderer);

    NetscapePluginHostProxy* m_pluginHostProxy;
    WebHostedNetscapePluginView *m_pluginView;

    void requestTimerFired(WebCore::Timer<NetscapePluginInstanceProxy>*);
    WebCore::Timer<NetscapePluginInstanceProxy> m_requestTimer;
    Deque<PluginRequest*> m_pluginRequests;
    
    HashMap<uint32_t, RefPtr<HostedNetscapePluginStream> > m_streams;

    uint32_t m_currentRequestID;
    
    uint32_t m_pluginID;
    uint32_t m_renderContextID;
    boolean_t m_useSoftwareRenderer;
};
    
} // namespace WebKit

#endif // NetscapePluginInstanceProxy_h
#endif // USE(PLUGIN_HOST_PROCESS)
