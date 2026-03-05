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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleFontFamilyName.h>
#include <WebCore/StyleValueTypes.h>
#include <WebCore/WebKitFontFamilyNames.h>
#include <ranges>

namespace WebCore {
namespace Style {

enum class FontFamilyKind : bool { Specified, Generic };

// <single-font-family> = [ <family-name> | <generic-family> ]
// https://drafts.csswg.org/css-fonts-4/#propdef-font-family
struct SingleFontFamily {
    AtomString value;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        using namespace WebKitFontFamilyNames;

        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        // <generic-family>
        if (value == cursiveFamily)
            return visitor(CSS::Keyword::Cursive { });
        if (value == fantasyFamily)
            return visitor(CSS::Keyword::Fantasy { });
        if (value == monospaceFamily)
            return visitor(CSS::Keyword::Monospace { });
        if (value == mathFamily)
            return visitor(CSS::Keyword::Math { });
        if (value == pictographFamily)
            return visitor(CSS::Keyword::WebkitPictograph { });
        if (value == sansSerifFamily)
            return visitor(CSS::Keyword::SansSerif { });
        if (value == serifFamily)
            return visitor(CSS::Keyword::Serif { });
        if (value == systemUiFamily)
            return visitor(CSS::Keyword::SystemUi { });
        // <family-name>
        return visitor(FontFamilyName { value });
    }

    bool operator==(const SingleFontFamily&) const = default;
};

// <'font-family'> = [ <family-name> | <generic-family> ]#
// https://drafts.csswg.org/css-fonts-4/#propdef-font-family
struct FontFamilies {
    FontFamilies(Ref<RefCountedFixedVector<AtomString>>&& families, FontFamilyKind firstFontKind)
        : m_families { WTF::move(families) }
        , m_firstFontKind { firstFontKind }
    {
    }

    FontFamilies(Ref<RefCountedFixedVector<AtomString>>&& families, bool isSpecifiedFont)
        : FontFamilies { WTF::move(families), isSpecifiedFont ? FontFamilyKind::Specified : FontFamilyKind::Generic }
    {
    }

    FontFamilies(AtomString family, FontFamilyKind firstFontKind)
        : FontFamilies { RefCountedFixedVector<AtomString>::create({ WTF::move(family) }), firstFontKind }
    {
    }

    class const_iterator {
    public:
        using inner_iterator = RefCountedFixedVector<AtomString>::const_iterator;
        using iterator_category = std::forward_iterator_tag;
        using value_type = SingleFontFamily;
        using difference_type = std::ptrdiff_t;

        const_iterator(inner_iterator it) : m_it { it } { }

        value_type operator*() const { return SingleFontFamily { *m_it }; }

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        const_iterator& operator++() { ++m_it; return *this; }
        const_iterator operator++(int) { auto result = *this; ++(*this); return result; }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

        bool operator==(const const_iterator&) const = default;

    private:
        inner_iterator m_it;
    };

    const_iterator begin() const LIFETIME_BOUND { return const_iterator(m_families->begin()); }
    const_iterator end() const LIFETIME_BOUND { return const_iterator(m_families->end()); }

    unsigned size() const { return m_families->size(); }

    SingleFontFamily first() const { return SingleFontFamily { m_families.get()[0] }; }
    SingleFontFamily last() const { return SingleFontFamily { m_families.get()[size() - 1] }; }

    RefCountedFixedVector<AtomString>& toPlatform() LIFETIME_BOUND { return m_families.get(); }
    const RefCountedFixedVector<AtomString>& toPlatform() const LIFETIME_BOUND { return m_families.get(); }
    Ref<RefCountedFixedVector<AtomString>> takePlatform() { return WTF::move(m_families); }

    FontFamilyKind firstFontKind() const { return m_firstFontKind; }
    bool isSpecifiedFont() const { return m_firstFontKind == FontFamilyKind::Specified; }

    bool operator==(const FontFamilies& other) const
    {
        return arePointingToEqualData(m_families, other.m_families)
            && m_firstFontKind == other.m_firstFontKind;
    }

private:
    Ref<RefCountedFixedVector<AtomString>> m_families;
    FontFamilyKind m_firstFontKind { FontFamilyKind::Generic };
};

// MARK: - Conversion

template<> struct CSSValueConversion<FontFamilies> { auto operator()(BuilderState&, const CSSValue&) -> FontFamilies; };

} // namespace Style
} // namespace WebCore

DEFINE_COMMA_SEPARATED_RANGE_LIKE_CONFORMANCE(WebCore::Style::FontFamilies)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SingleFontFamily)
