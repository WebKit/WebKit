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

#include "config.h"
#include "ImageFrameWorkQueue.h"

#include "BitmapImageSource.h"
#include "ImageDecoder.h"
#include "ImageFrameWorkQueuePool.h"
#include "Logging.h"
#include <wtf/SystemTracing.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<ImageFrameWorkQueue> ImageFrameWorkQueue::create()
{
    return adoptRef(*new ImageFrameWorkQueue());
}

ImageFrameWorkQueue::RequestQueue& ImageFrameWorkQueue::requestQueue()
{
    if (!m_requestQueue)
        m_requestQueue = RequestQueue::create();

    return *m_requestQueue;
}

void ImageFrameWorkQueue::start()
{
    if (m_workQueue)
        return;

    m_workQueue = WorkQueue::create("org.webkit.ImageDecoder"_s, WorkQueue::QOS::Default);

    protect(m_workQueue)->dispatch([protectedThis = Ref { *this }, weakRunLoop = ThreadSafeWeakPtr { RunLoop::currentSingleton() }, protectedWorkQueue = Ref { *m_workQueue }, protectedRequestQueue = Ref { requestQueue() }] mutable {
        while (auto request = protectedRequestQueue->dequeue()) {
            TraceScope tracingScope(AsyncImageDecodeStart, AsyncImageDecodeEnd);

            auto minimumDecodingDuration = request->minimumDecodingDurationForTesting;
            auto startingTime = MonotonicTime::now();

            Ref decoder = request->decoder;
            DecodingDestination decodingDestination = request->options.decodingDestination();

            RefPtr<NativeImage> nativeImage;
            if (auto result = decoder->createNativeImageAtIndex(request->index, request->subsamplingLevel, request->options)) {
                nativeImage = WTF::move(std::get<Ref<NativeImage>>(*result));
                decodingDestination = std::get<DecodingDestination>(*result);
            }

            request->options = { request->options.decodingMode(), decodingDestination, request->options.sizeForDrawing() };

            // Pretend as if decoding the frame took minimumDecodingDuration.
            if (minimumDecodingDuration) {
                auto actualDecodingDuration = MonotonicTime::now() - startingTime;
                if (*minimumDecodingDuration > actualDecodingDuration)
                    sleep(*minimumDecodingDuration - actualDecodingDuration);
            }

            if (RefPtr protectedRunLoop = weakRunLoop.get()) {
                // Even if we fail to decode the frame, it is important to sync the creation thread with this result.
                callOnRunLoop(*protectedRunLoop, [protectedThis, protectedWorkQueue, request = WTF::move(request), nativeImage = WTF::move(nativeImage)] mutable {
                    // The WorkQueue may have been recreated before the frame was decoded.
                    if (protectedWorkQueue.ptr() != protectedThis->m_workQueue) {
                        LOG(Images, "ImageFrameWorkQueue::%s - %p. WorkQueue was recreated at index = %d.", __FUNCTION__, protectedThis.ptr(), request->index);
                        return;
                    }

                    protectedThis->settleRequest(WTF::move(*request), WTF::move(nativeImage));
                });
            }
        }

        if (RefPtr protectedRunLoop = weakRunLoop) {
            // Ensure destruction happens on creation thread.
            callOnRunLoop(*protectedRunLoop, [protectedThis = WTF::move(protectedThis), weakRunLoop = WTF::move(weakRunLoop), protectedWorkQueue = WTF::move(protectedWorkQueue)] mutable { });
        }
    });
}

void ImageFrameWorkQueue::close()
{
    if (!m_decodeQueue.isEmpty())
        return;

    if (m_requestQueue) {
        protect(m_requestQueue)->close();
        m_requestQueue = nullptr;
    }

    m_workQueue = nullptr;
}

void ImageFrameWorkQueue::dispatchRequest(Request&& request)
{
    protect(requestQueue())->enqueue(request);
    decodeQueue().append(WTF::move(request));

    start();
}

void ImageFrameWorkQueue::settleRequest(Request&& request, RefPtr<NativeImage>&& nativeImage)
{
    // The DecodeQueue may have been cleared before the frame was decoded.
    if (decodeQueue().isEmpty() || request != decodeQueue().first()) {
        LOG(Images, "ImageFrameWorkQueue::%s - %p. DecodeQueue was cleared at index = %d.", __FUNCTION__, this, request.index);
        return;
    }

    decodeQueue().removeFirst();
    sharedWorkQueuePool().settleRequest(WTF::move(request), WTF::move(nativeImage));
}

bool ImageFrameWorkQueue::hasPendingDecodingRequest(const BitmapImageSource& source, unsigned index, SubsamplingLevel subsamplingLevel, ImageAnimatingState animatingState, const DecodingOptions& options) const
{
    return m_decodeQueue.containsIf([&](const Request& request) {
        return request.source.ptr() == &source
            && request.index == index
            && request.subsamplingLevel == subsamplingLevel
            && request.animatingState == animatingState
            && request.options.isCompatibleWith(options);
    });
}

bool ImageFrameWorkQueue::hasPendingDecodingRequests(const BitmapImageSource& source) const
{
    return m_decodeQueue.containsIf([&](const Request& request) {
        return request.source.ptr() == &source;
    });
}

Vector<unsigned> ImageFrameWorkQueue::cancelPendingDecodingRequests(BitmapImageSource& source)
{
    Vector<Request> pendingDecodingRequests;

    if (RefPtr requestQueue = m_requestQueue) {
        requestQueue->removeAllMatching([&](const auto& request) {
            if (request.source.ptr() != &source)
                return false;
            pendingDecodingRequests.append(request);
            return true;
        });
    }

    m_decodeQueue.removeAllMatching([&](const auto& request) {
        return pendingDecodingRequests.contains(request);
    });

    if (m_decodeQueue.isEmpty())
        close();

    return WTF::map(pendingDecodingRequests, [&](const auto& request) {
        LOG(Images, "ImageFrameWorkQueue::%s - %p. Decoding was cancelled for frame at index = %d.", __FUNCTION__, this, request.index);
        return request.index;
    });
}

void ImageFrameWorkQueue::dump(TextStream& ts) const
{
    ts.dumpProperty("pending-for-decoding"_s, m_decodeQueue.size());
}

} // namespace WebCore
