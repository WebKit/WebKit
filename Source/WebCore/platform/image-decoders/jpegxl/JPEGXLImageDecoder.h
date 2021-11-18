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

#pragma once

#include "ScalableImageDecoder.h"

#if USE(JPEGXL)

#include <jxl/decode_cxx.h>

namespace WebCore {

// This class decodes the JPEG XL image format.
class JPEGXLImageDecoder final : public ScalableImageDecoder {
public:
    static Ref<ScalableImageDecoder> create(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    {
        return adoptRef(*new JPEGXLImageDecoder(alphaOption, gammaAndColorProfileOption));
    }

    virtual ~JPEGXLImageDecoder();

    // ScalableImageDecoder
    String filenameExtension() const override { return "jxl"_s; }
    ScalableImageDecoderFrame* frameBufferAtIndex(size_t index) override;

    bool setFailed() override;

private:
    JPEGXLImageDecoder(AlphaOption, GammaAndColorProfileOption);
    void tryDecodeSize(bool allDataReceived) override { decode(true, allDataReceived); }

    void decode(bool onlySize, bool allDataReceived);

    void clear();

    JxlDecoderStatus processInput(bool onlySize);

    static void imageOutCallback(void* that, size_t x, size_t y, size_t numPixels, const void* pixels);
    void imageOut(size_t x, size_t y, size_t numPixels, const uint8_t* pixels);

    JxlDecoderPtr m_decoder;
    size_t m_readOffset = 0;
};

} // namespace WebCore

#endif // USE(JPEGXL)
