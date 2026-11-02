/*
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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
#include <wtf/Seconds.h>
#include <wtf/SynchronizedFixedQueue.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>

namespace WebCore {

class BitmapImageSource;
class ImageDecoder;
class NativeImage;

class ImageFrameWorkQueue : public ThreadSafeRefCounted<ImageFrameWorkQueue> {
public:
    struct Request {
        Ref<BitmapImageSource> source;
        Ref<ImageDecoder> decoder;
        unsigned index;
        SubsamplingLevel subsamplingLevel;
        ImageAnimatingState animatingState;
        DecodingOptions options;
        std::optional<Seconds> minimumDecodingDurationForTesting;
        constexpr bool operator==(const Request& other) const
        {
            return source == other.source
                && index == other.index
                && subsamplingLevel == other.subsamplingLevel
                && animatingState == other.animatingState
                && options == other.options
                && minimumDecodingDurationForTesting == other.minimumDecodingDurationForTesting;
        }
    };

    NODELETE static Ref<ImageFrameWorkQueue> create();

    void start();
    void close();

    void dispatchRequest(Request&&);
    void settleRequest(Request&&, RefPtr<NativeImage>&&);

    bool hasPendingDecodingRequest(const BitmapImageSource&, unsigned index, SubsamplingLevel, ImageAnimatingState, const DecodingOptions&) const;
    bool hasPendingDecodingRequests(const BitmapImageSource&) const;
    bool hasPendingDecodingRequests() const { return pendingDecodingRequests(); }

    size_t pendingDecodingRequests() const { return m_decodeQueue.size(); }
    Vector<unsigned> cancelPendingDecodingRequests(BitmapImageSource&);

    void dump(TextStream&) const;

private:
    ImageFrameWorkQueue() = default;

    static const int BufferSize = 32;
    using RequestQueue = SynchronizedFixedQueue<Request, BufferSize>;
    using DecodeQueue = Deque<Request, BufferSize>;

    RequestQueue& requestQueue();
    DecodeQueue& decodeQueue() LIFETIME_BOUND { return m_decodeQueue; }

    RefPtr<RequestQueue> m_requestQueue;
    DecodeQueue m_decodeQueue;
    RefPtr<WorkQueue> m_workQueue;
};

} // namespace WebCore
