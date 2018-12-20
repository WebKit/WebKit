/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#if USE(DIRECT2D)

#include "COMPtr.h"
#include "ImageDecoder.h"
#include <wtf/Optional.h>

interface ID2D1RenderTarget;
interface IWICBitmapDecoder;
interface IWICImagingFactory;

namespace WebCore {

class ImageDecoderDirect2D final : public ImageDecoder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ImageDecoderDirect2D();

    static Ref<ImageDecoderDirect2D> create(SharedBuffer&, AlphaOption, GammaAndColorProfileOption)
    {
        return adoptRef(*new ImageDecoderDirect2D());
    }

    static bool supportsMediaType(MediaType type) { return type == MediaType::Image; }

    size_t bytesDecodedToDetermineProperties() const final;

    String filenameExtension() const final;
    EncodedDataStatus encodedDataStatus() const final;
    bool isSizeAvailable() const final;

    // Always original size, without subsampling.
    IntSize size() const final;
    size_t frameCount() const final;

    RepetitionCount repetitionCount() const final;
    Optional<IntPoint> hotSpot() const final;

    IntSize frameSizeAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default) const final;
    bool frameIsCompleteAtIndex(size_t) const final;
    ImageOrientation frameOrientationAtIndex(size_t) const final;

    Seconds frameDurationAtIndex(size_t) const final;
    bool frameHasAlphaAtIndex(size_t) const final;
    bool frameAllowSubsamplingAtIndex(size_t) const final;
    unsigned frameBytesAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default) const final;

    NativeImagePtr createFrameImageAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default, const DecodingOptions& = DecodingOptions(DecodingMode::Synchronous)) final;

    void setData(SharedBuffer&, bool allDataReceived) final;
    bool isAllDataReceived() const final { return m_isAllDataReceived; }
    void clearFrameBufferCache(size_t) final { }

    void setTargetContext(ID2D1RenderTarget*);

    static IWICImagingFactory* systemImagingFactory();

protected:
    bool m_isAllDataReceived { false };
    mutable IntSize m_size;
    COMPtr<IWICBitmapDecoder> m_nativeDecoder;
    COMPtr<ID2D1RenderTarget> m_renderTarget;
};

}

#endif
