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

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "ChildProcess.h"
#include <WebCore/CountedUserActivity.h>
#include <WebCore/AudioHardwareListener.h>
#include <wtf/Forward.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class NetscapePluginModule;
class WebProcessConnection;
struct PluginProcessCreationParameters;
        
class PluginProcess : public ChildProcess, private WebCore::AudioHardwareListener::Client
{
    WTF_MAKE_NONCOPYABLE(PluginProcess);
    friend class NeverDestroyed<PluginProcess>;
public:
    static PluginProcess& shared();

    void removeWebProcessConnection(WebProcessConnection*);

    NetscapePluginModule* netscapePluginModule();

    const String& pluginPath() const { return m_pluginPath; }

#if PLATFORM(COCOA)
    void setModalWindowIsShowing(bool);
    void setFullscreenWindowIsShowing(bool);

    mach_port_t compositingRenderServerPort() const { return m_compositingRenderServerPort; }

    bool launchProcess(const String& launchPath, const Vector<String>& arguments);
    bool launchApplicationAtURL(const String& urlString, const Vector<String>& arguments);
    bool openURL(const String& urlString, int32_t& status, String& launchedURLString);
    bool openFile(const String& urlString);
#endif

    CountedUserActivity& connectionActivity() { return m_connectionActivity; }

private:
    PluginProcess();
    ~PluginProcess();

    // ChildProcess
    virtual void initializeProcess(const ChildProcessInitializationParameters&) override;
    virtual void initializeProcessName(const ChildProcessInitializationParameters&) override;
    virtual void initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&) override;
    virtual bool shouldTerminate() override;
    void platformInitializeProcess(const ChildProcessInitializationParameters&);

#if USE(APPKIT)
    virtual void stopRunLoop() override;
#endif

    // IPC::Connection::Client
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;
    virtual void didClose(IPC::Connection*) override;
    virtual void didReceiveInvalidMessage(IPC::Connection*, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;

    // Message handlers.
    void didReceivePluginProcessMessage(IPC::Connection*, IPC::MessageDecoder&);
    void initializePluginProcess(const PluginProcessCreationParameters&);
    void createWebProcessConnection();
    void getSitesWithData(uint64_t callbackID);
    void clearSiteData(const Vector<String>& sites, uint64_t flags, uint64_t maxAgeInSeconds, uint64_t callbackID);
    
    // AudioHardwareListenerClient
    virtual void audioHardwareDidBecomeActive() override;
    virtual void audioHardwareDidBecomeInactive() override;
    virtual void audioOutputDeviceChanged() override { }

    void platformInitializePluginProcess(const PluginProcessCreationParameters&);
    
    void setMinimumLifetime(double);
    void minimumLifetimeTimerFired();
    // Our web process connections.
    Vector<RefPtr<WebProcessConnection>> m_webProcessConnections;

    // The plug-in path.
    String m_pluginPath;

#if PLATFORM(COCOA)
    String m_pluginBundleIdentifier;
#endif

    // The plug-in module.
    RefPtr<NetscapePluginModule> m_pluginModule;
    
    bool m_supportsAsynchronousPluginInitialization;

    RunLoop::Timer<PluginProcess> m_minimumLifetimeTimer;

#if PLATFORM(COCOA)
    // The Mach port used for accelerated compositing.
    mach_port_t m_compositingRenderServerPort;

    String m_nsurlCacheDirectory;
#endif

    static void lowMemoryHandler(bool critical);
    CountedUserActivity m_connectionActivity;

    RefPtr<WebCore::AudioHardwareListener> m_audioHardwareListener;
};

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)

#endif // PluginProcess_h
