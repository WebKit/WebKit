/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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
#include <wtf/Lock.h>

class GIFImageReader;

namespace WebCore {

// This class decodes the GIF image format.
class GIFImageDecoder final : public ScalableImageDecoder {
public:
    static Ref<ScalableImageDecoder> create(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    {
        return adoptRef(*new GIFImageDecoder(alphaOption, gammaAndColorProfileOption));
    }

    virtual ~GIFImageDecoder();

    enum GIFQuery { GIFFullQuery, GIFSizeQuery, GIFFrameCountQuery };

    // ScalableImageDecoder
    String filenameExtension() const final { return "gif"_s; }
    void setData(const FragmentedSharedBuffer& data, bool allDataReceived) final;
    bool setSize(const IntSize&) final;
    size_t frameCount() const final;
    RepetitionCount repetitionCount() const final;
    ScalableImageDecoderFrame* frameBufferAtIndex(size_t index) final;
    // CAUTION: setFailed() deletes |m_reader|. Be careful to avoid
    // accessing deleted memory, especially when calling this from inside
    // GIFImageReader!
    bool setFailed() final;
    void clearFrameBufferCache(size_t clearBeforeFrame) final;

    // Callbacks from the GIF reader.
    bool haveDecodedRow(unsigned frameIndex, const Vector<unsigned char>& rowBuffer, size_t width, size_t rowNumber, unsigned repeatCount, bool writeTransparentPixels);
    bool frameComplete(unsigned frameIndex, unsigned frameDuration, ScalableImageDecoderFrame::DisposalMethod);
    void gifComplete();

private:
    GIFImageDecoder(AlphaOption, GammaAndColorProfileOption);
    void tryDecodeSize(bool allDataReceived) final { decode(0, GIFSizeQuery, allDataReceived); }
    size_t findFirstRequiredFrameToDecode(size_t);

    // If the query is GIFFullQuery, decodes the image up to (but not
    // including) |haltAtFrame|. Otherwise, decodes as much as is needed to
    // answer the query, ignoring bitmap data. If decoding fails but there
    // is no more data coming, sets the "decode failure" flag.
    void decode(unsigned haltAtFrame, GIFQuery, bool allDataReceived);

    // Called to initialize the frame buffer with the given index, based on
    // the previous frame's disposal method. Returns true on success. On
    // failure, this will mark the image as failed.
    bool initFrameBuffer(unsigned frameIndex);

    bool m_currentBufferSawAlpha;
    mutable RepetitionCount m_repetitionCount { RepetitionCountOnce };
    std::unique_ptr<GIFImageReader> m_reader;
};

} // namespace WebCore
