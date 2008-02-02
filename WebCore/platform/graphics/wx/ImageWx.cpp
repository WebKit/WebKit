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
        m_duration = 0.;
        m_hasAlpha = true;
    }
}

// ================================================
// Image Class
// ================================================

Image* Image::loadPlatformResource(const char *name)
{
    Vector<char> arr = loadResourceIntoArray(name);
    Image* img = new BitmapImage();
    RefPtr<SharedBuffer> buffer = new SharedBuffer(arr.data(), arr.size());
    img->setData(buffer, true);
    return img;
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

#if USE(WXGC)
    wxGCDC* context = (wxGCDC*)ctxt->platformContext();
#else
    wxWindowDC* context = ctxt->platformContext();
#endif

    wxBitmap* bitmap = frameAtIndex(m_currentFrame);
    if (!bitmap) // If it's too early we won't have an image yet.
        return;

    IntSize selfSize = size();                
    FloatRect srcRect(src);
    FloatRect dstRect(dst);
    
    // If we're drawing a sub portion of the image or scaling then create
    // a pattern transformation on the image and draw the transformed pattern.
    // Test using example site at http://www.meyerweb.com/eric/css/edge/complexspiral/demo.html
    // FIXME: NYI
   
    wxMemoryDC mydc; 
    ASSERT(bitmap->GetRefData());
    mydc.SelectObject(*bitmap); 
    context->Blit((wxCoord)dst.x(),(wxCoord)dst.y(), (wxCoord)dst.width(), (wxCoord)dst.height(), &mydc, 
                    (wxCoord)src.x(), (wxCoord)src.y(), wxCOPY, true); 
    mydc.SelectObject(wxNullBitmap);
    
    // NB: delete is causing crashes during page load, but not during the deletion
    // itself. It occurs later on when a valid bitmap created in frameAtIndex
    // suddenly becomes invalid after returning. It's possible these errors deal
    // with reentrancy and threding problems.
    //delete bitmap;
    startAnimation();
}


void BitmapImage::drawPattern(GraphicsContext* ctxt, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, CompositeOperator, const FloatRect& dstRect)
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
#endif

    wxMemoryDC mydc;
    mydc.SelectObject(*bitmap);

    while ( currentW < dstRect.width() ) {
        while ( currentH < dstRect.height() ) {
            context->Blit((wxCoord)dstRect.x() + currentW, (wxCoord)dstRect.y() + currentH,  
                            (wxCoord)srcRect.width(), (wxCoord)srcRect.height(), &mydc, 
                            (wxCoord)srcRect.x(), (wxCoord)srcRect.y(), wxCOPY, true); 
            currentH += srcRect.height();
        }
        currentW += srcRect.width();
        currentH = 0;
    }
    ctxt->restore();
    mydc.SelectObject(wxNullBitmap);
    
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
