/*
 * Copyright (C) 2012 Intel Inc. All rights reserved.
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

#include "DirectConvolver.h"

#include "VectorMath.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
    
WTF_MAKE_TZONE_ALLOCATED_IMPL(DirectConvolver);

DirectConvolver::DirectConvolver(size_t inputBlockSize)
    : m_inputBlockSize(inputBlockSize)
    , m_buffer(inputBlockSize * 2)
{
}

void DirectConvolver::process(AudioFloatArray* convolutionKernel, std::span<const float> source, std::span<float> destination)
{
    ASSERT(source.size() == m_inputBlockSize);
    if (source.size() != m_inputBlockSize)
        return;

    // Only support kernelSize <= m_inputBlockSize
    size_t kernelSize = convolutionKernel->size();
    ASSERT(kernelSize <= m_inputBlockSize);
    if (kernelSize > m_inputBlockSize)
        return;

    auto kernelP = convolutionKernel->span();

    // Sanity check
    bool isCopyGood = kernelP.data() && source.data() && destination.data() && m_buffer.data();
    ASSERT(isCopyGood);
    if (!isCopyGood)
        return;

    auto inputBuffer = m_buffer.span();
    auto inputP = inputBuffer.subspan(m_inputBlockSize);

    // Copy samples to 2nd half of input buffer.
    memcpySpan(inputP, source);

    VectorMath::convolve(inputBuffer.subspan(inputP.data() - inputBuffer.data() - kernelSize + 1), kernelP, destination.first(source.size()));

    // Copy 2nd half of input buffer to 1st half.
    memcpySpan(m_buffer.span(), inputP.first(source.size()));
}

void DirectConvolver::reset()
{
    m_buffer.zero();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
