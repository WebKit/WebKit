/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "CSSToLengthConversionData.h"
#include "PropertyCascade.h"
#include "RuleSet.h"
#include "SelectorChecker.h"
#include "StyleForVisitedLink.h"
#include "StyleMutator.h"
#include "StyleSubstitutionContext.h"
#include "TextFlags.h"
#include "TreeResolutionState.h"
#include <wtf/BitSet.h>

namespace WebCore {

struct CSSRegisteredCustomProperty;

namespace CSSCalc {
struct RandomCachingKey;
}

namespace Style {

class BuilderState;
class Builder;
class CustomPropertyRegistry;
class Image;
class LocalPropertyRegistry;
class Scope;
struct Color;

enum class PositionTryFallbackTactic : uint8_t;

void maybeUpdateFontForLetterSpacingOrWordSpacing(BuilderState&, CSSValue&);

enum class ApplyValueType : uint8_t { Value, Initial, Inherit };

struct BuilderPositionTryFallback {
    RefPtr<const StyleProperties> properties;
    Vector<PositionTryFallbackTactic> tactics;
};

struct RegisteredSubstitutionAttribute {
    AtomString name;
    WeakPtr<const Scope> targetScope;
};

struct BuilderContext {
    Ref<const Document> document;
    const Style::ComputedStyle* parentStyle { };
    const Style::ComputedStyle* rootElementStyle { };
    RefPtr<const Element> element { };
    CheckedPtr<TreeResolutionState> treeResolutionState { };
    std::optional<BuilderPositionTryFallback> positionTryFallback { };
    const LocalPropertyRegistry* localPropertyRegistry { };
    // For a custom function's hypothetical element: the builder of the calling context, used to
    // resolve inherited custom properties on demand. https://drafts.csswg.org/css-mixins/#evaluating-custom-functions
    Builder* callingContextBuilder { nullptr };
};

class BuilderState : public CanMakeCheckedPtr<BuilderState> {
    WTF_MAKE_TZONE_ALLOCATED(BuilderState);
    WTF_MAKE_NONCOPYABLE(BuilderState);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(BuilderState);
public:
    template<typename T, class... Args> friend WTF::UniqueRef<T> WTF::makeUniqueRefWithoutFastMallocCheck(Args&&...);

    static UniqueRef<BuilderState> create(ComputedStyle& style, BuilderContext&& builderContext)
    {
        return makeUniqueRefWithoutRefCountedCheck<BuilderState>(style, WTF::move(builderContext));
    }

    Mutator& mutator() LIFETIME_BOUND { return m_mutator; }
    const Mutator& mutator() const LIFETIME_BOUND { return m_mutator; }

    ComputedStyle& style() LIFETIME_BOUND { return m_mutator.style(); }
    const ComputedStyle& style() const LIFETIME_BOUND { return m_mutator.style(); }

    const ComputedStyle& parentStyle() const LIFETIME_BOUND { return m_mutator.parentStyle(); }
    const ComputedStyle* rootElementStyle() const LIFETIME_BOUND { return m_mutator.rootElementStyle(); }

    Builder* callingContextBuilder() const { return m_callingContextBuilder; }

    const Document& document() const { return m_mutator.document(); }
    const Element* element() const { return m_mutator.element(); }

    const CSSRegisteredCustomProperty* registeredProperty(const AtomString&) const;

    bool applyPropertyToRegularStyle() const { return m_linkMatch != SelectorChecker::MatchVisited; }
    bool applyPropertyToVisitedLinkStyle() const { return m_linkMatch != SelectorChecker::MatchLink; }

    float NODELETE zoomWithTextZoomFactor();

    bool NODELETE useSVGZoomRules() const { return m_mutator.useSVGZoomRules(); }
    bool NODELETE useSVGZoomRulesForLength() const { return m_mutator.useSVGZoomRulesForLength(); }

    bool NODELETE evaluationTimeZoomEnabled() const;

