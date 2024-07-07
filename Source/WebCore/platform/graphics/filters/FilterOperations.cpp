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

#include "config.h"
#include "FilterOperations.h"

#include "AnimationUtilities.h"
#include "ColorBlending.h"
#include "FEDropShadow.h"
#include "FEGaussianBlur.h"
#include "LengthFunctions.h"
#include "RectEdges.h"
#include <ranges>
#include <wtf/text/TextStream.h>

namespace WebCore {

template<typename> struct FilterOperationDescriptor;

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-blur
template<> struct FilterOperationDescriptor<FilterOperations::Blur> {
    static constexpr bool allowsValuesGreaterThanOne = false;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto initialValueForInterpolation = double { 0 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-brightness
template<> struct FilterOperationDescriptor<FilterOperations::Brightness> {
    static constexpr bool allowsValuesGreaterThanOne = true;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto initialValueForInterpolation = double { 1 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-contrast
template<> struct FilterOperationDescriptor<FilterOperations::Contrast> {
    static constexpr bool allowsValuesGreaterThanOne = true;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto initialValueForInterpolation = double { 1 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-drop-shadow
template<> struct FilterOperationDescriptor<FilterOperations::DropShadow> {
    static constexpr auto initialColorValueForInterpolation = Color::transparentBlack;
    static constexpr auto initialLengthValueForInterpolation = double { 0 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-grayscale
template<> struct FilterOperationDescriptor<FilterOperations::Grayscale> {
    static constexpr bool allowsValuesGreaterThanOne = false;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto initialValueForInterpolation = double { 0 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-hue-rotate
template<> struct FilterOperationDescriptor<FilterOperations::HueRotate> {
    static constexpr auto initialValueForInterpolation = double { 0 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-invert
template<> struct FilterOperationDescriptor<FilterOperations::Invert> {
    static constexpr bool allowsValuesGreaterThanOne = false;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto initialValueForInterpolation = double { 0 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-invert
template<> struct FilterOperationDescriptor<FilterOperations::Opacity> {
    static constexpr bool allowsValuesGreaterThanOne = false;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto initialValueForInterpolation = double { 1 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-saturate
template<> struct FilterOperationDescriptor<FilterOperations::Saturate> {
    static constexpr bool allowsValuesGreaterThanOne = true;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto initialValueForInterpolation = double { 1 };
};

// https://drafts.fxtf.org/filter-effects/#funcdef-filter-sepia
template<> struct FilterOperationDescriptor<FilterOperations::Sepia> {
    static constexpr bool allowsValuesGreaterThanOne = false;
    static constexpr bool allowsValuesLessThanZero = false;
    static constexpr auto initialValueForInterpolation = double { 0 };
};

template<> struct FilterOperationDescriptor<FilterOperations::AppleInvertLightness> { };

template<typename T> static constexpr bool filterFunctionAllowsValuesGreaterThanOne()
{
    return FilterOperationDescriptor<T>::allowsValuesGreaterThanOne;
}

template<typename T> static constexpr bool filterFunctionAllowsValuesLessThanZero()
{
    return FilterOperationDescriptor<T>::allowsValuesLessThanZero;
}

template<typename T> static constexpr decltype(auto) filterFunctionInitialValueForInterpolation()
{
    return FilterOperationDescriptor<T>::initialValueForInterpolation;
}

FilterOperations::FilterOperations(Storage&& operations)
    : m_operations { WTFMove(operations) }
{
}

bool FilterOperations::operator==(const FilterOperations& other) const
{
    return m_operations == other.m_operations;
}

bool FilterOperations::operationsMatch(const FilterOperations& other) const
{
    static_assert(std::ranges::sized_range<decltype(m_operations)>);

    return std::ranges::equal(m_operations, other.m_operations, [](auto& a, auto& b) { return a.index() == b.index(); });
}

bool FilterOperations::hasReferenceFilter() const
{
    return hasFilterOfType<FilterOperations::Reference>();
}

bool FilterOperations::filterAffectsOpacity(const FilterOperation& operation)
{
    return WTF::switchOn(operation,
        [](const FilterOperations::Reference&) -> bool {
            return true;
        },
        [](const FilterOperations::Opacity&) -> bool {
            return true;
        },
        [](const FilterOperations::Blur&) -> bool {
            return true;
        },
        [](const FilterOperations::DropShadow&) -> bool {
            return true;
        },
        [](const auto&) -> bool {
            return false;
        }
    );
}

bool FilterOperations::hasFilterThatAffectsOpacity() const
{
    return std::ranges::any_of(m_operations, [](auto& op) { return filterAffectsOpacity(op); });
}

bool FilterOperations::filterMovesPixels(const FilterOperation& operation)
{
    return WTF::switchOn(operation,
        [](const FilterOperations::Reference&) -> bool {
            return true;
        },
        [](const FilterOperations::Blur&) -> bool {
            return true;
        },
        [](const FilterOperations::DropShadow&) -> bool {
            return true;
        },
        [](const auto&) -> bool {
            return false;
        }
    );
}

bool FilterOperations::hasFilterThatMovesPixels() const
{
    return std::ranges::any_of(m_operations, [](auto& op) { return filterMovesPixels(op); });
}

bool FilterOperations::filterShouldBeRestrictedBySecurityOrigin(const FilterOperation& operation)
{
    return WTF::switchOn(operation,
        [](const FilterOperations::Reference&) -> bool {
            // FIXME: This only needs to return true for graphs that include ConvolveMatrix,
            //        DisplacementMap, Morphology and possibly Lighting.
            // https://bugs.webkit.org/show_bug.cgi?id=171753
            return true;
        },
        [](const auto&) -> bool {
            return false;
        }
    );
}

bool FilterOperations::hasFilterThatShouldBeRestrictedBySecurityOrigin() const
{
    return std::ranges::any_of(m_operations, [](auto& op) { return filterShouldBeRestrictedBySecurityOrigin(op); });
}

bool FilterOperations::canInterpolate(const FilterOperations& from, const FilterOperations& to, CompositeOperation compositeOperation)
{
    // https://drafts.fxtf.org/filter-effects/#interpolation-of-filters

    // We can't interpolate between lists if a reference filter is involved.
    if (from.hasReferenceFilter() || to.hasReferenceFilter())
        return false;

    // Additive and accumulative composition will always yield interpolation.
    if (compositeOperation != CompositeOperation::Replace)
        return true;

    // Provided the two filter lists have a shared set of initial primitives, we will be able to interpolate.
    // Note that this means that if either list is empty, interpolation is supported.
    for (size_t i = 0; i < std::min(from.size(), to.size()); ++i) {
        if (from.m_operations[i].index() != to.m_operations[i].index())
            return false;
    }

    return true;
}

static FilterOperations::FilterOperation blendOperation(const FilterOperations::FilterOperation& from, const FilterOperations::FilterOperation& to, const BlendingContext& context)
{
    ASSERT(from.index() == to.index());

    return std::visit(WTF::makeVisitor(
        [&](const FilterOperations::Reference& from, const FilterOperations::Reference&) -> FilterOperations::FilterOperation {
            ASSERT_NOT_REACHED();
            return from;
        },
        [&](const FilterOperations::DropShadow& from, const FilterOperations::DropShadow& to) -> FilterOperations::FilterOperation {
            return FilterOperations::DropShadow {
                WebCore::blend(from.location, to.location, context),
                WebCore::blend(from.stdDeviation, to.stdDeviation, context, ValueRange::NonNegative),
                WebCore::blend(from.color, to.color, context)
            };
        },
        [&](const FilterOperations::Blur& from, const FilterOperations::Blur& to) -> FilterOperations::FilterOperation {
            return FilterOperations::Blur { WebCore::blend(from.stdDeviation, to.stdDeviation, context, ValueRange::NonNegative) };
        },
        [&](const FilterOperations::HueRotate& from, const FilterOperations::HueRotate& to) -> FilterOperations::FilterOperation {
            return FilterOperations::HueRotate { WebCore::blend(from.amount, to.amount, context) };
        },
        [&](const FilterOperations::AppleInvertLightness&, const FilterOperations::AppleInvertLightness&) -> FilterOperations::FilterOperation {
            return FilterOperations::AppleInvertLightness { };
        },
        [&]<typename T>(const T& from, const T& to) -> FilterOperations::FilterOperation {
            double blendedAmount = [&] {
                if constexpr (filterFunctionInitialValueForInterpolation<T>() == 1) {
                    if (context.compositeOperation == CompositeOperation::Accumulate)
                        return from.amount + to.amount - 1;
                }
                return WebCore::blend(from.amount, to.amount, context);
            }();

            if constexpr (!filterFunctionAllowsValuesGreaterThanOne<T>())
                blendedAmount = std::min(blendedAmount, 1.0);
            if constexpr (!filterFunctionAllowsValuesLessThanZero<T>())
                blendedAmount = std::max(blendedAmount, 0.0);

            return T { blendedAmount };
        },
        []<typename T, typename U>(const T& from, const U&) -> FilterOperations::FilterOperation {
            static_assert(!std::same_as<T, U>);
            RELEASE_ASSERT_NOT_REACHED();
            return from;
        }
    ), from, to);
}

FilterOperations::FilterOperation FilterOperations::initialValueForInterpolationMatchingType(const FilterOperation& operation)
{
    return WTF::switchOn(operation,
        [](const Reference& op) -> FilterOperation {
            RELEASE_ASSERT_NOT_REACHED();
            return op;
        },
        [](const Blur&) -> FilterOperation {
            using Descriptor = FilterOperationDescriptor<Blur>;
            return Blur {
                { Descriptor::initialValueForInterpolation, LengthType::Fixed },
            };
        },
        [](const DropShadow&) -> FilterOperation {
            using Descriptor = FilterOperationDescriptor<DropShadow>;
            return DropShadow {
                { { Descriptor::initialLengthValueForInterpolation, LengthType::Fixed }, { Descriptor::initialLengthValueForInterpolation, LengthType::Fixed } },
                { Descriptor::initialLengthValueForInterpolation, LengthType::Fixed },
                { Descriptor::initialColorValueForInterpolation }
            };
        },
        [](const AppleInvertLightness&) -> FilterOperation {
            return AppleInvertLightness { };
        },
        []<typename T>(const T&) -> FilterOperation {
            return T { filterFunctionInitialValueForInterpolation<T>() };
        }
    );
}

FilterOperations::FilterOperation FilterOperations::initialValueForInterpolationMatchingType(Type type)
{
    switch (type) {
    case Type::Grayscale:
        return Grayscale { filterFunctionInitialValueForInterpolation<Grayscale>() };
    case Type::Sepia:
        return Sepia { filterFunctionInitialValueForInterpolation<Sepia>() };
    case Type::Saturate:
        return Saturate { filterFunctionInitialValueForInterpolation<Saturate>() };
    case Type::HueRotate:
        return HueRotate { filterFunctionInitialValueForInterpolation<HueRotate>() };
    case Type::Invert:
        return Invert { filterFunctionInitialValueForInterpolation<Invert>() };
    case Type::Opacity:
        return Opacity { filterFunctionInitialValueForInterpolation<Opacity>() };
    case Type::Brightness:
        return Brightness { filterFunctionInitialValueForInterpolation<Brightness>() };
    case Type::Contrast:
        return Contrast { filterFunctionInitialValueForInterpolation<Contrast>() };
    case Type::AppleInvertLightness:
        return AppleInvertLightness { };
    case Type::Blur:
        return Blur { { FilterOperationDescriptor<Blur>::initialValueForInterpolation, LengthType::Fixed } };
    case Type::DropShadow:
        return DropShadow {
            { { FilterOperationDescriptor<DropShadow>::initialLengthValueForInterpolation, LengthType::Fixed }, { FilterOperationDescriptor<DropShadow>::initialLengthValueForInterpolation, LengthType::Fixed } },
            { FilterOperationDescriptor<DropShadow>::initialLengthValueForInterpolation, LengthType::Fixed },
            { FilterOperationDescriptor<DropShadow>::initialColorValueForInterpolation }
        };
    case Type::Reference:
        RELEASE_ASSERT_NOT_REACHED();
        return Reference { emptyString(), emptyAtom() };
    }

    RELEASE_ASSERT_NOT_REACHED();
    return Grayscale { filterFunctionInitialValueForInterpolation<Grayscale>() };
}

FilterOperations FilterOperations::blend(const FilterOperations& from, const FilterOperations& to, const BlendingContext& context)
{
    ASSERT(canInterpolate(from, to, context.compositeOperation));

    if (context.compositeOperation == CompositeOperation::Add) {
        ASSERT(context.progress == 1.0);

        auto fromSize = from.m_operations.size();
        auto size = fromSize + to.m_operations.size();
        return { Storage { size, [&](size_t i) -> FilterOperation {
            return i < fromSize ? from.m_operations[i] : to.m_operations[i - fromSize];
        } } };
    }

    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

    auto fromSize = from.m_operations.size();
    auto toSize = to.m_operations.size();
    auto size = std::max(fromSize, toSize);

    return { Storage { size, [&](size_t i) -> FilterOperation {
        const FilterOperation* fromOp = (i < fromSize) ? &from.m_operations[i] : nullptr;
        const FilterOperation* toOp = (i < toSize) ? &to.m_operations[i] : nullptr;
        ASSERT(fromOp || toOp);

        if (toOp && fromOp)
            return blendOperation(*fromOp, *toOp, context);
        if (fromOp)
            return blendOperation(*fromOp, initialValueForInterpolationMatchingType(*fromOp), context);
        return blendOperation(initialValueForInterpolationMatchingType(*toOp), *toOp, context);
    } } };
}

bool FilterOperations::isIdentity(const FilterOperation& operation)
{
    return WTF::switchOn(operation,
        [&](const FilterOperations::Reference&) -> bool {
            // Answering this question requires access to the renderer and the referenced filterElement.
            ASSERT_NOT_REACHED();
            return false;
        },
        [&](const FilterOperations::Blur& op) -> bool {
            auto stdDeviation = floatValueForLength(op.stdDeviation, 0);
            return stdDeviation <= 0;
        },
        [&](const FilterOperations::DropShadow& op) -> bool {
            auto location = floatPointForLengthPoint(op.location, { 0, 0 });
            auto stdDeviation = floatValueForLength(op.stdDeviation, 0);
            return stdDeviation < 0 || (!stdDeviation && location.isZero());
        },
        [&](const FilterOperations::AppleInvertLightness&) -> bool {
            return false;
        },
        [&]<typename T>(const T& op) -> bool {
            return op.amount == T::identity;
        }
    );
}

RectEdges<int> FilterOperations::calculateOutsets(const FilterOperation& operation)
{
    return WTF::switchOn(operation,
        [&](const FilterOperations::Reference&) -> RectEdges<int> {
            // Answering this question requires access to the renderer and the referenced filterElement.
            ASSERT_NOT_REACHED();
            return { };
        },
        [&](const FilterOperations::Blur& op) -> RectEdges<int> {
            auto stdDeviation = floatValueForLength(op.stdDeviation, 0);
            return FEGaussianBlur::calculateOutsets({ stdDeviation, stdDeviation });
        },
        [&](const FilterOperations::DropShadow& op) -> RectEdges<int> {
            auto location = toFloatSize(floatPointForLengthPoint(op.location, { 0, 0 }));
            auto stdDeviation = floatValueForLength(op.stdDeviation, 0);
            return FEDropShadow::calculateOutsets(location, { stdDeviation, stdDeviation });
        },
        [&](const auto&) -> RectEdges<int> {
            return { };
        }
    );
}

bool FilterOperations::hasOutsets() const
{
    return !outsets().isZero();
}

RectEdges<int> FilterOperations::outsets() const
{
    RectEdges<int> result;
    for (const auto& op : m_operations)
        result += calculateOutsets(op);
    return result;
}

// MARK: - TextStream Support

TextStream& operator<<(TextStream& ts, const FilterOperations::FilterOperation& operation)
{
    WTF::switchOn(operation,
        [&](const FilterOperations::Reference&) {
            ts << "reference";
        },
        [&](const FilterOperations::Grayscale& op) {
            ts << "grayscale(" << op.amount << ")";
        },
        [&](const FilterOperations::Sepia& op) {
            ts << "sepia(" << op.amount << ")";
        },
        [&](const FilterOperations::Saturate& op) {
            ts << "saturate(" << op.amount << ")";
        },
        [&](const FilterOperations::HueRotate& op) {
            ts << "hue-rotate(" << op.amount << ")";
        },
        [&](const FilterOperations::Invert& op) {
            ts << "invert(" << op.amount << ")";
        },
        [&](const FilterOperations::Opacity& op) {
            ts << "opacity(" << op.amount << ")";
        },
        [&](const FilterOperations::Brightness& op) {
            ts << "brightness(" << op.amount << ")";
        },
        [&](const FilterOperations::Contrast& op) {
            ts << "contrast(" << op.amount << ")";
        },
        [&](const FilterOperations::Blur& op) {
            ts << "blur(" << op.stdDeviation << ")";
        },
        [&](const FilterOperations::DropShadow& op) {
            ts << "drop-shadow(" << op.location << " " << op.stdDeviation << " " << op.color << ")";
        },
        [&](const FilterOperations::AppleInvertLightness&) {
            ts << "apple-invert-lightness()";
        }
    );
    return ts;
}

TextStream& operator<<(TextStream& ts, const FilterOperations& filter)
{
    WTF::streamSizedContainer(ts, filter);
    return ts;
}

} // namespace WebCore
