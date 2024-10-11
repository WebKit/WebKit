/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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

#include "CSSGradient.h"
#include "StyleImage.h"

namespace WebCore {

namespace Style {
class BuilderState;
}

// MARK: - Linear.

class CSSLinearGradientValue final : public CSSValue {
public:
    static Ref<CSSLinearGradientValue> create(CSS::LinearGradient&& gradient)
    {
        return adoptRef(*new CSSLinearGradientValue(WTFMove(gradient)));
    }

    String customCSSText() const;
    bool equals(const CSSLinearGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        return CSS::visitCSSValueChildren(func, m_gradient);
    }

private:
    CSSLinearGradientValue(CSS::LinearGradient&& gradient)
        : CSSValue(ClassType::LinearGradient)
        , m_gradient(WTFMove(gradient))
    {
    }

    CSSLinearGradientValue(const CSSLinearGradientValue& other)
        : CSSValue(ClassType::LinearGradient)
        , m_gradient(other.m_gradient)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    CSS::LinearGradient m_gradient;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

class CSSPrefixedLinearGradientValue final : public CSSValue {
public:
    static Ref<CSSPrefixedLinearGradientValue> create(CSS::PrefixedLinearGradient&& gradient)
    {
        return adoptRef(*new CSSPrefixedLinearGradientValue(WTFMove(gradient)));
    }

    String customCSSText() const;
    bool equals(const CSSPrefixedLinearGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        return CSS::visitCSSValueChildren(func, m_gradient);
    }

private:
    CSSPrefixedLinearGradientValue(CSS::PrefixedLinearGradient&& gradient)
        : CSSValue(ClassType::PrefixedLinearGradient)
        , m_gradient(WTFMove(gradient))
    {
    }

    CSSPrefixedLinearGradientValue(const CSSPrefixedLinearGradientValue& other)
        : CSSValue(ClassType::PrefixedLinearGradient)
        , m_gradient(other.m_gradient)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    CSS::PrefixedLinearGradient m_gradient;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

class CSSDeprecatedLinearGradientValue final : public CSSValue {
public:
    static Ref<CSSDeprecatedLinearGradientValue> create(CSS::DeprecatedLinearGradient&& gradient)
    {
        return adoptRef(*new CSSDeprecatedLinearGradientValue(WTFMove(gradient)));
    }

    String customCSSText() const;
    bool equals(const CSSDeprecatedLinearGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        return CSS::visitCSSValueChildren(func, m_gradient);
    }

private:
    CSSDeprecatedLinearGradientValue(CSS::DeprecatedLinearGradient&& gradient)
        : CSSValue(ClassType::DeprecatedLinearGradient)
        , m_gradient(WTFMove(gradient))
    {
    }

    CSSDeprecatedLinearGradientValue(const CSSDeprecatedLinearGradientValue& other)
        : CSSValue(ClassType::DeprecatedLinearGradient)
        , m_gradient(other.m_gradient)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    CSS::DeprecatedLinearGradient m_gradient;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

// MARK: - Radial.

class CSSRadialGradientValue final : public CSSValue {
public:
    static Ref<CSSRadialGradientValue> create(CSS::RadialGradient&& gradient)
    {
        return adoptRef(*new CSSRadialGradientValue(WTFMove(gradient)));
    }

    String customCSSText() const;
    bool equals(const CSSRadialGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        return CSS::visitCSSValueChildren(func, m_gradient);
    }

private:
    CSSRadialGradientValue(CSS::RadialGradient&& gradient)
        : CSSValue(ClassType::RadialGradient)
        , m_gradient(WTFMove(gradient))
    {
    }

    CSSRadialGradientValue(const CSSRadialGradientValue& other)
        : CSSValue(ClassType::RadialGradient)
        , m_gradient(other.m_gradient)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    CSS::RadialGradient m_gradient;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

class CSSPrefixedRadialGradientValue final : public CSSValue {
public:
    static Ref<CSSPrefixedRadialGradientValue> create(CSS::PrefixedRadialGradient&& gradient)
    {
        return adoptRef(*new CSSPrefixedRadialGradientValue(WTFMove(gradient)));
    }

    String customCSSText() const;
    bool equals(const CSSPrefixedRadialGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        return CSS::visitCSSValueChildren(func, m_gradient);
    }

private:
    CSSPrefixedRadialGradientValue(CSS::PrefixedRadialGradient&& gradient)
        : CSSValue(ClassType::PrefixedRadialGradient)
        , m_gradient(WTFMove(gradient))
    {
    }

    CSSPrefixedRadialGradientValue(const CSSPrefixedRadialGradientValue& other)
        : CSSValue(ClassType::PrefixedRadialGradient)
        , m_gradient(other.m_gradient)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    CSS::PrefixedRadialGradient m_gradient;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

class CSSDeprecatedRadialGradientValue final : public CSSValue {
public:
    static Ref<CSSDeprecatedRadialGradientValue> create(CSS::DeprecatedRadialGradient&& gradient)
    {
        return adoptRef(*new CSSDeprecatedRadialGradientValue(WTFMove(gradient)));
    }

    String customCSSText() const;
    bool equals(const CSSDeprecatedRadialGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        return CSS::visitCSSValueChildren(func, m_gradient);
    }

private:
    CSSDeprecatedRadialGradientValue(CSS::DeprecatedRadialGradient&& gradient)
        : CSSValue(ClassType::DeprecatedRadialGradient)
        , m_gradient(WTFMove(gradient))
    {
    }

    CSSDeprecatedRadialGradientValue(const CSSDeprecatedRadialGradientValue& other)
        : CSSValue(ClassType::DeprecatedRadialGradient)
        , m_gradient(other.m_gradient)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    CSS::DeprecatedRadialGradient m_gradient;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

// MARK: - Conic.

class CSSConicGradientValue final : public CSSValue {
public:
    static Ref<CSSConicGradientValue> create(CSS::ConicGradient&& gradient)
    {
        return adoptRef(*new CSSConicGradientValue(WTFMove(gradient)));
    }

    String customCSSText() const;
    bool equals(const CSSConicGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        return CSS::visitCSSValueChildren(func, m_gradient);
    }

private:
    explicit CSSConicGradientValue(CSS::ConicGradient&& gradient)
        : CSSValue(ClassType::ConicGradient)
        , m_gradient(WTFMove(gradient))
    {
    }

    CSSConicGradientValue(const CSSConicGradientValue& other)
        : CSSValue(ClassType::ConicGradient)
        , m_gradient(other.m_gradient)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    CSS::ConicGradient m_gradient;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSLinearGradientValue, isLinearGradientValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSRadialGradientValue, isRadialGradientValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSConicGradientValue, isConicGradientValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSDeprecatedLinearGradientValue, isDeprecatedLinearGradientValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSDeprecatedRadialGradientValue, isDeprecatedRadialGradientValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPrefixedLinearGradientValue, isPrefixedLinearGradientValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPrefixedRadialGradientValue, isPrefixedRadialGradientValue())
