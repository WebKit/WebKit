/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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

#include <WebCore/MediaSessionManagerInterface.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakListHashSet.h>

namespace WebCore {

struct MediaConfiguration;
struct NowPlayingInfo;
struct NowPlayingMetadata;

class WEBCORE_EXPORT PlatformMediaSessionManager : public MediaSessionManagerInterface
{
    WTF_MAKE_TZONE_ALLOCATED(PlatformMediaSessionManager);
public:
    static RefPtr<PlatformMediaSessionManager> create(PageIdentifier);

    void forEachMatchingSession(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& predicate, NOESCAPE const Function<void(PlatformMediaSessionInterface&)>& matchingCallback) final;

protected:
    explicit PlatformMediaSessionManager(PageIdentifier);

    void addSession(PlatformMediaSessionInterface&) override;
    void removeSession(PlatformMediaSessionInterface&) override;
    void setCurrentSession(PlatformMediaSessionInterface&) override;
    RefPtr<PlatformMediaSessionInterface> currentSession() const final;

    WeakListHashSet<PlatformMediaSessionInterface>& sessions() const final;
    Vector<WeakPtr<PlatformMediaSessionInterface>> copySessionsToVector() const final;
    WeakPtr<PlatformMediaSessionInterface> bestEligibleSessionForRemoteControls(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>&, PlatformMediaSessionPlaybackControlsPurpose) final;

private:
    mutable WeakListHashSet<PlatformMediaSessionInterface> m_sessions;
};

} // namespace WebCore
