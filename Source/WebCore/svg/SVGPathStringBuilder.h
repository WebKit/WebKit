/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "SVGPathConsumer.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

class SVGPathStringBuilder final : public SVGPathConsumer {
public:
    WEBCORE_EXPORT SVGPathStringBuilder();
    WEBCORE_EXPORT virtual ~SVGPathStringBuilder();

    WEBCORE_EXPORT String result();

    void incrementPathSegmentCount() final;
    bool continueConsuming() final;

    // Used in UnalteredParsing/NormalizedParsing modes.
    WEBCORE_EXPORT void moveTo(const FloatPoint&, bool closed, PathCoordinateMode) final;
    WEBCORE_EXPORT void lineTo(const FloatPoint&, PathCoordinateMode) final;
    WEBCORE_EXPORT void curveToCubic(const FloatPoint&, const FloatPoint&, const FloatPoint&, PathCoordinateMode) final;
    WEBCORE_EXPORT void closePath() final;

    // Only used in UnalteredParsing mode.
    void lineToHorizontal(float, PathCoordinateMode) final;
    void lineToVertical(float, PathCoordinateMode) final;
    void curveToCubicSmooth(const FloatPoint&, const FloatPoint&, PathCoordinateMode) final;
    WEBCORE_EXPORT void curveToQuadratic(const FloatPoint&, const FloatPoint&, PathCoordinateMode) final;
    void curveToQuadraticSmooth(const FloatPoint&, PathCoordinateMode) final;
    void arcTo(float, float, float, bool largeArcFlag, bool sweepFlag, const FloatPoint&, PathCoordinateMode) final;

private:
    StringBuilder m_stringBuilder;
};

} // namespace WebCore
