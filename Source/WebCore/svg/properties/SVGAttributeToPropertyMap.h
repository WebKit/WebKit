/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include "SVGPropertyInfo.h"
#include <wtf/HashMap.h>

namespace WebCore {

class SVGAnimatedProperty;
class SVGElement;

class SVGAttributeToPropertyMap {
public:
    bool isEmpty() const { return m_map.isEmpty(); }

    void addProperty(const SVGPropertyInfo&);
    void addProperties(const SVGAttributeToPropertyMap&);

    Vector<RefPtr<SVGAnimatedProperty>> properties(SVGElement&, const QualifiedName& attributeName) const;
    Vector<AnimatedPropertyType> types(const QualifiedName& attributeName) const;

    void synchronizeProperties(SVGElement&) const;
    bool synchronizeProperty(SVGElement&, const QualifiedName& attributeName) const;

private:
    typedef Vector<const SVGPropertyInfo*> PropertyInfoVector;
    HashMap<QualifiedName, PropertyInfoVector> m_map;
};

} // namespace WebCore
