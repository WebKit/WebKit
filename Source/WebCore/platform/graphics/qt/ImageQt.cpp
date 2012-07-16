/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2010 Sencha, Inc.
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Image.h"

#include "AffineTransform.h"
#include "BitmapImage.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageObserver.h"
#include "NativeImageQt.h"
#include "PlatformString.h"
#include "ShadowBlur.h"
#include "StillImageQt.h"

#include <QCoreApplication>
#include <QDebug>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPixmap>
#include <QTransform>

#include <math.h>

#if OS(WINDOWS) && HAVE(QT5)
Q_GUI_EXPORT QPixmap qt_pixmapFromWinHBITMAP(HBITMAP, int hbitmapFormat = 0);
#endif

typedef QHash<QByteArray, QImage> WebGraphicHash;
Q_GLOBAL_STATIC(WebGraphicHash, _graphics)

static void earlyClearGraphics()
{
    _graphics()->clear();
}

static WebGraphicHash* graphics()
{
    WebGraphicHash* hash = _graphics();

    if (hash->isEmpty()) {

        // prevent ~QImage running after ~QApplication (leaks native images)
        qAddPostRoutine(earlyClearGraphics);

        // QWebSettings::MissingImageGraphic
        hash->insert("missingImage", QImage(QLatin1String(":webkit/resources/missingImage.png")));
        // QWebSettings::MissingPluginGraphic
        hash->insert("nullPlugin", QImage(QLatin1String(":webkit/resources/nullPlugin.png")));
        // QWebSettings::DefaultFrameIconGraphic
        hash->insert("urlIcon", QImage(QLatin1String(":webkit/resources/urlIcon.png")));
        // QWebSettings::TextAreaSizeGripCornerGraphic
        hash->insert("textAreaResizeCorner", QImage(QLatin1String(":webkit/resources/textAreaResizeCorner.png")));
        // QWebSettings::DeleteButtonGraphic
        hash->insert("deleteButton", QImage(QLatin1String(":webkit/resources/deleteButton.png")));
        // QWebSettings::InputSpeechButtonGraphic
        hash->insert("inputSpeech", QImage(QLatin1String(":webkit/resources/inputSpeech.png")));
    }

    return hash;
}

// This function loads resources into WebKit
static QImage loadResourceImage(const char *name)
{
    return graphics()->value(name);
}

