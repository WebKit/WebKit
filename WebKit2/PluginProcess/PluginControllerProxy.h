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
#include "RunLoop.h"
#include "SharedMemory.h"
#include <wtf/Noncopyable.h>

namespace WebKit {

class BackingStore;
class WebProcessConnection;

class PluginControllerProxy : PluginController {
    WTF_MAKE_NONCOPYABLE(PluginControllerProxy);

public:
    static PassOwnPtr<PluginControllerProxy> create(WebProcessConnection* connection, uint64_t pluginInstanceID);
    ~PluginControllerProxy();

    uint64_t pluginInstanceID() const { return m_pluginInstanceID; }

    bool initialize(const Plugin::Parameters&);
    void destroy();

    void didReceivePluginControllerProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

private:
    PluginControllerProxy(WebProcessConnection* connection, uint64_t pluginInstanceID);

    void paint();

    // PluginController
    virtual void invalidate(const WebCore::IntRect&);
    virtual String userAgent(const WebCore::KURL&);
    virtual void loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, const WebCore::HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups);
    virtual void cancelStreamLoad(uint64_t streamID);
    virtual void cancelManualStreamLoad();
    virtual NPObject* windowScriptNPObject();
    virtual NPObject* pluginElementNPObject();
    virtual bool evaluate(NPObject*, const String& scriptString, NPVariant* result, bool allowPopups);
    virtual void setStatusbarText(const String&);
    virtual bool isAcceleratedCompositingEnabled();
    virtual void pluginProcessCrashed();

    // Message handlers.
    void geometryDidChange(const WebCore::IntRect& frameRect, const WebCore::IntRect& clipRect, const SharedMemory::Handle& backingStoreHandle);

    WebProcessConnection* m_connection;
    uint64_t m_pluginInstanceID;

    RefPtr<Plugin> m_plugin;

    // The plug-in rect and clip rect in window coordinates.
    WebCore::IntRect m_frameRect;
    WebCore::IntRect m_clipRect;

    // The dirty rect in plug-in coordinates.
    WebCore::IntRect m_dirtyRect;

    // The paint timer, used for coalescing painting.
    RunLoop::Timer<PluginControllerProxy> m_paintTimer;

    // The backing store that this plug-in draws into.
    RefPtr<BackingStore> m_backingStore;
};

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)

#endif // PluginControllerProxy_h
