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
#include "FloatRect.h"
#include "Image.h"

namespace WebCore {

CanvasPattern::CanvasPattern(CachedImage* cachedImage, const String& repetitionType)
    : m_cachedImage(cachedImage)
    , m_repeatX(!(equalIgnoringCase(repetitionType, "repeat-y") || equalIgnoringCase(repetitionType, "no-repeat")))
    , m_repeatY(!(equalIgnoringCase(repetitionType, "repeat-x") || equalIgnoringCase(repetitionType, "no-repeat")))
{
    if (cachedImage)
        cachedImage->ref(this);
}

CanvasPattern::~CanvasPattern()
{
    if (m_cachedImage)
        m_cachedImage->deref(this);
}

#if __APPLE__

static void patternCallback(void* info, CGContextRef context)
{
    CachedImage* cachedImage = static_cast<CanvasPattern*>(info)->cachedImage();
    if (!cachedImage)
        return;
    Image* image = cachedImage->image();
    if (!image)
        return;

    FloatRect rect = image->rect();
    
    // FIXME: Is using CGImageRef directly really superior to asking the image to draw?
    if (image->getCGImageRef()) {
        CGContextDrawImage(context, rect, image->getCGImageRef());
        return;
    }

    image->drawInRect(rect, rect, Image::CompositeSourceOver, context);
}

static void patternReleaseCallback(void* info)
{
    static_cast<CanvasPattern*>(info)->deref();
}

CGPatternRef CanvasPattern::createPattern(const CGAffineTransform& transform)
{
    if (!m_cachedImage)
        return 0;
    Image* image = m_cachedImage->image();
    if (!image)
        return 0;

    CGAffineTransform patternTransform =
        CGAffineTransformTranslate(CGAffineTransformScale(transform, 1, -1), 0, -image->height());

    float xStep = m_repeatX ? image->width() : FLT_MAX;
    // If FLT_MAX should also be used for yStep, nothing is rendered. Using fractions of FLT_MAX also
    // result in nothing being rendered. This is not a problem with xStep.
    // INT_MAX is almost correct, but there seems to be some number wrapping occuring making the fill
    // pattern is not filled correctly. 
    // So, just pick a really large number that works. 
    float yStep = m_repeatY ? image->height() : (100000000.0);

    const CGPatternCallbacks patternCallbacks = { 0, patternCallback, patternReleaseCallback };
    ref();
    return CGPatternCreate(this, image->rect(), patternTransform, xStep, yStep,
        kCGPatternTilingConstantSpacing, TRUE, &patternCallbacks);
}

#endif

}
