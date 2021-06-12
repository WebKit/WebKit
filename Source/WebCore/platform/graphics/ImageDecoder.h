/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#include "DecodingOptions.h"
#include "ImageOrientation.h"
#include "ImageTypes.h"
#include "IntPoint.h"
#include "IntSize.h"
#include "PlatformImage.h"
#include <wtf/Seconds.h>
#include <wtf/text/WTFString.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class SharedBuffer;

class ImageDecoder : public ThreadSafeRefCounted<ImageDecoder> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static RefPtr<ImageDecoder> create(SharedBuffer&, const String& mimeType, AlphaOption, GammaAndColorProfileOption);
    virtual ~ImageDecoder() = default;

    enum class MediaType {
        Image,
        Video,
    };

    struct FrameMetadata {
        ImageOrientation orientation;
        std::optional<IntSize> densityCorrectedSize;
    };

    struct FrameInfo {
        bool hasAlpha;
        Seconds duration;

        template<class Encoder>
        void encode(Encoder& encoder) const
        {
            encoder << hasAlpha;
            encoder << duration;
        }

        template<class Decoder>
        static std::optional<FrameInfo> decode(Decoder& decoder)
        {
            std::optional<bool> hasAlpha;
            decoder >> hasAlpha;
            if (!hasAlpha)
                return std::nullopt;

            std::optional<Seconds> duration;
            decoder >> duration;
            if (!duration)
                return std::nullopt;

            return {{
                *hasAlpha,
                *duration
            }};
        }
    };

    static bool supportsMediaType(MediaType);

#if ENABLE(GPU_PROCESS)
    using SupportsMediaTypeFunc = WTF::Function<bool(MediaType)>;
    using CanDecodeTypeFunc = WTF::Function<bool(const String&)>;
    using CreateImageDecoderFunc = WTF::Function<RefPtr<ImageDecoder>(SharedBuffer&, const String&, AlphaOption, GammaAndColorProfileOption)>;

    struct ImageDecoderFactory {
        SupportsMediaTypeFunc supportsMediaType;
        CanDecodeTypeFunc canDecodeType;
        CreateImageDecoderFunc createImageDecoder;
    };

    WEBCORE_EXPORT static void installFactory(ImageDecoderFactory&&);
    WEBCORE_EXPORT static void resetFactories();
    WEBCORE_EXPORT static void clearFactories();
#endif

    virtual size_t bytesDecodedToDetermineProperties() const = 0;

    virtual EncodedDataStatus encodedDataStatus() const = 0;
    virtual void setEncodedDataStatusChangeCallback(WTF::Function<void(EncodedDataStatus)>&&) { }
    virtual bool isSizeAvailable() const { return encodedDataStatus() >= EncodedDataStatus::SizeAvailable; }
    virtual IntSize size() const = 0;
    virtual size_t frameCount() const = 0;
    virtual RepetitionCount repetitionCount() const = 0;
    virtual String uti() const { return emptyString(); }
    virtual String filenameExtension() const = 0;
    virtual String accessibilityDescription() const { return emptyString(); };
    virtual std::optional<IntPoint> hotSpot() const = 0;

    virtual IntSize frameSizeAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default) const = 0;
    virtual bool frameIsCompleteAtIndex(size_t) const = 0;
    virtual FrameMetadata frameMetadataAtIndex(size_t) const = 0;

    virtual Seconds frameDurationAtIndex(size_t) const = 0;
    virtual bool frameHasAlphaAtIndex(size_t) const = 0;
    virtual bool frameAllowSubsamplingAtIndex(size_t) const = 0;
    virtual unsigned frameBytesAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default) const = 0;

    virtual PlatformImagePtr createFrameImageAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default, const DecodingOptions& = DecodingOptions(DecodingMode::Synchronous)) = 0;

    virtual void setExpectedContentSize(long long) { }
    virtual void setData(SharedBuffer&, bool allDataReceived) = 0;
    virtual bool isAllDataReceived() const = 0;
    virtual void clearFrameBufferCache(size_t) = 0;

#if USE(DIRECT2D)
    virtual void setTargetContext(ID2D1RenderTarget*) = 0;
#endif

protected:
    ImageDecoder() = default;
};

}
