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
#include <atomic>
#include <wtf/Deque.h>
#include <wtf/Seconds.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebCore {

class BitmapImageSource;
class ImageDecoder;
class ImageDecodingQueue;

class ImageFrameWorkQueue : public ThreadSafeRefCounted<ImageFrameWorkQueue> {
public:
    struct Request {
        unsigned index;
        SubsamplingLevel subsamplingLevel;
        ImageAnimatingState animatingState;
        DecodingOptions options;
        bool isCompatibleWith(const Request& other) const
        {
            return index == other.index
                && subsamplingLevel == other.subsamplingLevel
                && animatingState == other.animatingState
                && options.isCompatibleWith(other.options);
        }
    };

    static Ref<ImageFrameWorkQueue> create(BitmapImageSource&);
    ~ImageFrameWorkQueue();

    void dispatch(const Request&);
    void stop();

    bool isIdle() const { return m_decodeQueue.isEmpty(); }
    bool isPendingDecodingAtIndex(unsigned index, SubsamplingLevel, const DecodingOptions&) const;

    void setMinimumDecodingDurationForTesting(Seconds duration) { m_minimumDecodingDurationForTesting = duration; }
    void dump(TextStream&) const;

private:
    explicit ImageFrameWorkQueue(BitmapImageSource&);

    static constexpr unsigned s_bufferSize = 8;
    using DecodeQueue = Deque<Request, s_bufferSize>;

    DecodeQueue& decodeQueue() LIFETIME_BOUND { return m_decodeQueue; }

    ThreadSafeWeakRef<BitmapImageSource> m_source;

    RefPtr<ImageDecoder> m_decoder;
    DecodeQueue m_decodeQueue;
    const Ref<ImageDecodingQueue> m_workQueue;
    std::atomic<uint64_t> m_generation { 0 };

    Seconds m_minimumDecodingDurationForTesting;
};

} // namespace WebCore
