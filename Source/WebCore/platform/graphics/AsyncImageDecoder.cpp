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
#include "AsyncImageDecoder.h"

#include "ImageDecoder.h"
#include "ImageDecoderClient.h"
#include "Logging.h"
#include "NativeImage.h"
#include <wtf/SystemTracing.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

RefPtr<AsyncImageDecoder> AsyncImageDecoder::create(FragmentedSharedBuffer& data, const String& mimeType, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption, ImageDecoderClient& client)
{
    if (RefPtr decoder = ImageDecoder::create(data, mimeType, alphaOption, gammaAndColorProfileOption))
        return create(decoder.releaseNonNull(), client);
    return nullptr;
}

Ref<AsyncImageDecoder> AsyncImageDecoder::create(Ref<ImageDecoder>&& decoder, ImageDecoderClient& client)
{
    return adoptRef(*new AsyncImageDecoder(WTF::move(decoder), client));
}

AsyncImageDecoder::AsyncImageDecoder(Ref<ImageDecoder>&& decoder, ImageDecoderClient& client)
    : m_decoder(WTF::move(decoder))
    , m_client(client)
{
}

AsyncImageDecoder::RequestQueue& AsyncImageDecoder::requestQueue()
{
    if (!m_requestQueue)
        m_requestQueue = RequestQueue::create();

    return *m_requestQueue;
}

void AsyncImageDecoder::startWorkQueue()
{
    if (m_workQueue)
        return;

    m_workQueue = WorkQueue::create("org.webkit.ImageDecoder"_s, WorkQueue::QOS::Default);

    protect(m_workQueue)->dispatch([protectedThis = Ref { *this }, weakRunLoop = ThreadSafeWeakPtr { RunLoop::currentSingleton() }, protectedWorkQueue = Ref { *m_workQueue }, protectedClient = m_client.get(), protectedRequestQueue = Ref { requestQueue() }] () mutable {
        Request request;
        while (protectedRequestQueue->dequeue(request)) {
            TraceScope tracingScope(AsyncImageDecodeStart, AsyncImageDecodeEnd);

            auto minimumDecodingDuration = protectedThis->minimumDecodingDurationForTesting();

            MonotonicTime startingTime;
            if (minimumDecodingDuration > 0_s)
                startingTime = MonotonicTime::now();

            RefPtr<NativeImage> nativeImage;
            DecodingDestination decodingDestination = request.options.decodingDestination();

            if (auto result = protectedThis->createNativeImageAtIndex(request.index, request.subsamplingLevel, request.options)) {
                nativeImage = WTF::move(std::get<Ref<NativeImage>>(*result));
                decodingDestination = std::get<DecodingDestination>(*result);
            }

            request.options = { request.options.decodingMode(), decodingDestination, request.options.sizeForDrawing() };

            // Pretend as if decoding the frame took minimumDecodingDuration.
            if (minimumDecodingDuration > 0_s) {
                auto actualDecodingDuration = MonotonicTime::now() - startingTime;
                if (minimumDecodingDuration > actualDecodingDuration)
                    sleep(minimumDecodingDuration - actualDecodingDuration);
            }

            if (RefPtr protectedRunLoop = weakRunLoop.get()) {
                // Even if we fail to decode the frame, it is important to sync the creation thread with this result.
                callOnRunLoop(*protectedRunLoop, [protectedThis, protectedWorkQueue, protectedClient, request, nativeImage = WTF::move(nativeImage)] () mutable {
                    // The WorkQueue may have been recreated before the frame was decoded.
                    if (protectedWorkQueue.ptr() != protectedThis->m_workQueue || protectedClient.get() != protectedThis->m_client.get()) {
                        LOG(Images, "AsyncImageDecoder::%s - %p. WorkQueue was recreated at index = %d.", __FUNCTION__, protectedThis.ptr(), request.index);
                        return;
                    }

                    // The DecodeQueue may have been cleared before the frame was decoded.
                    if (protectedThis->decodeQueue().isEmpty() || !request.isCompatibleWith(protectedThis->decodeQueue().first())) {
                        LOG(Images, "AsyncImageDecoder::%s - %p. DecodeQueue was cleared at index = %d.", __FUNCTION__, protectedThis.ptr(), request.index);
                        return;
                    }

                    protectedThis->decodeQueue().removeFirst();
                    protectedClient->imageFrameDecodeAtIndexHasFinished(request.index, request.subsamplingLevel, request.animatingState, request.options, WTF::move(nativeImage));
                });
            }
        }

        if (RefPtr protectedRunLoop = weakRunLoop.get()) {
            // Ensure destruction happens on creation thread.
            callOnRunLoop(*protectedRunLoop, [protectedThis = WTF::move(protectedThis), weakRunLoop = WTF::move(weakRunLoop), protectedWorkQueue = WTF::move(protectedWorkQueue), protectedClient = WTF::move(protectedClient)] () mutable { });
        }
    });
}

void AsyncImageDecoder::dispatchRequest(const Request& request)
{
    protect(requestQueue())->enqueue(request);
    decodeQueue().append(request);

    startWorkQueue();
}

void AsyncImageDecoder::stopWorkQueue()
{
    RefPtr client = m_client.get();

    for (auto& request : m_decodeQueue) {
        LOG(Images, "AsyncImageDecoder::%s - %p. Decoding was cancelled for frame at index = %d.", __FUNCTION__, this, request.index);
        client->destroyNativeImageAtIndex(request.index);
    }

    if (m_requestQueue) {
        protect(m_requestQueue)->close();
        m_requestQueue = nullptr;
    }

    m_decodeQueue.clear();
    m_workQueue = nullptr;
}

