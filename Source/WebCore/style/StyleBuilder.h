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

namespace WebCore {

namespace Style {

class Builder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Builder(RenderStyle&, BuilderContext&&, const MatchResult&, CascadeLevel, PropertyCascade::IncludedProperties = PropertyCascade::IncludedProperties::All, const HashSet<AnimatableProperty>* = nullptr);
    ~Builder();

    void applyAllProperties();
    void applyTopPriorityProperties();
    void applyHighPriorityProperties();
    void applyNonHighPriorityProperties();

    void applyProperty(CSSPropertyID propertyID) { applyProperties(propertyID, propertyID); }
    void applyCustomProperty(const AtomString& name);

    BuilderState& state() { return m_state; }

private:
    void applyProperties(int firstProperty, int lastProperty);
    void applyDeferredProperties();
    void applyCustomProperties();

    enum CustomPropertyCycleTracking { Enabled = 0, Disabled };
    template<CustomPropertyCycleTracking trackCycles>
    void applyPropertiesImpl(int firstProperty, int lastProperty);
    void applyCascadeProperty(const PropertyCascade::Property&);
    void applyRollbackCascadeProperty(const PropertyCascade::Property&, SelectorChecker::LinkMatchMask);
    void applyProperty(CSSPropertyID, CSSValue&, SelectorChecker::LinkMatchMask);

    Ref<CSSValue> resolveVariableReferences(CSSPropertyID, CSSValue&);
    RefPtr<CSSCustomPropertyValue> resolveCustomPropertyValueWithVariableReferences(CSSCustomPropertyValue&);

    const PropertyCascade* ensureRollbackCascadeForRevert();
    const PropertyCascade* ensureRollbackCascadeForRevertLayer();

    using RollbackCascadeKey = std::tuple<unsigned, unsigned, unsigned>;
    RollbackCascadeKey makeRollbackCascadeKey(CascadeLevel, ScopeOrdinal = ScopeOrdinal::Element, CascadeLayerPriority = 0);

    const PropertyCascade m_cascade;
    // Rollback cascades are build on demand to resolve 'revert' and 'revert-layer' keywords.
    HashMap<RollbackCascadeKey, std::unique_ptr<const PropertyCascade>> m_rollbackCascades;

    BuilderState m_state;
};

}
}
