/*
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

#include "SVGAnimatedPathSegListPropertyTearOff.h"

namespace WebCore {

class SVGPathSegWithContext : public SVGPathSeg {
public:
    SVGPathSegWithContext(const SVGPathElement& element, SVGPathSegRole role)
        : m_role(role)
        , m_element(element.createWeakPtr())
    {
    }

    RefPtr<SVGAnimatedProperty> animatedProperty() const
    {
        if (!m_element)
            return nullptr;

        switch (m_role) {
        case PathSegUndefinedRole:
            return nullptr;
        case PathSegUnalteredRole:
            return SVGAnimatedProperty::lookupWrapper<SVGPathElement, SVGAnimatedPathSegListPropertyTearOff>(m_element.get(), SVGPathElement::dPropertyInfo());
        case PathSegNormalizedRole:
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=15412 - Implement normalized path segment lists!
            return nullptr;
        };

        return nullptr;
    }

    SVGPathElement* contextElement() const { return m_element.get(); }
    SVGPathSegRole role() const { return m_role; }

    void setContextAndRole(SVGPathElement* element, SVGPathSegRole role)
    {
        m_role = role;
        m_element = element ? element->createWeakPtr() : WeakPtr<SVGPathElement>();
    }

protected:
    void commitChange()
    {
        if (!m_element || m_role == PathSegUndefinedRole)
            return;
        m_element->pathSegListChanged(m_role);
    }

private:
    SVGPathSegRole m_role;
    WeakPtr<SVGPathElement> m_element;
};

class SVGPathSegSingleCoordinate : public SVGPathSegWithContext { 
public:
    float x() const { return m_x; }
    void setX(float x)
    {
        m_x = x;
        commitChange();
    }

    float y() const { return m_y; }
    void setY(float y)
    {
        m_y = y;
        commitChange();
    }

protected:
    SVGPathSegSingleCoordinate(const SVGPathElement& element, SVGPathSegRole role, float x, float y)
        : SVGPathSegWithContext(element, role)
        , m_x(x)
        , m_y(y)
    {
    }

private:
    float m_x;
    float m_y;
};

} // namespace WebCore
