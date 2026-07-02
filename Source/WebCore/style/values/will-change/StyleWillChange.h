/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
#include <WebCore/StyleCustomIdent.h>
#include <WebCore/StyleValueTypes.h>
#include <wtf/PointerComparison.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/TrailingArray.h>

namespace WebCore {
namespace Style {

struct WillChange;

// <animateable-feature>
struct WillChangeAnimatableFeature {
    // <custom-ident> values that are case-insensitive matches for a CSS property are stored
    // as `CustomIdentWithCachedPropertyID`. `CustomIdentWithCachedPropertyID` must maintain
    // the `CustomIdent` to serialize correctly.
    struct CustomIdentWithCachedPropertyID {
        CustomIdent customIdent;
        CSSPropertyID propertyID;

        template<typename... F> decltype(auto) switchOn(F&&... f) const
        {
            return WTF::makeVisitor(std::forward<F>(f)...)(customIdent);
        }

        bool operator==(const CustomIdentWithCachedPropertyID&) const = default;
        bool operator==(const CustomIdent& other) const { return customIdent == other; }
        bool operator==(CSSPropertyID other) const { return propertyID == other; }
    };

    Variant<CSS::Keyword::ScrollPosition, CSS::Keyword::Contents, CustomIdent, CustomIdentWithCachedPropertyID> value;

    WillChangeAnimatableFeature(CSS::Keyword::ScrollPosition feature)
        : value { feature }
    {
    }

    WillChangeAnimatableFeature(CSS::Keyword::Contents feature)
        : value { feature }
    {
    }

    WillChangeAnimatableFeature(CustomIdent&& feature)
        : value { WTF::move(feature) }
    {
    }

    WillChangeAnimatableFeature(CustomIdentWithCachedPropertyID&& feature)
        : value { WTF::move(feature) }
    {
    }

    static bool NODELETE propertyCreatesStackingContext(CSSPropertyID);
    static bool NODELETE propertyTriggersCompositing(CSSPropertyID);
    static bool NODELETE propertyTriggersCompositingOnBoxesOnly(CSSPropertyID);

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }

    template<typename U> bool holdsAlternative() const
    {
        return std::holds_alternative<U>(value);
    }

    bool operator==(const WillChangeAnimatableFeature&) const = default;
};

// <animateable-feature>#
struct WillChangeAnimatableFeatures {
    using value_type = WillChangeAnimatableFeature;
    using const_iterator = WillChangeAnimatableFeature*;

    WillChangeAnimatableFeatures(WillChangeAnimatableFeature&& feature)
        : m_data { Data::create(WTF::move(feature)) }
    {
    }

    WillChangeAnimatableFeatures(CSS::Keyword::ScrollPosition feature)
        : WillChangeAnimatableFeatures { WillChangeAnimatableFeature { feature } }
    {
    }

    WillChangeAnimatableFeatures(CSS::Keyword::Contents feature)
        : WillChangeAnimatableFeatures { WillChangeAnimatableFeature { feature } }
    {
    }

    WillChangeAnimatableFeatures(CustomIdent&& feature)
        : WillChangeAnimatableFeatures { WillChangeAnimatableFeature { WTF::move(feature) } }
    {
    }

    WillChangeAnimatableFeatures(WillChangeAnimatableFeature::CustomIdentWithCachedPropertyID&& feature)
        : WillChangeAnimatableFeatures { WillChangeAnimatableFeature { WTF::move(feature) } }
    {
    }

    template<typename SizedRange, typename Mapper>
    static WillChangeAnimatableFeatures map(SizedRange&& range, NOESCAPE Mapper&& mapper)
    {
        return { Data::map(std::forward<SizedRange>(range), std::forward<Mapper>(mapper)) };
    }

    bool containsScrollPosition() const { return m_data->containsScrollPosition(); }
    bool containsContents() const { return m_data->containsContents(); }
    bool containsProperty(CSSPropertyID property) const { return m_data->containsProperty(property); }
    bool createsContainingBlockForAbsolutelyPositioned(bool isRootElement) const { return m_data->createsContainingBlockForAbsolutelyPositioned(isRootElement); }
    bool createsContainingBlockForOutOfFlowPositioned(bool isRootElement) const { return m_data->createsContainingBlockForOutOfFlowPositioned(isRootElement); }
    bool canCreateStackingContext() const { return m_data->canCreateStackingContext(); }
    bool canBeBackdropRoot() const { return m_data->canBeBackdropRoot(); }
    bool canTriggerCompositing() const { return m_data->canTriggerCompositing(); }
    bool canTriggerCompositingOnInline() const { return m_data->canTriggerCompositingOnInline(); }

    const_iterator begin() const { return m_data->begin(); }
    const_iterator end() const { return m_data->end(); }

private:
    friend struct WillChange;

    class Data final : public RefCounted<Data>, public TrailingArray<Data, WillChangeAnimatableFeature> {
        WTF_MAKE_TZONE_ALLOCATED(Data);
    public:
        using Base = TrailingArray<Data, WillChangeAnimatableFeature>;

        static Ref<Data> create(WillChangeAnimatableFeature&& feature)
        {
            return adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(1))) Data(WTF::move(feature)));
        }

        template<typename SizedRange, typename Mapper>
        static Ref<Data> map(SizedRange&& range, NOESCAPE Mapper&& mapper)
        {
            auto size = range.size();
            return adoptRef(*new (NotNull, fastMalloc(Base::allocationSize(size))) Data(size, std::forward<SizedRange>(range), std::forward<Mapper>(mapper)));
        }

        bool containsScrollPosition() const;
        bool containsContents() const;
        bool containsProperty(CSSPropertyID) const;
        bool createsContainingBlockForAbsolutelyPositioned(bool isRootElement) const;
        bool createsContainingBlockForOutOfFlowPositioned(bool isRootElement) const;
        bool canBeBackdropRoot() const;
        bool canCreateStackingContext() const { return m_canCreateStackingContext; }
        bool canTriggerCompositing() const { return m_canTriggerCompositing; }
        bool canTriggerCompositingOnInline() const { return m_canTriggerCompositingOnInline; }

        bool operator==(const Data&) const;

    private:
        Data(WillChangeAnimatableFeature&& feature)
            : Base(1, WTF::move(feature))
        {
            initializeCachedChecks();
        }

        template<typename SizedRange, typename Mapper>
        Data(unsigned size, SizedRange&& range, NOESCAPE Mapper&& mapper)
            : Base(size, std::forward<SizedRange>(range), std::forward<Mapper>(mapper))
        {
            initializeCachedChecks();
        }

        void initializeCachedChecks();

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
    WillChange(CSS::Keyword::Auto)
        : m_data { nullptr }
    {
    }

    WillChange(WillChangeAnimatableFeatures&& animateableFeatures)
        : m_data { WTF::move(animateableFeatures.m_data) }
    {
    }

    WillChange(WillChangeAnimatableFeature&& animateableFeature)
        : m_data { WillChangeAnimatableFeatures::Data::create(WTF::move(animateableFeature)) }
    {
    }

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
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::WillChangeAnimatableFeature::CustomIdentWithCachedPropertyID);
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::WillChangeAnimatableFeature);
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::WillChange);
