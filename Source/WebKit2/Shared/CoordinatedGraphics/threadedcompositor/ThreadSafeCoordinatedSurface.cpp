/*
 * Copyright (C) 2014 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(COORDINATED_GRAPHICS)
#include "ThreadSafeCoordinatedSurface.h"

#include <WebCore/TextureMapperGL.h>
#include <wtf/StdLibExtras.h>

using namespace WebCore;

namespace WebKit {

Ref<ThreadSafeCoordinatedSurface> ThreadSafeCoordinatedSurface::create(const IntSize& size, CoordinatedSurface::Flags flags)
{
    // Making an unconditionally unaccelerated buffer here is OK because this code
    // isn't used by any platforms that respect the accelerated bit.
    return adoptRef(*new ThreadSafeCoordinatedSurface(size, flags, ImageBuffer::create(size, Unaccelerated)));
}

Ref<ThreadSafeCoordinatedSurface> ThreadSafeCoordinatedSurface::create(const IntSize& size, CoordinatedSurface::Flags flags, std::unique_ptr<ImageBuffer> buffer)
{
    return adoptRef(*new ThreadSafeCoordinatedSurface(size, flags, WTFMove(buffer)));
}

ThreadSafeCoordinatedSurface::ThreadSafeCoordinatedSurface(const IntSize& size, CoordinatedSurface::Flags flags, std::unique_ptr<ImageBuffer> buffer)
    : CoordinatedSurface(size, flags)
    , m_imageBuffer(WTFMove(buffer))
{
}

ThreadSafeCoordinatedSurface::~ThreadSafeCoordinatedSurface()
{
}

void ThreadSafeCoordinatedSurface::paintToSurface(const IntRect& rect, CoordinatedSurface::Client* client)
{
    ASSERT(client);

    GraphicsContext& context = beginPaint(rect);
    client->paintToSurfaceContext(context);
    endPaint();
}

GraphicsContext& ThreadSafeCoordinatedSurface::beginPaint(const IntRect& rect)
{
    ASSERT(m_imageBuffer);
    GraphicsContext& graphicsContext = m_imageBuffer->context();
    graphicsContext.save();
    graphicsContext.clip(rect);
    graphicsContext.translate(rect.x(), rect.y());
    return graphicsContext;
}

void ThreadSafeCoordinatedSurface::endPaint()
{
    ASSERT(m_imageBuffer);
    m_imageBuffer->context().restore();
}

void ThreadSafeCoordinatedSurface::copyToTexture(PassRefPtr<BitmapTexture> passTexture, const IntRect& target, const IntPoint& sourceOffset)
{
    RefPtr<BitmapTexture> texture(passTexture);

    ASSERT(m_imageBuffer);
    RefPtr<Image> image = m_imageBuffer->copyImage(DontCopyBackingStore);
    texture->updateContents(image.get(), target, sourceOffset, BitmapTexture::UpdateCanModifyOriginalImageData);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
