/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef GradientAttributes_h
#define GradientAttributes_h

#if ENABLE(SVG)
#include "SVGGradientElement.h"
#include "SVGLength.h"

namespace WebCore {

struct GradientAttributes {
    GradientAttributes()
        : m_spreadMethod(SVGSpreadMethodPad)
        , m_gradientUnits(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        , m_spreadMethodSet(false)
        , m_gradientUnitsSet(false)
        , m_gradientTransformSet(false)
        , m_stopsSet(false)
    {
    }

    SVGSpreadMethodType spreadMethod() const { return m_spreadMethod; }
    SVGUnitTypes::SVGUnitType gradientUnits() const { return m_gradientUnits; }
    AffineTransform gradientTransform() const { return m_gradientTransform; }
    const Vector<Gradient::ColorStop>& stops() const { return m_stops; }

    void setSpreadMethod(SVGSpreadMethodType value)
    {
        m_spreadMethod = value;
        m_spreadMethodSet = true;
    }

    void setGradientUnits(SVGUnitTypes::SVGUnitType unitType)
    {
        m_gradientUnits = unitType;
        m_gradientUnitsSet = true;
    }

    void setGradientTransform(const AffineTransform& value)
    {
        m_gradientTransform = value;
        m_gradientTransformSet = true;
    }

    void setStops(const Vector<Gradient::ColorStop>& value)
    {
        m_stops = value;
        m_stopsSet = true;
    } 

    bool hasSpreadMethod() const { return m_spreadMethodSet; }
    bool hasGradientUnits() const { return m_gradientUnitsSet; }
    bool hasGradientTransform() const { return m_gradientTransformSet; }
    bool hasStops() const { return m_stopsSet; }

private:
    // Properties
    SVGSpreadMethodType m_spreadMethod;
    SVGUnitTypes::SVGUnitType m_gradientUnits;
    AffineTransform m_gradientTransform;
    Vector<Gradient::ColorStop> m_stops;

    // Property states
    bool m_spreadMethodSet : 1;
    bool m_gradientUnitsSet : 1;
    bool m_gradientTransformSet : 1;
    bool m_stopsSet : 1;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
