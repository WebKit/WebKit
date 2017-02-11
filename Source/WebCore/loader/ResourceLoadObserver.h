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

#include "ResourceLoadStatisticsStore.h"
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class Frame;
class Page;
class ResourceRequest;
class ResourceResponse;
class URL;

struct ResourceLoadStatistics;

class ResourceLoadObserver {
    friend class NeverDestroyed<ResourceLoadObserver>;
public:
    WEBCORE_EXPORT static ResourceLoadObserver& sharedObserver();
    
    void logFrameNavigation(const Frame& frame, const Frame& topFrame, const ResourceRequest& newRequest, const ResourceResponse& redirectResponse);
    void logSubresourceLoading(const Frame*, const ResourceRequest& newRequest, const ResourceResponse& redirectResponse);
    void logWebSocketLoading(const Frame*, const URL&);
    void logUserInteractionWithReducedTimeResolution(const Document&);

    WEBCORE_EXPORT void logUserInteraction(const URL&);
    WEBCORE_EXPORT bool hasHadUserInteraction(const URL&);
    WEBCORE_EXPORT void clearUserInteraction(const URL&);

    WEBCORE_EXPORT void setPrevalentResource(const URL&);
    WEBCORE_EXPORT bool isPrevalentResource(const URL&);
    WEBCORE_EXPORT void clearPrevalentResource(const URL&);

    WEBCORE_EXPORT void setTimeToLiveUserInteraction(double seconds);
    WEBCORE_EXPORT void setReducedTimestampResolution(double seconds);

    WEBCORE_EXPORT void fireDataModificationHandler();

    WEBCORE_EXPORT RefPtr<ResourceLoadStatisticsStore> statisticsStore();
    WEBCORE_EXPORT void setStatisticsStore(Ref<ResourceLoadStatisticsStore>&&);

    WEBCORE_EXPORT String statisticsForOrigin(const String&);

private:
    bool shouldLog(Page*);
    static String primaryDomain(const URL&);

    RefPtr<ResourceLoadStatisticsStore> m_store;
    HashMap<String, size_t> m_originsVisitedMap;
};
    
} // namespace WebCore
