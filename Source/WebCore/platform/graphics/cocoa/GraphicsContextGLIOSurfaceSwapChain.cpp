/*
 * Copyright (C) 2009, 2014-2019 Apple Inc. All rights reserved.
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
#include "GraphicsContextGLIOSurfaceSwapChain.h"

#if ENABLE(WEBGL) && PLATFORM(COCOA)

namespace WebCore {

GraphicsContextGLIOSurfaceSwapChain::~GraphicsContextGLIOSurfaceSwapChain() = default;

const GraphicsContextGLIOSurfaceSwapChain::Buffer& GraphicsContextGLIOSurfaceSwapChain::displayBuffer() const
{
    return m_displayBuffer;
}

void GraphicsContextGLIOSurfaceSwapChain::markDisplayBufferInUse()
{
    m_displayBufferInUse = true;
}

GraphicsContextGLIOSurfaceSwapChain::Buffer GraphicsContextGLIOSurfaceSwapChain::recycleBuffer()
{
    if (m_spareBuffer.surface) {
        if (m_spareBuffer.surface->isInUse())
            m_spareBuffer.surface.reset();
    }
    return std::exchange(m_spareBuffer, { });
}

void* GraphicsContextGLIOSurfaceSwapChain::detachClient()
{
    ASSERT(!m_spareBuffer.surface);
    return std::exchange(m_displayBuffer.handle, nullptr);
}

void GraphicsContextGLIOSurfaceSwapChain::present(Buffer&& buffer)
{
    ASSERT(!m_spareBuffer.surface);
    ASSERT(!m_spareBuffer.handle);
    m_spareBuffer = std::exchange(m_displayBuffer, WTFMove(buffer));
    if (m_displayBufferInUse) {
        m_displayBufferInUse = false;
        m_spareBuffer.surface.reset();
    }
}

}

#endif
