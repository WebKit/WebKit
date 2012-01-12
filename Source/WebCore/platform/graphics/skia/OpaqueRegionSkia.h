/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
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

#ifndef OpaqueRegionSkia_h
#define OpaqueRegionSkia_h

#include "IntRect.h"

#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkPoint.h"
#include "SkRect.h"

namespace WebCore {
class PlatformContextSkia;

// This class is an encapsulation of functionality for PlatformContextSkia, and its methods are mirrored
// there for the outside world. It tracks paints and computes what area will be opaque.
class OpaqueRegionSkia {
public:
    OpaqueRegionSkia();
    virtual ~OpaqueRegionSkia();

    // The resulting opaque region as a single rect.
    IntRect asRect() const;

    void didDrawRect(const PlatformContextSkia*, const SkRect&, const SkPaint&, const SkBitmap*);
    void didDrawPath(const PlatformContextSkia*, const SkPath&, const SkPaint&);
    void didDrawPoints(const PlatformContextSkia*, SkCanvas::PointMode, int numPoints, const SkPoint[], const SkPaint&);
    void didDrawBounded(const PlatformContextSkia*, const SkRect&, const SkPaint&);

private:
    void didDraw(const PlatformContextSkia*, const SkRect&, const SkPaint&, bool drawsOpaque, bool fillsBounds);
    void didDrawUnbounded(const SkPaint&, bool drawsOpaque);
    void markRectAsOpaque(const PlatformContextSkia*, const SkRect&);
    void markRectAsNonOpaque(const SkRect&);

    SkRect m_opaqueRect;
};

}
#endif // OpaqueRegionSkia_h
