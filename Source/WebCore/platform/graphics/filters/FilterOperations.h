/*
 * Copyright (C) 2011-2024 Apple Inc. All rights reserved.
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

#include "Color.h"
#include "LengthPoint.h"
#include "RectEdges.h"
#include <algorithm>
#include <wtf/ArgumentCoder.h>
#include <wtf/FixedVector.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct BlendingContext;
enum class CompositeOperation : uint8_t;

class FilterOperations {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Type : uint8_t {
        Reference, // url(#somefilter)
        Grayscale,
        Sepia,
        Saturate,
        HueRotate,
        Invert,
        AppleInvertLightness,
        Opacity,
        Brightness,
        Contrast,
        Blur,
        DropShadow
    };
    struct Reference {
        static constexpr auto type = Type::Reference;

        String url;
        AtomString fragment;
        bool operator==(const Reference&) const = default;
    };
    struct Grayscale {
        static constexpr auto type = Type::Grayscale;
        static constexpr auto identity = double { 0 };

        double amount;
        bool operator==(const Grayscale&) const = default;
    };
    struct Sepia {
        static constexpr auto type = Type::Sepia;
        static constexpr auto identity = double { 0 };

        double amount;
        bool operator==(const Sepia&) const = default;
    };
    struct Saturate {
        static constexpr auto type = Type::Saturate;
        static constexpr auto identity = double { 1 };

        double amount;
        bool operator==(const Saturate&) const = default;
    };
    struct HueRotate {
        static constexpr auto type = Type::HueRotate;
        static constexpr auto identity = double { 0 };

        double amount;
        bool operator==(const HueRotate&) const = default;
    };
    struct Invert {
        static constexpr auto type = Type::Invert;
        static constexpr auto identity = double { 0 };

        double amount;
        bool operator==(const Invert&) const = default;
    };
    struct Opacity {
        static constexpr auto type = Type::Opacity;
        static constexpr auto identity = double { 1 };

        double amount;
        bool operator==(const Opacity&) const = default;
    };
    struct Brightness {
        static constexpr auto type = Type::Brightness;
        static constexpr auto identity = double { 1 };

        double amount;
        bool operator==(const Brightness&) const = default;
    };
    struct Contrast {
        static constexpr auto type = Type::Contrast;
        static constexpr auto identity = double { 1 };

        double amount;
        bool operator==(const Contrast&) const = default;
    };
    struct Blur {
        static constexpr auto type = Type::Blur;

        Length stdDeviation;
        bool operator==(const Blur&) const = default;
    };
    struct DropShadow {
        static constexpr auto type = Type::DropShadow;

        LengthPoint location;
        Length stdDeviation;
        Color color;
        bool operator==(const DropShadow&) const = default;
    };
    struct AppleInvertLightness {
        static constexpr auto type = Type::AppleInvertLightness;

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
    WEBCORE_EXPORT FilterOperations(Storage&&);

    WEBCORE_EXPORT bool operator==(const FilterOperations&) const;

    FilterOperations clone() const { return *this; }

    bool operationsMatch(const FilterOperations&) const; // GraphicsLayer

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
    static Type type(const FilterOperation&);

    bool hasReferenceFilter() const;

    bool hasFilterThatAffectsOpacity() const;
    bool hasFilterThatMovesPixels() const;
    bool hasFilterThatShouldBeRestrictedBySecurityOrigin() const;

    static bool filterAffectsOpacity(const FilterOperation&);
    static bool filterMovesPixels(const FilterOperation&);
    static bool filterShouldBeRestrictedBySecurityOrigin(const FilterOperation&);

    WEBCORE_EXPORT static bool canInterpolate(const FilterOperations&, const FilterOperations&, CompositeOperation);
    WEBCORE_EXPORT static FilterOperations blend(const FilterOperations&, const FilterOperations&, const BlendingContext&);

    static FilterOperation initialValueForInterpolationMatchingType(const FilterOperation&);
    static FilterOperation initialValueForInterpolationMatchingType(Type);

    static bool isIdentity(const FilterOperation&);

    bool hasOutsets() const;
    RectEdges<int> outsets() const;
    static RectEdges<int> calculateOutsets(const FilterOperation&);

private:
    friend struct IPC::ArgumentCoder<FilterOperations, void>;

    Storage m_operations;
};

template<typename T> bool FilterOperations::hasFilterOfType() const
{
    return std::ranges::any_of(m_operations, [](auto& op) { return std::holds_alternative<T>(op); });
}

inline FilterOperations::Type FilterOperations::type(const FilterOperations::FilterOperation& operation)
{
    return WTF::switchOn(operation, [](const auto& op) { return op.type; });
}

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const FilterOperations::FilterOperation&);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const FilterOperations&);

} // namespace WebCore
