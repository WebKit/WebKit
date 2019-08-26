/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#include "Image.h"
#include "IntSize.h"
#include <JavaScriptCore/Uint8ClampedArray.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

interface ID2D1RenderTarget;
interface ID2D1Bitmap;

namespace WebCore {

class PlatformContextDirect2D;

struct ImageBufferData {
    IntSize backingStoreSize;
    Checked<unsigned, RecordOverflow> bytesPerRow;

    // Only for software ImageBuffers.
    Vector<char> data;
    std::unique_ptr<PlatformContextDirect2D> platformContext;
    std::unique_ptr<GraphicsContext> context;
    COMPtr<ID2D1Bitmap> bitmap;

    RefPtr<Uint8ClampedArray> getData(AlphaPremultiplication, const IntRect&, const IntSize&, bool accelerateRendering, float resolutionScale) const;
    void putData(const Uint8ClampedArray& source, AlphaPremultiplication sourceFormat, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, const IntSize&, bool accelerateRendering, float resolutionScale);

    COMPtr<ID2D1Bitmap> compatibleBitmap(ID2D1RenderTarget*);
};

} // namespace WebCore
