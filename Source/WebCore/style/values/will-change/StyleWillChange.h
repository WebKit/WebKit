/*
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSPropertyNames.h>
#include <WebCore/StyleValueTypes.h>
#include <wtf/PointerComparison.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace Style {

struct WillChange;

// <animateable-feature>
struct WillChangeAnimatableFeature {
    // FIXME: This should be storing <custom-ident> for non-CSSPropertyID values to correctly implement computed value serialization.
    // It should likely be a Variant<CSS::Keyword::ScrollPosition, CSS::Keyword::Contents, PropertyIdentifier, CustomIdentifier>;

    enum class Feature: uint8_t {
        ScrollPosition,
        Contents,
        Property,
    };

    static const int numCSSPropertyIDBits = 14;
    static_assert(cssPropertyIDEnumValueCount <= (1 << numCSSPropertyIDBits), "CSSPropertyID should fit in 14 bits");

    WillChangeAnimatableFeature(Feature, CSSPropertyID = CSSPropertyInvalid);

    Feature m_feature { Feature::Property };
    unsigned m_cssPropertyID : numCSSPropertyIDBits { CSSPropertyInvalid };

    Feature feature() const { return m_feature; }
    CSSPropertyID property() const { return feature() == Feature::Property ? static_cast<CSSPropertyID>(m_cssPropertyID) : CSSPropertyInvalid; }

    static bool NODELETE propertyCreatesStackingContext(CSSPropertyID);
    static bool NODELETE propertyTriggersCompositing(CSSPropertyID);
    static bool NODELETE propertyTriggersCompositingOnBoxesOnly(CSSPropertyID);

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_feature) {
        case Feature::ScrollPosition:
            return visitor(CSS::Keyword::ScrollPosition { });
        case Feature::Contents:
            return visitor(CSS::Keyword::Contents { });
        case Feature::Property:
            return visitor(PropertyIdentifier { property() });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const WillChangeAnimatableFeature&) const = default;
};

// <animateable-feature>#
struct WillChangeAnimatableFeatures {
    using Feature = WillChangeAnimatableFeature::Feature;
    using value_type = Vector<WillChangeAnimatableFeature, 1>::value_type;
    using const_iterator = Vector<WillChangeAnimatableFeature, 1>::const_iterator;

    WillChangeAnimatableFeatures() : m_data { Data::create() } { }
    WillChangeAnimatableFeatures(Feature feature, CSSPropertyID propertyID = CSSPropertyInvalid) : m_data { Data::create(feature, propertyID) } { }

    bool containsScrollPosition() const { return m_data->containsScrollPosition(); }
    bool containsContents() const { return m_data->containsContents(); }
    bool containsProperty(CSSPropertyID property) const { return m_data->containsProperty(property); }
    bool createsContainingBlockForAbsolutelyPositioned(bool isRootElement) const { return m_data->createsContainingBlockForAbsolutelyPositioned(isRootElement); }
    bool createsContainingBlockForOutOfFlowPositioned(bool isRootElement) const { return m_data->createsContainingBlockForOutOfFlowPositioned(isRootElement); }
    bool canCreateStackingContext() const { return m_data->canCreateStackingContext(); }
    bool canBeBackdropRoot() const { return m_data->canBeBackdropRoot(); }
    bool canTriggerCompositing() const { return m_data->canTriggerCompositing(); }
    bool canTriggerCompositingOnInline() const { return m_data->canTriggerCompositingOnInline(); }

    void addFeature(Feature feature, CSSPropertyID property = CSSPropertyInvalid) { m_data->addFeature(feature, property); }

    const_iterator begin() const { return m_data->begin(); }
    const_iterator end() const { return m_data->end(); }

private:
    friend struct WillChange;

    class Data : public RefCounted<Data> {
        WTF_MAKE_TZONE_ALLOCATED(Data);
    public:
        static Ref<Data> create() { return adoptRef(*new Data); }
        static Ref<Data> create(Feature feature, CSSPropertyID propertyID) { return adoptRef(*new Data(feature, propertyID)); }

        bool containsScrollPosition() const;
        bool containsContents() const;
        bool containsProperty(CSSPropertyID) const;
        bool createsContainingBlockForAbsolutelyPositioned(bool isRootElement) const;
        bool createsContainingBlockForOutOfFlowPositioned(bool isRootElement) const;
        bool canCreateStackingContext() const { return m_canCreateStackingContext; }
        bool canBeBackdropRoot() const;
        bool canTriggerCompositing() const { return m_canTriggerCompositing; }
        bool canTriggerCompositingOnInline() const { return m_canTriggerCompositingOnInline; }

        void addFeature(Feature, CSSPropertyID);

        const_iterator begin() const { return m_animatableFeatures.begin(); }
        const_iterator end() const { return m_animatableFeatures.end(); }

        bool NODELETE operator==(const Data&) const;

    private:
        Data() { }
        Data(Feature feature, CSSPropertyID propertyID) { addFeature(feature, propertyID); }

        Vector<WillChangeAnimatableFeature, 1> m_animatableFeatures;
        bool m_canCreateStackingContext { false };
        bool m_canTriggerCompositing { false };
        bool m_canTriggerCompositingOnInline { false };
    };

    WillChangeAnimatableFeatures(Ref<Data> data) : m_data { WTF::move(data) } { }

    Ref<Data> m_data;
};

// <'will-change'> = auto | <animateable-feature>#
// https://drafts.csswg.org/css-will-change/#propdef-will-change
struct WillChange {
    WillChange(CSS::Keyword::Auto) : m_data { nullptr} { }
    WillChange(WillChangeAnimatableFeatures&& animateableFeatures) : m_data { WTF::move(animateableFeatures.m_data) } { }

    WillChange(WillChangeAnimatableFeature::Feature feature, CSSPropertyID propertyID = CSSPropertyInvalid) : m_data { WillChangeAnimatableFeatures::Data::create(feature, propertyID) } { }

    bool isAuto() const { return !m_data; }
    bool isAnimateableFeatures() const { return !!m_data; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isAuto())
            return visitor(CSS::Keyword::Auto { });
        return visitor(WillChangeAnimatableFeatures { *m_data });
    }

    bool containsScrollPosition() const { return m_data && m_data->containsScrollPosition(); }
    bool containsContents() const { return m_data && m_data->containsContents(); }
    bool containsProperty(CSSPropertyID property) const { return m_data && m_data->containsProperty(property); }
    bool createsContainingBlockForAbsolutelyPositioned(bool isRootElement) const { return m_data && m_data->createsContainingBlockForAbsolutelyPositioned(isRootElement); }
    bool createsContainingBlockForOutOfFlowPositioned(bool isRootElement) const { return m_data && m_data->createsContainingBlockForOutOfFlowPositioned(isRootElement); }
    bool canCreateStackingContext() const { return m_data && m_data->canCreateStackingContext(); }
    bool canBeBackdropRoot() const { return m_data && m_data->canBeBackdropRoot(); }
    bool canTriggerCompositing() const { return m_data && m_data->canTriggerCompositing(); }
    bool canTriggerCompositingOnInline() const { return m_data && m_data->canTriggerCompositingOnInline(); }

    bool operator==(const WillChange& other) const
    {
        return arePointingToEqualData(m_data, other.m_data);
    }

private:
    RefPtr<WillChangeAnimatableFeatures::Data> m_data;
};

// MARK: - Conversion

template<> struct CSSValueConversion<WillChange> { auto operator()(BuilderState&, const CSSValue&) -> WillChange; };

} // namespace Style
} // namespace WebCore

DEFINE_COMMA_SEPARATED_RANGE_LIKE_CONFORMANCE(WebCore::Style::WillChangeAnimatableFeatures)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::WillChangeAnimatableFeature);
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::WillChange);