    // Defaults to Element when called outside property cascade application (e.g. attr() resolution
    // during container-query evaluation), where there is no current property in flight.
    ScopeOrdinal styleScopeOrdinal() const { return m_currentProperty ? m_currentProperty->styleScopeOrdinal : ScopeOrdinal::Element; }

    RefPtr<Image> createStyleImage(const CSSValue&) const;

    const Vector<RegisteredSubstitutionAttribute>& registeredSubstitutionAttributes() const LIFETIME_BOUND { return m_registeredSubstitutionAttributes; }
    void registerSubstitutionAttribute(const AtomString& attributeLocalName, const Scope* targetScope = nullptr);

    const CSSToLengthConversionData& cssToLengthConversionData() const LIFETIME_BOUND { return m_cssToLengthConversionData; }

    GuardedSubstitutionContexts::Guard guardSubstitutionContext(SubstitutionContext&& context) { return m_guardedSubstitutionContexts.guard(WTF::move(context)); }
    void addGuardedFunctionContexts(const BuilderState& other) { m_guardedSubstitutionContexts.addFunctionContextsFrom(other.m_guardedSubstitutionContexts); }

    void setIsBuildingKeyframeStyle() { m_isBuildingKeyframeStyle = true; }
    bool hasRevertRuleOrLayerInKeyframeStyle() const { return m_hasRevertRuleOrLayerInKeyframeStyle; }

    bool isAuthorOrigin() const
    {
        return m_currentProperty && m_currentProperty->origin == PropertyCascade::Origin::Author;
    }

    CSSPropertyID NODELETE cssPropertyID() const;

    bool NODELETE isCurrentPropertyInvalidAtComputedValueTime() const;
    void NODELETE setCurrentPropertyInvalidAtComputedValueTime();

    void NODELETE setUsesViewportUnits();
    void NODELETE setIsContainerDependent();

    double lookupCSSRandomBaseValue(const CSSCalc::RandomCachingKey&, std::optional<CSS::Keyword::ElementScoped>) const;

    // Accessors for sibling information used by the sibling-count() and sibling-index() CSS functions.
    unsigned NODELETE siblingCount();
    unsigned NODELETE siblingIndex();

    AnchorPositionedStates* anchorPositionedStates() LIFETIME_BOUND { return m_treeResolutionState ? &m_treeResolutionState->anchorPositionedStates : nullptr; }
    const std::optional<BuilderPositionTryFallback>& positionTryFallback() const LIFETIME_BOUND { return m_positionTryFallback; }

    void disableNativeAppearanceIfNeeded(CSSPropertyID, PropertyCascade::Origin);

private:
    friend class Builder;
    friend class SubstitutionResolver;

    BuilderState(ComputedStyle&, BuilderContext&&);

    void NODELETE adjustStyleForInterCharacterRuby();

    Mutator m_mutator;

    CheckedPtr<TreeResolutionState> m_treeResolutionState;
    std::optional<BuilderPositionTryFallback> m_positionTryFallback;
    const LocalPropertyRegistry* m_localPropertyRegistry;
    Builder* m_callingContextBuilder;

    const CSSToLengthConversionData m_cssToLengthConversionData;

    HashSet<AtomString> m_appliedCustomProperties;
    GuardedSubstitutionContexts m_guardedSubstitutionContexts;
    WTF::BitSet<cssPropertyIDEnumValueCount> m_inProgressProperties;
    WTF::BitSet<cssPropertyIDEnumValueCount> m_invalidAtComputedValueTimeProperties;

    const PropertyCascade::Property* m_currentProperty { nullptr };
    SelectorChecker::LinkMatchMask m_linkMatch { };
    const PropertyCascade* m_currentRollbackCascade { nullptr };

    Vector<RegisteredSubstitutionAttribute> m_registeredSubstitutionAttributes;

    bool m_isBuildingKeyframeStyle { false };
    bool m_hasRevertRuleOrLayerInKeyframeStyle { false };
};

} // namespace Style
} // namespace WebCore
