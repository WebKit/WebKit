/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Pattern.h"

#if USE(CG)

#include "AffineTransform.h"
#include "GraphicsContextCG.h"
#include <CoreGraphics/CoreGraphics.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/MainThread.h>

namespace WebCore {

static void patternCallback(void* info, CGContextRef context)
{
    CGImageRef platformImage = static_cast<CGImageRef>(info);
    if (!platformImage)
        return;

    CGRect rect = GraphicsContextCG(context).roundToDevicePixels(
        FloatRect(0, 0, CGImageGetWidth(platformImage), CGImageGetHeight(platformImage)));
    CGContextDrawImage(context, rect, platformImage);
}

static void patternReleaseCallback(void* info)
{
    callOnMainThread([image = adoptCF(static_cast<CGImageRef>(info))] { });
}

RetainPtr<CGPatternRef> Pattern::createPlatformPattern(const AffineTransform& userSpaceTransform) const
{
    FloatRect tileRect = { { }, tileImage().size() };

    AffineTransform patternTransform = userSpaceTransform * patternSpaceTransform();
    patternTransform.scaleNonUniform(1, -1);
    patternTransform.translate(0, -tileRect.height());

    PlatformImagePtr platformImage = tileImage().platformImage();
    if (!platformImage)
        return nullptr;

    // If we're repeating in both directions, we can use image-backed patterns
    // instead of custom patterns, and avoid tiling-edge pixel cracks.
    if (repeatX() && repeatY())
        return adoptCF(CGPatternCreateWithImage2(platformImage.get(), patternTransform, kCGPatternTilingConstantSpacing));

    // If FLT_MAX should also be used for xStep or yStep, nothing is rendered. Using fractions of FLT_MAX also
    // result in nothing being rendered.
    // INT_MAX is almost correct, but there seems to be some number wrapping occurring making the fill
    // pattern is not filled correctly.
    // To make error of floating point less than 0.5, we use the half of the number of mantissa of float (1 << 22).
    CGFloat xStep = repeatX() ? tileRect.width() : (1 << 22);
    CGFloat yStep = repeatY() ? tileRect.height() : (1 << 22);

    // The pattern will release the CGImageRef when it's done rendering in patternReleaseCallback
    CGImageRef image = platformImage.leakRef();

    const CGPatternCallbacks patternCallbacks = { 0, patternCallback, patternReleaseCallback };
    return adoptCF(CGPatternCreate(image, tileRect, patternTransform, xStep, yStep, kCGPatternTilingConstantSpacing, TRUE, &patternCallbacks));
}

}

#endif
