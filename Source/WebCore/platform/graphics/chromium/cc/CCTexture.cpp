/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "CCTexture.h"

namespace WebCore {

void CCTexture::setDimensions(const IntSize& size, GC3Denum format)
{
    m_size = size;
    m_format = format;
}

size_t CCTexture::bytes() const
{
    if (m_size.isEmpty())
        return 0u;

    return memorySizeBytes(m_size, m_format);
}

size_t CCTexture::memorySizeBytes(const IntSize& size, GC3Denum format)
{
    unsigned int componentsPerPixel;
    unsigned int bytesPerComponent;
    if (!GraphicsContext3D::computeFormatAndTypeParameters(format, GraphicsContext3D::UNSIGNED_BYTE, &componentsPerPixel, &bytesPerComponent)) {
        ASSERT_NOT_REACHED();
        return 0u;
    }
    return componentsPerPixel * bytesPerComponent * size.width() * size.height();
}

}
