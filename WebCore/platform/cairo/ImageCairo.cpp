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
#include <cairo.h>
#include <kxmlcore/Vector.h>
#include "Array.h"
#include "IntSize.h"
#include "FloatRect.h"
#include "Image.h"
#include "ImageDecoder.h"
#include "ImageData.h"

// FIXME: Hack for image viewer test app.  Will have to remove when we add to
// WebCore for real.
class QString {};

namespace WebCore {

void FrameData::clear()
{
    if (m_frame) {
        cairo_surface_destroy(m_frame);
        m_frame = 0;
        m_duration = 0.;
    }
}

// ================================================
// Image Class
// ================================================

void Image::initNativeData()
{
}

void Image::destroyNativeData()
{
}

void Image::invalidateNativeData()
{
}

Image* Image::loadResource(const char *name)
{
    // FIXME: Read the error image from disk.
    return 0;
}

bool Image::supportsType(const QString& type)
{
    // FIXME: Implement.
    return true;
}

// Drawing Routines
static cairo_t* graphicsContext(void* context)
{
    // FIXME: If there is no context do we have some way of getting the current one?
    return (cairo_t*)context;
}

static void setCompositingOperation(cairo_t* context, Image::CompositeOperator op)
{
    // FIXME: Add support for more operators.
    // FIXME: This should really move to be a graphics context function once we have
    // a C++ abstraction for GraphicsContext.
    cairo_set_operator(context, CAIRO_OPERATOR_OVER);
}

void Image::drawInRect(const FloatRect& dst, const FloatRect& src,
                       CompositeOperator compositeOp, void* ctxt)
{
    cairo_t* context = graphicsContext(ctxt);

    if (!m_decoder.initialized())
        return;
    
    cairo_surface_t* image = frameAtIndex(m_currentFrame);
    if (!image) // If it's too early we won't have an image yet.
        return;

    IntSize selfSize = size();                       
    FloatRect srcRect(src);
    FloatRect dstRect(dst);

    cairo_save(context);

    // Set the compositing operation.
    setCompositingOperation(context, compositeOp);
    
    // If we're drawing a sub portion of the image or scaling then create
    // a pattern transformation on the image and draw the transformed pattern.
    // Test using example site at http://www.meyerweb.com/eric/css/edge/complexspiral/demo.html
    cairo_pattern_t* pattern = cairo_pattern_create_for_surface(image);
    float scaleX = srcRect.width() / dstRect.width();
    float scaleY = srcRect.height() / dstRect.height();
    cairo_matrix_t mat = { scaleX,  0, 0 , scaleY, srcRect.x(), srcRect.y() };
    cairo_pattern_set_matrix(pattern, &mat);

    // Draw the image.
    cairo_translate(context, dstRect.x(), dstRect.y());
    cairo_set_source(context, pattern);
    cairo_rectangle(context, 0, 0, dstRect.width(), dstRect.height());
    cairo_fill(context);

    cairo_restore(context);

    startAnimation();

}

/*
static void drawPattern(void* info, CGContextRef context)
{
    ImageData* data = (ImageData*)info;
    CGImageRef image = data->frameAtIndex(data->currentFrame());
    float w = CGImageGetWidth(image);
    float h = CGImageGetHeight(image);
    CGContextDrawImage(context, CGRectMake(0, data->size().height() - h, w, h), image);    
}

static const CGPatternCallbacks patternCallbacks = { 0, drawPattern, NULL };
*/

void Image::tileInRect(const FloatRect& dstRect, const FloatPoint& srcPoint, void* ctxt)
{
    /* FIXME: IMPLEMENT
    if (!m_data)
        return;
    
    CGImageRef image = frameAtIndex(m_currentFrame);
    if (!image)
        return;

    CGContextRef context = graphicsContext(ctxt);

    if (m_currentFrame == 0 && m_isSolidColor)
        return fillSolidColorInRect(m_solidColor, dstRect, Image::CompositeSourceOver, context);

    CGSize tileSize = size();
    CGRect rect = dstRect;
    CGPoint point = srcPoint;

    // Check and see if a single draw of the image can cover the entire area we are supposed to tile.
    NSRect oneTileRect;
    oneTileRect.origin.x = rect.origin.x + fmodf(fmodf(-point.x, tileSize.width) - tileSize.width, tileSize.width);
    oneTileRect.origin.y = rect.origin.y + fmodf(fmodf(-point.y, tileSize.height) - tileSize.height, tileSize.height);
    oneTileRect.size.height = tileSize.height;
    oneTileRect.size.width = tileSize.width;

    // If the single image draw covers the whole area, then just draw once.
    if (NSContainsRect(oneTileRect, NSMakeRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height))) {
        CGRect fromRect;
        fromRect.origin.x = rect.origin.x - oneTileRect.origin.x;
        fromRect.origin.y = rect.origin.y - oneTileRect.origin.y;
        fromRect.size = rect.size;

        drawInRect(dstRect, FloatRect(fromRect), Image::CompositeSourceOver, context);
        return;
    }

    CGPatternRef pattern = CGPatternCreate(m_data, CGRectMake(0, 0, tileSize.width, tileSize.height),
                                           CGAffineTransformIdentity, tileSize.width, tileSize.height, 
                                           kCGPatternTilingConstantSpacing, true, &patternCallbacks);
    
    if (pattern) {
        CGContextSaveGState(context);

        CGPoint tileOrigin = CGPointMake(oneTileRect.origin.x, oneTileRect.origin.y);
        [[WebCoreImageRendererFactory sharedFactory] setPatternPhaseForContext:context inUserSpace:tileOrigin];

        CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(NULL);
        CGContextSetFillColorSpace(context, patternSpace);
        CGColorSpaceRelease(patternSpace);

        float patternAlpha = 1;
        CGContextSetFillPattern(context, pattern, &patternAlpha);

        setCompositingOperation(context, Image::CompositeSourceOver);

        CGContextFillRect(context, rect);

        CGContextRestoreGState(context);

        CGPatternRelease(pattern);
    }

    startAnimation();
    */
}

void Image::scaleAndTileInRect(const FloatRect& dstRect, const FloatRect& srcRect,
                               TileRule hRule, TileRule vRule, void* ctxt)
{
    /* FIXME: IMPLEMENT
    if (!m_data)
        return;
    
    CGImageRef image = frameAtIndex(m_currentFrame);
    if (!image)
        return;

    CGContextRef context = graphicsContext(ctxt);
    
    if (m_currentFrame == 0 && m_isSolidColor)
        return fillSolidColorInRect(m_solidColor, dstRect, Image::CompositeSourceOver, context);

    CGContextSaveGState(context);
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
    CGPatternRef pattern = CGPatternCreate(m_data, CGRectMake(fr.origin.x, fr.origin.y, tileSize.width, tileSize.height),
                                           patternTransform, tileSize.width, tileSize.height, 
                                           kCGPatternTilingConstantSpacing, true, &patternCallbacks);
    if (pattern) {
        // We want to construct the phase such that the pattern is centered (when stretch is not
        // set for a particular rule).
        float hPhase = scaleX * fr.origin.x;
        float vPhase = scaleY * (tileSize.height - fr.origin.y);
        if (hRule == Image::RepeatTile)
            hPhase -= fmodf(ir.size.width, scaleX * tileSize.width) / 2.0f;
        if (vRule == Image::RepeatTile)
            vPhase -= fmodf(ir.size.height, scaleY * tileSize.height) / 2.0f;
        
        CGPoint tileOrigin = CGPointMake(ir.origin.x - hPhase, ir.origin.y - vPhase);
        [[WebCoreImageRendererFactory sharedFactory] setPatternPhaseForContext:context inUserSpace:tileOrigin];

        CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(NULL);
        CGContextSetFillColorSpace(context, patternSpace);
        CGColorSpaceRelease(patternSpace);

        float patternAlpha = 1;
        CGContextSetFillPattern(context, pattern, &patternAlpha);

        setCompositingOperation(context, Image::CompositeSourceOver);
        
        CGContextFillRect(context, ir);

        CGPatternRelease (pattern);
    }

    CGContextRestoreGState(context);

    startAnimation();
    */
}

}
