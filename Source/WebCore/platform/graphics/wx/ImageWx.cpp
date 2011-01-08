/*
 * Copyright (C) 2007 Apple Computer, Kevin Ollivier.  All rights reserved.
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
#include "FloatConversion.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageObserver.h"
#include "NotImplemented.h"
#include "SharedBuffer.h"

#include <math.h>
#include <stdio.h>

#include <wx/defs.h>
#include <wx/bitmap.h>
#include <wx/dc.h>
#include <wx/dcmemory.h>
#include <wx/dcgraph.h>
#include <wx/graphics.h>
#include <wx/image.h>
#include <wx/thread.h>

namespace WebCore {

// this is in GraphicsContextWx.cpp
int getWxCompositingOperation(CompositeOperator op, bool hasAlpha);

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

PassRefPtr<Image> Image::loadPlatformResource(const char *name)
{
    // FIXME: We need to have some 'placeholder' graphics for things like missing
    // plugins or broken images.
    Vector<char> arr;
    RefPtr<Image> img = BitmapImage::create();
    RefPtr<SharedBuffer> buffer = SharedBuffer::create(arr.data(), arr.size());
    img->setData(buffer, true);
    return img.release();
}

void BitmapImage::initPlatformData()
{
    // FIXME: NYI
}

// Drawing Routines

void BitmapImage::draw(GraphicsContext* ctxt, const FloatRect& dst, const FloatRect& src, ColorSpace styleColorSpace, CompositeOperator op)
{
    if (!m_source.initialized())
        return;

    if (mayFillWithSolidColor()) {
        fillWithSolidColor(ctxt, dst, solidColor(), styleColorSpace, op);
        return;
    }

#if USE(WXGC)
    wxGCDC* context = (wxGCDC*)ctxt->platformContext();
    wxGraphicsContext* gc = context->GetGraphicsContext();
    wxGraphicsBitmap* bitmap = frameAtIndex(m_currentFrame);
#else
    wxWindowDC* context = ctxt->platformContext();
    wxBitmap* bitmap = frameAtIndex(m_currentFrame);
#endif

    startAnimation();
    if (!bitmap) // If it's too early we won't have an image yet.
        return;
    
    // If we're drawing a sub portion of the image or scaling then create
    // a pattern transformation on the image and draw the transformed pattern.
    // Test using example site at http://www.meyerweb.com/eric/css/edge/complexspiral/demo.html
    // FIXME: NYI
   
    ctxt->save();

    // Set the compositing operation.
    ctxt->setCompositeOperation(op);
    
#if USE(WXGC)
    float scaleX = src.width() / dst.width();
    float scaleY = src.height() / dst.height();

    FloatRect adjustedDestRect = dst;
    FloatSize selfSize = currentFrameSize();
    
    if (src.size() != selfSize) {
        adjustedDestRect.setLocation(FloatPoint(dst.x() - src.x() / scaleX, dst.y() - src.y() / scaleY));
        adjustedDestRect.setSize(FloatSize(selfSize.width() / scaleX, selfSize.height() / scaleY));
    }

    gc->Clip(dst.x(), dst.y(), dst.width(), dst.height());
#if wxCHECK_VERSION(2,9,0)
    gc->DrawBitmap(*bitmap, adjustedDestRect.x(), adjustedDestRect.y(), adjustedDestRect.width(), adjustedDestRect.height());
#else
    gc->DrawGraphicsBitmap(*bitmap, adjustedDestRect.x(), adjustedDestRect.y(), adjustedDestRect.width(), adjustedDestRect.height());
#endif

#else // USE(WXGC)
    IntRect srcIntRect(src);
    IntRect dstIntRect(dst);
    bool rescaling = false;
    if ((dstIntRect.width() != srcIntRect.width()) || (dstIntRect.height() != srcIntRect.height()))
    {
        rescaling = true;
        wxImage img = bitmap->ConvertToImage();
        img.Rescale(dstIntRect.width(), dstIntRect.height());
        bitmap = new wxBitmap(img);
    } 
    
    wxMemoryDC mydc; 
    ASSERT(bitmap->GetRefData());
    mydc.SelectObject(*bitmap); 
    
    context->Blit((wxCoord)dstIntRect.x(),(wxCoord)dstIntRect.y(), (wxCoord)dstIntRect.width(), (wxCoord)dstIntRect.height(), &mydc, 
                    (wxCoord)srcIntRect.x(), (wxCoord)srcIntRect.y(), wxCOPY, true); 
    mydc.SelectObject(wxNullBitmap);
    
    // NB: delete is causing crashes during page load, but not during the deletion
    // itself. It occurs later on when a valid bitmap created in frameAtIndex
    // suddenly becomes invalid after returning. It's possible these errors deal
    // with reentrancy and threding problems.
    //delete bitmap;
    if (rescaling)
    {
        delete bitmap;
        bitmap = NULL;
    }
#endif

    ctxt->restore();

    if (ImageObserver* observer = imageObserver())
        observer->didDraw(this);
}

void Image::drawPattern(GraphicsContext* ctxt, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, ColorSpace, CompositeOperator, const FloatRect& dstRect)
{
    

#if USE(WXGC)
    wxGCDC* context = (wxGCDC*)ctxt->platformContext();
    wxGraphicsBitmap* bitmap = nativeImageForCurrentFrame();
#else
    wxWindowDC* context = ctxt->platformContext();
    wxBitmap* bitmap = nativeImageForCurrentFrame();
#endif

    if (!bitmap) // If it's too early we won't have an image yet.
        return;

    ctxt->save();
    ctxt->clip(IntRect(dstRect.x(), dstRect.y(), dstRect.width(), dstRect.height()));
    
    float currentW = 0;
    float currentH = 0;
    
#if USE(WXGC)
    wxGraphicsContext* gc = context->GetGraphicsContext();

    float adjustedX = phase.x() + srcRect.x() *
                      narrowPrecisionToFloat(patternTransform.a());
    float adjustedY = phase.y() + srcRect.y() *
                      narrowPrecisionToFloat(patternTransform.d());
                      
    gc->ConcatTransform(patternTransform);
#else
    wxMemoryDC mydc;
    mydc.SelectObject(*bitmap);
#endif

    wxPoint origin(context->GetDeviceOrigin());
    wxSize clientSize(context->GetSize());

    while ( currentW < dstRect.width()  && currentW < clientSize.x - origin.x ) {
        while ( currentH < dstRect.height() && currentH < clientSize.y - origin.y) {
#if USE(WXGC)
#if wxCHECK_VERSION(2,9,0)
            gc->DrawBitmap(*bitmap, adjustedX + currentW, adjustedY + currentH, (wxDouble)srcRect.width(), (wxDouble)srcRect.height());
#else
            gc->DrawGraphicsBitmap(*bitmap, adjustedX + currentW, adjustedY + currentH, (wxDouble)srcRect.width(), (wxDouble)srcRect.height());
#endif
#else
            context->Blit((wxCoord)dstRect.x() + currentW, (wxCoord)dstRect.y() + currentH,  
                            (wxCoord)srcRect.width(), (wxCoord)srcRect.height(), &mydc, 
                            (wxCoord)srcRect.x(), (wxCoord)srcRect.y(), wxCOPY, true); 
#endif
            currentH += srcRect.height();
        }
        currentW += srcRect.width();
        currentH = 0;
    }
    ctxt->restore();

#if !USE(WXGC)
    mydc.SelectObject(wxNullBitmap);
#endif    
    
    // NB: delete is causing crashes during page load, but not during the deletion
    // itself. It occurs later on when a valid bitmap created in frameAtIndex
    // suddenly becomes invalid after returning. It's possible these errors deal
    // with reentrancy and threding problems.
    //delete bitmap;

    startAnimation();

    if (ImageObserver* observer = imageObserver())
        observer->didDraw(this);
}

void BitmapImage::checkForSolidColor()
{
    m_checkedForSolidColor = true;
}

void BitmapImage::invalidatePlatformData()
{

}

}
