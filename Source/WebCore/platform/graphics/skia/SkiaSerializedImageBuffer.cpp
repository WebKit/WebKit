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
    : m_memoryCost(imageBuffer.memoryCost())
{
    // Non accelerated ImageBuffer can be transferred to other threads.
    if (imageBuffer.renderingMode() != RenderingMode::Accelerated) {
        m_imageBuffer = imageBuffer;
        return;
    }

    // Accelerated ImageBuffer can't be transferred to other threads, so
    // we take an image snapshot and the parameters needed to create a
    // new ImageBuffer to draw the image snapshot into.
    imageBuffer.flushDrawingContext();
    m_image = imageBuffer.createNativeImageReference();
    m_logicalSize = imageBuffer.logicalSize();
    m_resolutionScale = imageBuffer.resolutionScale();
    m_colorSpace = imageBuffer.colorSpace();
    m_bufferFormat = { imageBuffer.pixelFormat() };
}

SkiaSerializedImageBuffer::~SkiaSerializedImageBuffer() = default;

RefPtr<ImageBuffer> SkiaSerializedImageBuffer::sinkIntoImageBuffer()
{
    if (m_imageBuffer)
        return m_imageBuffer;

    ASSERT(m_image);

    if (!PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
        return nullptr;

    auto copiedImageBuffer = ImageBuffer::create(m_logicalSize, RenderingMode::Accelerated, RenderingPurpose::Unspecified, m_resolutionScale, m_colorSpace, m_bufferFormat);
    if (!copiedImageBuffer)
        return nullptr;

    FloatRect destination({ }, m_logicalSize);
    FloatRect source = destination;
    source.scale(m_resolutionScale);
    copiedImageBuffer->context().drawNativeImage(*m_image, destination, source, { CompositeOperator::Copy });
    return copiedImageBuffer;
}

size_t SkiaSerializedImageBuffer::memoryCost() const
{
    return m_memoryCost;
}

} // namespace WebCore

#endif // USE(SKIA)
