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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "ReverbAccumulationBuffer.h"

#include "VectorMath.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

#include <algorithm>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ReverbAccumulationBuffer);

ReverbAccumulationBuffer::ReverbAccumulationBuffer(size_t length)
    : m_buffer(length)
{
}

void ReverbAccumulationBuffer::readAndClear(std::span<float> destination, size_t numberOfFrames)
{
    size_t bufferLength = m_buffer.size();
    bool isCopySafe = m_readIndex <= bufferLength && numberOfFrames <= bufferLength;
    
    ASSERT(isCopySafe);
    if (!isCopySafe)
        return;

    size_t framesAvailable = bufferLength - m_readIndex;
    size_t numberOfFrames1 = std::min(numberOfFrames, framesAvailable);
    size_t numberOfFrames2 = numberOfFrames - numberOfFrames1;

    auto source = m_buffer.span();
    memcpySpan(destination, source.subspan(m_readIndex).first(numberOfFrames1));
    zeroSpan(source.subspan(m_readIndex, numberOfFrames1));

    // Handle wrap-around if necessary
    if (numberOfFrames2 > 0) {
        memcpySpan(destination.subspan(numberOfFrames1), source.first(numberOfFrames2));
        zeroSpan(source.first(numberOfFrames2));
    }

    m_readIndex = (m_readIndex + numberOfFrames) % bufferLength;
    m_readTimeFrame += numberOfFrames;
}

void ReverbAccumulationBuffer::updateReadIndex(int* readIndex, size_t numberOfFrames) const
{
    // Update caller's readIndex
    *readIndex = (*readIndex + numberOfFrames) % m_buffer.size();
}

int ReverbAccumulationBuffer::accumulate(std::span<float> source, size_t numberOfFrames, int* readIndex, size_t delayFrames)
{
    size_t bufferLength = m_buffer.size();
    
    size_t writeIndex = (*readIndex + delayFrames) % bufferLength;

    // Update caller's readIndex
    *readIndex = (*readIndex + numberOfFrames) % bufferLength;

    size_t framesAvailable = bufferLength - writeIndex;
    size_t numberOfFrames1 = std::min(numberOfFrames, framesAvailable);
    size_t numberOfFrames2 = numberOfFrames - numberOfFrames1;

    auto destination = m_buffer.span();

    bool isSafe = writeIndex <= bufferLength && numberOfFrames1 + writeIndex <= bufferLength && numberOfFrames2 <= bufferLength;
    ASSERT(isSafe);
    if (!isSafe)
        return 0;

    VectorMath::add(source.first(numberOfFrames1), destination.subspan(writeIndex).first(numberOfFrames1), destination.subspan(writeIndex));

    // Handle wrap-around if necessary
    if (numberOfFrames2 > 0)       
        VectorMath::add(source.subspan(numberOfFrames1).first(numberOfFrames2), destination.first(numberOfFrames2), destination);

    return writeIndex;
}

void ReverbAccumulationBuffer::reset()
{
    m_buffer.zero();
    m_readIndex = 0;
    m_readTimeFrame = 0;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