namespace WebCore {

bool FrameData::clear(bool clearMetadata)
{
    if (clearMetadata)
        m_haveMetadata = false;

    if (m_frame) {
        delete m_frame;
        m_frame = 0;
        return true;
    }
    return false;
}


// ================================================
// Image Class
// ================================================

PassRefPtr<Image> Image::loadPlatformResource(const char* name)
{
    return StillImage::create(loadResourceImage(name));
}

void Image::setPlatformResource(const char* name, const QImage& image)
{
    WebGraphicHash* h = graphics();
    if (image.isNull())
        h->remove(name);
    else
        h->insert(name, image);
}

void Image::drawPattern(GraphicsContext* ctxt, const FloatRect& tileRect, const AffineTransform& patternTransform,
                        const FloatPoint& phase, ColorSpace, CompositeOperator op, const FloatRect& destRect)
{
    QImage* frameImage = nativeImageForCurrentFrame();
    if (!frameImage) // If it's too early we won't have an image yet.
        return;

#if ENABLE(IMAGE_DECODER_DOWN_SAMPLING)
    FloatRect tileRectAdjusted = adjustSourceRectForDownSampling(tileRect, framePixmap->size());
#else
    FloatRect tileRectAdjusted = tileRect;
#endif

    // Qt interprets 0 width/height as full width/height so just short circuit.
    QRectF dr = QRectF(destRect).normalized();
    QRect tr = QRectF(tileRectAdjusted).toRect().normalized();
    if (!dr.width() || !dr.height() || !tr.width() || !tr.height())
        return;

    QImage image = *frameImage;
    if (tr.x() || tr.y() || tr.width() != image.width() || tr.height() != image.height())
        image = image.copy(tr);

    CompositeOperator previousOperator = ctxt->compositeOperation();

    ctxt->setCompositeOperation(!image.hasAlphaChannel() && op == CompositeSourceOver ? CompositeCopy : op);

    QPainter* p = ctxt->platformContext();
    QTransform transform(patternTransform);

    // If this would draw more than one scaled tile, we scale the image first and then use the result to draw.
    if (transform.type() == QTransform::TxScale) {
        QRectF tileRectInTargetCoords = (transform * QTransform().translate(phase.x(), phase.y())).mapRect(tr);

        bool tileWillBePaintedOnlyOnce = tileRectInTargetCoords.contains(dr);
        if (!tileWillBePaintedOnlyOnce) {
            QSizeF scaledSize(float(image.width()) * transform.m11(), float(image.height()) * transform.m22());
            QImage scaledImage;
            if (image.hasAlphaChannel()) {
                scaledImage = QImage(scaledSize.toSize(), NativeImageQt::defaultFormatForAlphaEnabledImages());
                scaledImage.fill(Qt::transparent);
            } else
                scaledImage = QImage(scaledSize.toSize(), NativeImageQt::defaultFormatForOpaqueImages());

            {
                QPainter painter(&scaledImage);
                painter.setCompositionMode(QPainter::CompositionMode_Source);
                painter.setRenderHints(p->renderHints());
                painter.drawImage(QRect(0, 0, scaledImage.width(), scaledImage.height()), image);
            }
            image = scaledImage;
            transform = QTransform::fromTranslate(transform.dx(), transform.dy());
        }
    }

    /* Translate the coordinates as phase is not in world matrix coordinate space but the tile rect origin is. */
    transform *= QTransform().translate(phase.x(), phase.y());
    transform.translate(tr.x(), tr.y());

    QBrush b(image);
    b.setTransform(transform);
    p->fillRect(dr, b);

    ctxt->setCompositeOperation(previousOperator);

    if (imageObserver())
        imageObserver()->didDraw(this);
}

BitmapImage::BitmapImage(QImage* image, ImageObserver* observer)
    : Image(observer)
    , m_currentFrame(0)
    , m_frames(0)
    , m_frameTimer(0)
    , m_repetitionCount(cAnimationNone)
    , m_repetitionCountStatus(Unknown)
    , m_repetitionsComplete(0)
    , m_decodedSize(0)
    , m_frameCount(1)
    , m_isSolidColor(false)
    , m_checkedForSolidColor(false)
    , m_animationFinished(true)
    , m_allDataReceived(true)
    , m_haveSize(true)
    , m_sizeAvailable(true)
    , m_haveFrameCount(true)
{
    int width = image->width();
    int height = image->height();
    m_decodedSize = width * height * 4;
    m_size = IntSize(width, height);

    m_frames.grow(1);
    m_frames[0].m_frame = image;
    m_frames[0].m_hasAlpha = image->hasAlphaChannel();
    m_frames[0].m_haveMetadata = true;
    checkForSolidColor();
}

void BitmapImage::invalidatePlatformData()
{
}

// Drawing Routines
void BitmapImage::draw(GraphicsContext* ctxt, const FloatRect& dst,
                       const FloatRect& src, ColorSpace styleColorSpace, CompositeOperator op)
{
    QRectF normalizedDst = dst.normalized();
    QRectF normalizedSrc = src.normalized();

    startAnimation();

    if (normalizedSrc.isEmpty() || normalizedDst.isEmpty())
        return;

    QImage* image = nativeImageForCurrentFrame();

    if (!image)
        return;

    if (mayFillWithSolidColor()) {
        fillWithSolidColor(ctxt, normalizedDst, solidColor(), styleColorSpace, op);
        return;
    }

#if ENABLE(IMAGE_DECODER_DOWN_SAMPLING)
    normalizedSrc = adjustSourceRectForDownSampling(normalizedSrc, image->size());
#endif

    CompositeOperator previousOperator = ctxt->compositeOperation();
    ctxt->setCompositeOperation(!image->hasAlphaChannel() && op == CompositeSourceOver ? CompositeCopy : op);

    if (ctxt->hasShadow()) {
        ShadowBlur* shadow = ctxt->shadowBlur();
        GraphicsContext* shadowContext = shadow->beginShadowLayer(ctxt, normalizedDst);
        if (shadowContext) {
            QPainter* shadowPainter = shadowContext->platformContext();
            shadowPainter->drawImage(normalizedDst, *image, normalizedSrc);
            shadow->endShadowLayer(ctxt);
        }
    }

    ctxt->platformContext()->drawImage(normalizedDst, *image, normalizedSrc);

    ctxt->setCompositeOperation(previousOperator);

    if (imageObserver())
        imageObserver()->didDraw(this);
}

void BitmapImage::checkForSolidColor()
{
    m_isSolidColor = false;
    m_checkedForSolidColor = true;

    if (frameCount() > 1)
        return;

    QImage* frameImage = frameAtIndex(0);
    if (!frameImage || frameImage->width() != 1 || frameImage->height() != 1)
        return;

    m_isSolidColor = true;
    m_solidColor = QColor::fromRgba(frameImage->pixel(0, 0));
}

#if OS(WINDOWS)
PassRefPtr<BitmapImage> BitmapImage::create(HBITMAP hBitmap)
{
#if HAVE(QT5)
    QImage* nativeImage = new QImage(qt_pixmapFromWinHBITMAP(hBitmap).toImage());
#else
    QImage* nativeImage = new QImage(QPixmap::fromWinHBITMAP(hBitmap).toImage());
#endif

    return BitmapImage::create(nativeImage);
}
#endif

}


// vim: ts=4 sw=4 et
