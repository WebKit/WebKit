/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

namespace WebCore {

// This consumer translates incoming SVG draw commands into absolute coordinates, and forwards it
// to another consumer. convertSVGPathByteStreamToAbsoluteCoordinates uses it to convert relative
// draw commands in an SVG path into absolute.
class SVGPathAbsoluteConverter final : public SVGPathConsumer {
public:
    SVGPathAbsoluteConverter(SVGPathConsumer&);

private:
    void incrementPathSegmentCount() final;
    bool continueConsuming() final;

    // Used in UnalteredParsing/NormalizedParsing modes.
    void moveTo(const FloatPoint& targetPoint, bool closed, PathCoordinateMode) final;
    void lineTo(const FloatPoint& targetPoint, PathCoordinateMode) final;
    void curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode) final;
    void closePath() final;

    // Only used in UnalteredParsing mode.
    void lineToHorizontal(float targetX, PathCoordinateMode) final;
    void lineToVertical(float targetY, PathCoordinateMode) final;
    void curveToCubicSmooth(const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode) final;
    void curveToQuadratic(const FloatPoint& point1, const FloatPoint& targetPoint, PathCoordinateMode) final;
    void curveToQuadraticSmooth(const FloatPoint& targetPoint, PathCoordinateMode) final;
    void arcTo(float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, const FloatPoint& targetPoint, PathCoordinateMode) final;

    SingleThreadWeakRef<SVGPathConsumer> m_consumer;
    FloatPoint m_currentPoint;
    FloatPoint m_subpathPoint;
};

} // namespace WebCore
