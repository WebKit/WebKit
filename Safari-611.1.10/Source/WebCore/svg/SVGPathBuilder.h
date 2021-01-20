/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2009, 2015 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "FloatPoint.h"
#include "SVGPathConsumer.h"

namespace WebCore {

class Path;

class SVGPathBuilder final : public SVGPathConsumer {
public:
    SVGPathBuilder(Path&);

private:
    void incrementPathSegmentCount() final { }
    bool continueConsuming() final { return true; }

    // Used in UnalteredParsing/NormalizedParsing modes.
    void moveTo(const FloatPoint&, bool closed, PathCoordinateMode) final;
    void lineTo(const FloatPoint&, PathCoordinateMode) final;
    void curveToCubic(const FloatPoint&, const FloatPoint&, const FloatPoint&, PathCoordinateMode) final;
    void closePath() final;

    // Only used in UnalteredParsing mode.
    void lineToHorizontal(float, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }
    void lineToVertical(float, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }
    void curveToCubicSmooth(const FloatPoint&, const FloatPoint&, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }
    void curveToQuadratic(const FloatPoint&, const FloatPoint&, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }
    void curveToQuadraticSmooth(const FloatPoint&, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }
    void arcTo(float, float, float, bool, bool, const FloatPoint&, PathCoordinateMode) final { ASSERT_NOT_REACHED(); }

    Path& m_path;
    FloatPoint m_current;
};

} // namespace WebCore
