/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if HAVE(IOSURFACE)
#include "IOSurfaceDrawingBuffer.h"

#include <CoreGraphics/CoreGraphics.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>

#include <pal/cg/CoreGraphicsSoftLink.h>

namespace WebCore {

RefPtr<NativeImage> IOSurfaceDrawingBuffer::copyNativeImage() const
{
    if (!m_surface)
        return nullptr;
    if (!m_copyOnWriteContext) {
        m_copyOnWriteContext = m_surface->createPlatformContext();
        if (!m_copyOnWriteContext)
            return nullptr;
    }
    m_needCopy = true;
    return NativeImage::create(m_surface->createImage(m_copyOnWriteContext.get()));
}

void IOSurfaceDrawingBuffer::forceCopy()
{
    m_needCopy = false;
    // See https://webkit.org/b/157966 and https://webkit.org/b/228682 for more context.
    if (PAL::canLoad_CoreGraphics_CGIOSurfaceContextInvalidateSurface())
        PAL::softLink_CoreGraphics_CGIOSurfaceContextInvalidateSurface(m_copyOnWriteContext.get());
    else
        CGContextFillRect(m_copyOnWriteContext.get(), CGRect { });
}

}

#endif
