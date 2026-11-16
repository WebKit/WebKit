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

#include "config.h"
#include "ImageFrameWorkQueuePool.h"

#include "BitmapImageSource.h"
#include "ImageDecoder.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

ImageFrameWorkQueuePool& ImageFrameWorkQueuePool::singleton()
{
    static LazyNeverDestroyed<ImageFrameWorkQueuePool> pool;
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag, [] {
        pool.construct();
    });
    return pool;
}

#if ASSERT_ENABLED
bool ImageFrameWorkQueuePool::isValidPool() const
{
    if (m_mapper.size() < m_activeWorkQueues.size())
        return false;

    if (m_reverseMapper.size() != m_activeWorkQueues.size())
        return false;

    if (m_activeWorkQueues.size() + m_idleWorkQueues.size() > activeWorkQueuesMax)
        return false;

    bool isValidMapper = std::ranges::all_of(m_mapper.values(), [&](auto& sourceWorkQueue) {
        return std::ranges::any_of(m_activeWorkQueues, [&](auto& workQueue) {
            return sourceWorkQueue == workQueue;
        });
    });

    bool isValidReverseMapper = std::ranges::all_of(m_reverseMapper, [&](auto& pair) {
        return std::ranges::all_of(pair.value, [&](const auto* source) {
            return m_mapper.get(source) == pair.key.ptr();
        });
    });

    return isValidMapper && isValidReverseMapper;
}
#endif

Ref<ImageFrameWorkQueue> ImageFrameWorkQueuePool::ensureSourceWorkQueue(const BitmapImageSource& source)
{
    RefPtr sourceWorkQueue = m_mapper.get(&source);
    if (sourceWorkQueue)
        return sourceWorkQueue.releaseNonNull();

    // Limit is reached. Reuse the front WorkQueue for this request because it has the lightest load.
    if (m_activeWorkQueues.size() >= activeWorkQueuesMax)
        sourceWorkQueue = const_cast<ImageFrameWorkQueue&>(m_activeWorkQueues.peek().get());
    else {
        if (m_idleWorkQueues.isEmpty())
            sourceWorkQueue = ImageFrameWorkQueue::create();
        else
            sourceWorkQueue = m_idleWorkQueues.takeLast();
        m_activeWorkQueues.enqueue(*sourceWorkQueue);
    }

    // Add the bi-directional mapping source <-> sourceWorkQueue.
    m_mapper.set(&source, *sourceWorkQueue);

    auto& sources = m_reverseMapper.ensure(*sourceWorkQueue, [] -> Vector<const BitmapImageSource*> {
        return { };
    }).iterator->value;

    sources.append(&source);

    return sourceWorkQueue.releaseNonNull();
}

void ImageFrameWorkQueuePool::removeSourceWorkQueueIfNeeded(const BitmapImageSource& source, ImageFrameWorkQueue& sourceWorkQueue)
{
    if (sourceWorkQueue.hasPendingDecodingRequests(source))
        return;

    // Remove the bi-directional mapping source <-> sourceWorkQueue.
    m_mapper.remove(&source);

    auto iterator = m_reverseMapper.find(sourceWorkQueue);
    ASSERT_WITH_SECURITY_IMPLICATION(iterator != m_reverseMapper.end());
    auto& sources = iterator->value;

    sources.removeFirst(&source);
    if (!sources.isEmpty())
        return;

    m_reverseMapper.remove(sourceWorkQueue);

    // Release the platform WorkQueue so the system can reuse its thread.
    sourceWorkQueue.close();

    // Remove the sourceWorkQueue from the PriorityQueue.
    ASSERT(!m_activeWorkQueues.isEmpty());
    ASSERT(m_activeWorkQueues.peek().ptr() == &sourceWorkQueue);
    m_idleWorkQueues.append(m_activeWorkQueues.dequeue());
}

void ImageFrameWorkQueuePool::dispatchRequest(ImageFrameWorkQueue::Request&& request)
{
    Ref sourceWorkQueue = ensureSourceWorkQueue(request.source);

    sourceWorkQueue->dispatchRequest(WTF::move(request));

    // Push sourceWorkQueue towards the rear of the queue; it got one more request to process.
    m_activeWorkQueues.decreaseKey([&](Ref<ImageFrameWorkQueue>& workQueue) {
        return sourceWorkQueue == workQueue;
    });

    ASSERT(isValidPool());
}

void ImageFrameWorkQueuePool::settleRequest(ImageFrameWorkQueue::Request&& request, RefPtr<NativeImage>&& nativeImage)
{
    Ref source = request.source;
    RefPtr sourceWorkQueue = m_mapper.get(source.ptr());
    if (!sourceWorkQueue) {
        // Cancelling decoding doesn't remove the mapping if a request was being decoded.
        ASSERT_NOT_REACHED();
        return;
    }

    // Push sourceWorkQueue towards the front of the queue; it finished one request.
    m_activeWorkQueues.increaseKey([&](Ref<ImageFrameWorkQueue>& workQueue) {
        return sourceWorkQueue.get() == workQueue.ptr();
    });

    removeSourceWorkQueueIfNeeded(source, *sourceWorkQueue);
    ASSERT(isValidPool());

    source->imageFrameDecodeAtIndexHasFinished(request.index, request.subsamplingLevel, request.animatingState, request.options, WTF::move(nativeImage));
}

bool ImageFrameWorkQueuePool::hasPendingDecodingRequest(const BitmapImageSource& source, unsigned index, SubsamplingLevel subsamplingLevel, ImageAnimatingState animatingState, const DecodingOptions& options) const
{
    if (RefPtr sourceWorkQueue = m_mapper.get(&source))
        return sourceWorkQueue->hasPendingDecodingRequest(source, index, subsamplingLevel, animatingState, options);
    return false;
}

bool ImageFrameWorkQueuePool::hasPendingDecodingRequests(const BitmapImageSource& source) const
{
    if (RefPtr sourceWorkQueue = m_mapper.get(&source))
        return sourceWorkQueue->hasPendingDecodingRequests(source);
    return false;
}

Vector<unsigned> ImageFrameWorkQueuePool::cancelPendingDecodingRequests(BitmapImageSource& source)
{
    RefPtr sourceWorkQueue = m_mapper.get(&source);
    if (!sourceWorkQueue)
        return { };

    auto cancelledFrames = sourceWorkQueue->cancelPendingDecodingRequests(source);

    // Push sourceWorkQueue towards the front of the queue; one request or more were cancelled.
    m_activeWorkQueues.increaseKey([&](Ref<ImageFrameWorkQueue>& workQueue) {
        return sourceWorkQueue.get() == workQueue.ptr();
    });

    removeSourceWorkQueueIfNeeded(source, *sourceWorkQueue);
    ASSERT(isValidPool());

    return cancelledFrames;
}

void ImageFrameWorkQueuePool::dump(const BitmapImageSource& source, TextStream& ts) const
{
    if (RefPtr sourceWorkQueue = m_mapper.get(&source))
        sourceWorkQueue->dump(ts);
}

} // namespace WebCore
