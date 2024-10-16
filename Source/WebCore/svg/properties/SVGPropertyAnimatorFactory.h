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
#include <wtf/Function.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

class SVGPropertyAnimatorFactory {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(SVGPropertyAnimatorFactory);
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
    // This UncheckedKeyHashMap maps an attribute name to a pair of static methods. The first one creates a shared
    // Ref<SVGProperty> for the value type of this attribute. The second creates the animator given the
    // attribute name and the shared Ref<SVGProperty>.
    using AttributeAnimatorCreator = UncheckedKeyHashMap<
        QualifiedName::QualifiedNameImpl*,
        std::pair<
            Function<Ref<SVGProperty>()>,
            Function<Ref<SVGAttributeAnimator>(const QualifiedName&, Ref<SVGProperty>&&, AnimationMode, CalcMode, bool, bool)>
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
        using Pair = AttributeAnimatorCreator::KeyValuePairType;
        static NeverDestroyed<AttributeAnimatorCreator> map { AttributeAnimatorCreator::from(
            Pair { SVGNames::colorAttr->impl(),          { SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator} },
            Pair { SVGNames::fillAttr->impl(),           { SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator } },
            Pair { SVGNames::flood_colorAttr->impl(),    { SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator } },
            Pair { SVGNames::lighting_colorAttr->impl(), { SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator } },
            Pair { SVGNames::stop_colorAttr->impl(),     { SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator } },
            Pair { SVGNames::strokeAttr->impl(),         { SVGValueProperty<Color>::create, SVGPropertyAnimatorFactory::createColorAnimator } },

            Pair { SVGNames::font_sizeAttr->impl(),         { []() { return SVGLength::create(); }, SVGPropertyAnimatorFactory::createLengthAnimator } },
            Pair { SVGNames::letter_spacingAttr->impl(),    { []() { return SVGLength::create(); }, SVGPropertyAnimatorFactory::createLengthAnimator } },
            Pair { SVGNames::stroke_dashoffsetAttr->impl(), { []() { return SVGLength::create(); }, SVGPropertyAnimatorFactory::createLengthAnimator } },
            Pair { SVGNames::stroke_widthAttr->impl(),      { []() { return SVGLength::create(); }, SVGPropertyAnimatorFactory::createLengthAnimator } },
            Pair { SVGNames::word_spacingAttr->impl(),      { []() { return SVGLength::create(); }, SVGPropertyAnimatorFactory::createLengthAnimator } },

            Pair { SVGNames::stroke_dasharrayAttr->impl(), { []() { return SVGLengthList::create(); }, SVGPropertyAnimatorFactory::createLengthListAnimator } },

            Pair { SVGNames::fill_opacityAttr->impl(),      { SVGValueProperty<float>::create, SVGPropertyAnimatorFactory::createNumberAnimator } },
            Pair { SVGNames::flood_opacityAttr->impl(),     { SVGValueProperty<float>::create, SVGPropertyAnimatorFactory::createNumberAnimator } },
            Pair { SVGNames::opacityAttr->impl(),           { SVGValueProperty<float>::create, SVGPropertyAnimatorFactory::createNumberAnimator } },
            Pair { SVGNames::stop_opacityAttr->impl(),      { SVGValueProperty<float>::create, SVGPropertyAnimatorFactory::createNumberAnimator } },
            Pair { SVGNames::stroke_miterlimitAttr->impl(), { SVGValueProperty<float>::create, SVGPropertyAnimatorFactory::createNumberAnimator } },
            Pair { SVGNames::stroke_opacityAttr->impl(),    { SVGValueProperty<float>::create, SVGPropertyAnimatorFactory::createNumberAnimator } },

            Pair { SVGNames::alignment_baselineAttr->impl(),          { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::baseline_shiftAttr->impl(),              { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::buffered_renderingAttr->impl(),          { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::clip_pathAttr->impl(),                   { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::clip_ruleAttr->impl(),                   { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::color_interpolationAttr->impl(),         { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::color_interpolation_filtersAttr->impl(), { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::cursorAttr->impl(),                      { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::displayAttr->impl(),                     { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::dominant_baselineAttr->impl(),           { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::fill_ruleAttr->impl(),                   { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::filterAttr->impl(),                      { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::font_familyAttr->impl(),                 { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::font_stretchAttr->impl(),                { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::font_styleAttr->impl(),                  { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::font_variantAttr->impl(),                { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::font_weightAttr->impl(),                 { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::image_renderingAttr->impl(),             { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::marker_endAttr->impl(),                  { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::marker_midAttr->impl(),                  { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::marker_startAttr->impl(),                { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::maskAttr->impl(),                        { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::mask_typeAttr->impl(),                   { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::overflowAttr->impl(),                    { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::paint_orderAttr->impl(),                 { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::pointer_eventsAttr->impl(),              { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::shape_renderingAttr->impl(),             { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::stroke_linecapAttr->impl(),              { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::stroke_linejoinAttr->impl(),             { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::text_anchorAttr->impl(),                 { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::text_decorationAttr->impl(),             { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::text_renderingAttr->impl(),              { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::vector_effectAttr->impl(),               { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } },
            Pair { SVGNames::visibilityAttr->impl(),                  { SVGValueProperty<String>::create, SVGPropertyAnimatorFactory::createStringAnimator } }
        )};
        return map;
    }

    using AttributeProperty = UncheckedKeyHashMap<QualifiedName, Ref<SVGProperty>>;
    AttributeProperty m_attributeProperty;
};
    
}
