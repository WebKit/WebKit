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

#include "SVGPathSegment.h"
#include "SVGPathSource.h"
#include <wtf/Vector.h>

namespace WebCore {

// Feeds SVGPathParser from the sequence handed to setPathData(), so the write
// path reuses the same parser as the "d" attribute rather than duplicating it.
//
// Validation lives here. An entry whose type is not a single known command
// letter, whose value count is wrong for that command, or which holds a
// non-finite number, reports SVGPathSegType::Unknown. The parser stops at the
// first Unknown, and the byte stream keeps whatever came before it, which is
// the valid-prefix truncation the WPT tests require.
//
// The spec defines none of this. See setPathData-malformed.html, and the spec
// issues filed alongside it.
class SVGPathSegmentsSource final : public SVGPathSource {
public:
    explicit SVGPathSegmentsSource(const Vector<SVGPathSegment>&);

private:
    bool hasMoreData() const final;
    bool moveToNextToken() final;
    SVGPathSegType nextCommand(SVGPathSegType previousCommand) final;
    std::optional<SVGPathSegType> parseSVGSegmentType() final;

    std::optional<MoveToSegment> parseMoveToSegment(FloatPoint) final;
    std::optional<LineToSegment> parseLineToSegment(FloatPoint) final;
    std::optional<LineToHorizontalSegment> parseLineToHorizontalSegment(FloatPoint) final;
    std::optional<LineToVerticalSegment> parseLineToVerticalSegment(FloatPoint) final;
    std::optional<CurveToCubicSegment> parseCurveToCubicSegment(FloatPoint) final;
    std::optional<CurveToCubicSmoothSegment> parseCurveToCubicSmoothSegment(FloatPoint) final;
    std::optional<CurveToQuadraticSegment> parseCurveToQuadraticSegment(FloatPoint) final;
    std::optional<CurveToQuadraticSmoothSegment> parseCurveToQuadraticSmoothSegment(FloatPoint) final;
    std::optional<ArcToSegment> parseArcToSegment(FloatPoint) final;

    // The type of the entry at m_index, or Unknown if that entry is not valid.
    SVGPathSegType validatedTypeAtCurrentIndex() const;

    // As above, and steps past the entry when it is a closepath, which is the one
    // command the parser consumes without asking the source for values.
    SVGPathSegType consumeTypeAtCurrentIndex();

    // Values of the entry being consumed, then step past it.
    const Vector<float>& takeValues();

    const Vector<SVGPathSegment>& m_segments;
    size_t m_index { 0 };
};

} // namespace WebCore
