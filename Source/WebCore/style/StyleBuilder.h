/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "PropertyCascade.h"
#include "StyleBuilderState.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class CSSCustomPropertyValue;
enum class CSSWideKeyword : uint8_t;
struct CSSRegisteredCustomProperty;

namespace Style {

class CustomProperty;

class Builder {
    WTF_MAKE_TZONE_ALLOCATED(Builder);
public:
    Builder(RenderStyle&, BuilderContext&&, const MatchResult&, PropertyCascade::IncludedProperties&& = PropertyCascade::normalProperties(), const HashSet<AnimatableCSSProperty>* animatedProperties = nullptr);
    ~Builder();

    void applyAllProperties();
    void applyTopPriorityProperties();
    void applyHighPriorityProperties();
    void applyNonHighPriorityProperties();
    void adjustAfterApplying();

    void applyProperty(CSSPropertyID propertyID) { applyProperties(propertyID, propertyID); }
    void applyCustomProperty(const AtomString& name);

    RefPtr<const CustomProperty> resolveCustomPropertyForContainerQueries(const CSSCustomPropertyValue&);

    BuilderState& state() { return m_state; }

    const HashSet<AnimatableCSSProperty> overriddenAnimatedProperties() const { return m_cascade.overriddenAnimatedProperties(); }

private:
    void applyProperties(int firstProperty, int lastProperty);
    void applyLogicalGroupProperties();
    void applyCustomProperties();
    void applyCustomPropertyImpl(const AtomString&, const PropertyCascade::Property&);

    enum CustomPropertyCycleTracking { Enabled = 0, Disabled };
    template<CustomPropertyCycleTracking trackCycles>
    void applyPropertiesImpl(int firstProperty, int lastProperty);
    void applyCascadeProperty(const PropertyCascade::Property&);
    bool applyRollbackCascadeProperty(const PropertyCascade&, CSSPropertyID, SelectorChecker::LinkMatchMask);
    bool applyRollbackCascadeCustomProperty(const PropertyCascade&, const AtomString&);
    void applyProperty(CSSPropertyID, CSSValue&, SelectorChecker::LinkMatchMask, PropertyCascade::Origin);
    void applyCustomProperty(const AtomString& name, Variant<Ref<const Style::CustomProperty>, CSSWideKeyword>&&);

    Ref<CSSValue> resolveInternalAutoBaseFunction(CSSValue&);
    Ref<CSSValue> resolveVariableReferences(CSSPropertyID, CSSValue&);
    std::optional<Variant<Ref<const Style::CustomProperty>, CSSWideKeyword>> resolveCustomPropertyValue(CSSCustomPropertyValue&);

    void applyPageSizeDescriptor(CSSValue&);

    const PropertyCascade* ensureRollbackCascadeForRevert();
    const PropertyCascade* ensureRollbackCascadeForRevertLayer();

    using RollbackCascadeKey = std::tuple<unsigned, unsigned, unsigned, bool>;
    RollbackCascadeKey makeRollbackCascadeKey(PropertyCascade::Origin, ScopeOrdinal = ScopeOrdinal::Element, CascadeLayerPriority = 0);

    const PropertyCascade m_cascade;
    // Rollback cascades are build on demand to resolve 'revert' and 'revert-layer' keywords.
    HashMap<RollbackCascadeKey, std::unique_ptr<const PropertyCascade>> m_rollbackCascades;

    const UniqueRef<BuilderState> m_state;
};

}
}
