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

#ifndef PluginProxy_h
#define PluginProxy_h

#if ENABLE(PLUGIN_PROCESS)

#include "Connection.h"
#include "Plugin.h"

namespace WebKit {

class PluginProcessConnection;

class PluginProxy : public Plugin, public CoreIPC::Connection::MessageReceiver {
public:
    static PassRefPtr<PluginProxy> create(PassRefPtr<PluginProcessConnection>);
    ~PluginProxy();

    uint64_t pluginInstanceID() const { return m_pluginInstanceID; }
    void pluginProcessCrashed();

private:
    explicit PluginProxy(PassRefPtr<PluginProcessConnection>);

    // Plugin
    virtual bool initialize(PluginController*, const Parameters&);
    virtual void destroy();
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect& dirtyRect);
#if PLATFORM(MAC)
    virtual PlatformLayer* pluginLayer();
#endif
    virtual void geometryDidChange(const WebCore::IntRect& frameRect, const WebCore::IntRect& clipRect);
    virtual void frameDidFinishLoading(uint64_t requestID);
    virtual void frameDidFail(uint64_t requestID, bool wasCancelled);
    virtual void didEvaluateJavaScript(uint64_t requestID, const WTF::String& requestURLString, const WTF::String& result);
    virtual void streamDidReceiveResponse(uint64_t streamID, const WebCore::KURL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, const WTF::String& mimeType, const WTF::String& headers);
    virtual void streamDidReceiveData(uint64_t streamID, const char* bytes, int length);
    virtual void streamDidFinishLoading(uint64_t streamID);
    virtual void streamDidFail(uint64_t streamID, bool wasCancelled);
    virtual void manualStreamDidReceiveResponse(const WebCore::KURL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, const WTF::String& mimeType, const WTF::String& headers);
    virtual void manualStreamDidReceiveData(const char* bytes, int length);
    virtual void manualStreamDidFinishLoading();
    virtual void manualStreamDidFail(bool wasCancelled);
    
    virtual bool handleMouseEvent(const WebMouseEvent&);
    virtual bool handleWheelEvent(const WebWheelEvent&);
    virtual bool handleMouseEnterEvent(const WebMouseEvent&);
    virtual bool handleMouseLeaveEvent(const WebMouseEvent&);
    virtual void setFocus(bool);
    virtual NPObject* pluginScriptableNPObject();
#if PLATFORM(MAC)
    virtual void windowFocusChanged(bool);
    virtual void windowFrameChanged(const WebCore::IntRect&);
    virtual void windowVisibilityChanged(bool);
#endif

    virtual PluginController* controller();

    // CoreIPC::Connection::MessageReceiver
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

    RefPtr<PluginProcessConnection> m_connection;
    uint64_t m_pluginInstanceID;

    PluginController* m_pluginController;

    bool m_isStarted;
};

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)

#endif // PluginProxy_h
