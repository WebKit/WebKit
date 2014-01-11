/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(3D_GRAPHICS)

#include "GraphicsContext3D.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

class FormatConverter {
public:
    FormatConverter(unsigned width, unsigned height, const void* srcStart, void* dstStart, int srcStride, int dstStride)
        : m_width(width)
        , m_height(height)
        , m_srcStart(srcStart)
        , m_dstStart(dstStart)
        , m_srcStride(srcStride)
        , m_dstStride(dstStride)
        , m_success(false)
    {
        const unsigned MaxNumberOfComponents = 4;
        const unsigned MaxBytesPerComponent  = 4;
        m_unpackedIntermediateSrcData = std::make_unique<uint8_t[]>(m_width * MaxNumberOfComponents * MaxBytesPerComponent);

        ASSERT(m_unpackedIntermediateSrcData.get());
    }

    void convert(GraphicsContext3D::DataFormat srcFormat, GraphicsContext3D::DataFormat dstFormat, GraphicsContext3D::AlphaOp);
    bool success() const { return m_success; }

private:
    template<GraphicsContext3D::DataFormat SrcFormat>
    ALWAYS_INLINE void convert(GraphicsContext3D::DataFormat dstFormat, GraphicsContext3D::AlphaOp);

    template<GraphicsContext3D::DataFormat SrcFormat, GraphicsContext3D::DataFormat DstFormat>
    ALWAYS_INLINE void convert(GraphicsContext3D::AlphaOp);

    template<GraphicsContext3D::DataFormat SrcFormat, GraphicsContext3D::DataFormat DstFormat, GraphicsContext3D::AlphaOp alphaOp>
    ALWAYS_INLINE void convert();

    const unsigned m_width, m_height;
    const void* const m_srcStart;
    void* const m_dstStart;
    const int m_srcStride, m_dstStride;
    bool m_success;
    std::unique_ptr<uint8_t[]> m_unpackedIntermediateSrcData;
};

} // namespace WebCore

#endif // USE(3D_GRAPHICS)
