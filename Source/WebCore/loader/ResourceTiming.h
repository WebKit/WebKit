/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "LoadTiming.h"
#include "NetworkLoadMetrics.h"
#include "ServerTiming.h"
#include <wtf/URL.h>

namespace WebCore {

class CachedResource;
class PerformanceServerTiming;
class ResourceResponse;
class SecurityOrigin;

class ResourceTiming {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static ResourceTiming fromCache(const URL&, const String& initiator, const LoadTiming&, const ResourceResponse&, const SecurityOrigin&);
    static ResourceTiming fromLoad(CachedResource&, const String& initiator, const LoadTiming&, const NetworkLoadMetrics&, const SecurityOrigin&);
    static ResourceTiming fromSynchronousLoad(const URL&, const String& initiator, const LoadTiming&, const NetworkLoadMetrics&, const ResourceResponse&, const SecurityOrigin&);

    URL url() const { return m_url; }
    String initiator() const { return m_initiator; }
    LoadTiming loadTiming() const { return m_loadTiming; }
    NetworkLoadMetrics networkLoadMetrics() const { return m_networkLoadMetrics; }
    bool allowTimingDetails() const { return m_allowTimingDetails; }
    Vector<Ref<PerformanceServerTiming>> populateServerTiming();
    ResourceTiming isolatedCopy() const;

    void overrideInitiatorName(const String& name) { m_initiator = name; }

private:
    ResourceTiming(const URL&, const String& initiator, const LoadTiming&, const NetworkLoadMetrics&, const ResourceResponse&, const SecurityOrigin&);
    ResourceTiming(const URL&, const String& initiator, const LoadTiming&, const ResourceResponse&, const SecurityOrigin&);
    ResourceTiming(URL&& url, String&& initiator, LoadTiming&& loadTiming, NetworkLoadMetrics&& networkLoadMetrics, bool allowTimingDetails, Vector<ServerTiming>&& serverTiming)
        : m_url(WTFMove(url))
        , m_initiator(WTFMove(initiator))
        , m_loadTiming(WTFMove(loadTiming))
        , m_networkLoadMetrics(WTFMove(networkLoadMetrics))
        , m_allowTimingDetails(allowTimingDetails)
        , m_serverTiming(WTFMove(serverTiming))
    {
    }
    void initServerTiming(const ResourceResponse&);

    URL m_url;
    String m_initiator;
    LoadTiming m_loadTiming;
    NetworkLoadMetrics m_networkLoadMetrics;
    bool m_allowTimingDetails { false };
    Vector<ServerTiming> m_serverTiming;
};

} // namespace WebCore
