/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ImageDecoder.h"

#include "ImageFrame.h"
#include "ScalableImageDecoder.h"
#include <wtf/TZoneMallocInlines.h>

#if USE(CG)
#include "ImageDecoderCG.h"
#endif

#if HAVE(AVASSETREADER)
#include "ImageDecoderFactoryAVF.h"
#endif

#if USE(GSTREAMER) && ENABLE(VIDEO)
#include "ImageDecoderGStreamer.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ImageDecoder);

RefPtr<ImageDecoder> ImageDecoder::create(FragmentedSharedBuffer& data, const String& mimeType, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
{
#if HAVE(AVASSETREADER)
    if (!ImageDecoderCG::canDecodeType(mimeType)) {
        if (RefPtr imageDecoder = ImageDecoderFactoryAVF::singleton().createImageDecoder(data, mimeType, alphaOption, gammaAndColorProfileOption))
            return imageDecoder;
    }
#endif

#if USE(GSTREAMER) && ENABLE(VIDEO)
    if (ImageDecoderGStreamer::canDecodeType(mimeType))
        return ImageDecoderGStreamer::create(data, mimeType, alphaOption, gammaAndColorProfileOption);
#endif

    // ScalableImageDecoder is used on CG ports for some specific image formats which the platform doesn't support directly.
    if (RefPtr imageDecoder = ScalableImageDecoder::create(data, alphaOption, gammaAndColorProfileOption))
        return imageDecoder;

#if USE(CG)
    if (RefPtr imageDecoder = ImageDecoderCG::create(data, alphaOption, gammaAndColorProfileOption))
        return imageDecoder;
#endif

    return nullptr;
}

ImageDecoder::ImageDecoder() = default;

ImageDecoder::~ImageDecoder() = default;

bool ImageDecoder::supportsMediaType(MediaType type)
{
#if HAVE(AVASSETREADER)
    if (ImageDecoderFactoryAVF::singleton().supportsMediaType(type))
        return true;
#endif

#if USE(GSTREAMER) && ENABLE(VIDEO)
    if (ImageDecoderGStreamer::supportsMediaType(type))
        return true;
#endif

    // ScalableImageDecoder is used on CG ports for some specific image formats which the platform doesn't support directly.
    if (ScalableImageDecoder::supportsMediaType(type))
        return true;

#if USE(CG)
    if (ImageDecoderCG::supportsMediaType(type))
        return true;
#endif

    return false;
}

bool ImageDecoder::fetchFrameMetaDataAtIndex(size_t index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options, ImageFrame& frame) const
{
    if (options.hasSizeForDrawing()) {
        ASSERT(frame.hasNativeImage(options.shouldDecodeToHDR()));
        frame.m_size = frame.nativeImage(options.shouldDecodeToHDR())->size();
    } else
        frame.m_size = frameSizeAtIndex(index, subsamplingLevel);

    frame.m_densityCorrectedSize = frameDensityCorrectedSizeAtIndex(index);
    frame.m_subsamplingLevel = subsamplingLevel;
    frame.m_hasAlpha = frameHasAlphaAtIndex(index);
    frame.m_orientation = frameOrientationAtIndex(index);
    frame.m_decodingStatus = frameIsCompleteAtIndex(index) ? DecodingStatus::Complete : DecodingStatus::Partial;
    return true;
}

} // namespace WebCore
