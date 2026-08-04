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

#include <WebCore/DecodingOptions.h>
#include <WebCore/ImageDecoder.h>
#include <WebCore/ImageTypes.h>
#include <wtf/SynchronizedFixedQueue.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>

namespace WebCore {

class ImageDecoderClient;

class AsyncImageDecoder : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AsyncImageDecoder> {
public:
    static RefPtr<AsyncImageDecoder> create(FragmentedSharedBuffer&, const String& mimeType, AlphaOption, GammaAndColorProfileOption, ImageDecoderClient&);

    WEBCORE_EXPORT static Ref<AsyncImageDecoder> create(Ref<ImageDecoder>&&, ImageDecoderClient&);

    void requestNativeImageAtIndex(unsigned index, SubsamplingLevel = SubsamplingLevel::Default, ImageAnimatingState = ImageAnimatingState::No, const DecodingOptions& = { DecodingMode::Asynchronous });

    bool isWorkQueueIdle() const { return m_decodeQueue.isEmpty(); }
    bool isPendingDecodingAtIndex(unsigned index, SubsamplingLevel, const DecodingOptions&) const;

    WEBCORE_EXPORT void stopWorkQueue();

    size_t bytesDecodedToDetermineProperties() const;
    EncodedDataStatus encodedDataStatus() const;
    void setEncodedDataStatusChangeCallback(Function<void(EncodedDataStatus)>&&);

    size_t frameCount() const;
    bool hasHDRGainMap() const;
    IntSize size() const;
    size_t primaryFrameIndex() const;
    RepetitionCount repetitionCount() const;
    String uti() const;
    String filenameExtension() const;
    String accessibilityDescription() const;
    std::optional<IntPoint> hotSpot() const;

#if ENABLE(QUICKLOOK_FULLSCREEN)
    bool isPanorama() const;
#endif

#if ENABLE(SPATIAL_IMAGE_DETECTION)
    bool isSpatial() const;
    std::optional<unsigned> spatialLeftEyeFrameIndex() const;
    std::optional<unsigned> spatialRightEyeFrameIndex() const;
    std::optional<SpatialImageEyeProperties> spatialEyePropertiesAtIndex(unsigned) const;
#endif

#if ENABLE(SPATIAL_IMAGE_CONTROLS)
    bool isMaybePanoramic() const;
#endif

    Seconds frameDurationAtIndex(size_t) const;
    bool frameHasAlphaAtIndex(size_t) const;

    bool fetchFrameMetaDataAtIndex(size_t, SubsamplingLevel, const DecodingOptions&, ImageFrame&) const;

    std::optional<std::tuple<Ref<NativeImage>, DecodingDestination>> createNativeImageAtIndex(size_t index, SubsamplingLevel, const DecodingOptions&);

    void setExpectedContentSize(long long);
    void setData(const FragmentedSharedBuffer&, bool allDataReceived);
    bool isAllDataReceived() const;
    void clearFrameBufferCache(size_t);

    void setMinimumDecodingDurationForTesting(Seconds duration) { m_minimumDecodingDurationForTesting = duration; }
    Seconds minimumDecodingDurationForTesting() const { return m_minimumDecodingDurationForTesting; }

    void dump(TextStream&) const;

protected:
    WEBCORE_EXPORT AsyncImageDecoder(Ref<ImageDecoder>&&, ImageDecoderClient&);

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

    static const int BufferSize = 8;
    using RequestQueue = SynchronizedFixedQueue<Request, BufferSize>;
    using DecodeQueue = Deque<Request, BufferSize>;

    RequestQueue& requestQueue();
    DecodeQueue& decodeQueue() LIFETIME_BOUND { return m_decodeQueue; }

    void startWorkQueue();
    void dispatchRequest(const Request&);

    Ref<ImageDecoder> m_decoder;
    ThreadSafeWeakPtr<ImageDecoderClient> m_client;

    RefPtr<RequestQueue> m_requestQueue;
    DecodeQueue m_decodeQueue;
    RefPtr<WorkQueue> m_workQueue;

    Seconds m_minimumDecodingDurationForTesting;
};

} // namespace WebCore
