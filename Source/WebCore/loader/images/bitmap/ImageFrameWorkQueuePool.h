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

#include "ImageFrameWorkQueue.h"
#include <wtf/HashMap.h>
#include <wtf/PriorityQueue.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

class BitmapImageSource;
class NativeImage;

class ImageFrameWorkQueuePool {
public:
    static ImageFrameWorkQueuePool& singleton();

    ImageFrameWorkQueuePool() = default;

    void dispatchRequest(ImageFrameWorkQueue::Request&&);
    void settleRequest(ImageFrameWorkQueue::Request&&, RefPtr<NativeImage>&&);

    bool hasPendingDecodingRequest(const BitmapImageSource&, unsigned index, SubsamplingLevel, ImageAnimatingState, const DecodingOptions&) const;
    bool hasPendingDecodingRequests(const BitmapImageSource&) const;

    Vector<unsigned> cancelPendingDecodingRequests(BitmapImageSource&);

    void dump(const BitmapImageSource&, TextStream&) const;

private:
    static constexpr unsigned activeWorkQueuesMax = 16;

#if ASSERT_ENABLED
    bool isValidPool() const;
#endif

    Ref<ImageFrameWorkQueue> ensureSourceWorkQueue(const BitmapImageSource&);
    void removeSourceWorkQueueIfNeeded(const BitmapImageSource&, ImageFrameWorkQueue&);

    struct HasGreaterPendingRequests {
        bool operator()(const Ref<ImageFrameWorkQueue>& lhs, const Ref<ImageFrameWorkQueue>& rhs) const
        {
            return lhs->pendingDecodingRequests() > rhs->pendingDecodingRequests();
        }
    };
    PriorityQueue<Ref<ImageFrameWorkQueue>, HasGreaterPendingRequests> m_activeWorkQueues;
    Vector<Ref<ImageFrameWorkQueue>> m_idleWorkQueues;

    HashMap<const BitmapImageSource*, Ref<ImageFrameWorkQueue>> m_mapper;
    HashMap<Ref<ImageFrameWorkQueue>, Vector<const BitmapImageSource*>> m_reverseMapper;
};

inline ImageFrameWorkQueuePool& sharedWorkQueuePool()
{
    return ImageFrameWorkQueuePool::singleton();
}

} // namespace WebCore
