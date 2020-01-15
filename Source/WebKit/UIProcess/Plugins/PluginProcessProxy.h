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

#pragma once

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "AuxiliaryProcessProxy.h"
#include "Connection.h"
#include "PluginModuleInfo.h"
#include "PluginProcess.h"
#include "PluginProcessAttributes.h"
#include "ProcessLauncher.h"
#include "WebProcessProxyMessagesReplies.h"
#include <wtf/Deque.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
OBJC_CLASS NSObject;
OBJC_CLASS WKPlaceholderModalWindow;
#endif

namespace WebKit {

class PluginProcessManager;
class WebProcessProxy;
struct PluginProcessCreationParameters;

#if PLUGIN_ARCHITECTURE(UNIX)
struct RawPluginMetaData {
    String name;
    String description;
    String mimeDescription;
};
#endif

#if PLATFORM(COCOA)
int pluginProcessLatencyQOS();
int pluginProcessThroughputQOS();
#endif

class PluginProcessProxy final : public AuxiliaryProcessProxy, public ThreadSafeRefCounted<PluginProcessProxy> {
public:
    static Ref<PluginProcessProxy> create(PluginProcessManager*, const PluginProcessAttributes&, uint64_t pluginProcessToken);
    ~PluginProcessProxy();

    const PluginProcessAttributes& pluginProcessAttributes() const { return m_pluginProcessAttributes; }
    uint64_t pluginProcessToken() const { return m_pluginProcessToken; }

    // Asks the plug-in process to create a new connection to a web process. The connection identifier will be
    // encoded in the given argument encoder and sent back to the connection of the given web process.
    void getPluginProcessConnection(Messages::WebProcessProxy::GetPluginProcessConnectionDelayedReply&&);

    void fetchWebsiteData(CompletionHandler<void (Vector<String>)>&&);
    void deleteWebsiteData(WallTime modifiedSince, CompletionHandler<void ()>&&);
    void deleteWebsiteDataForHostNames(const Vector<String>& hostNames, CompletionHandler<void ()>&&);

#if OS(LINUX)
    void sendMemoryPressureEvent(bool isCritical);
#endif

    bool isValid() const { return m_connection; }

#if PLUGIN_ARCHITECTURE(UNIX)
    static bool scanPlugin(const String& pluginPath, RawPluginMetaData& result);
#endif

private:
    PluginProcessProxy(PluginProcessManager*, const PluginProcessAttributes&, uint64_t pluginProcessToken);

    // AuxiliaryProcessProxy
    ASCIILiteral processName() const final { return "Plugin"_s; }

    void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    void platformGetLaunchOptionsWithAttributes(ProcessLauncher::LaunchOptions&, const PluginProcessAttributes&);
    void processWillShutDown(IPC::Connection&) override;

    void pluginProcessCrashedOrFailedToLaunch();

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;

    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;

    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

    // Message handlers
    void didCreateWebProcessConnection(const IPC::Attachment&, bool supportsAsynchronousPluginInitialization);
    void didGetSitesWithData(const Vector<String>& sites, uint64_t callbackID);
    void didDeleteWebsiteData(uint64_t callbackID);
    void didDeleteWebsiteDataForHostNames(uint64_t callbackID);

#if PLATFORM(COCOA)
    bool getPluginProcessSerialNumber(ProcessSerialNumber&);
    void makePluginProcessTheFrontProcess();
    void makeUIProcessTheFrontProcess();

    void setFullscreenWindowIsShowing(bool);
    void enterFullscreen();
    void exitFullscreen();

    void setModalWindowIsShowing(bool);
    void beginModal();
    void endModal();

    void applicationDidBecomeActive();
    void launchProcess(const String& launchPath, const Vector<String>& arguments, CompletionHandler<void(bool)>&&);
    void launchApplicationAtURL(const String& urlString, const Vector<String>& arguments, CompletionHandler<void(bool)>&&);
    void openURL(const String& url, CompletionHandler<void(bool result, int32_t status, String launchedURLString)>&&);
    void openFile(const String& fullPath, CompletionHandler<void(bool)>&&);
#endif

    void platformInitializePluginProcess(PluginProcessCreationParameters& parameters);

    // The plug-in host process manager.
    PluginProcessManager* m_pluginProcessManager;

    PluginProcessAttributes m_pluginProcessAttributes;
    uint64_t m_pluginProcessToken;

    // The connection to the plug-in host process.
    RefPtr<IPC::Connection> m_connection;

    Deque<Messages::WebProcessProxy::GetPluginProcessConnectionDelayedReply> m_pendingConnectionReplies;

    Vector<uint64_t> m_pendingFetchWebsiteDataRequests;
    HashMap<uint64_t, CompletionHandler<void (Vector<String>)>> m_pendingFetchWebsiteDataCallbacks;

    struct DeleteWebsiteDataRequest {
        WallTime modifiedSince;
        uint64_t callbackID;
    };
    Vector<DeleteWebsiteDataRequest> m_pendingDeleteWebsiteDataRequests;
    HashMap<uint64_t, CompletionHandler<void ()>> m_pendingDeleteWebsiteDataCallbacks;

    struct DeleteWebsiteDataForHostNamesRequest {
        Vector<String> hostNames;
        uint64_t callbackID;
    };
    Vector<DeleteWebsiteDataForHostNamesRequest> m_pendingDeleteWebsiteDataForHostNamesRequests;
    HashMap<uint64_t, CompletionHandler<void ()>> m_pendingDeleteWebsiteDataForHostNamesCallbacks;

    // If createPluginConnection is called while the process is still launching we'll keep count of it and send a bunch of requests
    // when the process finishes launching.
    unsigned m_numPendingConnectionRequests;

#if PLATFORM(COCOA)
    RetainPtr<NSObject> m_activationObserver;
    RetainPtr<WKPlaceholderModalWindow> m_placeholderWindow;
    bool m_modalWindowIsShowing;
    bool m_fullscreenWindowIsShowing;
    unsigned m_preFullscreenAppPresentationOptions;
#endif
};

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
