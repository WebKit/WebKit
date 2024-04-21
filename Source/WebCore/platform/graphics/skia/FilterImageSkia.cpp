/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "FilterImage.h"

#if USE(SKIA)

#include "ImageBufferSkiaAcceleratedBackend.h"
#include "ImageBufferSkiaUnacceleratedBackend.h"
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkPicture.h>

namespace WebCore {


SkCanvas* FilterImage::beginRecording()
{
    ASSERT(!m_pictureRecorder.getRecordingCanvas());

    return m_pictureRecorder.beginRecording(SkRect(m_absoluteImageRect.width(), m_absoluteImageRect.height()));
}

void FilterImage::finishRecording()
{
    ASSERT(m_pictureRecorder.getRecordingCanvas());

    m_skPicture = m_pictureRecorder.finishRecordingAsPicture();
}

size_t FilterImage::memoryCostOfSkPicture() const
{
    ASSERT(m_skPicture);
    return m_skPicture->approximateBytesUsed();
}

ImageBuffer* FilterImage::imageBufferFromSkPicture()
{
    ASSERT(m_skPicture);

    if (m_imageBuffer)
        return m_imageBuffer.get();

    if (m_renderingMode == RenderingMode::Accelerated)
        m_imageBuffer = ImageBuffer::create<ImageBufferSkiaAcceleratedBackend>(m_absoluteImageRect.size(), 1, m_colorSpace, PixelFormat::BGRA8, RenderingPurpose::Unspecified, { });

    if (!m_imageBuffer)
        m_imageBuffer = ImageBuffer::create<ImageBufferSkiaUnacceleratedBackend>(m_absoluteImageRect.size(), 1, m_colorSpace, PixelFormat::BGRA8, RenderingPurpose::Unspecified, { });

    if (!m_imageBuffer)
        return nullptr;

    m_imageBuffer->context().platformContext()->drawPicture(m_skPicture);

    return m_imageBuffer.get();
}

} // namespace WebCore

#endif // USE(SKIA)