void AsyncImageDecoder::requestNativeImageAtIndex(unsigned index, SubsamplingLevel subsamplingLevel, ImageAnimatingState animatingState, const DecodingOptions& options)
{
    ASSERT(m_client.get());

    dispatchRequest({ index, subsamplingLevel, animatingState, options });
}

bool AsyncImageDecoder::isPendingDecodingAtIndex(unsigned index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options) const
{
    if (!m_workQueue)
        return false;

    ASSERT(m_client.get());

    auto it = std::find_if(m_decodeQueue.begin(), m_decodeQueue.end(), [index, subsamplingLevel, &options](const Request& request) {
        return request.index == index && subsamplingLevel >= request.subsamplingLevel && request.options.isCompatibleWith(options);
    });
    return it != m_decodeQueue.end();
}

size_t AsyncImageDecoder::bytesDecodedToDetermineProperties() const
{
    return protect(m_decoder)->bytesDecodedToDetermineProperties();
}

EncodedDataStatus AsyncImageDecoder::encodedDataStatus() const
{
    return protect(m_decoder)->encodedDataStatus();
}

void AsyncImageDecoder::setEncodedDataStatusChangeCallback(Function<void(EncodedDataStatus)>&& callback)
{
    protect(m_decoder)->setEncodedDataStatusChangeCallback(WTF::move(callback));
}

size_t AsyncImageDecoder::frameCount() const
{
    return protect(m_decoder)->frameCount();
}

bool AsyncImageDecoder::hasHDRGainMap() const
{
    return protect(m_decoder)->hasHDRGainMap();
}

IntSize AsyncImageDecoder::size() const
{
    return protect(m_decoder)->size();
}

size_t AsyncImageDecoder::primaryFrameIndex() const
{
    return protect(m_decoder)->primaryFrameIndex();
}

RepetitionCount AsyncImageDecoder::repetitionCount() const
{
    return protect(m_decoder)->repetitionCount();
}

String AsyncImageDecoder::uti() const
{
    return protect(m_decoder)->uti();
}

String AsyncImageDecoder::filenameExtension() const
{
    return protect(m_decoder)->filenameExtension();
}

String AsyncImageDecoder::accessibilityDescription() const
{
    return protect(m_decoder)->accessibilityDescription();
}

std::optional<IntPoint> AsyncImageDecoder::hotSpot() const
{
    return protect(m_decoder)->hotSpot();
}

#if ENABLE(QUICKLOOK_FULLSCREEN)
bool AsyncImageDecoder::isPanorama() const
{
    return protect(m_decoder)->isPanorama();
}
#endif

#if ENABLE(SPATIAL_IMAGE_DETECTION)
bool AsyncImageDecoder::isSpatial() const
{
    return protect(m_decoder)->isSpatial();
}

std::optional<unsigned> AsyncImageDecoder::spatialLeftEyeFrameIndex() const
{
    return protect(m_decoder)->spatialLeftEyeFrameIndex();
}

std::optional<unsigned> AsyncImageDecoder::spatialRightEyeFrameIndex() const
{
    return protect(m_decoder)->spatialRightEyeFrameIndex();
}

std::optional<SpatialImageEyeProperties> AsyncImageDecoder::spatialEyePropertiesAtIndex(unsigned index) const
{
    return protect(m_decoder)->spatialEyePropertiesAtIndex(index);
}
#endif

#if ENABLE(SPATIAL_IMAGE_CONTROLS)
bool AsyncImageDecoder::isMaybePanoramic() const
{
    return protect(m_decoder)->isMaybePanoramic();
}
#endif

Seconds AsyncImageDecoder::frameDurationAtIndex(size_t index) const
{
    return protect(m_decoder)->frameDurationAtIndex(index);
}

bool AsyncImageDecoder::frameHasAlphaAtIndex(size_t index) const
{
    return protect(m_decoder)->frameHasAlphaAtIndex(index);
}

bool AsyncImageDecoder::fetchFrameMetaDataAtIndex(size_t index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options, ImageFrame& frame) const
{
    return protect(m_decoder)->fetchFrameMetaDataAtIndex(index, subsamplingLevel, options, frame);
}

std::optional<std::tuple<Ref<NativeImage>, DecodingDestination>> AsyncImageDecoder::createNativeImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options)
{
    return protect(m_decoder)->createNativeImageAtIndex(index, subsamplingLevel, options);
}

void AsyncImageDecoder::setExpectedContentSize(long long expectedContentSize)
{
    protect(m_decoder)->setExpectedContentSize(expectedContentSize);
}

void AsyncImageDecoder::setData(const FragmentedSharedBuffer& data, bool allDataReceived)
{
    protect(m_decoder)->setData(data, allDataReceived);
}

bool AsyncImageDecoder::isAllDataReceived() const
{
    return protect(m_decoder)->isAllDataReceived();
}

void AsyncImageDecoder::clearFrameBufferCache(size_t index)
{
    protect(m_decoder)->clearFrameBufferCache(index);
}

void AsyncImageDecoder::dump(TextStream& ts) const
{
    if (isWorkQueueIdle())
        return;

    ts.dumpProperty("pending-for-decoding"_s, m_decodeQueue.size());
}

} // namespace WebCore
