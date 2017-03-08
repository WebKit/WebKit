/*
 * Copyright (C) 2006, 2010, 2011, 2012, 2014, 2016 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp.toker@collabora.co.uk>
 * Copyright (C) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc
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
#include "ImageSource.h"

#if USE(CG)
#include "ImageDecoderCG.h"
#elif USE(DIRECT2D)
#include "GraphicsContext.h"
#include "ImageDecoderDirect2D.h"
#include <WinCodec.h>
#else
#include "ImageDecoder.h"
#endif

#include "ImageOrientation.h"

#include <wtf/CurrentTime.h>

namespace WebCore {

ImageSource::ImageSource(NativeImagePtr&& nativeImage)
    : m_frameCache(ImageFrameCache::create(WTFMove(nativeImage)))
{
}
    
ImageSource::ImageSource(Image* image, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : m_frameCache(ImageFrameCache::create(image))
    , m_alphaOption(alphaOption)
    , m_gammaAndColorProfileOption(gammaAndColorProfileOption)
{
}

ImageSource::~ImageSource()
{
}

void ImageSource::clearFrameBufferCache(size_t clearBeforeFrame)
{
    if (!isDecoderAvailable())
        return;
    m_decoder->clearFrameBufferCache(clearBeforeFrame);
}

void ImageSource::clear(SharedBuffer* data)
{
    m_decoder = nullptr;
    m_frameCache->setDecoder(nullptr);
    setData(data, isAllDataReceived());
}

bool ImageSource::ensureDecoderAvailable(SharedBuffer* data)
{
    if (!data || isDecoderAvailable())
        return true;

    m_decoder = ImageDecoder::create(*data, m_alphaOption, m_gammaAndColorProfileOption);
    if (!isDecoderAvailable())
        return false;

    m_frameCache->setDecoder(m_decoder.get());
    return true;
}

void ImageSource::setDecoderTargetContext(const GraphicsContext* targetContext)
{
#if USE(DIRECT2D)
    if (!isDecoderAvailable())
        return;

    if (targetContext)
        m_decoder->setTargetContext(targetContext->platformContext());
#else
    UNUSED_PARAM(targetContext);
#endif
}

void ImageSource::setData(SharedBuffer* data, bool allDataReceived)
{
    if (!data || !ensureDecoderAvailable(data))
        return;

    m_decoder->setData(*data, allDataReceived);
}

bool ImageSource::dataChanged(SharedBuffer* data, bool allDataReceived)
{
    m_frameCache->destroyIncompleteDecodedData();

#if PLATFORM(IOS)
    // FIXME: We should expose a setting to enable/disable progressive loading and make this
    // code conditional on it. Then we can remove the PLATFORM(IOS)-guard.
    static const double chunkLoadIntervals[] = {0, 1, 3, 6, 15};
    double interval = chunkLoadIntervals[std::min(m_progressiveLoadChunkCount, static_cast<uint16_t>(4))];

    bool needsUpdate = false;

    // The first time through, the chunk time will be 0 and the image will get an update.
    if (currentTime() - m_progressiveLoadChunkTime > interval) {
        needsUpdate = true;
        m_progressiveLoadChunkTime = currentTime();
        ASSERT(m_progressiveLoadChunkCount <= std::numeric_limits<uint16_t>::max());
        ++m_progressiveLoadChunkCount;
    }

    if (needsUpdate || allDataReceived)
        setData(data, allDataReceived);
#else
    setData(data, allDataReceived);
#endif

    m_frameCache->clearMetadata();
    if (!isSizeAvailable())
        return false;

    m_frameCache->growFrames();
    return true;
}

bool ImageSource::isAllDataReceived()
{
    return isDecoderAvailable() ? m_decoder->isAllDataReceived() : m_frameCache->frameCount();
}

bool ImageSource::isAsyncDecodingRequired()
{
    // FIXME: figure out the best heuristic for enabling async image decoding.
    return size().area() * sizeof(RGBA32) >= 100 * KB;
}

SubsamplingLevel ImageSource::maximumSubsamplingLevel()
{
    if (m_maximumSubsamplingLevel)
        return m_maximumSubsamplingLevel.value();

    if (!isDecoderAvailable() || !m_decoder->frameAllowSubsamplingAtIndex(0))
        return SubsamplingLevel::Default;

    // FIXME: this value was chosen to be appropriate for iOS since the image
    // subsampling is only enabled by default on iOS. Choose a different value
    // if image subsampling is enabled on other platform.
    const int maximumImageAreaBeforeSubsampling = 5 * 1024 * 1024;
    SubsamplingLevel level = SubsamplingLevel::First;

    for (; level < SubsamplingLevel::Last; ++level) {
        if (frameSizeAtIndex(0, level).area().unsafeGet() < maximumImageAreaBeforeSubsampling)
            break;
    }

    m_maximumSubsamplingLevel = level;
    return m_maximumSubsamplingLevel.value();
}

SubsamplingLevel ImageSource::subsamplingLevelForScale(float scale)
{
    if (!(scale > 0 && scale <= 1))
        return SubsamplingLevel::Default;

    int result = std::ceil(std::log2(1 / scale));
    return static_cast<SubsamplingLevel>(std::min(result, static_cast<int>(maximumSubsamplingLevel())));
}

NativeImagePtr ImageSource::createFrameImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    return isDecoderAvailable() ? m_decoder->createFrameImageAtIndex(index, subsamplingLevel) : nullptr;
}

NativeImagePtr ImageSource::frameImageAtIndex(size_t index, const std::optional<SubsamplingLevel>& subsamplingLevel, const std::optional<IntSize>& sizeForDrawing, const GraphicsContext* targetContext)
{
    setDecoderTargetContext(targetContext);
    return m_frameCache->frameImageAtIndex(index, subsamplingLevel, sizeForDrawing);
}

void ImageSource::dump(TextStream& ts)
{
    ts.dumpProperty("type", filenameExtension());
    ts.dumpProperty("frame-count", frameCount());
    ts.dumpProperty("repetitions", repetitionCount());
    ts.dumpProperty("solid-color", singlePixelSolidColor());

    ImageOrientation orientation = frameOrientationAtIndex(0);
    if (orientation != OriginTopLeft)
        ts.dumpProperty("orientation", orientation);
}

}
