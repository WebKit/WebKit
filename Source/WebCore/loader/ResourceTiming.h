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
#include "NetworkLoadTiming.h"
#include "URL.h"

namespace WebCore {

class CachedResource;
class ResourceResponse;
class SecurityOrigin;

class ResourceTiming {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static ResourceTiming fromCache(const URL&, const String& initiator, const LoadTiming&);
    static ResourceTiming fromLoad(CachedResource&, const String& initiator, const LoadTiming&, const SecurityOrigin&);
    static ResourceTiming fromSynchronousLoad(const URL&, const String& initiator, const LoadTiming&, const NetworkLoadTiming&, const ResourceResponse&, const SecurityOrigin&);

    URL url() const { return m_url; }
    String initiator() const { return m_initiator; }
    LoadTiming loadTiming() const { return m_loadTiming; }
    NetworkLoadTiming networkLoadTiming() const { return m_networkLoadTiming; }
    bool allowTimingDetails() const { return m_allowTimingDetails; }

    ResourceTiming isolatedCopy() const;

    void overrideInitiatorName(const String& name) { m_initiator = name; }

private:
    ResourceTiming(CachedResource&, const String& initiator, const LoadTiming&, const SecurityOrigin&);
    ResourceTiming(const URL&, const String& initiator, const LoadTiming&, const NetworkLoadTiming&, const ResourceResponse&, const SecurityOrigin&);
    ResourceTiming(const URL&, const String& initiator, const LoadTiming&);
    ResourceTiming(const URL& url, const String& initiator, const LoadTiming& loadTiming, const NetworkLoadTiming& networkLoadTiming, bool allowTimingDetails)
        : m_url(url)
        , m_initiator(initiator)
        , m_loadTiming(loadTiming)
        , m_networkLoadTiming(networkLoadTiming)
        , m_allowTimingDetails(allowTimingDetails)
    {
    }

    URL m_url;
    String m_initiator;
    LoadTiming m_loadTiming;
    NetworkLoadTiming m_networkLoadTiming;
    bool m_allowTimingDetails { false };
};

} // namespace WebCore
