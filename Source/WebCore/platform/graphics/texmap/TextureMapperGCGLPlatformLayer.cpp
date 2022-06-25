/*
 * Copyright (C) 2011, 2012, 2017 Igalia S.L.
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "config.h"
#include "TextureMapperGCGLPlatformLayer.h"

#if ENABLE(WEBGL) && USE(TEXTURE_MAPPER) && !USE(NICOSIA)

#include "ANGLEHeaders.h"
#include "BitmapTextureGL.h"
#include "GLContext.h"
#include "TextureMapperGLHeaders.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxy.h"

namespace WebCore {

TextureMapperGCGLPlatformLayer::TextureMapperGCGLPlatformLayer(GraphicsContextGLTextureMapperANGLE& context)
    : m_context(context)
{
}

TextureMapperGCGLPlatformLayer::~TextureMapperGCGLPlatformLayer()
{
    if (client())
        client()->platformLayerWillBeDestroyed();
}

void TextureMapperGCGLPlatformLayer::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity)
{
    auto attrs = m_context.contextAttributes();
    TextureMapperGL& texmapGL = static_cast<TextureMapperGL&>(textureMapper);
    TextureMapperGL::Flags flags = TextureMapperGL::ShouldFlipTexture | (attrs.alpha ? TextureMapperGL::ShouldBlend : 0);
    IntSize textureSize(m_context.m_currentWidth, m_context.m_currentHeight);
    texmapGL.drawTexture(m_context.m_compositorTexture, flags, textureSize, targetRect, matrix, opacity);
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(TEXTURE_MAPPER)
