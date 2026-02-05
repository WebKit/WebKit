/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#pragma once

#include "CompositeOperation.h"
#include "KeyframeInterpolation.h"
#include "RenderStyle.h"
#include "StyleSingleAnimationRangeName.h"
#include "WebAnimationTypes.h"
#include <wtf/Vector.h>
#include <wtf/HashSet.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class KeyframeEffect;
class StyleProperties;
class TimingFunction;

namespace Style {
class Resolver;
}

class BlendingKeyframe final : public KeyframeInterpolation::Keyframe {
public:
    struct Offset {
        Style::SingleAnimationRangeName name;
        double value;

        Offset(double value)
            : name(Style::SingleAnimationRangeName::Omitted)
            , value(value)
        {
        }

        Offset(Style::SingleAnimationRangeName name, double value)
            : name(name)
            , value(value)
        {
        }
    };

    BlendingKeyframe(Offset&&, std::unique_ptr<RenderStyle>&&);
    BlendingKeyframe(const BlendingKeyframe&);

    BlendingKeyframe(BlendingKeyframe&&) = default;
    BlendingKeyframe& operator=(BlendingKeyframe&&) = default;

    // KeyframeInterpolation::Keyframe
    double offset() const final { return m_computedOffset; }
    std::optional<CompositeOperation> compositeOperation() const final { return m_compositeOperation; }
    bool animatesProperty(KeyframeInterpolation::Property) const final;
    bool isBlendingKeyframe() const final { return true; }

    void addProperty(const AnimatableCSSProperty&);
    const HashSet<AnimatableCSSProperty>& properties() const { return m_properties; }

    const Offset& specifiedOffset() const { return m_specifiedOffset; }
    void setComputedOffset(double offset) { m_computedOffset = offset; }

    bool usesRangeOffset() const;

    const RenderStyle* style() const { return m_style.get(); }
    void setStyle(std::unique_ptr<RenderStyle>&& style) { m_style = WTF::move(style); }

    TimingFunction* timingFunction() const { return m_timingFunction.get(); }
    void setTimingFunction(const RefPtr<TimingFunction>& timingFunction) { m_timingFunction = timingFunction; }

    void setCompositeOperation(std::optional<CompositeOperation> op) { m_compositeOperation = op; }

    bool containsDirectionAwareProperty() const { return m_containsDirectionAwareProperty; }
    void setContainsDirectionAwareProperty(bool containsDirectionAwareProperty) { m_containsDirectionAwareProperty = containsDirectionAwareProperty; }

private:
    Offset m_specifiedOffset;
    double m_computedOffset { std::numeric_limits<double>::quiet_NaN() };
    HashSet<AnimatableCSSProperty> m_properties; // The properties specified in this keyframe.
    std::unique_ptr<RenderStyle> m_style;
    RefPtr<TimingFunction> m_timingFunction;
    std::optional<CompositeOperation> m_compositeOperation;
    bool m_containsDirectionAwareProperty { false };
};

using KeyframesIdentifier = Variant<AtomString, uint64_t>;

class BlendingKeyframes {
public:
    BlendingKeyframes()
        : m_identifier(nextAnonymousIdentifier())
    { }
    explicit BlendingKeyframes(const KeyframesIdentifier& identifier)
        : m_identifier(identifier)
    { }
    ~BlendingKeyframes();

    BlendingKeyframes& operator=(BlendingKeyframes&&) = default;
    bool operator==(const BlendingKeyframes&) const;

    const KeyframesIdentifier& identifier() const { return m_identifier; }
    const AtomString& keyframesName() const { return std::holds_alternative<AtomString>(m_identifier) ? std::get<AtomString>(m_identifier) : nullAtom(); }
    const String& acceleratedAnimationName() const;

    void insert(BlendingKeyframe&&);

    void addProperty(const AnimatableCSSProperty&);
    bool containsProperty(const AnimatableCSSProperty&) const;
    const HashSet<AnimatableCSSProperty>& properties() const { return m_properties; }

    bool containsAnimatableCSSProperty() const;
    bool containsDirectionAwareProperty() const;

    void clear();
    bool isEmpty() const { return m_keyframes.isEmpty(); }
    size_t size() const { return m_keyframes.size(); }
    const BlendingKeyframe& operator[](size_t index) const LIFETIME_BOUND { return m_keyframes[index]; }

    void copyKeyframes(const BlendingKeyframes&);
    bool hasImplicitKeyframes() const;
    bool hasImplicitKeyframeForProperty(AnimatableCSSProperty) const;
    void fillImplicitKeyframes(const KeyframeEffect&, const RenderStyle& elementStyle);

    auto begin() const LIFETIME_BOUND { return m_keyframes.begin(); }
    auto end() const LIFETIME_BOUND { return m_keyframes.end(); }

    bool usesContainerUnits() const;
    bool usesRelativeFontWeight() const;
    bool hasCSSVariableReferences() const;
    bool hasColorSetToCurrentColor() const;
    bool hasPropertySetToCurrentColor() const;
    const HashSet<AnimatableCSSProperty>& propertiesSetToInherit() const;

    void updatePropertiesMetadata(const StyleProperties&);

    bool hasWidthDependentTransform() const { return m_hasWidthDependentTransform; }
    bool hasHeightDependentTransform() const { return m_hasHeightDependentTransform; }
    bool hasDiscreteTransformInterval() const { return m_hasDiscreteTransformInterval; }
    bool hasExplicitlyInheritedKeyframeProperty() const { return m_hasExplicitlyInheritedKeyframeProperty; }
    bool usesAnchorFunctions() const { return m_usesAnchorFunctions; }
    bool hasKeyframeNotUsingRangeOffset() const { return m_hasKeyframeNotUsingRangeOffset; }

    void updatedComputedOffsets(NOESCAPE const Function<double(const BlendingKeyframe::Offset&)>&);
    bool hasKeyframeWithUnresolvedComputedOffset() const;

private:
    void analyzeKeyframe(const BlendingKeyframe&);

    static uint64_t nextAnonymousIdentifier();

    KeyframesIdentifier m_identifier;
    mutable String m_acceleratedAnimationName;
    Vector<BlendingKeyframe> m_keyframes; // Kept sorted by key.
    HashSet<AnimatableCSSProperty> m_properties; // The properties being animated.
    HashSet<AnimatableCSSProperty> m_explicitToProperties; // The properties with an explicit value for the 100% keyframe.
    HashSet<AnimatableCSSProperty> m_explicitFromProperties; // The properties with an explicit value for the 0% keyframe.
    HashSet<AnimatableCSSProperty> m_propertiesSetToInherit;
    HashSet<AnimatableCSSProperty> m_propertiesSetToCurrentColor;
    bool m_usesRelativeFontWeight { false };
    bool m_containsCSSVariableReferences { false };
    bool m_usesAnchorFunctions { false };
    bool m_hasWidthDependentTransform { false };
    bool m_hasHeightDependentTransform { false };
    bool m_hasDiscreteTransformInterval { false };
    bool m_hasExplicitlyInheritedKeyframeProperty { false };
    bool m_hasKeyframeNotUsingRangeOffset { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_KEYFRAME_INTERPOLATION_KEYFRAME(BlendingKeyframe, isBlendingKeyframe());
