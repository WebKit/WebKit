/*
 * Copyright (C) 2016-2019 Apple Inc.  All rights reserved.
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

#include "CanvasActivityRecord.h"
#include "PageIdentifier.h"
#include "ResourceLoadStatistics.h"
#include "Timer.h"
#include <pal/SessionID.h>
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
class RegistrableDomain;
class ResourceRequest;
class ResourceResponse;
class ScriptExecutionContext;

struct ResourceLoadStatistics;

class ResourceLoadObserver {
    friend class WTF::NeverDestroyed<ResourceLoadObserver>;
public:
    using PerSessionResourceLoadData = Vector<std::pair<PAL::SessionID, Vector<ResourceLoadStatistics>>>;
    WEBCORE_EXPORT static ResourceLoadObserver& shared();

    void logSubresourceLoading(const Frame*, const ResourceRequest& newRequest, const ResourceResponse& redirectResponse);
    void logWebSocketLoading(const URL& targetURL, const URL& mainFrameURL, PAL::SessionID);
    void logUserInteractionWithReducedTimeResolution(const Document&);
    
    void logFontLoad(const Document&, const String& familyName, bool loadStatus);
    void logCanvasRead(const Document&);
    void logCanvasWriteOrMeasure(const Document&, const String& textWritten);
    void logNavigatorAPIAccessed(const Document&, const ResourceLoadStatistics::NavigatorAPI);
    void logScreenAPIAccessed(const Document&, const ResourceLoadStatistics::ScreenAPI);

    WEBCORE_EXPORT String statisticsForURL(PAL::SessionID, const URL&);

    WEBCORE_EXPORT void setStatisticsUpdatedCallback(Function<void(PerSessionResourceLoadData&&)>&&);
    WEBCORE_EXPORT void setRequestStorageAccessUnderOpenerCallback(Function<void(PAL::SessionID, const RegistrableDomain&, PageIdentifier, const RegistrableDomain&)>&&);
    WEBCORE_EXPORT void setLogUserInteractionNotificationCallback(Function<void(PAL::SessionID, const RegistrableDomain&)>&&);

    WEBCORE_EXPORT void updateCentralStatisticsStore();
    WEBCORE_EXPORT void clearState();

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    bool shouldLogUserInteraction() const { return m_shouldLogUserInteraction; }
    void setShouldLogUserInteraction(bool shouldLogUserInteraction) { m_shouldLogUserInteraction = shouldLogUserInteraction; }
#endif

private:
    ResourceLoadObserver();

    bool shouldLog(PAL::SessionID) const;
    ResourceLoadStatistics& ensureResourceStatisticsForRegistrableDomain(PAL::SessionID, const RegistrableDomain&);
    void scheduleNotificationIfNeeded();

    PerSessionResourceLoadData takeStatistics();

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    void requestStorageAccessUnderOpener(PAL::SessionID, const RegistrableDomain& domainInNeedOfStorageAccess, PageIdentifier openerPageID, Document& openerDocument);
#endif

    HashMap<PAL::SessionID, std::unique_ptr<HashMap<RegistrableDomain, ResourceLoadStatistics>>> m_perSessionResourceStatisticsMap;
    HashMap<RegistrableDomain, WTF::WallTime> m_lastReportedUserInteractionMap;
    Function<void(PerSessionResourceLoadData)> m_notificationCallback;
    Function<void(PAL::SessionID, const RegistrableDomain&, PageIdentifier, const RegistrableDomain&)> m_requestStorageAccessUnderOpenerCallback;
    Function<void(PAL::SessionID, const RegistrableDomain&)> m_logUserInteractionNotificationCallback;

    Timer m_notificationTimer;

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    uint64_t m_loggingCounter { 0 };
    bool m_shouldLogUserInteraction { false };
#endif

    URL nonNullOwnerURL(const Document&) const;
};
    
} // namespace WebCore
