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

#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGPrimitivePropertyAnimatorImpl.h"
#include "SVGValuePropertyAnimatorImpl.h"
#include "SVGValuePropertyListAnimatorImpl.h"

namespace WebCore {

class SVGPropertyAnimatorFactory {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SVGPropertyAnimatorFactory() = default;

    static bool isKnownAttribute(const QualifiedName& attributeName)
    {
        return attributeAnimatorCreator().contains(attributeName.impl());
    }

    RefPtr<SVGAttributeAnimator> createAnimator(const QualifiedName& attributeName, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
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
            std::function<Ref<SVGAttributeAnimator>(const QualifiedName&, Ref<SVGProperty>&&, AnimationMode, CalcMode, bool, bool)>
        >
    >;

    static auto createColorAnimator(const QualifiedName& attributeName, Ref<SVGProperty>&& property, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return SVGColorAnimator::create(attributeName, WTFMove(property), animationMode, calcMode, isAccumulated, isAdditive);
    }

    static auto createLengthAnimator(const QualifiedName& attributeName, Ref<SVGProperty>&& property, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return SVGLengthAnimator::create(attributeName, WTFMove(property), animationMode, calcMode, isAccumulated, isAdditive);
    }

    static auto createLengthListAnimator(const QualifiedName& attributeName, Ref<SVGProperty>&& property, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return SVGLengthListAnimator::create(attributeName, WTFMove(property), animationMode, calcMode, isAccumulated, isAdditive);
    }

    static auto createNumberAnimator(const QualifiedName& attributeName, Ref<SVGProperty>&& property, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return SVGNumberAnimator::create(attributeName,  WTFMove(property), animationMode, calcMode, isAccumulated, isAdditive);
    }
    
    static auto createStringAnimator(const QualifiedName& attributeName, Ref<SVGProperty>&& property, AnimationMode animationMode, CalcMode calcMode, bool isAccumulated, bool isAdditive)
    {
        return SVGStringAnimator::create(attributeName, WTFMove(property), animationMode, calcMode, isAccumulated, isAdditive);
    }

    static const AttributeAnimatorCreator& attributeAnimatorCreator()
    {
        static NeverDestroyed<AttributeAnimatorCreator> map = AttributeAnimatorCreator({
            { SVGNames::colorAttr->impl(),              std::make_pair(SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator) },
            { SVGNames::fillAttr->impl(),               std::make_pair(SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator) },
            { SVGNames::flood_colorAttr->impl(),        std::make_pair(SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator) },
            { SVGNames::lighting_colorAttr->impl(),     std::make_pair(SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator) },
            { SVGNames::stop_colorAttr->impl(),         std::make_pair(SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator) },
            { SVGNames::strokeAttr->impl(),             std::make_pair(SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator) },

            { SVGNames::font_sizeAttr->impl(),          std::make_pair([]() { return SVGLength::create(); }, SVGPropertyAnimatorFactory::createLengthAnimator) },
            { SVGNames::kerningAttr->impl(),            std::make_pair([]() { return SVGLength::create(); }, SVGPropertyAnimatorFactory::createLengthAnimator) },
            { SVGNames::letter_spacingAttr->impl(),     std::make_pair([]() { return SVGLength::create(); }, SVGPropertyAnimatorFactory::createLengthAnimator) },
            { SVGNames::stroke_dashoffsetAttr->impl(),  std::make_pair([]() { return SVGLength::create(); }, SVGPropertyAnimatorFactory::createLengthAnimator) },
            { SVGNames::stroke_widthAttr->impl(),       std::make_pair([]() { return SVGLength::create(); }, SVGPropertyAnimatorFactory::createLengthAnimator) },
            { SVGNames::word_spacingAttr->impl(),       std::make_pair([]() { return SVGLength::create(); }, SVGPropertyAnimatorFactory::createLengthAnimator) },

            { SVGNames::stroke_dasharrayAttr->impl(),   std::make_pair([]() { return SVGLengthList::create(); }, SVGPropertyAnimatorFactory::createLengthListAnimator) },
            
            { SVGNames::fill_opacityAttr->impl(),       std::make_pair(SVGValueProperty<float>::create, SVGPropertyAnimatorFactory::createNumberAnimator) },
            { SVGNames::flood_opacityAttr->impl(),      std::make_pair(SVGValueProperty<float>::create, SVGPropertyAnimatorFactory::createNumberAnimator) },
            { SVGNames::opacityAttr->impl(),            std::make_pair(SVGValueProperty<float>::create, SVGPropertyAnimatorFactory::createNumberAnimator) },
            { SVGNames::stop_opacityAttr->impl(),       std::make_pair(SVGValueProperty<float>::create, SVGPropertyAnimatorFactory::createNumberAnimator) },
            { SVGNames::stroke_miterlimitAttr->impl(),  std::make_pair(SVGValueProperty<float>::create, SVGPropertyAnimatorFactory::createNumberAnimator) },
            { SVGNames::stroke_opacityAttr->impl(),     std::make_pair(SVGValueProperty<float>::create, SVGPropertyAnimatorFactory::createNumberAnimator) },
            
            { SVGNames::alignment_baselineAttr->impl(),             std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::baseline_shiftAttr->impl(),                 std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::buffered_renderingAttr->impl(),             std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::clip_pathAttr->impl(),                      std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::clip_ruleAttr->impl(),                      std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::color_interpolationAttr->impl(),            std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::color_interpolation_filtersAttr->impl(),    std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::color_profileAttr->impl(),                  std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::color_renderingAttr->impl(),                std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::cursorAttr->impl(),                         std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::displayAttr->impl(),                        std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::dominant_baselineAttr->impl(),              std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::fill_ruleAttr->impl(),                      std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::filterAttr->impl(),                         std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::font_familyAttr->impl(),                    std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::font_stretchAttr->impl(),                   std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::font_styleAttr->impl(),                     std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::font_variantAttr->impl(),                   std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::font_weightAttr->impl(),                    std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::image_renderingAttr->impl(),                std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::marker_endAttr->impl(),                     std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::marker_midAttr->impl(),                     std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::marker_startAttr->impl(),                   std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::maskAttr->impl(),                           std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::mask_typeAttr->impl(),                      std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::overflowAttr->impl(),                       std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::paint_orderAttr->impl(),                    std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::pointer_eventsAttr->impl(),                 std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::shape_renderingAttr->impl(),                std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::stroke_linecapAttr->impl(),                 std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::stroke_linejoinAttr->impl(),                std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::text_anchorAttr->impl(),                    std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::text_decorationAttr->impl(),                std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::text_renderingAttr->impl(),                 std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::vector_effectAttr->impl(),                  std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) },
            { SVGNames::visibilityAttr->impl(),                     std::make_pair(SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator) }
        });
        return map;
    }

    using AttributeProperty = HashMap<QualifiedName, Ref<SVGProperty>>;
    AttributeProperty m_attributeProperty;
};
    
}
