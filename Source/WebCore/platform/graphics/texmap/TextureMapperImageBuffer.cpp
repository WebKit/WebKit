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

#if USE(TEXTURE_MAPPER)
namespace WebCore {

void BitmapTextureImageBuffer::updateContents(const void* data, const IntRect& targetRect)
{
#if PLATFORM(QT)
    QImage image(reinterpret_cast<const uchar*>(data), targetRect.width(), targetRect.height(), targetRect.width() * 4, QImage::Format_ARGB32_Premultiplied);
    QPainter* painter = m_image->context()->platformContext();
    painter->save();
    painter->setCompositionMode(QPainter::CompositionMode_Source);
    painter->drawImage(targetRect, image);
    painter->restore();
#endif
}

void BitmapTextureImageBuffer::updateContents(Image* image, const IntRect& targetRect, const IntRect& sourceRect, PixelFormat)
{
    m_image->context()->drawImage(image, ColorSpaceDeviceRGB, targetRect, sourceRect, CompositeCopy);
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

void TextureMapperImageBuffer::drawTexture(const BitmapTexture& texture, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity, const BitmapTexture* maskTexture)
{
    GraphicsContext* context = currentContext();
    if (!context)
        return;

    const BitmapTextureImageBuffer& textureImageBuffer = static_cast<const BitmapTextureImageBuffer&>(texture);
    ImageBuffer* image = textureImageBuffer.m_image.get();
    OwnPtr<ImageBuffer> maskedImage;

    if (maskTexture && maskTexture->isValid()) {
        const BitmapTextureImageBuffer* mask = static_cast<const BitmapTextureImageBuffer*>(maskTexture);
        maskedImage = ImageBuffer::create(maskTexture->contentSize());
        GraphicsContext* maskContext = maskedImage->context();
        maskContext->drawImageBuffer(image, ColorSpaceDeviceRGB, IntPoint::zero(), CompositeCopy);
        if (opacity < 1) {
            maskContext->setAlpha(opacity);
            opacity = 1;
        }
        maskContext->drawImageBuffer(mask->m_image.get(), ColorSpaceDeviceRGB, IntPoint::zero(), CompositeDestinationIn);
        image = maskedImage.get();
    }

    context->save();
    context->setAlpha(opacity);
#if ENABLE(3D_RENDERING)
    context->concat3DTransform(matrix);
#else
    context->concatCTM(matrix.toAffineTransform());
#endif
    context->drawImageBuffer(image, ColorSpaceDeviceRGB, targetRect);
    context->restore();
}

}
#endif
