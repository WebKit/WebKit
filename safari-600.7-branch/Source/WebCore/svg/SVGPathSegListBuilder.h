/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef SVGPathSegListBuilder_h
#define SVGPathSegListBuilder_h

#include "FloatPoint.h"
#include "SVGPathConsumer.h"
#include "SVGPathSegList.h"

namespace WebCore {

class SVGPathElement;

class SVGPathSegListBuilder : public SVGPathConsumer {
public:
    SVGPathSegListBuilder();

    void setCurrentSVGPathElement(SVGPathElement* pathElement) { m_pathElement = pathElement; }
    void setCurrentSVGPathSegList(SVGPathSegList& pathSegList) { m_pathSegList = &pathSegList; }
    void setCurrentSVGPathSegRole(SVGPathSegRole pathSegRole) { m_pathSegRole = pathSegRole; }

private:
    virtual void incrementPathSegmentCount() override { }
    virtual bool continueConsuming() override { return true; }
    virtual void cleanup() override
    {
        m_pathElement = 0;
        m_pathSegList = 0;
        m_pathSegRole = PathSegUndefinedRole;
    }

    // Used in UnalteredParsing/NormalizedParsing modes.
    virtual void moveTo(const FloatPoint&, bool closed, PathCoordinateMode) override;
    virtual void lineTo(const FloatPoint&, PathCoordinateMode) override;
    virtual void curveToCubic(const FloatPoint&, const FloatPoint&, const FloatPoint&, PathCoordinateMode) override;
    virtual void closePath() override;

    // Only used in UnalteredParsing mode.
    virtual void lineToHorizontal(float, PathCoordinateMode) override;
    virtual void lineToVertical(float, PathCoordinateMode) override;
    virtual void curveToCubicSmooth(const FloatPoint&, const FloatPoint&, PathCoordinateMode) override;
    virtual void curveToQuadratic(const FloatPoint&, const FloatPoint&, PathCoordinateMode) override;
    virtual void curveToQuadraticSmooth(const FloatPoint&, PathCoordinateMode) override;
    virtual void arcTo(float, float, float, bool largeArcFlag, bool sweepFlag, const FloatPoint&, PathCoordinateMode) override;

    SVGPathElement* m_pathElement;
    SVGPathSegList* m_pathSegList;
    SVGPathSegRole m_pathSegRole;
};

} // namespace WebCore

#endif // SVGPathSegListBuilder_h
