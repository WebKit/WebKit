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

#include "config.h"
#include "SVGAttributeToPropertyMap.h"

#include "SVGAnimatedProperty.h"

namespace WebCore {

void SVGAttributeToPropertyMap::addProperties(const SVGAttributeToPropertyMap& map)
{
    for (auto& vector : map.m_map.values()) {
        ASSERT(!vector.isEmpty());
        auto& properties = m_map.add(vector[0]->attributeName, PropertyInfoVector()).iterator->value;
        properties.reserveCapacity(properties.size() + vector.size());
        for (auto* property : vector)
            properties.uncheckedAppend(property);
    }
}

void SVGAttributeToPropertyMap::addProperty(const SVGPropertyInfo& info)
{
    m_map.add(info.attributeName, PropertyInfoVector()).iterator->value.append(&info);
}

Vector<RefPtr<SVGAnimatedProperty>> SVGAttributeToPropertyMap::properties(SVGElement& contextElement, const QualifiedName& attributeName) const
{
    Vector<RefPtr<SVGAnimatedProperty>> properties;
    auto it = m_map.find(attributeName);
    if (it == m_map.end())
        return properties;
    properties.reserveInitialCapacity(it->value.size());
    for (auto* property : it->value)
        properties.uncheckedAppend(property->lookupOrCreateWrapperForAnimatedProperty(&contextElement));
    return properties;
}

Vector<AnimatedPropertyType> SVGAttributeToPropertyMap::types(const QualifiedName& attributeName) const
{
    Vector<AnimatedPropertyType> types;
    auto it = m_map.find(attributeName);
    if (it == m_map.end())
        return types;
    types.reserveInitialCapacity(it->value.size());
    for (auto* property : it->value)
        types.uncheckedAppend(property->animatedPropertyType);
    return types;
}

void SVGAttributeToPropertyMap::synchronizeProperties(SVGElement& contextElement) const
{
    for (auto& vector : m_map.values()) {
        for (auto* property : vector)
            property->synchronizeProperty(&contextElement);
    } 
}

bool SVGAttributeToPropertyMap::synchronizeProperty(SVGElement& contextElement, const QualifiedName& attributeName) const
{
    auto it = m_map.find(attributeName);
    if (it == m_map.end())
        return false;
    for (auto* property : it->value)
        property->synchronizeProperty(&contextElement);
    return true;
}

}
