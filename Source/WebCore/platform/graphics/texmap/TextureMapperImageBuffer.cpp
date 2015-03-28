/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "TextureMapperImageBuffer.h"

#include "BitmapTexturePool.h"
#include "GraphicsLayer.h"
#include "NotImplemented.h"

#if USE(TEXTURE_MAPPER)
namespace WebCore {

static const int s_maximumAllowedImageBufferDimension = 4096;

TextureMapperImageBuffer::TextureMapperImageBuffer()
    : TextureMapper(SoftwareMode)
{
    m_texturePool = std::make_unique<BitmapTexturePool>();
}

IntSize TextureMapperImageBuffer::maxTextureSize() const
{
    return IntSize(s_maximumAllowedImageBufferDimension, s_maximumAllowedImageBufferDimension);
}

void TextureMapperImageBuffer::beginClip(const TransformationMatrix& matrix, const FloatRect& rect)
{
    GraphicsContext* context = currentContext();
    if (!context)
        return;
#if ENABLE(3D_RENDERING)
    TransformationMatrix previousTransform = context->get3DTransform();
#else
    AffineTransform previousTransform = context->getCTM();
#endif
    context->save();

#if ENABLE(3D_RENDERING)
    context->concat3DTransform(matrix);
#else
    context->concatCTM(matrix.toAffineTransform());
#endif

    context->clip(rect);

#if ENABLE(3D_RENDERING)
    context->set3DTransform(previousTransform);
#else
    context->setCTM(previousTransform);
#endif
}

void TextureMapperImageBuffer::drawTexture(const BitmapTexture& texture, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity, unsigned /* exposedEdges */)
{
    GraphicsContext* context = currentContext();
    if (!context)
        return;

    const BitmapTextureImageBuffer& textureImageBuffer = static_cast<const BitmapTextureImageBuffer&>(texture);
    ImageBuffer* image = textureImageBuffer.image();
    context->save();
    context->setCompositeOperation(isInMaskMode() ? CompositeDestinationIn : CompositeSourceOver);
    context->setAlpha(opacity);
#if ENABLE(3D_RENDERING)
    context->concat3DTransform(matrix);
#else
    context->concatCTM(matrix.toAffineTransform());
#endif
    context->drawImageBuffer(image, ColorSpaceDeviceRGB, targetRect);
    context->restore();
}

void TextureMapperImageBuffer::drawSolidColor(const FloatRect& rect, const TransformationMatrix& matrix, const Color& color)
{
    GraphicsContext* context = currentContext();
    if (!context)
        return;

    context->save();
    context->setCompositeOperation(isInMaskMode() ? CompositeDestinationIn : CompositeSourceOver);
#if ENABLE(3D_RENDERING)
    context->concat3DTransform(matrix);
#else
    context->concatCTM(matrix.toAffineTransform());
#endif

    context->fillRect(rect, color, ColorSpaceDeviceRGB);
    context->restore();
}

void TextureMapperImageBuffer::drawBorder(const Color&, float /* borderWidth */, const FloatRect&, const TransformationMatrix&)
{
    notImplemented();
}

void TextureMapperImageBuffer::drawNumber(int /* number */, const Color&, const FloatPoint&, const TransformationMatrix&)
{
    notImplemented();
}

PassRefPtr<BitmapTexture> BitmapTextureImageBuffer::applyFilters(TextureMapper*, const FilterOperations&)
{
    ASSERT_NOT_REACHED();
    return this;
}

}
#endif
