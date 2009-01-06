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

#include "TransformationMatrix.h"
#include "BitmapImage.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"

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

// This function loads resources from WebKit
Vector<char> loadResourceIntoArray(const char*);

namespace WebCore {

// this is in GraphicsContextWx.cpp
int getWxCompositingOperation(CompositeOperator op, bool hasAlpha);

void FrameData::clear()
{
    if (m_frame) {
        delete m_frame;
        m_frame = 0;
        // NOTE: We purposefully don't reset metadata here, so that even if we
        // throw away previously-decoded data, animation loops can still access
        // properties like frame durations without re-decoding.
    }
}

// ================================================
// Image Class
// ================================================

PassRefPtr<Image> Image::loadPlatformResource(const char *name)
{
    Vector<char> arr = loadResourceIntoArray(name);
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

void BitmapImage::draw(GraphicsContext* ctxt, const FloatRect& dst, const FloatRect& src, CompositeOperator op)
{
    if (!m_source.initialized())
        return;

    if (mayFillWithSolidColor()) {
        fillWithSolidColor(ctxt, dst, solidColor(), op);
        return;
    }

#if USE(WXGC)
    wxGCDC* context = (wxGCDC*)ctxt->platformContext();
    wxGraphicsContext* gc = context->GetGraphicsContext();
#else
    wxWindowDC* context = ctxt->platformContext();
#endif

    startAnimation();

    wxBitmap* bitmap = frameAtIndex(m_currentFrame);
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
    
    // If the image is only partially loaded, then shrink the destination rect that we're drawing into accordingly.
    int currHeight = bitmap->GetHeight(); 
    if (currHeight < selfSize.height())
        adjustedDestRect.setHeight(adjustedDestRect.height() * currHeight / selfSize.height());

    gc->DrawBitmap(*bitmap, adjustedDestRect.x(), adjustedDestRect.y(), adjustedDestRect.width(), adjustedDestRect.height());
#else
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
}

void BitmapImage::drawPattern(GraphicsContext* ctxt, const FloatRect& srcRect, const TransformationMatrix& patternTransform, const FloatPoint& phase, CompositeOperator, const FloatRect& dstRect)
{
    if (!m_source.initialized())
        return;

#if USE(WXGC)
    wxGCDC* context = (wxGCDC*)ctxt->platformContext();
#else
    wxWindowDC* context = ctxt->platformContext();
#endif

    ctxt->save();
    ctxt->clip(IntRect(dstRect.x(), dstRect.y(), dstRect.width(), dstRect.height()));
    wxBitmap* bitmap = frameAtIndex(m_currentFrame);
    if (!bitmap) // If it's too early we won't have an image yet.
        return;
    
    float currentW = 0;
    float currentH = 0;
    
#if USE(WXGC)
    wxGraphicsContext* gc = context->GetGraphicsContext();
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
            gc->DrawBitmap(*bitmap, (wxDouble)dstRect.x() + currentW, (wxDouble)dstRect.y() + currentH, (wxDouble)srcRect.width(), (wxDouble)srcRect.height());
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

}

void BitmapImage::checkForSolidColor()
{

}

void BitmapImage::invalidatePlatformData()
{

}

}
