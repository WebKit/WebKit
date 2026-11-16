/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/FloatRect.h>
#include <wtf/Vector.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

// Describes the display segments for foldable / dual-screen devices.
// columns == 1 && rows == 1 (the default) means the device is not folded;
// env(viewport-segment-*) variables are undefined in that state.
// segmentRects is stored row-major: segmentRects[y * columns + x] = rect for (col=x, row=y).
struct ViewportSegments {
    unsigned columns { 1 };
    unsigned rows { 1 };
    Vector<FloatRect> segmentRects;

    bool operator==(const ViewportSegments&) const = default;
};

inline WTF::TextStream& operator<<(WTF::TextStream& ts, const ViewportSegments& value)
{
    ts.dumpProperty("columns"_s, value.columns);
    ts.dumpProperty("rows"_s, value.rows);
    ts.dumpProperty("segmentRects"_s, value.segmentRects);

    return ts;
}

} // namespace WebCore
