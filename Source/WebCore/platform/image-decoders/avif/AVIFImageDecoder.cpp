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

#if USE(AVIF)

#include "AVIFImageDecoder.h"

#include "AVIFImageReader.h"

namespace WebCore {

AVIFImageDecoder::AVIFImageDecoder(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : ScalableImageDecoder(alphaOption, gammaAndColorProfileOption)
{
}

AVIFImageDecoder::~AVIFImageDecoder() = default;

RepetitionCount AVIFImageDecoder::repetitionCount() const
{
    if (failed() || m_frameCount <= 1)
        return RepetitionCountOnce;

    return m_repetitionCount ? m_repetitionCount : RepetitionCountInfinite;
}

size_t AVIFImageDecoder::findFirstRequiredFrameToDecode(size_t frameIndex)
{
    // The first frame doesn't depend on any other.
    if (!frameIndex)
        return 0;

    size_t firstIncompleteFrame = frameIndex;
    while (firstIncompleteFrame > 0) {
        if (m_frameBufferCache[firstIncompleteFrame - 1].isComplete())
            break;
        --firstIncompleteFrame;
    }

    return firstIncompleteFrame;
}

ScalableImageDecoderFrame* AVIFImageDecoder::frameBufferAtIndex(size_t index)
{
    const size_t imageCount = frameCount();
    if (index >= imageCount)
        return nullptr;

    if ((m_frameBufferCache.size() > index) && m_frameBufferCache[index].isComplete())
        return &m_frameBufferCache[index];

    if (imageCount && m_frameBufferCache.size() != imageCount)
        m_frameBufferCache.resize(imageCount);

    for (size_t i = findFirstRequiredFrameToDecode(index); i <= index; ++i) {
        if (m_frameBufferCache[i].isComplete())
            continue;
        decode(i, isAllDataReceived());
    }

    return &m_frameBufferCache[index];
}

bool AVIFImageDecoder::setFailed()
{
    m_reader = nullptr;
    return ScalableImageDecoder::setFailed();
}

bool AVIFImageDecoder::isComplete()
{
    if (m_frameBufferCache.isEmpty())
        return false;

    for (auto& frameBuffer : m_frameBufferCache) {
        if (!frameBuffer.isComplete())
            return false;
    }
    return true;
}

void AVIFImageDecoder::tryDecodeSize(bool allDataReceived)
{
    if (!m_reader)
        m_reader = makeUnique<AVIFImageReader>(this);

    if (!m_reader->parseHeader(*m_data, allDataReceived))
        return;

    m_frameCount = m_reader->imageCount();

    m_repetitionCount = m_frameCount > 1 ? RepetitionCountInfinite : RepetitionCountNone;
}

void AVIFImageDecoder::decode(size_t frameIndex, bool allDataReceived)
{
    if (failed())
        return;

    ASSERT(m_reader);
    m_reader->decodeFrame(frameIndex, m_frameBufferCache[frameIndex], *m_data);

    if (allDataReceived && !m_frameBufferCache.isEmpty() && isComplete())
        m_reader = nullptr;
}

}

#endif // USE(AVIF)
