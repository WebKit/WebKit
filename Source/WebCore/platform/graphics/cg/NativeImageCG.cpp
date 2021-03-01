/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#include "NativeImage.h"

#if USE(CG)

#include "GraphicsContextCG.h"
#include "SubimageCacheWithTimer.h"

namespace WebCore {

IntSize NativeImage::size() const
{
    return IntSize(CGImageGetWidth(m_platformImage.get()), CGImageGetHeight(m_platformImage.get()));
}

bool NativeImage::hasAlpha() const
{
    CGImageAlphaInfo info = CGImageGetAlphaInfo(m_platformImage.get());
    return (info >= kCGImageAlphaPremultipliedLast) && (info <= kCGImageAlphaFirst);
}

Color NativeImage::singlePixelSolidColor() const
{
    if (size() != IntSize(1, 1))
        return Color();

    unsigned char pixel[4]; // RGBA
    auto bitmapContext = adoptCF(CGBitmapContextCreate(pixel, 1, 1, 8, sizeof(pixel), sRGBColorSpaceRef(), kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big));

    if (!bitmapContext)
        return Color();

    CGContextSetBlendMode(bitmapContext.get(), kCGBlendModeCopy);
    CGContextDrawImage(bitmapContext.get(), CGRectMake(0, 0, 1, 1), m_platformImage.get());

    if (!pixel[3])
        return Color::transparentBlack;

    return makeFromComponentsClampingExceptAlpha<SRGBA<uint8_t>>(pixel[0] * 255 / pixel[3], pixel[1] * 255 / pixel[3], pixel[2] * 255 / pixel[3], pixel[3]);
}

void NativeImage::clearSubimages()
{
#if CACHE_SUBIMAGES
    SubimageCacheWithTimer::clearImage(m_platformImage.get());
#endif
}

}

#endif // USE(CG)
