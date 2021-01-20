/*
 * Copyright (C) 2019 Igalia S.L.
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

#include "ScalableImageDecoder.h"

#if USE(OPENJPEG)

namespace WebCore {

class JPEG2000ImageDecoder final : public ScalableImageDecoder {
public:
    enum class Format { JP2, J2K };
    static Ref<ScalableImageDecoder> create(Format format, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    {
        return adoptRef(*new JPEG2000ImageDecoder(format, alphaOption, gammaAndColorProfileOption));
    }

    virtual ~JPEG2000ImageDecoder() = default;

    // ScalableImageDecoder
    String filenameExtension() const override { return m_format == Format::JP2 ? "jp2"_s : "j2k"_s; }
    ScalableImageDecoderFrame* frameBufferAtIndex(size_t index) override;

private:
    JPEG2000ImageDecoder(Format, AlphaOption, GammaAndColorProfileOption);

    void decode(bool onlySize, bool allDataReceived);
    void tryDecodeSize(bool allDataReceived) override { decode(true, allDataReceived); }

    Format m_format;
};

} // namespace WebCore

#endif // USE(OPENJPEG)
