/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
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
#include <stdio.h> // Needed by jpeglib.h for FILE.

// ICU defines TRUE and FALSE macros, breaking libjpeg v9 headers
#undef TRUE
#undef FALSE
extern "C" {
#include "jpeglib.h"
}

namespace WebCore {

    class JPEGImageReader;

    // This class decodes the JPEG image format.
    class JPEGImageDecoder final : public ScalableImageDecoder {
    public:
        static Ref<ScalableImageDecoder> create(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
        {
            return adoptRef(*new JPEGImageDecoder(alphaOption, gammaAndColorProfileOption));
        }

        virtual ~JPEGImageDecoder();

        // ScalableImageDecoder
        String filenameExtension() const override { return "jpg"_s; }
        ScalableImageDecoderFrame* frameBufferAtIndex(size_t index) override;
        // CAUTION: setFailed() deletes |m_reader|.  Be careful to avoid
        // accessing deleted memory, especially when calling this from inside
        // JPEGImageReader!
        bool setFailed() override;

        bool outputScanlines();
        void jpegComplete();

        void setOrientation(ImageOrientation orientation) { m_orientation = orientation; }

    private:
        JPEGImageDecoder(AlphaOption, GammaAndColorProfileOption);
        void tryDecodeSize(bool allDataReceived) override { decode(true, allDataReceived); }

        // Decodes the image.  If |onlySize| is true, stops decoding after
        // calculating the image size.  If decoding fails but there is no more
        // data coming, sets the "decode failure" flag.
        void decode(bool onlySize, bool allDataReceived);

        template <J_COLOR_SPACE colorSpace>
        bool outputScanlines(ScalableImageDecoderFrame& buffer);

        template <J_COLOR_SPACE colorSpace, bool isScaled>
        bool outputScanlines(ScalableImageDecoderFrame& buffer);

        std::unique_ptr<JPEGImageReader> m_reader;
    };

} // namespace WebCore
