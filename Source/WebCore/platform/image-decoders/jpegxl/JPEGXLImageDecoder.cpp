/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

namespace WebCore {

JPEGXLImageDecoder::JPEGXLImageDecoder(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : ScalableImageDecoder(alphaOption, gammaAndColorProfileOption)
{
}

JPEGXLImageDecoder::~JPEGXLImageDecoder()
{
    clear();
}

ScalableImageDecoderFrame* JPEGXLImageDecoder::frameBufferAtIndex(size_t index)
{
    // TODO: To support animated JPEG XL in the future we need to handle second and subsequent frames.
    if (index)
        return nullptr;

    if (m_frameBufferCache.isEmpty())
        m_frameBufferCache.grow(1);

    auto& frame = m_frameBufferCache[0];
    if (!frame.isComplete())
        decode(false, isAllDataReceived());
    return &frame;
}

void JPEGXLImageDecoder::clear()
{
    m_decoder.reset();
    m_readOffset = 0;
}

bool JPEGXLImageDecoder::setFailed()
{
    clear();
    return ScalableImageDecoder::setFailed();
}

void JPEGXLImageDecoder::decode(bool onlySize, bool allDataReceived)
{
    if (failed())
        return;

    if (!m_decoder) {
        clear();
        m_decoder = JxlDecoderMake(nullptr);
        if (!m_decoder) {
            setFailed();
            return;
        }

        if (JxlDecoderSubscribeEvents(m_decoder.get(), JXL_DEC_BASIC_INFO | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS) {
            setFailed();
            return;
        }
    }

    m_data->data();
    size_t dataSize = m_data->size();
    if (JxlDecoderSetInput(m_decoder.get(), m_data->data() + m_readOffset, dataSize - m_readOffset) != JXL_DEC_SUCCESS) {
        setFailed();
        return;
    }

    JxlDecoderStatus status = processInput(onlySize);
    // We set the status as failed if the decoder reports an error or requires more data while all data has been received.
    if (status == JXL_DEC_ERROR || (allDataReceived && status == JXL_DEC_NEED_MORE_INPUT)) {
        setFailed();
        return;
    }

    // We release the decoder when we finish the decoding.
    if (status == JXL_DEC_SUCCESS) {
        clear();
        return;
    }

    // Otherwise we get the decoder ready for subsequent data.
    size_t remainingDataSize = JxlDecoderReleaseInput(m_decoder.get());
    m_readOffset = dataSize - remainingDataSize;
}

JxlDecoderStatus JPEGXLImageDecoder::processInput(bool onlySize)
{
    while (true) {
        auto status = JxlDecoderProcessInput(m_decoder.get());

        // JXL_DEC_ERROR and JXL_DEC_SUCCESS are terminal states. We also exit from the loop if more data is needed.
        if (status == JXL_DEC_ERROR || status == JXL_DEC_SUCCESS || status == JXL_DEC_NEED_MORE_INPUT)
            return status;

        if (status == JXL_DEC_BASIC_INFO) {
            JxlBasicInfo basicInfo;
            if (JxlDecoderGetBasicInfo(m_decoder.get(), &basicInfo) != JXL_DEC_SUCCESS)
                return JXL_DEC_ERROR;

            setSize(IntSize(basicInfo.xsize, basicInfo.ysize));
            if (onlySize)
                return status;
            continue;
        }

        if (status == JXL_DEC_FRAME) {
            JxlPixelFormat format { 4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };
            if (JxlDecoderSetImageOutCallback(m_decoder.get(), &format, imageOutCallback, this) != JXL_DEC_SUCCESS)
                return JXL_DEC_ERROR;
            continue;
        }

        if (status == JXL_DEC_FULL_IMAGE) {
            if (m_frameBufferCache.isEmpty())
                continue;

            auto& buffer = m_frameBufferCache[0];
            if (!buffer.isInvalid())
                buffer.setDecodingStatus(DecodingStatus::Complete);

            // TODO: To support animated JPEG XL in the future we need to handle subsequent data.
            return JXL_DEC_SUCCESS;
        }
    }
}

void JPEGXLImageDecoder::imageOutCallback(void* that, size_t x, size_t y, size_t numPixels, const void* pixels)
{
    static_cast<JPEGXLImageDecoder*>(that)->imageOut(x, y, numPixels, static_cast<const uint8_t*>(pixels));
}

void JPEGXLImageDecoder::imageOut(size_t x, size_t y, size_t numPixels, const uint8_t* pixels)
{
    if (m_frameBufferCache.isEmpty())
        return;

    auto& buffer = m_frameBufferCache[0];
    if (buffer.isInvalid()) {
        if (!buffer.initialize(size(), m_premultiplyAlpha))
            return;

        buffer.setDecodingStatus(DecodingStatus::Partial);
        buffer.setHasAlpha(false);
    }

    uint32_t* row = buffer.backingStore()->pixelAt(x, y);
    uint32_t* currentAddress = row;
    for (size_t i = 0; i < numPixels; i++) {
        uint8_t r = *pixels++;
        uint8_t g = *pixels++;
        uint8_t b = *pixels++;
        uint8_t a = *pixels++;
        buffer.backingStore()->setPixel(currentAddress++, r, g, b, a);
    }
}

}
#endif // USE(JPEGXL)
