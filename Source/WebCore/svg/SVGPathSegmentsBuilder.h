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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SVGPathConsumer.h"
#include "SVGPathSegment.h"
#include <wtf/Vector.h>

namespace WebCore {

// Collects the segments emitted by an SVGPathParser walk into the sequence
// getPathData() hands back. The shape mirrors SVGPathStringBuilder: same ten
// callbacks, same choice of command letter from the callback plus the
// PathCoordinateMode. The difference is only where a segment goes, and that
// the values stay as floats rather than going through a number formatter.
//
// The destination is owned by the caller, as in SVGPathByteStreamBuilder.
class SVGPathSegmentsBuilder final : public SVGPathConsumer {
public:
    explicit SVGPathSegmentsBuilder(Vector<SVGPathSegment>&);

private:
    void incrementPathSegmentCount() final { }
    bool continueConsuming() final { return true; }

    // Used in UnalteredParsing/NormalizedParsing modes.
    void moveTo(const FloatPoint&, bool closed, PathCoordinateMode) final;
    void lineTo(const FloatPoint&, PathCoordinateMode) final;
    void curveToCubic(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint&, PathCoordinateMode) final;
    void closePath() final;

    // Only used in UnalteredParsing mode.
    void lineToHorizontal(float, PathCoordinateMode) final;
    void lineToVertical(float, PathCoordinateMode) final;
    void curveToCubicSmooth(const FloatPoint& controlPoint2, const FloatPoint& targetPoint, PathCoordinateMode) final;
    void curveToQuadratic(const FloatPoint& controlPoint, const FloatPoint& targetPoint, PathCoordinateMode) final;
    void curveToQuadraticSmooth(const FloatPoint& targetPoint, PathCoordinateMode) final;
    void arcTo(float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, const FloatPoint&, PathCoordinateMode) final;

    void appendSegment(SVGPathSegType absoluteType, SVGPathSegType relativeType, PathCoordinateMode, Vector<float>&&);

    Vector<SVGPathSegment>& m_result;
};

} // namespace WebCore
