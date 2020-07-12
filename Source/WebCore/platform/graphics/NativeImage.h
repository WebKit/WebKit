/*
 * Copyright (C) 2004, 2005, 2006, 2016 Apple Inc.  All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
 * Copyright (C) 2012 Company 100 Inc.
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

#pragma once

#include "ImagePaintingOptions.h"

#if USE(CG)
#include <wtf/RetainPtr.h>
typedef struct CGImage* CGImageRef;
#elif USE(CAIRO)
#include "RefPtrCairo.h"
#elif USE(WINGDI)
#include "SharedBitmap.h"
#endif

#if USE(DIRECT2D)
#include "COMPtr.h"
#include <d2d1.h>
#include <wincodec.h>
#endif

namespace WebCore {

class Color;
class FloatRect;
class IntSize;
class GraphicsContext;

#if USE(CG)
typedef RetainPtr<CGImageRef> NativeImagePtr;
#elif USE(DIRECT2D)
typedef COMPtr<ID2D1Bitmap> NativeImagePtr;
#elif USE(CAIRO)
typedef RefPtr<cairo_surface_t> NativeImagePtr;
#elif USE(WINGDI)
typedef RefPtr<SharedBitmap> NativeImagePtr;
#endif

WEBCORE_EXPORT IntSize nativeImageSize(const NativeImagePtr&);
bool nativeImageHasAlpha(const NativeImagePtr&);
Color nativeImageSinglePixelSolidColor(const NativeImagePtr&);

void drawNativeImage(const NativeImagePtr&, GraphicsContext&, const FloatRect&, const FloatRect&, const IntSize&, const ImagePaintingOptions&);
WEBCORE_EXPORT void drawNativeImage(const NativeImagePtr&, GraphicsContext&, float scaleFactor, const IntPoint& destination, const IntRect& source);

void clearNativeImageSubimages(const NativeImagePtr&);

class NativeImageHandle {
public:
    NativeImagePtr image;
};
    
}
