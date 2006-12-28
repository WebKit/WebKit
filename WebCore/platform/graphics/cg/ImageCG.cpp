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

#include "FloatRect.h"
#include "GraphicsContext.h"
#include "PDFDocumentImage.h"
#include "PlatformString.h"
#include <ApplicationServices/ApplicationServices.h>
#include "WebCoreSystemInterface.h"

namespace WebCore {

void FrameData::clear()
{
    if (m_frame) {
        CFRelease(m_frame);
        m_frame = 0;
        m_duration = 0.;
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
            CGFloat pixel[4]; // RGBA
            CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
            CGContextRef bmap = CGBitmapContextCreate(&pixel, 1, 1, 8*sizeof(float), sizeof(pixel), space,
                kCGImageAlphaPremultipliedLast | kCGBitmapFloatComponents | kCGBitmapByteOrder32Host);
            if (bmap) {
                GraphicsContext(bmap).setCompositeOperation(CompositeCopy);
                CGRect dst = { {0, 0}, {1, 1} };
                CGContextDrawImage(bmap, dst, image);
                if (pixel[3] == 0)
                    m_solidColor = Color(0, 0, 0, 0);
                else
                    m_solidColor = Color(int(pixel[0] / pixel[3] * 255), int(pixel[1] / pixel[3] * 255), int(pixel[2] / pixel[3] * 255), int(pixel[3] * 255));
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

void fillWithSolidColor(GraphicsContext* ctxt, const FloatRect& dstRect, const Color& color, CompositeOperator op)
{
    if (color.alpha() <= 0)
        return;
    
    ctxt->save();
    ctxt->setCompositeOperation(!color.hasAlpha() && op == CompositeSourceOver ? CompositeCopy : op);
    ctxt->fillRect(dstRect, color);
    ctxt->restore();
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

}

static void drawPattern(void* info, CGContextRef context)
{
    BitmapImage* data = (BitmapImage*)info;
    CGImageRef image = data->nativeImageForCurrentFrame();
    float w = CGImageGetWidth(image);
    float h = CGImageGetHeight(image);
    CGContextDrawImage(context, GraphicsContext(context).roundToDevicePixels(FloatRect
        (0, data->size().height() - h, w, h)), image);    
}

static const CGPatternCallbacks patternCallbacks = { 0, drawPattern, NULL };

void BitmapImage::drawTiled(GraphicsContext* ctxt, const FloatRect& destRect, const FloatPoint& srcPoint,
                      const FloatSize& tileSize, CompositeOperator op)
{    
    CGImageRef image = frameAtIndex(m_currentFrame);
    if (!image)
        return;
    
    if (mayFillWithSolidColor()) {
        fillWithSolidColor(ctxt, destRect, solidColor(), op);
        return;
    }

    CGPoint scaledPoint = srcPoint;
    CGSize intrinsicTileSize = size();
    CGSize scaledTileSize = intrinsicTileSize;

    // If tileSize is not equal to the intrinsic size of the image, set patternTransform
    // to the appropriate scalar matrix, scale the source point, and set the size of the
    // scaled tile. 
    float scaleX = 1.0;
    float scaleY = 1.0;
    CGAffineTransform patternTransform = CGAffineTransformIdentity;
    if (tileSize.width() != intrinsicTileSize.width || tileSize.height() != intrinsicTileSize.height) {
        scaleX = tileSize.width() / intrinsicTileSize.width;
        scaleY = tileSize.height() / intrinsicTileSize.height;
        patternTransform = CGAffineTransformMakeScale(scaleX, scaleY);
        scaledTileSize = tileSize;
    }

    // Check and see if a single draw of the image can cover the entire area we are supposed to tile.
    CGRect oneTileRect;
    oneTileRect.origin.x = destRect.x() + fmodf(fmodf(-scaledPoint.x, scaledTileSize.width) - 
                            scaledTileSize.width, scaledTileSize.width);
    oneTileRect.origin.y = destRect.y() + fmodf(fmodf(-scaledPoint.y, scaledTileSize.height) - 
                            scaledTileSize.height, scaledTileSize.height);
    oneTileRect.size.width = scaledTileSize.width;
    oneTileRect.size.height = scaledTileSize.height;

    // If the single image draw covers the whole area, then just draw once.
    if (CGRectContainsRect(oneTileRect, destRect)) {
        CGRect fromRect;
        fromRect.origin.x = (destRect.x() - oneTileRect.origin.x) / scaleX;
        fromRect.origin.y = (destRect.y() - oneTileRect.origin.y) / scaleY;
        fromRect.size.width = destRect.width() / scaleX;
        fromRect.size.height = destRect.height() / scaleY;

        draw(ctxt, destRect, fromRect, op);
        return;
    }

    CGPatternRef pattern = CGPatternCreate(this, CGRectMake(0, 0, intrinsicTileSize.width, intrinsicTileSize.height),
                                           patternTransform, intrinsicTileSize.width, intrinsicTileSize.height, 
                                           kCGPatternTilingConstantSpacing, true, &patternCallbacks);
    
    if (pattern) {
        CGContextRef context = ctxt->platformContext();

        ctxt->save();

        // FIXME: Really want a public API for this.
        wkSetPatternPhaseInUserSpace(context, CGPointMake(oneTileRect.origin.x, oneTileRect.origin.y));

        CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(NULL);
        CGContextSetFillColorSpace(context, patternSpace);
        CGColorSpaceRelease(patternSpace);

        CGFloat patternAlpha = 1;
        CGContextSetFillPattern(context, pattern, &patternAlpha);

        ctxt->setCompositeOperation(op);

        CGContextFillRect(context, destRect);

        ctxt->restore();

        CGPatternRelease(pattern);
    }
    
    startAnimation();
}

// FIXME: Merge with the other drawTiled eventually, since we need a combination of both for some things.
void BitmapImage::drawTiled(GraphicsContext* ctxt, const FloatRect& dstRect, const FloatRect& srcRect, TileRule hRule,
                      TileRule vRule, CompositeOperator op)
{    
    CGImageRef image = frameAtIndex(m_currentFrame);
    if (!image)
        return;

    if (mayFillWithSolidColor()) {
        fillWithSolidColor(ctxt, dstRect, solidColor(), op);
        return;
    }

    ctxt->save();

    CGSize tileSize = srcRect.size();
    CGRect ir = dstRect;
    CGRect fr = srcRect;

    // Now scale the slice in the appropriate direction using an affine transform that we will pass into
    // the pattern.
    float scaleX = 1.0f, scaleY = 1.0f;

    if (hRule == Image::StretchTile)
        scaleX = ir.size.width / fr.size.width;
    if (vRule == Image::StretchTile)
        scaleY = ir.size.height / fr.size.height;
    
    if (hRule == Image::RepeatTile)
        scaleX = scaleY;
    if (vRule == Image::RepeatTile)
        scaleY = scaleX;
        
    if (hRule == Image::RoundTile) {
        // Complicated math ensues.
        float imageWidth = fr.size.width * scaleY;
        float newWidth = ir.size.width / ceilf(ir.size.width / imageWidth);
        scaleX = newWidth / fr.size.width;
    }
    
    if (vRule == Image::RoundTile) {
        // More complicated math ensues.
        float imageHeight = fr.size.height * scaleX;
        float newHeight = ir.size.height / ceilf(ir.size.height / imageHeight);
        scaleY = newHeight / fr.size.height;
    }
    
    CGAffineTransform patternTransform = CGAffineTransformMakeScale(scaleX, scaleY);

    // Possible optimization:  We may want to cache the CGPatternRef    
    CGPatternRef pattern = CGPatternCreate(this, CGRectMake(fr.origin.x, fr.origin.y, tileSize.width, tileSize.height),
                                           patternTransform, tileSize.width, tileSize.height, 
                                           kCGPatternTilingConstantSpacing, true, &patternCallbacks);
    if (pattern) {
        CGContextRef context = ctxt->platformContext();
    
        // We want to construct the phase such that the pattern is centered (when stretch is not
        // set for a particular rule).
        float hPhase = scaleX * fr.origin.x;
        float vPhase = scaleY * (tileSize.height - fr.origin.y);
        if (hRule == Image::RepeatTile)
            hPhase -= fmodf(ir.size.width, scaleX * tileSize.width) / 2.0f;
        if (vRule == Image::RepeatTile)
            vPhase -= fmodf(ir.size.height, scaleY * tileSize.height) / 2.0f;
        
        // FIXME: Really want a public API for this.
        wkSetPatternPhaseInUserSpace(context, CGPointMake(ir.origin.x - hPhase, ir.origin.y - vPhase));

        CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(NULL);
        CGContextSetFillColorSpace(context, patternSpace);
        CGColorSpaceRelease(patternSpace);

        CGFloat patternAlpha = 1;
        CGContextSetFillPattern(context, pattern, &patternAlpha);

        ctxt->setCompositeOperation(op);
        
        CGContextFillRect(context, ir);

        CGPatternRelease(pattern);
    }

    ctxt->restore();

    startAnimation();
}

}

#endif // PLATFORM(CG)
