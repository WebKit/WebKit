/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JPEGXLImageDecoder.h"

#if USE(JPEGXL)

#include "PixelBufferConversion.h"

#if USE(LCMS)
#include "PlatformDisplay.h"
#endif

#if PLATFORM(COCOA)
#include <wtf/darwin/WeakLinking.h>

WTF_WEAK_LINK_FORCE_IMPORT(JxlDecoderCreate);
#endif

namespace WebCore {

static const JxlPixelFormat s_pixelFormat { 4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };

#if USE(LCMS) || USE(CG)
static constexpr int s_eventsWanted = JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE | JXL_DEC_COLOR_ENCODING;
#else
static constexpr int s_eventsWanted = JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE;
#endif

RefPtr<ScalableImageDecoder> JPEGXLImageDecoder::create(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
{
#if PLATFORM(COCOA)
    if (!&JxlDecoderCreate)
        return nullptr;
#endif
    return adoptRef(*new JPEGXLImageDecoder(alphaOption, gammaAndColorProfileOption));
}

JPEGXLImageDecoder::JPEGXLImageDecoder(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : ScalableImageDecoder(alphaOption, gammaAndColorProfileOption)
{
}

JPEGXLImageDecoder::~JPEGXLImageDecoder()
{
    clear();
}

void JPEGXLImageDecoder::clear()
{
    m_decoder.reset();
#if USE(LCMS) || USE(CG)
    clearColorTransform();
#endif
}

size_t JPEGXLImageDecoder::frameCount() const
{
    if (!hasAnimation())
        return 1;

    if (!m_isLastFrameHeaderReceived)
        const_cast<JPEGXLImageDecoder*>(this)->updateFrameCount();

    return m_frameCount;
}

RepetitionCount JPEGXLImageDecoder::repetitionCount() const
{
    if (hasAnimation()) {
        if (!m_basicInfo->animation.num_loops) {
            // If num_loops is zero, repeat infinitely.
            return RepetitionCountInfinite;
        }
        return m_basicInfo->animation.num_loops;
    }
    return RepetitionCountNone;
}

ScalableImageDecoderFrame* JPEGXLImageDecoder::frameBufferAtIndex(size_t index)
{
    if (ScalableImageDecoder::encodedDataStatus() < EncodedDataStatus::SizeAvailable)
        return nullptr;

    if (index >= frameCount())
        index = frameCount() - 1;

    if (m_frameBufferCache.isEmpty())
        m_frameBufferCache.grow(1);

    auto& frame = m_frameBufferCache[index];
    if (!frame.isComplete())
        decode(Query::DecodedImage, index, isAllDataReceived());
    return &frame;
}

void JPEGXLImageDecoder::clearFrameBufferCache(size_t clearBeforeFrame)
{
    if (m_frameBufferCache.isEmpty())
        return;

    // Unlike the png and gif cases, we can always try to clear frames before "clearBeforeFrame" because
    // the dependenciy to the previous frame is handled by libjxl.
    const Vector<ScalableImageDecoderFrame>::iterator end(m_frameBufferCache.begin() + clearBeforeFrame);

    for (Vector<ScalableImageDecoderFrame>::iterator i(m_frameBufferCache.begin()); i != end; ++i) {
        // If the frame is partial, we're still on the way to decode the frame and it's likely
        // we continue the decode, so we don't clear the frame.
        if (i->isPartial())
            continue;

        i->clear();
    }
}

bool JPEGXLImageDecoder::setFailed()
{
    clear();
    return ScalableImageDecoder::setFailed();
}

void JPEGXLImageDecoder::tryDecodeSize(bool allDataReceived)
{
    if (m_basicInfo)
        return;
    decode(Query::Size, 0, allDataReceived);
}

bool JPEGXLImageDecoder::hasAlpha() const
{
    return m_basicInfo && m_basicInfo->alpha_bits > 0;
}

bool JPEGXLImageDecoder::hasAnimation() const
{
    return m_basicInfo && m_basicInfo->have_animation;
}

void JPEGXLImageDecoder::ensureDecoderInitialized()
{
    if (failed())
        return;

    if (m_decoder)
        return;

    m_decoder = JxlDecoderMake(nullptr);
    if (!m_decoder) {
        setFailed();
        return;
    }

    if (JxlDecoderSubscribeEvents(m_decoder.get(), s_eventsWanted) != JXL_DEC_SUCCESS) {
        setFailed();
        return;
    }

    m_readOffset = 0;
    m_currentFrame = 0;
    m_lastQuery = Query::Size;
}

bool JPEGXLImageDecoder::shouldRewind(Query query, size_t frameIndex) const
{
    if (m_lastQuery == Query::FrameCount) {
        // If the current query is not FrameCount, we've completed the previous FrameCount query
        // and the decoder has reached the EOF, so we need to rewind the decoder for the new query.
        // Otherwise we continue the FrameCount query, so we must not rewind the decoder.
        return query != Query::FrameCount;
    }

    if (m_lastQuery == Query::Size) {
        // The decoder is at the stream header and doesn't need rewind.
        return false;
    }

    // At this point we know m_lastQuery is Query::DecodedImage.

    if (query != Query::DecodedImage)
        return true;

    // There's two cases where we don't need to rewind:
    //   1. Previous decoding was interrupted with JXL_DEC_NEED_MORE_INPUT
    //      and trying to continue decoding the same frame.
    //   2. Previous decoding was completed (m_currentFrame was incremented) and starting a new frame.
    // In both cases frameIndex is equal to m_currentFrame.
    return frameIndex != m_currentFrame;
}

void JPEGXLImageDecoder::rewind()
{
    JxlDecoderRewind(m_decoder.get());
    JxlDecoderSubscribeEvents(m_decoder.get(), s_eventsWanted);
    m_readOffset = 0;
    m_currentFrame = 0;
}

void JPEGXLImageDecoder::updateFrameCount()
{
    if (failed())
        return;

    decode(Query::FrameCount, 0, isAllDataReceived());

    if (m_frameCount != m_frameBufferCache.size())
        m_frameBufferCache.resize(m_frameCount);
}

void JPEGXLImageDecoder::decode(Query query, size_t frameIndex, bool allDataReceived)
{
    ensureDecoderInitialized();

    if (failed())
        return;

    if (shouldRewind(query, frameIndex)) {
        rewind();
        // We care about the frameIndex only if the query is Query::DecodedImage.
        if (query == Query::DecodedImage && frameIndex) {
            JxlDecoderSkipFrames(m_decoder.get(), frameIndex);
            m_currentFrame = frameIndex;
        }
    }

    m_lastQuery = query;

    m_data->data();
    size_t dataSize = m_data->size();
    if (JxlDecoderSetInput(m_decoder.get(), m_data->data() + m_readOffset, dataSize - m_readOffset) != JXL_DEC_SUCCESS) {
        setFailed();
        return;
    }

    JxlDecoderStatus status = processInput(query);
    // We set the status as failed if the decoder reports an error or requires more data while all data has been received.
    if (status == JXL_DEC_ERROR || (allDataReceived && status == JXL_DEC_NEED_MORE_INPUT)) {
        setFailed();
        return;
    }

    if (query == Query::DecodedImage && status == JXL_DEC_FULL_IMAGE && m_isLastFrameHeaderReceived && m_currentFrame == m_frameCount) {
        // We free the decoder and LCMS data when the last frame is decoded.
        clear();
        return;
    }

    size_t remainingDataSize = JxlDecoderReleaseInput(m_decoder.get());
    m_readOffset = dataSize - remainingDataSize;
}

JxlDecoderStatus JPEGXLImageDecoder::processInput(Query query)
{
    while (true) {
        auto status = JxlDecoderProcessInput(m_decoder.get());

        // Return JXL_DEC_ERROR when we've reached EOF without receiving a frame marked as "is_last".
        if (status == JXL_DEC_SUCCESS && !m_isLastFrameHeaderReceived)
            return JXL_DEC_ERROR;

        // JXL_DEC_ERROR and JXL_DEC_SUCCESS are terminal states. We also exit from the loop if more data is needed.
        if (status == JXL_DEC_ERROR || status == JXL_DEC_SUCCESS || status == JXL_DEC_NEED_MORE_INPUT)
            return status;

        if (status == JXL_DEC_BASIC_INFO) {
            if (!m_basicInfo) {
                JxlBasicInfo basicInfo;
                if (JxlDecoderGetBasicInfo(m_decoder.get(), &basicInfo) != JXL_DEC_SUCCESS)
                    return JXL_DEC_ERROR;

                m_basicInfo = basicInfo;
            }

            if (query == Query::Size) {
                // setSize() must be called only if the query is Query::Size,
                // otherwise this would roll back the encoded data status from completed.
                setSize(IntSize(m_basicInfo->xsize, m_basicInfo->ysize));
                return status;
            }

            continue;
        }

#if USE(LCMS) || USE(CG)
        if (status == JXL_DEC_COLOR_ENCODING && !m_ignoreGammaAndColorProfile) {
            prepareColorTransform();
            continue;
        }
#endif

        if (status == JXL_DEC_FRAME) {
            JxlFrameHeader frameHeader;
            if (JxlDecoderGetFrameHeader(m_decoder.get(), &frameHeader) != JXL_DEC_SUCCESS)
                return JXL_DEC_ERROR;

            m_frameCount = std::max(m_frameCount, m_currentFrame + 1);

            if (frameHeader.is_last)
                m_isLastFrameHeaderReceived = true;

            if (query != Query::DecodedImage) {
                if (JxlDecoderSetImageOutCallback(m_decoder.get(), &s_pixelFormat, [](void*, size_t, size_t, size_t, const void*) { }, nullptr) != JXL_DEC_SUCCESS)
                    return JXL_DEC_ERROR;

                continue;
            }

            if (m_currentFrame >= m_frameBufferCache.size())
                m_frameBufferCache.grow(m_frameCount + 1);

            auto& buffer = m_frameBufferCache[m_currentFrame];
            if (buffer.isInvalid() && buffer.initialize(size(), m_premultiplyAlpha)) {
                buffer.setDecodingStatus(DecodingStatus::Partial);
                buffer.setHasAlpha(hasAlpha());
                if (m_basicInfo && m_basicInfo->have_animation) {
                    buffer.setDuration(Seconds((double)frameHeader.duration * m_basicInfo->animation.tps_denominator / m_basicInfo->animation.tps_numerator));
                    buffer.setDisposalMethod(ScalableImageDecoderFrame::DisposalMethod::DoNotDispose);
                }
            }

            if (JxlDecoderSetImageOutCallback(m_decoder.get(), &s_pixelFormat, imageOutCallback, this) != JXL_DEC_SUCCESS)
                return JXL_DEC_ERROR;

            continue;
        }

        if (status == JXL_DEC_FULL_IMAGE) {
            if (m_currentFrame < m_frameBufferCache.size()) {
                auto& buffer = m_frameBufferCache[m_currentFrame];
                if (!buffer.isInvalid())
                    buffer.setDecodingStatus(DecodingStatus::Complete);
            }

            m_currentFrame++;
            if (query == Query::DecodedImage)
                return JXL_DEC_FULL_IMAGE;

            ASSERT(query == Query::FrameCount);
            continue;
        }
    }
}

void JPEGXLImageDecoder::imageOutCallback(void* that, size_t x, size_t y, size_t numPixels, const void* pixels)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif
    static_cast<JPEGXLImageDecoder*>(that)->imageOut(x, y, numPixels, static_cast<const uint8_t*>(pixels));
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

void JPEGXLImageDecoder::imageOut(size_t x, size_t y, size_t numPixels, const uint8_t* pixels)
{
    if (m_currentFrame >= m_frameBufferCache.size())
        return;

    auto& buffer = m_frameBufferCache[m_currentFrame];
    if (buffer.isInvalid())
        return;

    uint32_t* row = buffer.backingStore()->pixelAt(x, y);
    uint32_t* currentAddress = row;
    for (size_t i = 0; i < numPixels; i++) {
        uint8_t r = *pixels++;
        uint8_t g = *pixels++;
        uint8_t b = *pixels++;
        uint8_t a = *pixels++;
        buffer.backingStore()->setPixel(currentAddress++, r, g, b, a);
    }

    maybePerformColorSpaceConversion(row, row, numPixels);
}

#if USE(LCMS)

void JPEGXLImageDecoder::clearColorTransform()
{
    m_iccTransform.reset();
}

void JPEGXLImageDecoder::prepareColorTransform()
{
    if (m_iccTransform)
        return;

    cmsHPROFILE displayProfile = PlatformDisplay::sharedDisplay().colorProfile();
    if (!displayProfile)
        return;

    auto profile = tryDecodeICCColorProfile();
    if (!profile)
        return; // TODO(bugs.webkit.org/show_bug.cgi?id=234222): We should try to use encoded color profile if ICC profile is not available.

    // TODO(bugs.webkit.org/show_bug.cgi?id=234221): We should handle CMYK color but it may require two extra channels (Alpha and K)
    // and libjxl has yet to support it.
    if (cmsGetColorSpace(profile.get()) == cmsSigRgbData && cmsGetColorSpace(displayProfile) == cmsSigRgbData)
        m_iccTransform = LCMSTransformPtr(cmsCreateTransform(profile.get(), TYPE_BGRA_8, displayProfile, TYPE_BGRA_8, INTENT_RELATIVE_COLORIMETRIC, 0));
}

LCMSProfilePtr JPEGXLImageDecoder::tryDecodeICCColorProfile()
{
    size_t profileSize = 0;
#if JPEGXL_NUMERIC_VERSION < JPEGXL_COMPUTE_NUMERIC_VERSION(0, 9, 0)
    if (JxlDecoderGetICCProfileSize(m_decoder.get(), &s_pixelFormat, JXL_COLOR_PROFILE_TARGET_DATA, &profileSize) != JXL_DEC_SUCCESS)
        return nullptr;

    Vector<uint8_t> profileData(profileSize);
    if (JxlDecoderGetColorAsICCProfile(m_decoder.get(), &s_pixelFormat, JXL_COLOR_PROFILE_TARGET_DATA, profileData.data(), profileData.size()) != JXL_DEC_SUCCESS)
        return nullptr;
#else
    if (JxlDecoderGetICCProfileSize(m_decoder.get(), JXL_COLOR_PROFILE_TARGET_DATA, &profileSize) != JXL_DEC_SUCCESS)
        return nullptr;

    Vector<uint8_t> profileData(profileSize);
    if (JxlDecoderGetColorAsICCProfile(m_decoder.get(), JXL_COLOR_PROFILE_TARGET_DATA, profileData.data(), profileData.size()) != JXL_DEC_SUCCESS)
        return nullptr;
#endif

    return LCMSProfilePtr(cmsOpenProfileFromMem(profileData.data(), profileData.size()));
}

void JPEGXLImageDecoder::maybePerformColorSpaceConversion(void* inputBuffer, void* outputBuffer, unsigned numberOfPixels)
{
    if (!m_iccTransform)
        return;

    cmsDoTransform(m_iccTransform.get(), inputBuffer, outputBuffer, numberOfPixels);
}

#elif USE(CG)

void JPEGXLImageDecoder::clearColorTransform()
{
    m_profile = nullptr;
}

void JPEGXLImageDecoder::prepareColorTransform()
{
    if (m_profile || !m_basicInfo || m_basicInfo->num_extra_channels > 1 || m_basicInfo->exponent_bits_per_sample || m_basicInfo->alpha_exponent_bits)
        return;

    m_profile = tryDecodeICCColorProfile();
    // TODO(bugs.webkit.org/show_bug.cgi?id=234222): We should try to use encoded color profile if ICC profile is not available.
}

RetainPtr<CGColorSpaceRef> JPEGXLImageDecoder::tryDecodeICCColorProfile()
{

    size_t profileSize = 0;
    if (JxlDecoderGetICCProfileSize(m_decoder.get(), &s_pixelFormat, JXL_COLOR_PROFILE_TARGET_DATA, &profileSize) != JXL_DEC_SUCCESS)
        return nullptr;

    auto data = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, profileSize));
    CFDataIncreaseLength(data.get(), profileSize);

    if (JxlDecoderGetColorAsICCProfile(m_decoder.get(), &s_pixelFormat, JXL_COLOR_PROFILE_TARGET_DATA, CFDataGetMutableBytePtr(data.get()), profileSize) != JXL_DEC_SUCCESS)
        return nullptr;

    return adoptCF(CGColorSpaceCreateWithICCData(data.get()));
}

void JPEGXLImageDecoder::maybePerformColorSpaceConversion(void* inputBuffer, void* outputBuffer, unsigned numberOfPixels)
{
    if (!m_profile)
        return;

    // https://developer.apple.com/documentation/accelerate/1399134-vimageconvert_anytoany?language=objc
    // "The destination buffer may only alias the srcs buffers if vImageConverter_MustOperateOutOfPlace returns 0 and the respective scanlines of the aliasing buffers start at the same address."
    // convertImagePixels() doesn't have any logic for this, so we can just pessimize here and assume aliasing is illegal.
    std::unique_ptr<uint8_t[]> intermediateBuffer(new uint8_t[4 * numberOfPixels]);
    auto alphaFormat = m_premultiplyAlpha ? AlphaPremultiplication::Premultiplied : AlphaPremultiplication::Unpremultiplied;
    ConstPixelBufferConversionView source {
        .format = {
            .alphaFormat = alphaFormat,
            .pixelFormat = PixelFormat::BGRA8,
            .colorSpace = DestinationColorSpace(m_profile.get()),
        },
        .bytesPerRow = static_cast<unsigned>(4 * size().width()),
        .rows = static_cast<const uint8_t*>(inputBuffer),
    };
    PixelBufferConversionView destination {
        .format = {
            .alphaFormat = alphaFormat,
            .pixelFormat = PixelFormat::BGRA8,
            .colorSpace = DestinationColorSpace::SRGB(),
        },
        .bytesPerRow = static_cast<unsigned>(4 * size().width()),
        .rows = intermediateBuffer.get(),
    };
    convertImagePixels(source, destination, { static_cast<int>(numberOfPixels), 1 });
    memcpy(outputBuffer, intermediateBuffer.get(), numberOfPixels * 4);
}

#else

void JPEGXLImageDecoder::maybePerformColorSpaceConversion(void*, void*, unsigned)
{
}

#endif // USE(LCMS), USE(CG)

}
#endif // USE(JPEGXL)
