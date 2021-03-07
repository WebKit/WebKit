/*
 * Copyright (C) 2021 Igalia S.L. All rights reserved.
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
#include "AVIFImageReader.h"

#include "AVIFImageDecoder.h"

namespace WebCore {

AVIFImageReader::AVIFImageReader(RefPtr<AVIFImageDecoder>&& decoder)
    : m_decoder(WTFMove(decoder))
    , m_avifDecoder(avifDecoderCreate())
{
}

AVIFImageReader::~AVIFImageReader()
{
}

void AVIFImageReader::parseHeader(const SharedBuffer::DataSegment& data, bool allDataReceived)
{
    avifROData avifData;
    avifData.data = reinterpret_cast<const uint8_t*>(data.data());
    avifData.size = data.size();

    if (!avifPeekCompatibleFileType(&avifData)) {
        m_decoder->setFailed();
        return;
    }

    if (avifDecoderParse(m_avifDecoder.get(), &avifData) != AVIF_RESULT_OK
        || avifDecoderNextImage(m_avifDecoder.get()) != AVIF_RESULT_OK) {
        m_decoder->setFailed();
        return;
    }

    if (!m_dataParsed && allDataReceived)
        m_dataParsed = true;

    const avifImage* firstImage = m_avifDecoder->image;
    m_decoder->setSize(IntSize(firstImage->width, firstImage->height));
}

void AVIFImageReader::decodeFrame(size_t frameIndex, ScalableImageDecoderFrame& buffer, const SharedBuffer::DataSegment& data)
{
    if (m_decoder->failed())
        return;

    if (!m_dataParsed) {
        avifROData avifData;
        avifData.data = reinterpret_cast<const uint8_t*>(data.data());
        avifData.size = data.size();

        if (avifDecoderParse(m_avifDecoder.get(), &avifData) != AVIF_RESULT_OK) {
            m_decoder->setFailed();
            return;
        }

        if (m_decoder->isAllDataReceived())
            m_dataParsed = true;
    }

    if (avifDecoderNthImage(m_avifDecoder.get(), frameIndex) != AVIF_RESULT_OK) {
        m_decoder->setFailed();
        return;
    }

    IntSize imageSize(m_decoder->size());
    if (buffer.isInvalid() && !buffer.initialize(imageSize, m_decoder->premultiplyAlpha())) {
        m_decoder->setFailed();
        return;
    }

    buffer.setDecodingStatus(DecodingStatus::Partial);

    avifRGBImage decodedRGBImage;
    avifRGBImageSetDefaults(&decodedRGBImage, m_avifDecoder->image);
    decodedRGBImage.depth = 8;
    decodedRGBImage.format = AVIF_RGB_FORMAT_BGRA;
    decodedRGBImage.rowBytes = imageSize.width() * sizeof(uint32_t);
    decodedRGBImage.pixels = reinterpret_cast<uint8_t*>(buffer.backingStore()->pixelAt(0, 0));
    if (avifImageYUVToRGB(m_avifDecoder->image, &decodedRGBImage) != AVIF_RESULT_OK) {
        m_decoder->setFailed();
        return;
    }

    buffer.setHasAlpha(avifRGBFormatHasAlpha(decodedRGBImage.format));
    buffer.setDecodingStatus(DecodingStatus::Complete);
}

}
