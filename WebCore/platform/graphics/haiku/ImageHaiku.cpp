/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2008 Andrea Anzani <andrea.anzani@gmail.com>
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
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include <Application.h>
#include <Bitmap.h>
#include <View.h>


// This function loads resources from WebKit
Vector<char> loadResourceIntoArray(const char*);


namespace WebCore {

bool FrameData::clear(bool clearMetadata)
{
    if (clearMetadata)
        m_haveMetadata = false;

    if (m_frame) {
        delete m_frame;
        m_frame = 0;
        m_duration = 0.0f;
        m_hasAlpha = true;
        return true;
    }

    return false;
}

WTF::PassRefPtr<Image> Image::loadPlatformResource(const char* name)
{
    Vector<char> array = loadResourceIntoArray(name);
    WTF::PassRefPtr<BitmapImage> image = BitmapImage::create();
    RefPtr<SharedBuffer> buffer = SharedBuffer::create(array.data(), array.size());
    image->setData(buffer, true);

    return image;
}

void BitmapImage::initPlatformData()
{
}

void BitmapImage::invalidatePlatformData()
{
}

// Drawing Routines
void BitmapImage::draw(GraphicsContext* ctxt, const FloatRect& dst, const FloatRect& src, CompositeOperator op)
{
    startAnimation();

    BBitmap* image = nativeImageForCurrentFrame();
    if (!image || !image->IsValid()) // If the image hasn't fully loaded.
        return;

    if (mayFillWithSolidColor()) {
        fillWithSolidColor(ctxt, dst, solidColor(), op);
        return;
    }

    ctxt->save();
    ctxt->setCompositeOperation(op);

    BRect srcRect(src);
    BRect dstRect(dst);

    // Test using example site at
    // http://www.meyerweb.com/eric/css/edge/complexspiral/demo.html
    ctxt->platformContext()->SetDrawingMode(B_OP_ALPHA);
    ctxt->platformContext()->DrawBitmap(image, srcRect & image->Bounds(), dstRect);
    ctxt->restore();
}

void Image::drawPattern(GraphicsContext* context, const FloatRect& tileRect, const TransformationMatrix& patternTransform, const FloatPoint& srcPoint, CompositeOperator op, const FloatRect& dstRect)
{
    // FIXME: finish this to support also phased position (srcPoint)
    startAnimation();

    BBitmap* image = nativeImageForCurrentFrame();
    if (!image || !image->IsValid()) // If the image hasn't fully loaded.
        return;

    float currentW = 0;
    float currentH = 0;

    context->save();
    context->platformContext()->SetDrawingMode(B_OP_ALPHA);
    context->clip(enclosingIntRect(dstRect));

    while (currentW < dstRect.width()) {
        while (currentH < dstRect.height()) {
            context->platformContext()->DrawBitmap(image, BPoint(dstRect.x() + currentW, dstRect.y() + currentH));
            currentH += tileRect.height();
        }
        currentW += tileRect.width();
        currentH = 0;
    }
    context->restore();
}

void BitmapImage::checkForSolidColor()
{
    // FIXME: need to check the RGBA32 buffer to see if it is 1x1.
    m_isSolidColor = false;
    m_checkedForSolidColor = true;
}

BBitmap* BitmapImage::getBBitmap() const
{
    return const_cast<BitmapImage*>(this)->frameAtIndex(0);
}

} // namespace WebCore

