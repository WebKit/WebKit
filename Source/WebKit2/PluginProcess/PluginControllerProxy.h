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

#ifndef PluginControllerProxy_h
#define PluginControllerProxy_h

#if ENABLE(PLUGIN_PROCESS)

#include "Connection.h"
#include "Plugin.h"
#include "PluginController.h"
#include "PluginControllerProxyMessages.h"
#include "RunLoop.h"
#include "ShareableBitmap.h"
#include <wtf/Noncopyable.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>

typedef struct __WKCARemoteLayerClientRef *WKCARemoteLayerClientRef;
#endif

namespace CoreIPC {
    class DataReference;
}

namespace WebKit {

class ShareableBitmap;
class WebProcessConnection;

class PluginControllerProxy : PluginController {
    WTF_MAKE_NONCOPYABLE(PluginControllerProxy);

public:
    static PassOwnPtr<PluginControllerProxy> create(WebProcessConnection* connection, uint64_t pluginInstanceID, const String& userAgent, bool isPrivateBrowsingEnabled, bool isAcceleratedCompositingEnabled);
    ~PluginControllerProxy();

    uint64_t pluginInstanceID() const { return m_pluginInstanceID; }

    bool initialize(const Plugin::Parameters&);
    void destroy();

    void didReceivePluginControllerProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    CoreIPC::SyncReplyMode didReceiveSyncPluginControllerProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);

#if PLATFORM(MAC)
    uint32_t remoteLayerClientID() const;
#endif

    PluginController* asPluginController() { return this; }

private:
    PluginControllerProxy(WebProcessConnection* connection, uint64_t pluginInstanceID, const String& userAgent, bool isPrivateBrowsingEnabled, bool isAcceleratedCompositingEnabled);

    void startPaintTimer();
    void paint();

    // PluginController
    virtual void invalidate(const WebCore::IntRect&);
    virtual String userAgent();
    virtual void loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, const WebCore::HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups);
    virtual void cancelStreamLoad(uint64_t streamID);
    virtual void cancelManualStreamLoad();
    virtual NPObject* windowScriptNPObject();
    virtual NPObject* pluginElementNPObject();
    virtual bool evaluate(NPObject*, const String& scriptString, NPVariant* result, bool allowPopups);
    virtual void setStatusbarText(const String&);
    virtual bool isAcceleratedCompositingEnabled();
    virtual void pluginProcessCrashed();

#if PLATFORM(MAC)
    virtual void setComplexTextInputEnabled(bool);
    virtual mach_port_t compositingRenderServerPort();
#endif

    virtual String proxiesForURL(const String&);
    virtual String cookiesForURL(const String&);
    virtual void setCookiesForURL(const String& urlString, const String& cookieString);
    virtual bool isPrivateBrowsingEnabled();
    virtual void protectPluginFromDestruction();
    virtual void unprotectPluginFromDestruction();
    
    // Message handlers.
    void frameDidFinishLoading(uint64_t requestID);
    void frameDidFail(uint64_t requestID, bool wasCancelled);
    void geometryDidChange(const WebCore::IntRect& frameRect, const WebCore::IntRect& clipRect, const ShareableBitmap::Handle& backingStoreHandle);
    void didEvaluateJavaScript(uint64_t requestID, const String& requestURLString, const String& result);
    void streamDidReceiveResponse(uint64_t streamID, const String& responseURLString, uint32_t streamLength, uint32_t lastModifiedTime, const String& mimeType, const String& headers);
    void streamDidReceiveData(uint64_t streamID, const CoreIPC::DataReference& data);
    void streamDidFinishLoading(uint64_t streamID);
    void streamDidFail(uint64_t streamID, bool wasCancelled);
    void manualStreamDidReceiveResponse(const String& responseURLString, uint32_t streamLength, uint32_t lastModifiedTime, const String& mimeType, const String& headers);
    void manualStreamDidReceiveData(const CoreIPC::DataReference& data);
    void manualStreamDidFinishLoading();
    void manualStreamDidFail(bool wasCancelled);
    void handleMouseEvent(const WebMouseEvent&, PassRefPtr<Messages::PluginControllerProxy::HandleMouseEvent::DelayedReply>);
    void handleWheelEvent(const WebWheelEvent&, bool& handled);
    void handleMouseEnterEvent(const WebMouseEvent&, bool& handled);
    void handleMouseLeaveEvent(const WebMouseEvent&, bool& handled);
    void handleKeyboardEvent(const WebKeyboardEvent&, bool& handled);
    void paintEntirePlugin();
    void snapshot(ShareableBitmap::Handle& backingStoreHandle);
    void setFocus(bool);
    void didUpdate();
    void getPluginScriptableNPObject(uint64_t& pluginScriptableNPObjectID);

#if PLATFORM(MAC)
    void windowFocusChanged(bool);
    void windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates);
    void windowVisibilityChanged(bool);
    void sendComplexTextInput(const String& textInput);
#endif

    void privateBrowsingStateChanged(bool);

    void platformInitialize();
    void platformDestroy();
    void platformGeometryDidChange();

    WebProcessConnection* m_connection;
    uint64_t m_pluginInstanceID;

    String m_userAgent;
    bool m_isPrivateBrowsingEnabled;
    bool m_isAcceleratedCompositingEnabled;

    RefPtr<Plugin> m_plugin;

    // The plug-in rect and clip rect in window coordinates.
    WebCore::IntRect m_frameRect;
    WebCore::IntRect m_clipRect;

    // The dirty rect in plug-in coordinates.
    WebCore::IntRect m_dirtyRect;

    // The paint timer, used for coalescing painting.
    RunLoop::Timer<PluginControllerProxy> m_paintTimer;
    
    // A counter used to prevent the plug-in from being destroyed.
    unsigned m_pluginDestructionProtectCount;

    // A timer that we use to prevent destruction of the plug-in while plug-in
    // code is on the stack.
    RunLoop::Timer<PluginControllerProxy> m_pluginDestroyTimer;

    // Whether we're waiting for the plug-in proxy in the web process to draw the contents of its
    // backing store into the web process backing store.
    bool m_waitingForDidUpdate;

    // Whether the plug-in has canceled the manual stream load.
    bool m_pluginCanceledManualStreamLoad;

#if PLATFORM(MAC)
    // Whether complex text input is enabled for this plug-in.
    bool m_isComplexTextInputEnabled;

    // For CA plug-ins, this holds the information needed to export the layer hierarchy to the UI process.
    RetainPtr<WKCARemoteLayerClientRef> m_remoteLayerClient;
#endif
    
    // The backing store that this plug-in draws into.
    RefPtr<ShareableBitmap> m_backingStore;

    // The window NPObject.
    NPObject* m_windowNPObject;

    // The plug-in element NPObject.
    NPObject* m_pluginElementNPObject;
};

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)

#endif // PluginControllerProxy_h
