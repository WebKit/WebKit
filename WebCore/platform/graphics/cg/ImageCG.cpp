/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "BitmapImage.h"

#if PLATFORM(CG)

#include "AffineTransform.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageObserver.h"
#include "PDFDocumentImage.h"
#include "PlatformString.h"
#include <ApplicationServices/ApplicationServices.h>
#include "WebCoreSystemInterface.h"

namespace WebCore {

void FrameData::clear()
{
    if (m_frame) {
        CGImageRelease(m_frame);
        m_frame = 0;
        m_duration = 0.0f;
        m_hasAlpha = true;
    }
}

// ================================================
// Image Class
// ================================================

// Drawing Routines

void BitmapImage::checkForSolidColor()
{
    if (frameCount() > 1)
        m_isSolidColor = false;
    else {
        CGImageRef image = frameAtIndex(0);
        
        // Currently we only check for solid color in the important special case of a 1x1 image.
        if (image && CGImageGetWidth(image) == 1 && CGImageGetHeight(image) == 1) {
            unsigned char pixel[4]; // RGBA
            CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
            CGContextRef bmap = CGBitmapContextCreate(pixel, 1, 1, 8, sizeof(pixel), space,
                kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
            if (bmap) {
                GraphicsContext(bmap).setCompositeOperation(CompositeCopy);
                CGRect dst = { {0, 0}, {1, 1} };
                CGContextDrawImage(bmap, dst, image);
                if (pixel[3] == 0)
                    m_solidColor = Color(0, 0, 0, 0);
                else
                    m_solidColor = Color(pixel[0] * 255 / pixel[3], pixel[1] * 255 / pixel[3], pixel[2] * 255 / pixel[3], pixel[3]);
                m_isSolidColor = true;
                CFRelease(bmap);
            } 
            CFRelease(space);
        }
    }
}

CGImageRef BitmapImage::getCGImageRef()
{
    return frameAtIndex(0);
}

void BitmapImage::draw(GraphicsContext* ctxt, const FloatRect& dstRect, const FloatRect& srcRect, CompositeOperator compositeOp)
{
    if (!m_source.initialized())
        return;
    
    CGRect fr = ctxt->roundToDevicePixels(srcRect);
    CGRect ir = ctxt->roundToDevicePixels(dstRect);

    CGImageRef image = frameAtIndex(m_currentFrame);
    if (!image) // If it's too early we won't have an image yet.
        return;
    
    if (mayFillWithSolidColor()) {
        fillWithSolidColor(ctxt, ir, solidColor(), compositeOp);
        return;
    }

    CGContextRef context = ctxt->platformContext();
    ctxt->save();

    // Get the height (in adjusted, i.e. scaled, coords) of the portion of the image
    // that is currently decoded.  This could be less that the actual height.
    CGSize selfSize = size();                          // full image size, in pixels
    float curHeight = CGImageGetHeight(image);         // height of loaded portion, in pixels
    
    CGSize adjustedSize = selfSize;
    if (curHeight < selfSize.height) {
        adjustedSize.height *= curHeight / selfSize.height;

        // Is the amount of available bands less than what we need to draw?  If so,
        // we may have to clip 'fr' if it goes outside the available bounds.
        if (CGRectGetMaxY(fr) > adjustedSize.height) {
            float frHeight = adjustedSize.height - fr.origin.y; // clip fr to available bounds
            if (frHeight <= 0)
                return;                                             // clipped out entirely
            ir.size.height *= (frHeight / fr.size.height);    // scale ir proportionally to fr
            fr.size.height = frHeight;
        }
    }

    // Flip the coords.
    ctxt->setCompositeOperation(compositeOp);
    CGContextTranslateCTM(context, ir.origin.x, ir.origin.y);
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, 0, -ir.size.height);
    
    // Translated to origin, now draw at 0,0.
    ir.origin.x = ir.origin.y = 0;
    
    // If we're drawing a sub portion of the image then create
    // a image for the sub portion and draw that.
    // Test using example site at http://www.meyerweb.com/eric/css/edge/complexspiral/demo.html
    if (fr.size.width != adjustedSize.width || fr.size.height != adjustedSize.height) {
        // Convert ft to image pixel coords:
        float xscale = adjustedSize.width / selfSize.width;
        float yscale = adjustedSize.height / curHeight;     // yes, curHeight, not selfSize.height!
        fr.origin.x /= xscale;
        fr.origin.y /= yscale;
        fr.size.width /= xscale;
        fr.size.height /= yscale;
        
        image = CGImageCreateWithImageInRect(image, fr);
        if (image) {
            CGContextDrawImage(context, ir, image);
            CFRelease(image);
        }
    } else // Draw the whole image.
        CGContextDrawImage(context, ir, image);
        
    ctxt->restore();
    
    startAnimation();

    if (imageObserver())
        imageObserver()->didDraw(this);
}

void Image::drawPatternCallback(void* info, CGContextRef context)
{
    Image* data = (Image*)info;
    CGImageRef image = data->nativeImageForCurrentFrame();
    float w = CGImageGetWidth(image);
    float h = CGImageGetHeight(image);
    CGContextDrawImage(context, GraphicsContext(context).roundToDevicePixels(FloatRect(0, -h, w, h)), image);
}

void Image::drawPattern(GraphicsContext* ctxt, const FloatRect& tileRect, const AffineTransform& patternTransform,
                                const FloatPoint& phase, CompositeOperator op, const FloatRect& destRect)
{
    static const CGPatternCallbacks patternCallbacks = { 0, drawPatternCallback, NULL };
    CGPatternRef pattern = CGPatternCreate(this, FloatRect(tileRect.x(), -tileRect.y() - tileRect.height(), tileRect.width(), tileRect.height()),
                                           CGAffineTransform(patternTransform), tileRect.width(), tileRect.height(), 
                                           kCGPatternTilingConstantSpacing, true, &patternCallbacks);
    if (!pattern)
        return;
    
    CGContextRef context = ctxt->platformContext();
    ctxt->save();
    
    // FIXME: Really want a public API for this.
    wkSetPatternPhaseInUserSpace(context, phase);
    
    CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(NULL);
    CGContextSetFillColorSpace(context, patternSpace);
    CGColorSpaceRelease(patternSpace);
    
    CGFloat patternAlpha = 1;
    CGContextSetFillPattern(context, pattern, &patternAlpha);
    
    ctxt->setCompositeOperation(op);
    
    CGContextFillRect(context, destRect);
    
    ctxt->restore();
    CGPatternRelease(pattern);

    if (imageObserver())
        imageObserver()->didDraw(this);
}


}

#endif // PLATFORM(CG)
