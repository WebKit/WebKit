/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
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

#include "RenderSVGShape.h"

namespace WebCore {

class RenderSVGPath final : public RenderSVGShape {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGPath);
public:
    RenderSVGPath(SVGGraphicsElement&, RenderStyle&&);
    virtual ~RenderSVGPath();

private:
    bool isSVGPath() const override { return true; }
    const char* renderName() const override { return "RenderSVGPath"; }

    void updateShapeFromElement() override;
    FloatRect calculateUpdatedStrokeBoundingBox() const;

    void strokeShape(GraphicsContext&) const override;
    bool shapeDependentStrokeContains(const FloatPoint&, PointCoordinateSpace = GlobalCoordinateSpace) override;

    bool shouldStrokeZeroLengthSubpath() const;
    Path* zeroLengthLinecapPath(const FloatPoint&) const;
    FloatRect zeroLengthSubpathRect(const FloatPoint&, float) const;
    void updateZeroLengthSubpaths();

    bool isRenderingDisabled() const override;

    Vector<FloatPoint> m_zeroLengthLinecapLocations;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGPath, isSVGPath())
