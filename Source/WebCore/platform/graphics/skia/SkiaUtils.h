/*
 * Copyright (c) 2006,2007,2008, Google Inc. All rights reserved.
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

// All of the functions in this file should move to new homes and this file should be deleted.

#ifndef SkiaUtils_h
#define SkiaUtils_h

#include <wtf/MathExtras.h>
#include "GraphicsContext.h"
#include "SkPath.h"
#include "SkXfermode.h"

class SkCanvas;
class SkRegion;

namespace WebCore {

SkXfermode::Mode WebCoreCompositeToSkiaComposite(CompositeOperator);

// move this guy into SkColor.h
SkColor SkPMColorToColor(SkPMColor);

// This should be an operator on Color
Color SkPMColorToWebCoreColor(SkPMColor);

// Skia has problems when passed infinite, etc floats, filter them to 0.
inline SkScalar WebCoreFloatToSkScalar(float f)
{
    return SkFloatToScalar(isfinite(f) ? f : 0);
}

inline SkScalar WebCoreDoubleToSkScalar(double d)
{
    return SkDoubleToScalar(isfinite(d) ? d : 0);
}

// Computes the smallest rectangle that, which when drawn to the given canvas,
// will cover the same area as the source rectangle. It will clip to the canvas'
// clip, doing the necessary coordinate transforms.
//
// srcRect and destRect can be the same.
void ClipRectToCanvas(const SkCanvas&, const SkRect& srcRect, SkRect* destRect);

// Determine if a given WebKit point is contained in a path
bool SkPathContainsPoint(SkPath*, const FloatPoint&, SkPath::FillType);

// Returns a statically allocated 1x1 GraphicsContext intended for temporary
// operations. Please save() the state and restore() it when you're done with
// the context.
GraphicsContext* scratchContext();

}  // namespace WebCore

#endif  // SkiaUtils_h
