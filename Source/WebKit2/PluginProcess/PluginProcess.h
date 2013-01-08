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

#ifndef PluginProcess_h
#define PluginProcess_h

#if ENABLE(PLUGIN_PROCESS)

#include "ChildProcess.h"
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class RunLoop;
}

namespace WebKit {

class NetscapePluginModule;
class WebProcessConnection;
struct PluginProcessCreationParameters;
        
class PluginProcess : public ChildProcess {
    WTF_MAKE_NONCOPYABLE(PluginProcess);
public:
    enum Type {
        // Start with value one since default HashTraits<> disallows zero as key.
        TypeRegularProcess = 1,
        TypeSnapshotProcess
    };

    static PluginProcess& shared();

    void removeWebProcessConnection(WebProcessConnection*);

    NetscapePluginModule* netscapePluginModule();

    const String& pluginPath() const { return m_pluginPath; }

#if PLATFORM(MAC)
    void setModalWindowIsShowing(bool);
    void setFullscreenWindowIsShowing(bool);

#if USE(ACCELERATED_COMPOSITING)
    mach_port_t compositingRenderServerPort() const { return m_compositingRenderServerPort; }
#endif
#endif

private:
    PluginProcess();
    ~PluginProcess();

    // ChildProcess
    virtual void initializeProcess(const ChildProcessInitializationParameters&) OVERRIDE;
    virtual bool shouldTerminate() OVERRIDE;

    void platformInitializeProcess(const ChildProcessInitializationParameters&);

    // CoreIPC::Connection::Client
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&) OVERRIDE;
    virtual void didClose(CoreIPC::Connection*) OVERRIDE;
    virtual void didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::StringReference messageReceiverName, CoreIPC::StringReference messageName) OVERRIDE;

    // Message handlers.
    void didReceivePluginProcessMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);
    void initializePluginProcess(const PluginProcessCreationParameters&);
    void createWebProcessConnection();
    void getSitesWithData(uint64_t callbackID);
    void clearSiteData(const Vector<String>& sites, uint64_t flags, uint64_t maxAgeInSeconds, uint64_t callbackID);

    void platformInitializePluginProcess(const PluginProcessCreationParameters&);
    
    void setMinimumLifetime(double);
    void minimumLifetimeTimerFired();

    // Our web process connections.
    Vector<RefPtr<WebProcessConnection> > m_webProcessConnections;

    // The plug-in path.
    String m_pluginPath;

    // The plug-in module.
    RefPtr<NetscapePluginModule> m_pluginModule;
    
    bool m_supportsAsynchronousPluginInitialization;

    WebCore::RunLoop::Timer<PluginProcess> m_minimumLifetimeTimer;
    
#if USE(ACCELERATED_COMPOSITING) && PLATFORM(MAC)
    // The Mach port used for accelerated compositing.
    mach_port_t m_compositingRenderServerPort;
#endif

};

} // namespace WebKit

namespace WTF {

template<> struct DefaultHash<WebKit::PluginProcess::Type> { typedef DefaultHash<uint32_t>::Hash Hash; };
template<> struct IsInteger<WebKit::PluginProcess::Type> { static const bool value = true; };

} // namespace WTF

#endif // ENABLE(PLUGIN_PROCESS)

#endif // PluginProcess_h
