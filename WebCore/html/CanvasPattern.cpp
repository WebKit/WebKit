/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "CanvasPattern.h"

#include "CachedImage.h"
#include "ExceptionCode.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "Image.h"

namespace WebCore {

void CanvasPattern::parseRepetitionType(const String& type, bool& repeatX, bool& repeatY, ExceptionCode& ec)
{
    if (type.isEmpty() || type == "repeat") {
        repeatX = true;
        repeatY = true;
        ec = 0;
        return;
    }
    if (type == "no-repeat") {
        repeatX = false;
        repeatY = false;
        ec = 0;
        return;
    }
    if (type == "repeat-x") {
        repeatX = true;
        repeatY = false;
        ec = 0;
        return;
    }
    if (type == "repeat-y") {
        repeatX = false;
        repeatY = true;
        ec = 0;
        return;
    }
    ec = SYNTAX_ERR;
}

#if PLATFORM(CG)

CanvasPattern::CanvasPattern(CGImageRef image, bool repeatX, bool repeatY)
    : m_platformImage(image)
    , m_cachedImage(0)
    , m_repeatX(repeatX)
    , m_repeatY(repeatY)
{
}

#elif PLATFORM(CAIRO)

CanvasPattern::CanvasPattern(cairo_surface_t* surface, bool repeatX, bool repeatY)
    : m_platformImage(cairo_surface_reference(surface))
    , m_cachedImage(0)
    , m_repeatX(repeatX)
    , m_repeatY(repeatY)
{
}

#endif

CanvasPattern::CanvasPattern(CachedImage* cachedImage, bool repeatX, bool repeatY)
    :
#if PLATFORM(CG) || PLATFORM(CAIRO)
      m_platformImage(0)
    ,
#endif
      m_cachedImage(cachedImage)
    , m_repeatX(repeatX)
    , m_repeatY(repeatY)
{
    if (cachedImage)
        cachedImage->ref(this);
}

CanvasPattern::~CanvasPattern()
{
#if PLATFORM(CAIRO)
    if (m_platformImage)
        cairo_surface_destroy(m_platformImage);
#endif
    if (m_cachedImage)
        m_cachedImage->deref(this);
}

#if PLATFORM(CG)

static void patternCallback(void* info, CGContextRef context)
{
    CGImageRef platformImage = static_cast<CanvasPattern*>(info)->platformImage();
    if (platformImage) {
        CGRect rect = GraphicsContext(context).roundToDevicePixels(
            FloatRect(0, 0, CGImageGetWidth(platformImage), CGImageGetHeight(platformImage)));
        CGContextDrawImage(context, rect, platformImage);
        return;
    }

    CachedImage* cachedImage = static_cast<CanvasPattern*>(info)->cachedImage();
    if (!cachedImage)
        return;
    Image* image = cachedImage->image();
    if (!image)
        return;

    FloatRect rect = GraphicsContext(context).roundToDevicePixels(image->rect());

    if (image->getCGImageRef()) {
        CGContextDrawImage(context, rect, image->getCGImageRef());
        // FIXME: We should refactor this code to use the platform-independent 
        // drawing API in all cases. Then, this didDraw call will happen 
        // automatically, and we can remove it.
        cachedImage->didDraw(image);
        return;
    }

    GraphicsContext(context).drawImage(image, rect);
}

static void patternReleaseCallback(void* info)
{
    static_cast<CanvasPattern*>(info)->deref();
}

CGPatternRef CanvasPattern::createPattern(const CGAffineTransform& transform)
{
    CGRect rect;
    rect.origin.x = 0;
    rect.origin.y = 0;
    if (m_platformImage) {
        rect.size.width = CGImageGetWidth(m_platformImage.get());
        rect.size.height = CGImageGetHeight(m_platformImage.get());
    } else {
        if (!m_cachedImage)
            return 0;
        Image* image = m_cachedImage->image();
        if (!image)
            return 0;
        rect.size.width = image->width();
        rect.size.height = image->height();
    }

    CGAffineTransform patternTransform =
        CGAffineTransformTranslate(CGAffineTransformScale(transform, 1, -1), 0, -rect.size.height);

    float xStep = m_repeatX ? rect.size.width : FLT_MAX;
    // If FLT_MAX should also be used for yStep, nothing is rendered. Using fractions of FLT_MAX also
    // result in nothing being rendered. This is not a problem with xStep.
    // INT_MAX is almost correct, but there seems to be some number wrapping occuring making the fill
    // pattern is not filled correctly. 
    // So, just pick a really large number that works. 
    float yStep = m_repeatY ? rect.size.height : (100000000.0f);

    const CGPatternCallbacks patternCallbacks = { 0, patternCallback, patternReleaseCallback };
    ref();
    return CGPatternCreate(this, rect, patternTransform, xStep, yStep,
        kCGPatternTilingConstantSpacing, TRUE, &patternCallbacks);
}

#elif PLATFORM(CAIRO)

cairo_pattern_t* CanvasPattern::createPattern(const cairo_matrix_t& m)
{
    cairo_surface_t* surface = 0;
    if (m_platformImage) {
        surface = m_platformImage;
    } else {
        if (!m_cachedImage)
            return 0;
        Image* image = m_cachedImage->image();
        if (!image)
            return 0;
        surface = image->nativeImageForCurrentFrame();
    }

    if (!surface)
        return 0;

    cairo_pattern_t* pattern = cairo_pattern_create_for_surface(surface);
    cairo_pattern_set_matrix(pattern, &m);
    if (m_repeatX || m_repeatY)
        cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
    return pattern;
}

#endif

}
