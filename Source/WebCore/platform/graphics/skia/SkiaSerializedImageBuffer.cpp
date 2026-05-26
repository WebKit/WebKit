/*
 * Copyright (C) 2026 Igalia S.L.
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
#include "SkiaSerializedImageBuffer.h"

#if USE(SKIA)
#include "GLContext.h"
#include "GraphicsContext.h"
#include "NativeImage.h"
#include "PlatformDisplay.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SkiaSerializedImageBuffer);

SkiaSerializedImageBuffer::SkiaSerializedImageBuffer(ImageBuffer& imageBuffer)
    : m_imageBuffer(imageBuffer)
{
    if (m_imageBuffer->renderingMode() != RenderingMode::Accelerated)
        return;

    m_imageBuffer->flushDrawingContext();
    m_image = m_imageBuffer->createNativeImageReference();
}

SkiaSerializedImageBuffer::~SkiaSerializedImageBuffer() = default;

RefPtr<ImageBuffer> SkiaSerializedImageBuffer::sinkIntoImageBuffer()
{
    if (!m_image)
        return m_imageBuffer.get();

    if (!PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
        return nullptr;

    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    if (grContext == m_image->grContext())
        return m_imageBuffer.get();

    auto copiedImageBuffer = m_imageBuffer->context().createImageBuffer(m_imageBuffer->logicalSize(), m_imageBuffer->resolutionScale(),
        m_imageBuffer->colorSpace(), RenderingMode::Accelerated, std::nullopt, { m_imageBuffer->pixelFormat() });
    if (!copiedImageBuffer)
        return nullptr;

    FloatRect destination({ }, m_imageBuffer->logicalSize());
    FloatRect source = destination;
    source.scale(m_imageBuffer->resolutionScale());
    copiedImageBuffer->context().drawNativeImage(*m_image, destination, source, { CompositeOperator::Copy });
    return copiedImageBuffer;
}

size_t SkiaSerializedImageBuffer::memoryCost() const
{
    return m_imageBuffer->memoryCost();
}

} // namespace WebCore

#endif // USE(SKIA)
