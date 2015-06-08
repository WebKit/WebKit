/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DNSResolveQueue_h
#define DNSResolveQueue_h

#include "Timer.h"
#include <atomic>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class DNSResolveQueue {
    friend NeverDestroyed<DNSResolveQueue>;

public:
    static DNSResolveQueue& singleton();

    void add(const String& hostname);
    void decrementRequestCount()
    {
        --m_requestsInFlight;
    }

private:
    DNSResolveQueue();

    // This function performs the actual DNS prefetch. Platforms must ensure that performing the
    // prefetch will not violate the user's expectations of privacy; for example, if an HTTP proxy
    // is in use, then performing a DNS lookup would be inappropriate, but this may be acceptable
    // for other types of proxies (e.g. SOCKS proxies).
    void platformMaybeResolveHost(const String&);

    void timerFired();

    Timer m_timer;

    HashSet<String> m_names;
    std::atomic<int> m_requestsInFlight;
};

}

#endif // DNSResolveQueue_h
