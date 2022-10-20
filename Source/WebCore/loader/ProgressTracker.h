/*
 * Copyright (C) 2007, 2018 Apple Inc.  All rights reserved.
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

#include "Page.h"
#include "ResourceLoaderIdentifier.h"
#include "Timer.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class LocalFrame;
class ResourceResponse;
class ProgressTrackerClient;
struct ProgressItem;

class ProgressTracker {
    WTF_MAKE_NONCOPYABLE(ProgressTracker);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ProgressTracker(Page&, UniqueRef<ProgressTrackerClient>&&);
    ~ProgressTracker();

    ProgressTrackerClient& client() { return m_client.get(); }

    WEBCORE_EXPORT double estimatedProgress() const;

    void progressStarted(LocalFrame&);
    void progressCompleted(LocalFrame&);

    void incrementProgress(ResourceLoaderIdentifier, const ResourceResponse&);
    void incrementProgress(ResourceLoaderIdentifier, unsigned bytesReceived);
    void completeProgress(ResourceLoaderIdentifier);

    long long totalPageAndResourceBytesToLoad() const { return m_totalPageAndResourceBytesToLoad; }
    long long totalBytesReceived() const { return m_totalBytesReceived; }

    bool isMainLoadProgressing() const;

private:
    void reset();
    void finalProgressComplete();
    void progressEstimateChanged(LocalFrame&);

    void progressHeartbeatTimerFired();

    Page& m_page;
    UniqueRef<ProgressTrackerClient> m_client;
    RefPtr<LocalFrame> m_originatingProgressFrame;
    HashMap<ResourceLoaderIdentifier, std::unique_ptr<ProgressItem>> m_progressItems;
    Timer m_progressHeartbeatTimer;

    long long m_totalPageAndResourceBytesToLoad { 0 };
    long long m_totalBytesReceived { 0 };
    long long m_totalBytesReceivedBeforePreviousHeartbeat { 0 };

    double m_lastNotifiedProgressValue { 0 };
    double m_progressValue { 0 };

    MonotonicTime m_mainLoadCompletionTime;
    MonotonicTime m_lastNotifiedProgressTime;

    int m_numProgressTrackedFrames { 0 };
    unsigned m_heartbeatsWithNoProgress { 0 };

    bool m_finalProgressChangedSent { false };
    bool m_isMainLoad { false };
};

} // namespace WebCore
