/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
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

#include "BitmapImage.h"
#include "FloatRect.h"
#include "PlatformString.h"
#include "GraphicsContext.h"
#include "AffineTransform.h"

#include <QPixmap>
#include <QPainter>
#include <QImage>
#include <QImageReader>
#if QT_VERSION >= 0x040300
#include <QTransform>
#endif

#include <QDebug>

#include <math.h>

#define notImplemented() qDebug("FIXME: UNIMPLEMENTED: %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__)

// This function loads resources from WebKit
Vector<char> loadResourceIntoArray(const char*);

namespace WebCore {

void FrameData::clear()
{
    if (m_frame) {
        m_frame = 0;
        m_duration = 0.0f;
        m_hasAlpha = true;
    }
}


    
// ================================================
// Image Class
// ================================================

Image* Image::loadPlatformResource(const char* name)
{
    Vector<char> arr = loadResourceIntoArray(name);
    Image* img = new BitmapImage();
    img->setNativeData(&arr, true);
    return img;
}

    
void Image::drawPattern(GraphicsContext* ctxt, const FloatRect& tileRect, const AffineTransform& patternTransform,
                        const FloatPoint& phase, CompositeOperator op, const FloatRect& destRect)
{
    notImplemented();
}


void BitmapImage::initPlatformData()
{
}

void BitmapImage::invalidatePlatformData()
{
}
    
// Drawing Routines
void BitmapImage::draw(GraphicsContext* ctxt, const FloatRect& dst,
                       const FloatRect& src, CompositeOperator op)
{
    QPixmap* image = nativeImageForCurrentFrame();
    if (!image)
        return;
    
    if (mayFillWithSolidColor()) {
        fillWithSolidColor(ctxt, dst, solidColor(), op);
        return;
    }

    IntSize selfSize = size();
    FloatRect srcRect(src);
    FloatRect dstRect(dst);

    ctxt->save();

    // Set the compositing operation.
    ctxt->setCompositeOperation(op);

    QPainter* painter(ctxt->platformContext());

    // Test using example site at
    // http://www.meyerweb.com/eric/css/edge/complexspiral/demo.html    
    painter->drawPixmap(dst, *image, src);

    ctxt->restore();

    startAnimation();
}

void BitmapImage::drawPattern(GraphicsContext* ctxt, const FloatRect& tileRect, const AffineTransform& patternTransform,
                              const FloatPoint& phase, CompositeOperator op, const FloatRect& destRect)
{
    QPixmap* framePixmap = nativeImageForCurrentFrame();
    if (!framePixmap) // If it's too early we won't have an image yet.
        return;

    QPixmap pixmap = *framePixmap;
    QRect tr = QRectF(tileRect).toRect();
    if (tr.x() || tr.y() || tr.width() != pixmap.width() || tr.height() != pixmap.height())
        pixmap = pixmap.copy(tr);
    
    QBrush b(pixmap);
    b.setMatrix(patternTransform);
    
    ctxt->save();
    
    // Set the compositing operation.
    ctxt->setCompositeOperation(op);
    QPainter* p = ctxt->platformContext();
    p->setBrushOrigin(phase);
    p->fillRect(tileRect, b);
    
    ctxt->restore();
}

void BitmapImage::checkForSolidColor()
{
    // FIXME: It's easy to implement this optimization. Just need to check the RGBA32 buffer to see if it is 1x1.
    m_isSolidColor = false;
}

QPixmap* BitmapImage::getPixmap() const
{
    return const_cast<BitmapImage*>(this)->frameAtIndex(0);
}

}


// vim: ts=4 sw=4 et
