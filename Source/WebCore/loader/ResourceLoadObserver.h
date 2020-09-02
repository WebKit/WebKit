/*
 * Copyright (C) 2016-2020 Apple Inc.  All rights reserved.
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

#include "ResourceLoadStatistics.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>

namespace WebCore {

class Document;
class Frame;
class ResourceRequest;
class ResourceResponse;

class ResourceLoadObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // https://fetch.spec.whatwg.org/#request-destination-script-like
    enum class FetchDestinationIsScriptLike : bool { Yes, No };

    WEBCORE_EXPORT static ResourceLoadObserver& shared();
    WEBCORE_EXPORT static ResourceLoadObserver* sharedIfExists();
    WEBCORE_EXPORT static void setShared(ResourceLoadObserver&);
    
    virtual ~ResourceLoadObserver() { }

    virtual void logSubresourceLoading(const Frame*, const ResourceRequest& /* newRequest */, const ResourceResponse& /* redirectResponse */, FetchDestinationIsScriptLike) { }
    virtual void logWebSocketLoading(const URL& /* targetURL */, const URL& /* mainFrameURL */) { }
    virtual void logUserInteractionWithReducedTimeResolution(const Document&) { }
    virtual void logFontLoad(const Document&, const String& /* familyName */, bool /* loadStatus */) { }
    virtual void logCanvasRead(const Document&) { }
    virtual void logCanvasWriteOrMeasure(const Document&, const String& /* textWritten */) { }
    virtual void logNavigatorAPIAccessed(const Document&, const ResourceLoadStatistics::NavigatorAPI) { }
    virtual void logScreenAPIAccessed(const Document&, const ResourceLoadStatistics::ScreenAPI) { }
    virtual void logSubresourceLoadingForTesting(const RegistrableDomain& /* firstPartyDomain */, const RegistrableDomain& /* thirdPartyDomain */, bool /* shouldScheduleNotification */) { }

    virtual String statisticsForURL(const URL&) { return { }; }
    virtual void updateCentralStatisticsStore(CompletionHandler<void()>&& completionHandler) { completionHandler(); }
    virtual void clearState() { }
    
    virtual bool hasStatistics() const { return false; }

    virtual void setDomainsWithUserInteraction(HashSet<RegistrableDomain>&&) { }
    virtual bool hasHadUserInteraction(const RegistrableDomain&) const { return false; }
};
    
} // namespace WebCore
