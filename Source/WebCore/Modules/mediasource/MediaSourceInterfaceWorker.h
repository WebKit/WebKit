/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_SOURCE_IN_WORKERS)

#include "MediaSourceInterfaceProxy.h"

namespace WebCore {

class MediaSourceHandle;

class MediaSourceInterfaceWorker : public MediaSourceInterfaceProxy {
public:
    static Ref<MediaSourceInterfaceWorker> create(Ref<MediaSourceHandle>&& handle) { return adoptRef(*new MediaSourceInterfaceWorker(WTFMove(handle))); }

private:
    RefPtr<MediaSourcePrivateClient> client() const final;
    void monitorSourceBuffers() final;
    bool isClosed() const final;
    MediaTime duration() const final;
    PlatformTimeRanges buffered() const final;
    Ref<TimeRanges> seekable() const final;
    bool isStreamingContent() const final;
    bool attachToElement(WeakPtr<HTMLMediaElement>&&) final;
    void detachFromElement() final;
    void elementIsShuttingDown() final;
    void openIfDeferredOpen() final;
    bool isManaged() const final;
    void setAsSrcObject(bool) final;
    void memoryPressure() final;
    bool detachable() const final;

    explicit MediaSourceInterfaceWorker(Ref<MediaSourceHandle>&&);

    Ref<MediaSourceHandle> m_handle;
    RefPtr<MediaSourcePrivateClient> m_client;
};

} // namespace WebCore

#endif // MEDIA_SOURCE_IN_WORKERS
