/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/SharedMemory.h>
#include <wtf/Function.h>
#include <wtf/MediaTime.h>
#include <wtf/MonotonicTime.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class SharedTimebaseHandle {
public:
    SharedMemoryHandle memory;
    void takeOwnershipOfMemory(MemoryLedger);
};

class WEBCORE_EXPORT SharedTimebase final {
    WTF_MAKE_TZONE_ALLOCATED(SharedTimebase);
public:
    using Handle = SharedTimebaseHandle;

    struct Snapshot {
        MediaTime currentTime;
        double playbackRate { 0 };
        MonotonicTime hostTime;
    };

    static std::unique_ptr<SharedTimebase> create();
    ~SharedTimebase();

    std::optional<Handle> createHandle() const;

    void storeSnapshot(Snapshot);

private:
    friend constexpr std::unique_ptr<SharedTimebase> std::make_unique<SharedTimebase>(Ref<WebCore::SharedMemory>&&);
    explicit SharedTimebase(Ref<SharedMemory>&&);

    struct Impl;
    UniqueRef<Impl> m_impl;
};

// Reader-side companion to SharedTimebase. Owns its own read-only mapping of
// the shared memory plus the per-reader state needed to turn the writer's raw
// snapshot stream into a forward-monotonic playback time, with rate-based
// extrapolation from the latest snapshot.
//
// Not thread-safe: callers must serialize all method invocations on a given
// instance.
class WEBCORE_EXPORT SharedTimebaseReader final {
    WTF_MAKE_TZONE_ALLOCATED(SharedTimebaseReader);
public:
    // clock is the source of "now" for extrapolation; defaults to
    // MonotonicTime::now. Override for tests that need deterministic timing.
    static std::unique_ptr<SharedTimebaseReader> create(SharedTimebase::Handle&&, Function<MonotonicTime()>&& clock = { });
    SharedTimebaseReader(Ref<SharedMemory>&&, Function<MonotonicTime()>&&);
    ~SharedTimebaseReader();

    MediaTime currentTime() const;
    double currentRate() const;

    void resetForTimeDiscontinuity();

private:
    struct Impl;
    UniqueRef<Impl> m_impl;
    const Function<MonotonicTime()> m_clock;
    mutable std::optional<MediaTime> m_lastReturnedTime;
};

}
