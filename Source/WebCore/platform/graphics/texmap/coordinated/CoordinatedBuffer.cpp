/*
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CoordinatedBuffer.h"

#if USE(COORDINATED_GRAPHICS)

#include "ImageBuffer.h"
#include "IntRect.h"

namespace WebCore {

Ref<CoordinatedBuffer> CoordinatedBuffer::create(const IntSize& size, Flags flags)
{
    return adoptRef(*new CoordinatedBuffer(size, flags));
}

CoordinatedBuffer::CoordinatedBuffer(const IntSize& size, Flags flags)
    : m_imageBuffer(ImageBuffer::create(size, Unaccelerated))
    , m_size(size)
    , m_flags(flags)
{
}

CoordinatedBuffer::~CoordinatedBuffer() = default;

void CoordinatedBuffer::paintToSurface(const IntRect& rect, Client& client)
{
    GraphicsContext& context = m_imageBuffer->context();
    context.save();
    context.clip(rect);
    context.translate(rect.x(), rect.y());
    client.paintToSurfaceContext(context);
    context.restore();
}

RefPtr<Image> CoordinatedBuffer::uploadImage()
{
    return m_imageBuffer->copyImage(DontCopyBackingStore);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
