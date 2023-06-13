/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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
#include "ImageBufferCGBackend.h"

#if USE(CG)

#include "BitmapImage.h"
#include "GraphicsContextCG.h"
#include "ImageBufferUtilitiesCG.h"
#include "IntRect.h"
#include "PixelBuffer.h"
#include "RuntimeApplicationChecks.h"
#include <CoreGraphics/CoreGraphics.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

class ThreadSafeImageBufferFlusherCG : public ThreadSafeImageBufferFlusher {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ThreadSafeImageBufferFlusherCG(CGContextRef context)
        : m_context(context)
    {
    }

    void flush() override
    {
        CGContextFlush(m_context.get());
    }

private:
    RetainPtr<CGContextRef> m_context;
};

unsigned ImageBufferCGBackend::calculateBytesPerRow(const IntSize& backendSize)
{
    ASSERT(!backendSize.isEmpty());
    return CheckedUint32(backendSize.width()) * 4;
}

std::unique_ptr<ThreadSafeImageBufferFlusher> ImageBufferCGBackend::createFlusher()
{
    return makeUnique<ThreadSafeImageBufferFlusherCG>(context().platformContext());
}

ImageBufferCGBackend::~ImageBufferCGBackend() = default;

bool ImageBufferCGBackend::originAtBottomLeftCorner() const
{
    return isOriginAtBottomLeftCorner;
}

void ImageBufferCGBackend::applyBaseTransform(GraphicsContextCG& context) const
{
    context.applyDeviceScaleFactor(m_parameters.resolutionScale);
    context.setCTM(calculateBaseTransform(m_parameters, originAtBottomLeftCorner()));
}

String ImageBufferCGBackend::debugDescription() const
{
    TextStream stream;
    stream << "ImageBufferCGBackend " << this;
    return stream.release();
}

} // namespace WebCore

#endif // USE(CG)
