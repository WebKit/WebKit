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

#ifndef PluginProcessManager_h
#define PluginProcessManager_h

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "PluginModuleInfo.h"
#include "PluginProcess.h"
#include "PluginProcessAttributes.h"
#include "ProcessThrottler.h"
#include "WebProcessProxyMessages.h"
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounter.h>
#include <wtf/Vector.h>

namespace IPC {
class Encoder;
}

namespace WebKit {

class PluginInfoStore;
class PluginProcessProxy;
class WebProcessProxy;

class PluginProcessManager {
    WTF_MAKE_NONCOPYABLE(PluginProcessManager);
    friend class NeverDestroyed<PluginProcessManager>;
public:
    static PluginProcessManager& singleton();

    uint64_t pluginProcessToken(const PluginModuleInfo&, PluginProcessType, PluginProcessSandboxPolicy);

    void getPluginProcessConnection(uint64_t pluginProcessToken, PassRefPtr<Messages::WebProcessProxy::GetPluginProcessConnection::DelayedReply>);
    void removePluginProcessProxy(PluginProcessProxy*);

    void fetchWebsiteData(const PluginModuleInfo&, std::function<void (Vector<String>)> completionHandler);
    void deleteWebsiteData(const PluginModuleInfo&, std::chrono::system_clock::time_point modifiedSince, std::function<void ()> completionHandler);
    void deleteWebsiteDataForHostNames(const PluginModuleInfo&, const Vector<String>& hostNames, std::function<void ()> completionHandler);

#if PLATFORM(COCOA)
    inline ProcessSuppressionDisabledToken processSuppressionDisabledToken();
    inline bool processSuppressionDisabled() const;
    void updateProcessSuppressionDisabled(RefCounterEvent);
#endif

private:
    PluginProcessManager();

    PluginProcessProxy* getOrCreatePluginProcess(uint64_t pluginProcessToken);

    Vector<std::pair<PluginProcessAttributes, uint64_t>> m_pluginProcessTokens;
    HashSet<uint64_t> m_knownTokens;

    Vector<RefPtr<PluginProcessProxy>> m_pluginProcesses;

#if PLATFORM(COCOA)
    ProcessSuppressionDisabledCounter m_processSuppressionDisabledForPageCounter;
#endif
};

#if PLATFORM(COCOA)
inline ProcessSuppressionDisabledToken PluginProcessManager::processSuppressionDisabledToken()
{
    return m_processSuppressionDisabledForPageCounter.count();
}

inline bool PluginProcessManager::processSuppressionDisabled() const
{
    return m_processSuppressionDisabledForPageCounter.value();
}
#endif

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)

#endif // PluginProcessManager_h
