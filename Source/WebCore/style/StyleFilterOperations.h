/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "CSSPropertyParserConsumer+RawTypes.h"
#include "CSSValueKeywords.h"
#include "CachedSVGDocumentReference.h"
#include "FilterOperations.h"
#include "Length.h"
#include "LengthPoint.h"
#include "StyleColor.h"
#include <algorithm>
#include <optional>
#include <variant>
#include <wtf/FixedVector.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CachedResourceLoader;
class RenderElement;
struct BlendingContext;
struct CSSUnresolvedFilterResolutionContext;
struct ResourceLoaderOptions;

namespace Style {

class FilterOperations {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct Reference {
        String url;
        AtomString fragment;
        std::unique_ptr<CachedSVGDocumentReference> cachedSVGDocumentReference = nullptr;

        Reference(String url, AtomString fragment)
            : url { WTFMove(url) }
            , fragment { WTFMove(fragment) }
        {
        }

        Reference(const Reference& other)
            : url { other.url }
            , fragment { other.fragment }
        {
        }

        bool operator==(const Reference& other) const
        {
            return url == other.url && fragment == other.fragment;
        }
    };
    struct Grayscale {
        static constexpr auto functionID = CSSValueGrayscale;
        static constexpr auto identity = NumberRaw { 0 };

        NumberRaw amount;
        bool operator==(const Grayscale&) const = default;
    };
    struct Sepia {
        static constexpr auto functionID = CSSValueSepia;
        static constexpr auto identity = NumberRaw { 0 };

        NumberRaw amount;
        bool operator==(const Sepia&) const = default;
    };
    struct Saturate {
        static constexpr auto functionID = CSSValueSaturate;
        static constexpr auto identity = NumberRaw { 1 };

        NumberRaw amount;
        bool operator==(const Saturate&) const = default;
    };
    struct HueRotate {
        static constexpr auto functionID = CSSValueHueRotate;
        static constexpr auto identity = CanonicalAngleRaw { 0 };

        CanonicalAngleRaw amount;
        bool operator==(const HueRotate&) const = default;
    };
    struct Invert {
        static constexpr auto functionID = CSSValueInvert;
        static constexpr auto identity = NumberRaw { 0 };

        NumberRaw amount;
        bool operator==(const Invert&) const = default;
    };
    struct Opacity {
        static constexpr auto functionID = CSSValueOpacity;
        static constexpr auto identity = NumberRaw { 1 };

        NumberRaw amount;
        bool operator==(const Opacity&) const = default;
    };
    struct Brightness {
        static constexpr auto functionID = CSSValueBrightness;
        static constexpr auto identity = NumberRaw { 1 };

        NumberRaw amount;
        bool operator==(const Brightness&) const = default;
    };
    struct Contrast {
        static constexpr auto functionID = CSSValueContrast;
        static constexpr auto identity = NumberRaw { 1 };

        NumberRaw amount;
        bool operator==(const Contrast&) const = default;
    };
    struct Blur {
        static constexpr auto functionID = CSSValueBlur;

        Length stdDeviation;
        bool operator==(const Blur&) const = default;
    };
    struct DropShadow {
        static constexpr auto functionID = CSSValueDropShadow;

        LengthPoint location;
        Length stdDeviation;
        StyleColor color;
        bool operator==(const DropShadow&) const = default;
    };
    struct AppleInvertLightness {
        static constexpr auto functionID = CSSValueAppleInvertLightness;

        bool operator==(const AppleInvertLightness&) const = default;
    };

    using FilterOperation = std::variant<Reference, Grayscale, Sepia, Saturate, HueRotate, Invert, Opacity, Brightness, Contrast, Blur, DropShadow, AppleInvertLightness>;

    using Storage = FixedVector<FilterOperation>;

    using value_type = typename Storage::value_type;
    using pointer = typename Storage::pointer;
    using reference = typename Storage::reference;
    using const_reference = typename Storage::const_reference;
    using const_pointer = typename Storage::const_pointer;
    using size_type = typename Storage::size_type;
    using difference_type = typename Storage::difference_type;
    using iterator = typename Storage::iterator;
    using const_iterator = typename Storage::const_iterator;
    using reverse_iterator = typename Storage::reverse_iterator;
    using const_reverse_iterator = typename Storage::const_reverse_iterator;

    FilterOperations() = default;
    FilterOperations(Storage&&);

    WebCore::FilterOperations resolve(const RenderStyle&) const;
    WebCore::FilterOperations resolve(const CSSUnresolvedFilterResolutionContext&) const;
    static WebCore::FilterOperations::FilterOperation resolve(const FilterOperation&, const RenderStyle&);
    static WebCore::FilterOperations::FilterOperation resolve(const FilterOperation&, const CSSUnresolvedFilterResolutionContext&);

    Ref<CSSValue> computedStyleValue(const RenderStyle&) const;

    const_iterator begin() const { return m_operations.begin(); }
    const_iterator end() const { return m_operations.end(); }
    const_reverse_iterator rbegin() const { return m_operations.rbegin(); }
    const_reverse_iterator rend() const { return m_operations.rend(); }

    bool isEmpty() const { return m_operations.isEmpty(); }
    size_t size() const { return m_operations.size(); }
    const FilterOperation* at(size_t index) const { return index < m_operations.size() ? &m_operations[index] : nullptr; }

    const FilterOperation& operator[](size_t i) const { return m_operations[i]; }
    const FilterOperation& first() const { return m_operations.first(); }
    const FilterOperation& last() const { return m_operations.last(); }

    template<typename T> bool hasFilterOfType() const;

    bool hasFilterThatAffectsOpacity() const;
    bool hasFilterThatMovesPixels() const;
    bool hasFilterThatShouldBeRestrictedBySecurityOrigin() const;
    bool hasFilterThatUsesCurrentColor() const;

    bool hasReferenceFilter() const;
    bool hasReferenceFilterOnly() const;

    IntOutsets outsets() const;
    IntOutsets outsets(RenderElement&, const FloatRect& targetBoundingBox) const;
    bool isIdentity(RenderElement&) const;

    static bool canInterpolate(const FilterOperations& from, const FilterOperations& to, CompositeOperation);
    static FilterOperations blend(const FilterOperations& from, const FilterOperations& to, const RenderStyle& fromStyle, const RenderStyle& toStyle, const BlendingContext&);

    static bool filtersProduceDifferentOutput(const FilterOperations&, const FilterOperations&, const RenderStyle& aStyle, const RenderStyle& bStyle);

    bool transformColor(Color&) const;
    bool inverseTransformColor(Color&) const;

    // Loads any external documents.
    void load(CachedResourceLoader&, const ResourceLoaderOptions&);

    bool operator==(const FilterOperations&) const;

private:
    Storage m_operations;
};

template<typename T> bool FilterOperations::hasFilterOfType() const
{
    return std::ranges::any_of(m_operations, [](auto& op) { return std::holds_alternative<T>(op); });
}

WTF::TextStream& operator<<(WTF::TextStream&, const FilterOperations::FilterOperation&);
WTF::TextStream& operator<<(WTF::TextStream&, const FilterOperations&);

} // namespace Style
} // namespace WebCore
