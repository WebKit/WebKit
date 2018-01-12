/*
 * Copyright (C) 2016-2017 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#pragma once

#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

namespace WTF {
class Lock;
class WorkQueue;
class WallTime;
}

namespace WebCore {

class Document;
class Frame;
class Page;
class ResourceRequest;
class ResourceResponse;
class URL;

struct ResourceLoadStatistics;

class ResourceLoadObserver {
    friend class WTF::NeverDestroyed<ResourceLoadObserver>;
public:
    WEBCORE_EXPORT static ResourceLoadObserver& shared();

    void logFrameNavigation(const Frame&, const Frame& topFrame, const ResourceRequest& newRequest, const URL& redirectUrl);
    void logSubresourceLoading(const Frame*, const ResourceRequest& newRequest, const ResourceResponse& redirectResponse);
    void logWebSocketLoading(const Frame*, const URL&);
    void logUserInteractionWithReducedTimeResolution(const Document&);

    WEBCORE_EXPORT String statisticsForOrigin(const String&);

    WEBCORE_EXPORT void setNotificationCallback(WTF::Function<void (Vector<ResourceLoadStatistics>&&)>&&);

    WEBCORE_EXPORT void notifyObserver();
    WEBCORE_EXPORT void clearState();

#if HAVE(CFNETWORK_STORAGE_PARTITIONING) && !RELEASE_LOG_DISABLED
    bool shouldLogUserInteraction() const { return m_shouldLogUserInteraction; }
    void setShouldLogUserInteraction(bool shouldLogUserInteraction) { m_shouldLogUserInteraction = shouldLogUserInteraction; }
#endif

private:
    ResourceLoadObserver();

    bool shouldLog(Page*) const;
    ResourceLoadStatistics& ensureResourceStatisticsForPrimaryDomain(const String&);

    void scheduleNotificationIfNeeded();
    Vector<ResourceLoadStatistics> takeStatistics();

    HashMap<String, ResourceLoadStatistics> m_resourceStatisticsMap;
    HashMap<String, WTF::WallTime> m_lastReportedUserInteractionMap;
    WTF::Function<void (Vector<ResourceLoadStatistics>&&)> m_notificationCallback;
    Timer m_notificationTimer;
#if HAVE(CFNETWORK_STORAGE_PARTITIONING) && !RELEASE_LOG_DISABLED
    uint64_t m_loggingCounter { 0 };
    bool m_shouldLogUserInteraction { false };
#endif

    URL nonNullOwnerURL(const Document&) const;
};
    
} // namespace WebCore
