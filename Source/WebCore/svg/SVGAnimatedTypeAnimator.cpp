/*
 * Copyright (C) Research In Motion Limited 2011-2012. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
#include "SVGAnimatedTypeAnimator.h"

#include "SVGAttributeToPropertyMap.h"
#include "SVGElement.h"

namespace WebCore {

SVGAnimatedTypeAnimator::SVGAnimatedTypeAnimator(AnimatedPropertyType type, SVGAnimationElement* animationElement, SVGElement* contextElement)
    : m_type(type)
    , m_animationElement(animationElement)
    , m_contextElement(contextElement)
{
}

SVGAnimatedTypeAnimator::~SVGAnimatedTypeAnimator() = default;

void SVGAnimatedTypeAnimator::calculateFromAndToValues(std::unique_ptr<SVGAnimatedType>& from, std::unique_ptr<SVGAnimatedType>& to, const String& fromString, const String& toString)
{
    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedTypeAnimator::calculateFromAndByValues(std::unique_ptr<SVGAnimatedType>& from, std::unique_ptr<SVGAnimatedType>& to, const String& fromString, const String& byString)
{
    from = constructFromString(fromString);
    to = constructFromString(byString);
    addAnimatedTypes(from.get(), to.get());
}

SVGElementAnimatedPropertyList SVGAnimatedTypeAnimator::findAnimatedPropertiesForAttributeName(SVGElement& targetElement, const QualifiedName& attributeName)
{
    SVGElementAnimatedPropertyList result;

    if (!SVGAnimatedType::supportsAnimVal(m_type))
        return result;

    auto& propertyMap = targetElement.localAttributeToPropertyMap();
    auto targetProperties = propertyMap.properties(targetElement, attributeName);

    if (targetProperties.isEmpty())
        return result;

    result.append(SVGElementAnimatedProperties { &targetElement, WTFMove(targetProperties) });

    for (SVGElement* instance : targetElement.instances())
        result.append(SVGElementAnimatedProperties { instance, propertyMap.properties(*instance, attributeName) });

#if !ASSERT_DISABLED
    for (auto& animatedProperties : result) {
        for (auto& property : animatedProperties.properties) {
            if (property->animatedPropertyType() != m_type) {
                ASSERT(m_type == AnimatedAngle);
                ASSERT(property->animatedPropertyType() == AnimatedEnumeration);
            }
        }
    }
#endif

    return result;
}

void SVGAnimatedTypeAnimator::setInstanceUpdatesBlocked(SVGElement& element, bool blocked)
{
    element.setInstanceUpdatesBlocked(blocked);
}

} // namespace WebCore
