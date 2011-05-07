/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#include "config.h"
#include "BitLockerSkia.h"

#include "IntRect.h"

#include "SkCanvas.h"
#include "SkDevice.h"
#include "SkRegion.h"

#include <CoreGraphics/CoreGraphics.h>

namespace WebCore {

static CGAffineTransform SkMatrixToCGAffineTransform(const SkMatrix& matrix)
{
    // CGAffineTransforms don't support perspective transforms, so make sure
    // we don't get those.
    ASSERT(!matrix[SkMatrix::kMPersp0]);
    ASSERT(!matrix[SkMatrix::kMPersp1]);
    ASSERT(matrix[SkMatrix::kMPersp2] == 1.0f);

    return CGAffineTransformMake(
        matrix[SkMatrix::kMScaleX],
        matrix[SkMatrix::kMSkewY],
        matrix[SkMatrix::kMSkewX],
        matrix[SkMatrix::kMScaleY],
        matrix[SkMatrix::kMTransX],
        matrix[SkMatrix::kMTransY]);
}

BitLockerSkia::BitLockerSkia(SkCanvas* canvas)
    : m_canvas(canvas)
    , m_cgContext(0)
{
}

BitLockerSkia::~BitLockerSkia()
{
    releaseIfNeeded();
}

void BitLockerSkia::releaseIfNeeded()
{
    if (!m_cgContext)
        return;
    m_canvas->getDevice()->accessBitmap(true).unlockPixels();
    CGContextRelease(m_cgContext);
    m_cgContext = 0;
}

CGContextRef BitLockerSkia::cgContext()
{
    SkDevice* device = m_canvas->getDevice();
    ASSERT(device);
    if (!device)
        return 0;
    releaseIfNeeded();
    const SkBitmap& bitmap = device->accessBitmap(true);
    bitmap.lockPixels();
    void* pixels = bitmap.getPixels();
    m_cgContext = CGBitmapContextCreate(pixels, device->width(),
        device->height(), 8, bitmap.rowBytes(), CGColorSpaceCreateDeviceRGB(), 
        kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);

    // Apply device matrix.
    CGAffineTransform contentsTransform = CGAffineTransformMakeScale(1, -1);
    contentsTransform = CGAffineTransformTranslate(contentsTransform, 0, -device->height());
    CGContextConcatCTM(m_cgContext, contentsTransform);

    // Apply clip in device coordinates.
    CGMutablePathRef clipPath = CGPathCreateMutable();
    SkRegion::Iterator iter(m_canvas->getTotalClip());
    for (; !iter.done(); iter.next()) {
        IntRect rect = iter.rect();
        CGPathAddRect(clipPath, 0, rect);
    }
    CGContextAddPath(m_cgContext, clipPath);
    CGContextClip(m_cgContext);
    CGPathRelease(clipPath);

    // Apply content matrix.
    const SkMatrix& skMatrix = m_canvas->getTotalMatrix();
    CGAffineTransform affine = SkMatrixToCGAffineTransform(skMatrix);
    CGContextConcatCTM(m_cgContext, affine);

    return m_cgContext;
}

}
