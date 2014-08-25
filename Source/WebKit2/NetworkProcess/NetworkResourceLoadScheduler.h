/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef NetworkResourceLoadScheduler_h
#define NetworkResourceLoadScheduler_h

#include <WebCore/Timer.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

#if ENABLE(NETWORK_PROCESS)

namespace WebKit {

class NetworkResourceLoader;

class NetworkResourceLoadScheduler {
    WTF_MAKE_NONCOPYABLE(NetworkResourceLoadScheduler); WTF_MAKE_FAST_ALLOCATED;

public:
    NetworkResourceLoadScheduler();
    
    void scheduleLoader(PassRefPtr<NetworkResourceLoader>);
    void removeLoader(NetworkResourceLoader*);

    // For NetworkProcess statistics reporting.
    uint64_t loadsPendingCount() const;
    uint64_t loadsActiveCount() const;

private:
    static void platformInitializeNetworkSettings();

    HashSet<RefPtr<NetworkResourceLoader>> m_activeLoaders;
    Vector<RefPtr<NetworkResourceLoader>> m_pendingSerialLoaders;
};

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)

#endif // NetworkResourceLoadScheduler_h
