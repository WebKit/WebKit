/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#if ENABLE(3D_CANVAS)

#include "Extensions3DChromium.h"

#include "GraphicsContext3D.h"
#include "GraphicsContext3DInternal.h"

namespace WebCore {

Extensions3DChromium::Extensions3DChromium(GraphicsContext3DInternal* internal)
    : m_internal(internal)
{
}

Extensions3DChromium::~Extensions3DChromium()
{
}

bool Extensions3DChromium::supports(const String& name)
{
    return m_internal->supportsExtension(name);
}

int Extensions3DChromium::getGraphicsResetStatusARB()
{
    return m_internal->isContextLost() ? static_cast<int>(Extensions3D::UNKNOWN_CONTEXT_RESET_ARB) : static_cast<int>(GraphicsContext3D::NO_ERROR);
}

void* Extensions3DChromium::mapBufferSubDataCHROMIUM(unsigned target, int offset, int size, unsigned access)
{
    return m_internal->mapBufferSubDataCHROMIUM(target, offset, size, access);
}

void Extensions3DChromium::unmapBufferSubDataCHROMIUM(const void* data)
{
    m_internal->unmapBufferSubDataCHROMIUM(data);
}

void* Extensions3DChromium::mapTexSubImage2DCHROMIUM(unsigned target, int level, int xoffset, int yoffset, int width, int height, unsigned format, unsigned type, unsigned access)
{
    return m_internal->mapTexSubImage2DCHROMIUM(target, level, xoffset, yoffset, width, height, format, type, access);
}

void Extensions3DChromium::unmapTexSubImage2DCHROMIUM(const void* data)
{
    m_internal->unmapTexSubImage2DCHROMIUM(data);
}

void Extensions3DChromium::copyTextureToParentTextureCHROMIUM(unsigned texture, unsigned parentTexture)
{
    m_internal->copyTextureToParentTextureCHROMIUM(texture, parentTexture);
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
