/*
 * Copyright (C) 2019 Apple Inc.  All rights reserved.
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

#pragma once

#include "SVGNames.h"
#include "SVGPrimitivePropertyAnimatorImpl.h"

namespace WebCore {

class SVGPropertyAnimatorFactory {
public:
    SVGPropertyAnimatorFactory() = default;

    static bool isKnownAttribute(const QualifiedName& attributeName)
    {
        return attributeAnimatorCreator().contains(attributeName.impl());
    }

    std::unique_ptr<SVGAttributeAnimator> createAnimator(const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        auto iterator = attributeAnimatorCreator().find(attributeName.impl());
        if (iterator == attributeAnimatorCreator().end())
            return nullptr;

        auto addResult = m_attributeProperty.ensure(attributeName, [&iterator]() {
            return iterator->value.first();
        });
        
        return iterator->value.second(attributeName, addResult.iterator->value.copyRef(), animationMode, calcMode, isAccumulated, isAdditive);
    }

    void animatorWillBeDeleted(const QualifiedName& attributeName)
    {
        auto iterator = m_attributeProperty.find(attributeName);
        if (iterator == m_attributeProperty.end())
            return;

        // If refCount = 1 (in the animator) + 1 (in m_attributeProperty) = 2, the entry can be deleted.
        if (iterator->value->refCount() == 2)
            m_attributeProperty.remove(iterator);
    }

private:
    // This HashMap maps an attribute name to a pair of static methods. The first one creates a shared
    // Ref<SVGProperty> for the value type of this attribute. The second creates the animator given the
    // attribute name and the shared Ref<SVGProperty>.
    using AttributeAnimatorCreator = HashMap<
        QualifiedName::QualifiedNameImpl*,
        std::pair<
            std::function<Ref<SVGProperty>()>,
            std::function<std::unique_ptr<SVGAttributeAnimator>(const QualifiedName&, Ref<SVGProperty>&&, AnimationMode, CalcMode, bool, bool)>
        >
    >;

    static auto createColorAnimator(const QualifiedName& attributeName, Ref<SVGProperty>&& property, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return SVGColorAnimator::create(attributeName, WTFMove(property), animationMode, calcMode, isAccumulated, isAdditive);
    }

    static const AttributeAnimatorCreator& attributeAnimatorCreator()
    {
        static NeverDestroyed<AttributeAnimatorCreator> map = AttributeAnimatorCreator({
            { SVGNames::colorAttr->impl(),          std::make_pair(SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator) },
            { SVGNames::fillAttr->impl(),           std::make_pair(SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator) },
            { SVGNames::flood_colorAttr->impl(),    std::make_pair(SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator) },
            { SVGNames::lighting_colorAttr->impl(), std::make_pair(SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator) },
            { SVGNames::stop_colorAttr->impl(),     std::make_pair(SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator) },
            { SVGNames::strokeAttr->impl(),         std::make_pair(SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator) },
        });
        return map;
    }

    using AttributeProperty = HashMap<QualifiedName, Ref<SVGProperty>>;
    AttributeProperty m_attributeProperty;
};
    
}
