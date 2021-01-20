/*
 * Copyright (C) 2018-2019 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SVGAttributeAnimator.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSPropertyParser.h"
#include "SVGElement.h"

namespace WebCore {

bool SVGAttributeAnimator::isAnimatedStylePropertyAniamtor(const SVGElement* targetElement) const
{
    return targetElement->isAnimatedStyleAttribute(m_attributeName);
}

void SVGAttributeAnimator::invalidateStyle(SVGElement* targetElement)
{
    SVGElement::InstanceInvalidationGuard guard(*targetElement);
    targetElement->invalidateSVGPresentationAttributeStyle();
}

void SVGAttributeAnimator::applyAnimatedStylePropertyChange(SVGElement* element, CSSPropertyID id, const String& value)
{
    ASSERT(element);
    ASSERT(!element->m_deletionHasBegun);
    ASSERT(id != CSSPropertyInvalid);
    
    if (!element->ensureAnimatedSMILStyleProperties().setProperty(id, value, false))
        return;
    element->invalidateStyle();
}

void SVGAttributeAnimator::applyAnimatedStylePropertyChange(SVGElement* targetElement, const String& value)
{
    ASSERT(targetElement);
    ASSERT(m_attributeName != anyQName());
    
    // FIXME: Do we really need to check both isConnected and !parentNode?
    if (!targetElement->isConnected() || !targetElement->parentNode())
        return;
    
    CSSPropertyID id = cssPropertyID(m_attributeName.localName());
    
    SVGElement::InstanceUpdateBlocker blocker(*targetElement);
    applyAnimatedStylePropertyChange(targetElement, id, value);
    
    // If the target element has instances, update them as well, w/o requiring the <use> tree to be rebuilt.
    for (auto* instance : targetElement->instances())
        applyAnimatedStylePropertyChange(instance, id, value);
}
    
void SVGAttributeAnimator::removeAnimatedStyleProperty(SVGElement* element, CSSPropertyID id)
{
    ASSERT(element);
    ASSERT(!element->m_deletionHasBegun);
    ASSERT(id != CSSPropertyInvalid);

    element->ensureAnimatedSMILStyleProperties().removeProperty(id);
    element->invalidateStyle();
}

void SVGAttributeAnimator::removeAnimatedStyleProperty(SVGElement* targetElement)
{
    ASSERT(targetElement);
    ASSERT(m_attributeName != anyQName());

    // FIXME: Do we really need to check both isConnected and !parentNode?
    if (!targetElement->isConnected() || !targetElement->parentNode())
        return;

    CSSPropertyID id = cssPropertyID(m_attributeName.localName());

    SVGElement::InstanceUpdateBlocker blocker(*targetElement);
    removeAnimatedStyleProperty(targetElement, id);

    // If the target element has instances, update them as well, w/o requiring the <use> tree to be rebuilt.
    for (auto* instance : targetElement->instances())
        removeAnimatedStyleProperty(instance, id);
}
    
void SVGAttributeAnimator::applyAnimatedPropertyChange(SVGElement* element, const QualifiedName& attributeName)
{
    ASSERT(!element->m_deletionHasBegun);
    element->svgAttributeChanged(attributeName);
}

void SVGAttributeAnimator::applyAnimatedPropertyChange(SVGElement* targetElement)
{
    ASSERT(targetElement);
    ASSERT(m_attributeName != anyQName());

    // FIXME: Do we really need to check both isConnected and !parentNode?
    if (!targetElement->isConnected() || !targetElement->parentNode())
        return;

    SVGElement::InstanceUpdateBlocker blocker(*targetElement);
    applyAnimatedPropertyChange(targetElement, m_attributeName);

    // If the target element has instances, update them as well, w/o requiring the <use> tree to be rebuilt.
    for (auto* instance : targetElement->instances())
        applyAnimatedPropertyChange(instance, m_attributeName);
}

}
