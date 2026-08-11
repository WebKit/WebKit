/*
 * Copyright (C) 2006, 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "FloatPoint.h"
#include "Path.h"
#include <wtf/Vector.h>

namespace WebCore {

// Precomputes a path's arc-length table so repeated point-at-length queries cost
// O(log n) each rather than re-walking the path per call.
class PathArcLengthMapper {
public:
    struct Position {
        FloatPoint point;
        float angleInDegrees { 0 };
    };

    void appendPathElement(const PathElement& element) { appendPathElement(element.type, element.points); }

    float totalLength() const { return m_totalLength; }

    // Point and tangent at `length` along the path; `length` is clamped to [0, totalLength()].
    Position positionAtLength(float length) const;

private:
    void appendPathElement(PathElement::Type, std::span<const FloatPoint>);
    void addVertex(const FloatPoint&, float segmentLength);

    struct Vertex {
        FloatPoint point;
        float accumulatedLength { 0 };
    };
    Vector<Vertex> m_vertices;
    FloatPoint m_current;
    FloatPoint m_subpathStart;
    float m_totalLength { 0 };
};

} // namespace WebCore
