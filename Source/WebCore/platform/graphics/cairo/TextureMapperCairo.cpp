/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "TextureMapperCairo.h"

#include "CairoUtilities.h"
#include "NotImplemented.h"

namespace WebCore {

void BitmapTextureCairo::destroy()
{
    if (m_context)
        delete m_context;
}


IntSize BitmapTextureCairo::size() const
{
    notImplemented();
    return IntSize();
}

void BitmapTextureCairo::reset(const IntSize& size, bool isOpaque)
{
    notImplemented();
}

PlatformGraphicsContext* BitmapTextureCairo::beginPaint(const IntRect& dirtyRect)
{
    notImplemented();
    return m_context;
}

void BitmapTextureCairo::endPaint()
{
    notImplemented();
}

void BitmapTextureCairo::updateContents(PixelFormat pixelFormat, const IntRect& rect, void* bits)
{
    notImplemented();
}


bool BitmapTextureCairo::save(const String& path)
{
    notImplemented();
    return false;
}

void BitmapTextureCairo::setContentsToImage(Image* image)
{
    notImplemented();
}

void TextureMapperCairo::beginClip(const TransformationMatrix& matrix, const FloatRect& rect)
{
    notImplemented();
}

void TextureMapperCairo::endClip()
{
    notImplemented();
}

IntSize TextureMapperCairo::viewportSize() const
{
    notImplemented();
}


TextureMapperCairo::TextureMapperCairo()
    : m_context(0)
{
}

void TextureMapperCairo::setGraphicsContext(GraphicsContext* context)
{
    m_context = context;
}

GraphicsContext* TextureMapperCairo::graphicsContext()
{
    return m_context;
}

void TextureMapperCairo::bindSurface(BitmapTexture* texture)
{
    notImplemented();
}


void TextureMapperCairo::drawTexture(const BitmapTexture& texture, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity, const BitmapTexture* maskTexture)
{
    notImplemented();
}

PassOwnPtr<TextureMapper> TextureMapper::create(GraphicsContext* context)
{
    return adoptPtr(new TextureMapperCairo);
}

PassRefPtr<BitmapTexture> TextureMapperCairo::createTexture()
{
    return adoptRef(new BitmapTextureCairo());
}

BitmapTextureCairo::BitmapTextureCairo()
    : m_context(0)
    , m_surface(0)
{
}

void TextureMapperCairo::beginPainting()
{
    notImplemented();
}

void TextureMapperCairo::endPainting()
{
    notImplemented();
}

};
