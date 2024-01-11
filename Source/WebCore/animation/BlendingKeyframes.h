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
    BlendingKeyframe(double offset, std::unique_ptr<RenderStyle> style)
        : m_offset(offset)
        , m_style(WTFMove(style))
    {
    }

    // KeyframeInterpolation::Keyframe
    double offset() const final { return m_offset; }
    std::optional<CompositeOperation> compositeOperation() const final { return m_compositeOperation; }
    bool animatesProperty(KeyframeInterpolation::Property) const final;
    bool isBlendingKeyframe() const final { return true; }

    void addProperty(const AnimatableCSSProperty&);
    const HashSet<AnimatableCSSProperty>& properties() const { return m_properties; }

    void setOffset(double offset) { m_offset = offset; }

    const RenderStyle* style() const { return m_style.get(); }
    void setStyle(std::unique_ptr<RenderStyle> style) { m_style = WTFMove(style); }

    TimingFunction* timingFunction() const { return m_timingFunction.get(); }
    void setTimingFunction(const RefPtr<TimingFunction>& timingFunction) { m_timingFunction = timingFunction; }

    void setCompositeOperation(std::optional<CompositeOperation> op) { m_compositeOperation = op; }

    bool containsDirectionAwareProperty() const { return m_containsDirectionAwareProperty; }
    void setContainsDirectionAwareProperty(bool containsDirectionAwareProperty) { m_containsDirectionAwareProperty = containsDirectionAwareProperty; }

private:
    double m_offset;
    HashSet<AnimatableCSSProperty> m_properties; // The properties specified in this keyframe.
    std::unique_ptr<RenderStyle> m_style;
    RefPtr<TimingFunction> m_timingFunction;
    std::optional<CompositeOperation> m_compositeOperation;
    bool m_containsDirectionAwareProperty { false };
};

class BlendingKeyframes {
public:
    explicit BlendingKeyframes(const AtomString& animationName)
        : m_animationName(animationName)
    {
    }
    ~BlendingKeyframes();

    BlendingKeyframes& operator=(BlendingKeyframes&&) = default;
    bool operator==(const BlendingKeyframes&) const;

    const AtomString& animationName() const { return m_animationName; }

    void insert(BlendingKeyframe&&);

    void addProperty(const AnimatableCSSProperty&);
    bool containsProperty(const AnimatableCSSProperty&) const;
    const HashSet<AnimatableCSSProperty>& properties() const { return m_properties; }

    bool containsAnimatableCSSProperty() const;
    bool containsDirectionAwareProperty() const;

    void clear();
    bool isEmpty() const { return m_keyframes.isEmpty(); }
    size_t size() const { return m_keyframes.size(); }
    const BlendingKeyframe& operator[](size_t index) const { return m_keyframes[index]; }

    void copyKeyframes(const BlendingKeyframes&);
    bool hasImplicitKeyframes() const;
    void fillImplicitKeyframes(const KeyframeEffect&, const RenderStyle& elementStyle);

    auto begin() const { return m_keyframes.begin(); }
    auto end() const { return m_keyframes.end(); }

    bool usesContainerUnits() const;
    bool usesRelativeFontWeight() const;
    bool hasCSSVariableReferences() const;
    bool hasColorSetToCurrentColor() const;
    bool hasPropertySetToCurrentColor() const;
    const HashSet<AnimatableCSSProperty>& propertiesSetToInherit() const;

    void updatePropertiesMetadata(const StyleProperties&);

    bool hasWidthDependentTransform() const { return m_hasWidthDependentTransform; }
    bool hasHeightDependentTransform() const { return m_hasHeightDependentTransform; }
    bool hasExplicitlyInheritedKeyframeProperty() const { return m_hasExplicitlyInheritedKeyframeProperty; }

private:
    void analyzeKeyframe(const BlendingKeyframe&);

    AtomString m_animationName;
    Vector<BlendingKeyframe> m_keyframes; // Kept sorted by key.
    HashSet<AnimatableCSSProperty> m_properties; // The properties being animated.
    HashSet<AnimatableCSSProperty> m_propertiesSetToInherit;
    HashSet<AnimatableCSSProperty> m_propertiesSetToCurrentColor;
    bool m_usesRelativeFontWeight { false };
    bool m_containsCSSVariableReferences { false };
    bool m_hasWidthDependentTransform { false };
    bool m_hasHeightDependentTransform { false };
    bool m_hasExplicitlyInheritedKeyframeProperty { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_KEYFRAME_INTERPOLATION_KEYFRAME(BlendingKeyframe, isBlendingKeyframe());
