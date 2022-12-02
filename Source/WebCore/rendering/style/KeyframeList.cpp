/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "KeyframeList.h"

#include "Animation.h"
#include "CSSAnimation.h"
#include "CSSKeyframeRule.h"
#include "CSSPropertyAnimation.h"
#include "CompositeOperation.h"
#include "Element.h"
#include "KeyframeEffect.h"
#include "RenderObject.h"
#include "StyleResolver.h"

namespace WebCore {

KeyframeList::~KeyframeList() = default;

void KeyframeList::clear()
{
    m_keyframes.clear();
    m_properties.clear();
}

bool KeyframeList::operator==(const KeyframeList& o) const
{
    if (m_keyframes.size() != o.m_keyframes.size())
        return false;

    auto it2 = o.m_keyframes.begin();
    for (auto it1 = m_keyframes.begin(); it1 != m_keyframes.end(); ++it1, ++it2) {
        if (it1->key() != it2->key())
            return false;
        if (*it1->style() != *it2->style())
            return false;
    }

    return true;
}

void KeyframeList::insert(KeyframeValue&& keyframe)
{
    if (keyframe.key() < 0 || keyframe.key() > 1)
        return;

    bool inserted = false;
    size_t i = 0;
    for (; i < m_keyframes.size(); ++i) {
        if (m_keyframes[i].key() > keyframe.key()) {
            // insert before
            m_keyframes.insert(i, WTFMove(keyframe));
            inserted = true;
            break;
        }
    }

    if (!inserted)
        m_keyframes.append(WTFMove(keyframe));

    auto& insertedKeyframe = m_keyframes[i];
    for (auto& property : insertedKeyframe.properties())
        m_properties.add(property);
    for (auto& customProperty : insertedKeyframe.customProperties())
        m_customProperties.add(customProperty);
}

bool KeyframeList::hasImplicitKeyframes() const
{
    return size() && (m_keyframes[0].key() || m_keyframes[size() - 1].key() != 1);
}

void KeyframeList::copyKeyframes(KeyframeList& other)
{
    for (auto& keyframe : other) {
        KeyframeValue keyframeValue(keyframe.key(), RenderStyle::clonePtr(*keyframe.style()));
        for (auto propertyId : keyframe.properties())
            keyframeValue.addProperty(propertyId);
        for (auto& customProperty : keyframe.customProperties())
            keyframeValue.addCustomProperty(customProperty);
        keyframeValue.setTimingFunction(keyframe.timingFunction());
        keyframeValue.setCompositeOperation(keyframe.compositeOperation());
        insert(WTFMove(keyframeValue));
    }
}

static const StyleRuleKeyframe& zeroPercentKeyframe()
{
    static LazyNeverDestroyed<Ref<StyleRuleKeyframe>> rule;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        rule.construct(StyleRuleKeyframe::create(MutableStyleProperties::create()));
        rule.get()->setKey(0);
    });
    return rule.get().get();
}

static const StyleRuleKeyframe& hundredPercentKeyframe()
{
    static LazyNeverDestroyed<Ref<StyleRuleKeyframe>> rule;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        rule.construct(StyleRuleKeyframe::create(MutableStyleProperties::create()));
        rule.get()->setKey(1);
    });
    return rule.get().get();
}

