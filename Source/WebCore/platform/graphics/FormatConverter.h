/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Mozilla Corporation. All rights reserved.
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

#if ENABLE(WEBGL)

#include "GraphicsContextGL.h"
#include "IntRect.h"
#include <wtf/MallocSpan.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class FormatConverter {
public:
    FormatConverter(
        const IntRect& sourceDataSubRectangle,
        int depth,
        int unpackImageHeight,
        std::span<const uint8_t> source,
        std::span<uint8_t> destinationCursor,
        std::span<uint8_t> destination,
        int srcStride,
        int srcRowOffset,
        int dstStride)
            : m_srcSubRectangle(sourceDataSubRectangle)
            , m_depth(depth)
            , m_unpackImageHeight(unpackImageHeight)
            , m_source(source)
            , m_destinationCursor(destinationCursor)
            , m_destination(destination)
            , m_srcStride(srcStride)
            , m_srcRowOffset(srcRowOffset)
            , m_dstStride(dstStride)
            , m_success(false)
    {
        const unsigned MaxNumberOfComponents = 4;
        const unsigned MaxBytesPerComponent  = 4;
        m_unpackedIntermediateSrcData = MallocSpan<uint8_t>::malloc(Checked<size_t>(m_srcSubRectangle.width()) * MaxNumberOfComponents * MaxBytesPerComponent);
        ASSERT(m_unpackedIntermediateSrcData);
    }

    void convert(GraphicsContextGL::DataFormat srcFormat, GraphicsContextGL::DataFormat dstFormat, GraphicsContextGL::AlphaOp);
    bool success() const { return m_success; }

private:
    template<GraphicsContextGL::DataFormat SrcFormat>
    ALWAYS_INLINE void convert(GraphicsContextGL::DataFormat dstFormat, GraphicsContextGL::AlphaOp);

    template<GraphicsContextGL::DataFormat SrcFormat, GraphicsContextGL::DataFormat DstFormat>
    ALWAYS_INLINE void convert(GraphicsContextGL::AlphaOp);

    template<GraphicsContextGL::DataFormat SrcFormat, GraphicsContextGL::DataFormat DstFormat, GraphicsContextGL::AlphaOp alphaOp>
    ALWAYS_INLINE void convert();

    const IntRect& m_srcSubRectangle;
    const int m_depth;
    const int m_unpackImageHeight;
    std::span<const uint8_t> m_source;
    std::span<uint8_t> m_destinationCursor;
    std::span<uint8_t> m_destination;
    const int m_srcStride, m_srcRowOffset, m_dstStride;
    bool m_success;
    MallocSpan<uint8_t> m_unpackedIntermediateSrcData;
};

} // namespace WebCore

#endif // ENABLE(WEBGL)
