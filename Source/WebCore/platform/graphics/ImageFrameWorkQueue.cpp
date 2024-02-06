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

#include "config.h"
#include "ImageFrameWorkQueue.h"

#include "ImageDecoder.h"
#include "ImageSource.h"
#include "Logging.h"
#include <wtf/SystemTracing.h>

namespace WebCore {

Ref<ImageFrameWorkQueue> ImageFrameWorkQueue::create(ImageSource& source)
{
    return adoptRef(*new ImageFrameWorkQueue(source));
}

ImageFrameWorkQueue::ImageFrameWorkQueue(ImageSource& source)
    : m_source(source)
{
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

    ASSERT(isMainThread());
    m_workQueue = WorkQueue::create("org.webkit.ImageDecoder", WorkQueue::QOS::Default);

    m_workQueue->dispatch([protectedThis = Ref { *this }, protectedWorkQueue = Ref { *m_workQueue }, protectedSource = Ref { source() }] () mutable {
        Request request;
        while (protectedThis->requestQueue().dequeue(request)) {
            TraceScope tracingScope(AsyncImageDecodeStart, AsyncImageDecodeEnd);

            auto decoder = protectedSource->m_decoder;
            auto minimumDecodingDuration = protectedThis->minimumDecodingDurationForTesting();

            MonotonicTime startingTime;
            if (minimumDecodingDuration > 0_s)
                startingTime = MonotonicTime::now();

            auto platformImage = decoder->createFrameImageAtIndex(request.index, request.subsamplingLevel, request.options);
            auto nativeImage = NativeImage::create(WTFMove(platformImage));

            // Pretend as if decoding the frame took minimumDecodingDuration.
            if (minimumDecodingDuration > 0_s) {
                auto actualDecodingDuration = MonotonicTime::now() - startingTime;
                if (minimumDecodingDuration > actualDecodingDuration)
                    sleep(minimumDecodingDuration - actualDecodingDuration);
            }

            // Even if we fail to decode the frame, it is important to sync the main thread with this result.
            callOnMainThread([protectedThis, protectedWorkQueue, protectedSource, request, nativeImage = WTFMove(nativeImage)] () mutable {
                // The WorkQueue may have been recreated before the frame was decoded.
                if (protectedWorkQueue.ptr() != protectedThis->m_workQueue || protectedSource.ptr() != &protectedThis->source()) {
                    LOG(Images, "ImageFrameWorkQueue::%s - %p - url: %s. WorkQueue was recreated at index = %d.", __FUNCTION__, protectedThis.ptr(), protectedSource->sourceUTF8(), request.index);
                    return;
                }

                // The DecodeQueue may have been cleared before the frame was decoded.
                if (protectedThis->decodeQueue().isEmpty() || protectedThis->decodeQueue().first() != request) {
                    LOG(Images, "ImageFrameWorkQueue::%s - %p - url: %s. DecodeQueue was cleared at index = %d.", __FUNCTION__, protectedThis.ptr(), protectedSource->sourceUTF8(), request.index);
                    return;
                }

                protectedThis->decodeQueue().removeFirst();
                protectedSource->cacheNativeImageAtIndexAsync(request.index, request.subsamplingLevel, request.options, WTFMove(nativeImage));
            });
        }

        // Ensure destruction happens on creation thread.
        callOnMainThread([protectedThis = WTFMove(protectedThis), protectedWorkQueue = WTFMove(protectedWorkQueue), protectedSource = WTFMove(protectedSource)] () mutable { });
    });
}

void ImageFrameWorkQueue::dispatch(const Request& request)
{
    requestQueue().enqueue(request);
    decodeQueue().append(request);

    start();
}

void ImageFrameWorkQueue::stop()
{
    for (auto& request : m_decodeQueue) {
        LOG(Images, "ImageFrameWorkQueue::%s - %p - url: %s. Decoding was cancelled for frame at index = %d.", __FUNCTION__, this, source().sourceUTF8(), request.index);
        source().clearFrameAtIndex(request.index);
    }

    if (m_requestQueue) {
        m_requestQueue->close();
        m_requestQueue = nullptr;
    }

    m_decodeQueue.clear();
    m_workQueue = nullptr;
}

bool ImageFrameWorkQueue::isPendingDecodingAtIndex(unsigned index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options) const
{
    auto it = std::find_if(m_decodeQueue.begin(), m_decodeQueue.end(), [index, subsamplingLevel, &options](const Request& request) {
        return request.index == index && subsamplingLevel >= request.subsamplingLevel && request.options.isCompatibleWith(options);
    });
    return it != m_decodeQueue.end();
}

void ImageFrameWorkQueue::dump(TextStream& ts) const
{
    if (isIdle())
        return;

    ts.dumpProperty("pending-for-decoding", m_decodeQueue.size());
}

} // namespace WebCore