void KeyframeList::fillImplicitKeyframes(const KeyframeEffect& effect, const RenderStyle& underlyingStyle)
{
    if (isEmpty())
        return;

    ASSERT(effect.target());
    auto& element = *effect.target();
    auto& styleResolver = element.styleResolver();

    // We need to establish which properties are implicit for 0% and 100%.
    // We start each list off with the full list of properties, and see if
    // any 0% and 100% keyframes specify them.
    auto zeroKeyframeImplicitProperties = m_properties;
    auto oneKeyframeImplicitProperties = m_properties;
    zeroKeyframeImplicitProperties.remove(CSSPropertyCustom);
    oneKeyframeImplicitProperties.remove(CSSPropertyCustom);

    KeyframeValue* implicitZeroKeyframe = nullptr;
    KeyframeValue* implicitOneKeyframe = nullptr;

    auto isSuitableKeyframeForImplicitValues = [&](const KeyframeValue& keyframe) {
        auto* timingFunction = keyframe.timingFunction();

        // If there is no timing function set on the keyframe, then it uses the element's
        // timing function, which makes this keyframe suitable.
        if (!timingFunction)
            return true;

        if (auto* cssAnimation = dynamicDowncast<CSSAnimation>(effect.animation())) {
            auto* animationWideTimingFunction = cssAnimation->backingAnimation().defaultTimingFunctionForKeyframes();
            // If we're dealing with a CSS Animation and if that CSS Animation's backing animation
            // has a default timing function set, then if that keyframe's timing function matches,
            // that keyframe is suitable.
            if (animationWideTimingFunction)
                return timingFunction == animationWideTimingFunction;
            // Otherwise, the keyframe will be suitable if its timing function matches the default.
            return timingFunction == &CubicBezierTimingFunction::defaultTimingFunction();
        }

        return false;
    };

    for (auto& keyframe : m_keyframes) {
        if (!keyframe.key()) {
            for (auto cssPropertyId : keyframe.properties())
                zeroKeyframeImplicitProperties.remove(cssPropertyId);
            if (!implicitZeroKeyframe && isSuitableKeyframeForImplicitValues(keyframe))
                implicitZeroKeyframe = &keyframe;
        }
    }

    auto addImplicitKeyframe = [&](double key, const HashSet<CSSPropertyID>& implicitProperties, const StyleRuleKeyframe& keyframeRule, KeyframeValue* existingImplicitKeyframeValue) {
        // If we're provided an existing implicit keyframe, we need to add all the styles for the implicit properties.
        if (existingImplicitKeyframeValue) {
            ASSERT(existingImplicitKeyframeValue->style());
            auto keyframeStyle = RenderStyle::clonePtr(*existingImplicitKeyframeValue->style());
            for (auto cssPropertyId : implicitProperties) {
                CSSPropertyAnimation::blendProperties(&effect, cssPropertyId, *keyframeStyle, underlyingStyle, underlyingStyle, 1, CompositeOperation::Replace);
                existingImplicitKeyframeValue->addProperty(cssPropertyId);
            }
            existingImplicitKeyframeValue->setStyle(WTFMove(keyframeStyle));
            return;
        }

        // Otherwise we create a new keyframe.
        KeyframeValue keyframeValue(key, nullptr);
        keyframeValue.setStyle(styleResolver.styleForKeyframe(element, underlyingStyle, { nullptr }, keyframeRule, keyframeValue));
        for (auto cssPropertyId : implicitProperties)
            keyframeValue.addProperty(cssPropertyId);
        insert(WTFMove(keyframeValue));
    };

    if (!zeroKeyframeImplicitProperties.isEmpty())
        addImplicitKeyframe(0, zeroKeyframeImplicitProperties, zeroPercentKeyframe(), implicitZeroKeyframe);

    for (auto& keyframe : m_keyframes) {
        if (keyframe.key() == 1) {
            for (auto cssPropertyId : keyframe.properties())
                oneKeyframeImplicitProperties.remove(cssPropertyId);
            if (!implicitOneKeyframe && isSuitableKeyframeForImplicitValues(keyframe))
                implicitOneKeyframe = &keyframe;
        }
    }

    if (!oneKeyframeImplicitProperties.isEmpty())
        addImplicitKeyframe(1, oneKeyframeImplicitProperties, hundredPercentKeyframe(), implicitOneKeyframe);
}

bool KeyframeList::containsAnimatableProperty() const
{
    if (!m_customProperties.isEmpty())
        return true;

    for (auto cssPropertyId : m_properties) {
        if (CSSPropertyAnimation::isPropertyAnimatable(cssPropertyId))
            return true;
    }
    return false;
}

bool KeyframeList::containsDirectionAwareProperty() const
{
    for (auto& keyframe : m_keyframes) {
        if (keyframe.containsDirectionAwareProperty())
            return true;
    }
    return false;
}

bool KeyframeList::usesContainerUnits() const
{
    for (auto& keyframe : m_keyframes) {
        if (keyframe.style()->usesContainerUnits())
            return true;
    }
    return false;
}


} // namespace WebCore
