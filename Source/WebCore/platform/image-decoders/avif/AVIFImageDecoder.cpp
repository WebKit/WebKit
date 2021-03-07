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
#include "AVIFImageDecoder.h"

#include "AVIFImageReader.h"

namespace WebCore {

AVIFImageDecoder::AVIFImageDecoder(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : ScalableImageDecoder(alphaOption, gammaAndColorProfileOption)
{
}

AVIFImageDecoder::~AVIFImageDecoder() = default;

ScalableImageDecoderFrame* AVIFImageDecoder::frameBufferAtIndex(size_t index)
{
    if (index)
        return nullptr;

    if (m_frameBufferCache.isEmpty())
        m_frameBufferCache.grow(1);

    auto& frame = m_frameBufferCache[0];
    if (!frame.isComplete())
        decode(frame, isAllDataReceived());
    return &frame;
}

bool AVIFImageDecoder::setFailed()
{
    m_reader = nullptr;
    return ScalableImageDecoder::setFailed();
}

void AVIFImageDecoder::tryDecodeSize(bool allDataReceived)
{
    if (!m_reader)
        m_reader = makeUnique<AVIFImageReader>(this);
    m_reader->parseHeader(*m_data, allDataReceived);
}

void AVIFImageDecoder::decode(ScalableImageDecoderFrame& frame, bool allDataReceived)
{
    if (failed())
        return;

    ASSERT(m_reader);
    m_reader->decodeFrame(0, frame, *m_data);

    if (allDataReceived && !m_frameBufferCache.isEmpty() && frame.isComplete())
        m_reader = nullptr;
}

}
