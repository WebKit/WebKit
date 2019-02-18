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

#include <wtf/HashMap.h>
#include <wtf/RunLoop.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebProcessPool;
class WebProcessProxy;
class WebsiteDataStore;

class WebProcessCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WebProcessCache(WebProcessPool&);

    bool addProcess(const String& registrableDomain, Ref<WebProcessProxy>&&);
    RefPtr<WebProcessProxy> takeProcess(const String& registrableDomain, WebsiteDataStore&);

    void updateCapacity(WebProcessPool&);
    unsigned capacity() { return m_capacity; }

    unsigned size() const { return m_processesPerRegistrableDomain.size(); }

    void clear();
    void setApplicationIsActive(bool);

private:
    static Seconds cachedProcessLifetime;
    static Seconds clearingDelayAfterApplicationResignsActive;

    void evictProcess(WebProcessProxy&);
    void platformInitialize();

    unsigned m_capacity { 0 };

    class CachedProcess {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        CachedProcess(Ref<WebProcessProxy>&&);
        ~CachedProcess();

        Ref<WebProcessProxy> takeProcess();
        WebProcessProxy& process() { ASSERT(m_process); return *m_process; }

    private:
        void evictionTimerFired();

        RefPtr<WebProcessProxy> m_process;
        RunLoop::Timer<CachedProcess> m_evictionTimer;
    };

    HashMap<String, std::unique_ptr<CachedProcess>> m_processesPerRegistrableDomain;
    RunLoop::Timer<WebProcessCache> m_evictionTimer;
};

} // namespace WebKit
