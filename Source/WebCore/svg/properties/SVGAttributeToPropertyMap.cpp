/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#if ENABLE(SVG)
#include "SVGAttributeToPropertyMap.h"

#include "SVGAnimatedProperty.h"
#include "SVGPropertyInfo.h"

namespace WebCore {

void SVGAttributeToPropertyMap::addProperties(SVGAttributeToPropertyMap& map)
{
    AttributeToPropertiesMap::iterator end = map.m_map.end();
    for (AttributeToPropertiesMap::iterator it = map.m_map.begin(); it != end; ++it) {
        PropertiesVector* vector = it->second;
        ASSERT(vector);

        PropertiesVector::iterator vectorEnd = vector->end();
        for (PropertiesVector::iterator vectorIt = vector->begin(); vectorIt != vectorEnd; ++vectorIt)
            addProperty(*vectorIt);
    }
}

void SVGAttributeToPropertyMap::addProperty(const SVGPropertyInfo* info)
{
    ASSERT(info);
    ASSERT(info->attributeName != anyQName());
    if (PropertiesVector* vector = m_map.get(info->attributeName)) {
        vector->append(info);
        return;
    }
    PropertiesVector* vector = new PropertiesVector;
    vector->append(info);
    m_map.set(info->attributeName, vector);
}

void SVGAttributeToPropertyMap::animatedPropertiesForAttribute(SVGElement* ownerType, const QualifiedName& attributeName, Vector<RefPtr<SVGAnimatedProperty> >& properties)
{
    ASSERT(ownerType);
    PropertiesVector* vector = m_map.get(attributeName);
    if (!vector)
        return;

    PropertiesVector::iterator vectorEnd = vector->end();
    for (PropertiesVector::iterator vectorIt = vector->begin(); vectorIt != vectorEnd; ++vectorIt)
        properties.append(animatedProperty(ownerType, attributeName, *vectorIt));
}

void SVGAttributeToPropertyMap::animatedPropertyTypeForAttribute(const QualifiedName& attributeName, Vector<AnimatedPropertyType>& propertyTypes)
{
    PropertiesVector* vector = m_map.get(attributeName);
    if (!vector)
        return;

    PropertiesVector::iterator vectorEnd = vector->end();
    for (PropertiesVector::iterator vectorIt = vector->begin(); vectorIt != vectorEnd; ++vectorIt)
        propertyTypes.append((*vectorIt)->animatedPropertyType);
}

void SVGAttributeToPropertyMap::synchronizeProperties(SVGElement* contextElement)
{
    ASSERT(contextElement);
    AttributeToPropertiesMap::iterator end = m_map.end();
    for (AttributeToPropertiesMap::iterator it = m_map.begin(); it != end; ++it) {
        PropertiesVector* vector = it->second;
        ASSERT(vector);

        PropertiesVector::iterator vectorEnd = vector->end();
        for (PropertiesVector::iterator vectorIt = vector->begin(); vectorIt != vectorEnd; ++vectorIt)
            synchronizeProperty(contextElement, it->first, *vectorIt);
    } 
}

bool SVGAttributeToPropertyMap::synchronizeProperty(SVGElement* contextElement, const QualifiedName& attributeName)
{
    ASSERT(contextElement);
    PropertiesVector* vector = m_map.get(attributeName);
    if (!vector)
        return false;

    PropertiesVector::iterator vectorEnd = vector->end();
    for (PropertiesVector::iterator vectorIt = vector->begin(); vectorIt != vectorEnd; ++vectorIt)
        synchronizeProperty(contextElement, attributeName, *vectorIt);

    return true;
}

void SVGAttributeToPropertyMap::synchronizeProperty(SVGElement* contextElement, const QualifiedName& attributeName, const SVGPropertyInfo* info)
{
    ASSERT(info);
    ASSERT_UNUSED(attributeName, attributeName == info->attributeName);
    ASSERT(info->synchronizeProperty);
    (*info->synchronizeProperty)(contextElement);
}

PassRefPtr<SVGAnimatedProperty> SVGAttributeToPropertyMap::animatedProperty(SVGElement* contextElement, const QualifiedName& attributeName, const SVGPropertyInfo* info)
{
    ASSERT(info);
    ASSERT_UNUSED(attributeName, attributeName == info->attributeName);
    ASSERT(info->lookupOrCreateWrapperForAnimatedProperty);
    return (*info->lookupOrCreateWrapperForAnimatedProperty)(contextElement);
}

}

#endif
