/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "WebProcessProxy.h"
#include <WebCore/RegistrableDomain.h>
#include <pal/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/RunLoop.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebProcessPool;
class WebsiteDataStore;

class WebProcessCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WebProcessCache(WebProcessPool&);

    bool addProcessIfPossible(Ref<WebProcessProxy>&&);
    RefPtr<WebProcessProxy> takeProcess(const WebCore::RegistrableDomain&, WebsiteDataStore&, WebProcessProxy::LockdownMode);

    void updateCapacity(WebProcessPool&);
    unsigned capacity() const { return m_capacity; }

    unsigned size() const { return m_processesPerRegistrableDomain.size(); }

    void clear();
    void setApplicationIsActive(bool);

    void clearAllProcessesForSession(PAL::SessionID);

    enum class ShouldShutDownProcess { No, Yes };
    void removeProcess(WebProcessProxy&, ShouldShutDownProcess);
    static void setCachedProcessSuspensionDelayForTesting(Seconds);

private:
    static Seconds cachedProcessLifetime;
    static Seconds clearingDelayAfterApplicationResignsActive;

    class CachedProcess {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        CachedProcess(Ref<WebProcessProxy>&&);
        ~CachedProcess();

        Ref<WebProcessProxy> takeProcess();
        WebProcessProxy& process() { ASSERT(m_process); return *m_process; }
        void startSuspensionTimer();

#if PLATFORM(MAC) || PLATFORM(GTK) || PLATFORM(WPE)
        bool isSuspended() const { return !m_suspensionTimer.isActive(); }
#endif

    private:
        void evictionTimerFired();
#if PLATFORM(MAC) || PLATFORM(GTK) || PLATFORM(WPE)
        void suspensionTimerFired();
#endif

        RefPtr<WebProcessProxy> m_process;
        RunLoop::Timer m_evictionTimer;
#if PLATFORM(MAC) || PLATFORM(GTK) || PLATFORM(WPE)
        RunLoop::Timer m_suspensionTimer;
#endif
    };

    bool canCacheProcess(WebProcessProxy&) const;
    void platformInitialize();
    bool addProcess(std::unique_ptr<CachedProcess>&&);

    unsigned m_capacity { 0 };

    HashMap<uint64_t, std::unique_ptr<CachedProcess>> m_pendingAddRequests;
    HashMap<WebCore::RegistrableDomain, std::unique_ptr<CachedProcess>> m_processesPerRegistrableDomain;
    RunLoop::Timer m_evictionTimer;
};

} // namespace WebKit
