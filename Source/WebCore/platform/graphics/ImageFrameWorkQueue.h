/*
 * Copyright (C) 2024 Apple Inc.  All rights reserved.
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

#include "DecodingOptions.h"
#include "ImageTypes.h"
#include <wtf/SynchronizedFixedQueue.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>

namespace WebCore {

class BitmapImageSource;

class ImageFrameWorkQueue : public ThreadSafeRefCounted<ImageFrameWorkQueue> {
public:
    struct Request {
        unsigned index;
        SubsamplingLevel subsamplingLevel;
        ImageAnimatingState animatingState;
        DecodingOptions options;
        friend bool operator==(const Request&, const Request&) = default;
    };

    static Ref<ImageFrameWorkQueue> create(BitmapImageSource&);

    void start();
    void dispatch(const Request&);
    void stop();

    bool isIdle() const { return m_decodeQueue.isEmpty(); }
    bool isPendingDecodingAtIndex(unsigned index, SubsamplingLevel, const DecodingOptions&) const;

    void setMinimumDecodingDurationForTesting(Seconds duration) { m_minimumDecodingDurationForTesting = duration; }
    void dump(TextStream&) const;

private:
    ImageFrameWorkQueue(BitmapImageSource&);

    Ref<BitmapImageSource> protectedSource() const { return m_source.get().releaseNonNull(); }

    static const int BufferSize = 8;
    using RequestQueue = SynchronizedFixedQueue<Request, BufferSize>;
    using DecodeQueue = Deque<Request, BufferSize>;

    RequestQueue& requestQueue();
    DecodeQueue& decodeQueue() { return m_decodeQueue; }

    Seconds minimumDecodingDurationForTesting() const { return m_minimumDecodingDurationForTesting; }

    ThreadSafeWeakPtr<BitmapImageSource> m_source;

    RefPtr<RequestQueue> m_requestQueue;
    DecodeQueue m_decodeQueue;
    RefPtr<WorkQueue> m_workQueue;

    Seconds m_minimumDecodingDurationForTesting;
};

} // namespace WebCore
