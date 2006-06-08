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
#include "Image.h"

#include "FloatRect.h"
#include "GraphicsContext.h"
#include <cairo.h>
#include <math.h>

// This function loads resources from WebKit
DeprecatedByteArray loadResourceIntoArray(const char*);

namespace WebCore {

void FrameData::clear()
{
    if (m_frame) {
        cairo_surface_destroy(m_frame);
        m_frame = 0;
        m_duration = 0.;
        m_hasAlpha = true;
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
    DeprecatedByteArray arr = loadResourceIntoArray(name);
    Image* img = new Image;
    img->setData(arr, true);
    return img;
}

bool Image::supportsType(const String& type)
{
    // FIXME: Implement.
    return false;
}

// Drawing Routines

static void setCompositingOperation(cairo_t* context, CompositeOperator op, bool hasAlpha)
{
    // FIXME: Add support for more operators.
    // FIXME: This should really move to be a graphics context function once we have
    // a C++ abstraction for GraphicsContext.
    if (op == CompositeSourceOver && !hasAlpha)
        op = CompositeCopy;

    if (op == CompositeCopy)
        cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
    else
        cairo_set_operator(context, CAIRO_OPERATOR_OVER);
}

void Image::draw(GraphicsContext* ctxt, const FloatRect& dst, const FloatRect& src, CompositeOperator op)
{
    cairo_t* context = ctxt->platformContext();

    if (!m_source.initialized())
        return;
    
    cairo_surface_t* image = frameAtIndex(m_currentFrame);
    if (!image) // If it's too early we won't have an image yet.
        return;

    IntSize selfSize = size();                       
    FloatRect srcRect(src);
    FloatRect dstRect(dst);

    cairo_save(context);

    // Set the compositing operation.
    setCompositingOperation(context, op, frameHasAlphaAtIndex(m_currentFrame));
    
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

void Image::drawTiled(GraphicsContext* ctxt, const FloatRect& dstRect, const FloatPoint& srcPoint, const FloatSize& tileSize)
{
    if (!m_source.initialized())
        return;

    cairo_surface_t* image = frameAtIndex(m_currentFrame);
    if (!image) // If it's too early we won't have an image yet.
        return;

    IntSize intrinsicImageSize = size();                       
    FloatRect srcRect(srcPoint, intrinsicImageSize);
    FloatPoint point = srcPoint;

    // Check and see if a single draw of the image can cover the entire area we are supposed to tile.
    float tileWidth = size().width();
    float tileHeight = size().height();
    
    // If the scale is not equal to the intrinsic size of the image, set transform matrix
    // to the appropriate scalar matrix, scale the source point, and set the size of the
    // scaled tile. 
    float scaleX = 1.0;
    float scaleY = 1.0;
    cairo_matrix_t mat;
    cairo_matrix_init_identity(&mat);
    if (tileSize.width() != intrinsicImageSize.width() || tileSize.height() != intrinsicImageSize.height()) {
        scaleX = intrinsicImageSize.width() / tileSize.width();
        scaleY = intrinsicImageSize.height() / tileSize.height();
        cairo_matrix_init_scale(&mat, scaleX, scaleY);
        
        tileWidth = tileSize.width();
        tileHeight = tileSize.height();
    }
   
    // We could get interesting source offsets (negative ones or positive ones.  Deal with both
    // out of bounds cases.
    float dstTileX = dstRect.x() + fmodf(fmodf(-point.x(), tileWidth) - tileWidth, tileWidth);
    float dstTileY = dstRect.y() + fmodf(fmodf(-point.y(), tileHeight) - tileHeight, tileHeight);
    FloatRect dstTileRect(dstTileX, dstTileY, tileWidth, tileHeight);
    
    float srcX = dstRect.x() - dstTileRect.x();
    float srcY = dstRect.y() - dstTileRect.y();

    // If the single image draw covers the whole area, then just draw once.
    if (dstTileRect.contains(dstRect)) {
        draw(ctxt, dstRect,
             FloatRect(srcX * scaleX, srcY * scaleY, dstRect.width() * scaleX, dstRect.height() * scaleY),
             CompositeSourceOver);
        return;
    }

    // We have to tile.
    cairo_t* context = ctxt->platformContext();

    cairo_save(context);

    // Set the compositing operation.
    // FIXME: This should be part of the drawTiled API.
    setCompositingOperation(context, CompositeSourceOver, frameHasAlphaAtIndex(m_currentFrame));

    cairo_translate(context, dstTileRect.x(), dstTileRect.y());
    cairo_pattern_t* pattern = cairo_pattern_create_for_surface(image);
    cairo_pattern_set_matrix(pattern, &mat);
    cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);

    // Draw the image.
    cairo_set_source(context, pattern);
    cairo_rectangle(context, srcX, srcY, dstRect.width(), dstRect.height());
    cairo_fill(context);
    cairo_restore(context);

    startAnimation();
}

}
