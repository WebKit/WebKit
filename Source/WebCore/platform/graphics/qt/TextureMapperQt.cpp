/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

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
#include "TextureMapperQt.h"

#include <QtCore/qdebug.h>
#include <QtGui/qpaintengine.h>
#include <QtGui/qpixmap.h>

#if USE(TEXTURE_MAPPER_GL)
# include "opengl/TextureMapperGL.h"
#endif

namespace WebCore {

void BitmapTextureQt::destroy()
{
    if (m_pixmap.paintingActive())
        qFatal("Destroying an active pixmap");
    m_pixmap = QPixmap();
}

void BitmapTextureQt::reset(const IntSize& size, bool isOpaque)
{
    BitmapTexture::reset(size, isOpaque);

    if (size.width() > m_pixmap.size().width() || size.height() > m_pixmap.size().height() || m_pixmap.isNull())
        m_pixmap = QPixmap(size.width(), size.height());
    if (!isOpaque)
        m_pixmap.fill(Qt::transparent);
}

PlatformGraphicsContext* BitmapTextureQt::beginPaint(const IntRect& dirtyRect)
{
    m_painter.begin(&m_pixmap);
    TextureMapperQt::initialize(&m_painter);
    m_painter.setCompositionMode(QPainter::CompositionMode_Clear);
    m_painter.fillRect(QRect(dirtyRect), Qt::transparent);
    m_painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    return &m_painter;
}

void BitmapTextureQt::endPaint()
{
    m_painter.end();
}

void BitmapTextureQt::updateContents(PixelFormat pixelFormat, const IntRect& rect, void* bits)
{
    m_painter.begin(&m_pixmap);
    QImage::Format qtFormat = QImage::Format_ARGB32_Premultiplied;
    if (pixelFormat == BGRFormat || pixelFormat == RGBFormat)
        qtFormat = QImage::Format_RGB32;
    QImage image(static_cast<uchar*>(bits), rect.width(), rect.height(), qtFormat);
    if (pixelFormat == BGRFormat || pixelFormat == BGRAFormat)
        image = image.rgbSwapped();
    m_painter.setCompositionMode(QPainter::CompositionMode_Source);
    m_painter.drawImage(rect, image);
    m_painter.end();
}

bool BitmapTextureQt::save(const String& path)
{
    return m_pixmap.save(path, "PNG");
}

void BitmapTextureQt::setContentsToImage(Image* image)
{
    if (!image)
        return;
    const QPixmap* pixmap = image->nativeImageForCurrentFrame();
    if (!pixmap)
        return;
    BitmapTexture::reset(pixmap->size(), !pixmap->hasAlphaChannel());
    m_pixmap = *pixmap;
}

void BitmapTextureQt::pack()
{
    if (m_pixmap.isNull())
        return;

    m_image = m_pixmap.toImage();
    m_pixmap = QPixmap();
    m_isPacked = true;
}

void BitmapTextureQt::unpack()
{
    m_isPacked = false;
    if (m_image.isNull())
        return;

    m_pixmap = QPixmap::fromImage(m_image);
    m_image = QImage();
}

void TextureMapperQt::beginClip(const TransformationMatrix& matrix, const FloatRect& rect)
{
     QPainter* painter = currentPainter();
     painter->save();
     QTransform prevTransform = painter->transform();
     painter->setTransform(matrix, false);
     painter->setClipRect(rect);
     painter->setTransform(prevTransform, false);
}

void TextureMapperQt::endClip()
{
    currentPainter()->restore();
}

IntSize TextureMapperQt::viewportSize() const
{
    return IntSize(m_painter->device()->width(), m_painter->device()->height());
}


TextureMapperQt::TextureMapperQt()
    : m_currentSurface(0)
{
}

void TextureMapperQt::setGraphicsContext(GraphicsContext* context)
{
    m_context = context;
    m_painter = context ? context->platformContext() : 0;
    initialize(m_painter);
}

GraphicsContext* TextureMapperQt::graphicsContext()
{
    return m_context;
}

void TextureMapperQt::bindSurface(BitmapTexture* surface)
{
    if (m_currentSurface == surface)
        return;
    if (m_currentSurface)
        m_currentSurface->m_painter.end();
    if (!surface) {
        m_currentSurface = 0;
        return;
    }
    BitmapTextureQt* surfaceQt = static_cast<BitmapTextureQt*>(surface);
    if (!surfaceQt->m_painter.isActive())
        surfaceQt->m_painter.begin(&surfaceQt->m_pixmap);
    m_currentSurface = surfaceQt;
}


void TextureMapperQt::drawTexture(const BitmapTexture& texture, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity, const BitmapTexture* maskTexture)
{
    const BitmapTextureQt& textureQt = static_cast<const BitmapTextureQt&>(texture);
    QPainter* painter = m_painter;
    QPixmap pixmap = textureQt.m_pixmap;
    if (m_currentSurface)
        painter = &m_currentSurface->m_painter;

    if (maskTexture && maskTexture->isValid()) {
        const BitmapTextureQt* mask = static_cast<const BitmapTextureQt*>(maskTexture);
        QPixmap intermediatePixmap(pixmap.size());
        intermediatePixmap.fill(Qt::transparent);
        QPainter maskPainter(&intermediatePixmap);
        maskPainter.setCompositionMode(QPainter::CompositionMode_Source);
        maskPainter.drawPixmap(0, 0, pixmap);
        maskPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        maskPainter.drawPixmap(QRect(0, 0, pixmap.width(), pixmap.height()), mask->m_pixmap, mask->sourceRect());
        maskPainter.end();
        pixmap = intermediatePixmap;
    }

    const qreal prevOpacity = painter->opacity();
    const QTransform prevTransform = painter->transform();
    painter->setOpacity(opacity);
    painter->setTransform(matrix, true);
    painter->drawPixmap(targetRect, pixmap, FloatRect(textureQt.sourceRect()));
    painter->setTransform(prevTransform);
    painter->setOpacity(prevOpacity);
}

PassOwnPtr<TextureMapper> TextureMapper::create(GraphicsContext* context)
{
#if USE(TEXTURE_MAPPER_GL)
    if (context && context->platformContext()->paintEngine()->type() == QPaintEngine::OpenGL2)
        return adoptPtr(new TextureMapperGL);
#endif
    return adoptPtr(new TextureMapperQt);
}

PassRefPtr<BitmapTexture> TextureMapperQt::createTexture()
{
    return adoptRef(new BitmapTextureQt());
}

BitmapTextureQt::BitmapTextureQt()
    : m_isPacked(false)
{

}

void TextureMapperQt::beginPainting()
{
    m_painter->save();
}

void TextureMapperQt::endPainting()
{
    m_painter->restore();
}

#if USE(TEXTURE_MAPPER_GL)
class BGRA32PremultimpliedBufferQt : public BGRA32PremultimpliedBuffer {
public:
    virtual PlatformGraphicsContext* beginPaint(const IntRect& rect, bool opaque)
    {
        // m_image is only using during paint, it's safe to override it.
        m_image = QImage(rect.size().width(), rect.size().height(), opaque ? QImage::Format_RGB32 : QImage::Format_ARGB32_Premultiplied);
        if (!opaque)
            m_image.fill(0);
        m_painter.begin(&m_image);
        TextureMapperQt::initialize(&m_painter);
        m_painter.translate(-rect.x(), -rect.y());
        return &m_painter;
    }

    virtual void endPaint() { m_painter.end(); }
    virtual void* data() { return m_image.bits(); }

private:
    QPainter m_painter;
    QImage m_image;
};

PassOwnPtr<BGRA32PremultimpliedBuffer> BGRA32PremultimpliedBuffer::create()
{
    return adoptPtr(new BGRA32PremultimpliedBufferQt());
}

uint64_t uidForImage(Image* image)
{
    return image->nativeImageForCurrentFrame()->serialNumber();
}
#endif
};
