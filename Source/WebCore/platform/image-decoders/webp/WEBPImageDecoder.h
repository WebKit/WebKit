/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ScalableImageDecoder.h"

#if USE(WEBP)

#include "webp/decode.h"
#include "webp/demux.h"

namespace WebCore {

class WEBPImageDecoder final : public ScalableImageDecoder {
public:
    static Ref<ScalableImageDecoder> create(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    {
        return adoptRef(*new WEBPImageDecoder(alphaOption, gammaAndColorProfileOption));
    }

    virtual ~WEBPImageDecoder();

    String filenameExtension() const override { return "webp"_s; }
    void setData(SharedBuffer&, bool) final;
    ScalableImageDecoderFrame* frameBufferAtIndex(size_t index) override;
    RepetitionCount repetitionCount() const override;
    size_t frameCount() const override { return m_frameCount; }
    void clearFrameBufferCache(size_t) override;

private:
    WEBPImageDecoder(AlphaOption, GammaAndColorProfileOption);
    void tryDecodeSize(bool) override { parseHeader(); }
    void decode(size_t, bool);
    void decodeFrame(size_t, WebPDemuxer*);
    void parseHeader();
    bool initFrameBuffer(size_t, const WebPIterator*);
    void applyPostProcessing(size_t, WebPIDecoder*, WebPDecBuffer&, bool);
    size_t findFirstRequiredFrameToDecode(size_t, WebPDemuxer*);

    int m_repetitionCount { 0 };
    size_t m_frameCount { 0 };
    int m_formatFlags { 0 };
    bool m_headerParsed { false };
};

} // namespace WebCore

#endif
