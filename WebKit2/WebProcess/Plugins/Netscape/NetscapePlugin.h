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

#ifndef NetscapePlugin_h
#define NetscapePlugin_h

#include "NetscapePluginModule.h"
#include "Plugin.h"
#include <WebCore/IntRect.h>

namespace WebKit {

class NetscapePlugin : public Plugin {
public:
    static PassRefPtr<NetscapePlugin> create(PassRefPtr<NetscapePluginModule> pluginModule)
    {
        return adoptRef(new NetscapePlugin(pluginModule));
    }
    virtual ~NetscapePlugin();

    static PassRefPtr<NetscapePlugin> fromNPP(NPP);

#if PLATFORM(MAC)
    NPError setDrawingModel(NPDrawingModel);
    NPError setEventModel(NPEventModel);
#endif

    void invalidate(const NPRect*);
    const char* userAgent();
    void loadURL(const WebCore::String& urlString, const WebCore::String& target, bool sendNotification, void* notificationData);
    
private:
    NetscapePlugin(PassRefPtr<NetscapePluginModule> pluginModule);

    void callSetWindow();

    bool platformPostInitialize();
    void platformPaint(WebCore::GraphicsContext*, const WebCore::IntRect& dirtyRect);

    // Plugin
    virtual bool initialize(PluginController*, const Parameters&);
    virtual void destroy();
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect& dirtyRect);
    virtual void geometryDidChange(const WebCore::IntRect& frameRect, const WebCore::IntRect& clipRect);
    virtual void frameDidFinishLoading(uint64_t requestID);
    virtual void frameDidFail(uint64_t requestID, bool wasCancelled);

    PluginController* m_pluginController;
    uint64_t m_nextRequestID;

    RefPtr<NetscapePluginModule> m_pluginModule;
    NPP_t m_npp;
    NPWindow m_npWindow;

    WebCore::IntRect m_frameRect;
    WebCore::IntRect m_clipRect;

    CString m_userAgent;

    bool m_isStarted;
    bool m_inNPPNew;

#if PLATFORM(MAC)
    NPDrawingModel m_drawingModel;
    NPEventModel m_eventModel;
#endif
};

// Move these functions to NetscapePluginWin.cpp
#if !PLATFORM(MAC)
inline bool NetscapePlugin::platformPostInitialize()
{
    return true;
}

inline void NetscapePlugin::platformPaint(WebCore::GraphicsContext*, const WebCore::IntRect&)
{
}
#endif

} // namespace WebKit

#endif // NetscapePlugin_h
