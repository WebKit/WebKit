/*
 * Copyright (C) 2021 Igalia S.L. All rights reserved.
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

namespace WebCore {

class AVIFImageReader;

// This class decodes the AVIF image format.
class AVIFImageDecoder final : public ScalableImageDecoder {
public:
    static Ref<ScalableImageDecoder> create(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    {
        return adoptRef(*new AVIFImageDecoder(alphaOption, gammaAndColorProfileOption));
    }

    virtual ~AVIFImageDecoder();

    // ScalableImageDecoder
    String filenameExtension() const final { return "avif"_s; }
    size_t frameCount() const final { return m_frameCount; };
    RepetitionCount repetitionCount() const final;
    ScalableImageDecoderFrame* frameBufferAtIndex(size_t index) final WTF_REQUIRES_LOCK(m_lock);
    bool setFailed() final;

private:
    AVIFImageDecoder(AlphaOption, GammaAndColorProfileOption);

    void tryDecodeSize(bool allDataReceived) final;
    void decode(size_t frameIndex, bool allDataReceived) WTF_REQUIRES_LOCK(m_lock);
    bool isComplete() WTF_REQUIRES_LOCK(m_lock);
    size_t findFirstRequiredFrameToDecode(size_t frameIndex) WTF_REQUIRES_LOCK(m_lock);

    std::unique_ptr<AVIFImageReader> m_reader { nullptr };

    size_t m_frameCount { 0 };
    int m_repetitionCount { 0 };
};

} // namespace WebCore
