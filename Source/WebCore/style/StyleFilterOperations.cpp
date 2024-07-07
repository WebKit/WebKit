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

#include "config.h"
#include "StyleFilterOperations.h"

#include "AnimationUtilities.h"
#include "CSSFilterFunctionDescriptor.h"
#include "CSSUnresolvedFilterResolutionContext.h"
#include "CachedResourceLoader.h"
#include "ColorBlending.h"
#include "ComputedStyleExtractor.h"
#include "FEDropShadow.h"
#include "FEGaussianBlur.h"
#include "Logging.h"
#include "ReferencedSVGResources.h"
#include "RenderElement.h"
#include "RenderStyle.h"
#include "SVGFilter.h"
#include "SVGFilterElement.h"
#include "SVGURIReference.h"
#include <ranges>

namespace WebCore {
namespace Style {

FilterOperations::FilterOperations(Storage&& operations)
    : m_operations { WTFMove(operations) }
{
}

template<typename ColorResolver> static WebCore::FilterOperations::FilterOperation resolveOperation(const FilterOperations::FilterOperation& operation, ColorResolver&& colorResolver)
{
    return WTF::switchOn(operation,
        [&](const FilterOperations::Reference& op) -> WebCore::FilterOperations::FilterOperation {
            return WebCore::FilterOperations::Reference { op.url, op.fragment };
        },
        [&](const FilterOperations::Grayscale& op) -> WebCore::FilterOperations::FilterOperation {
            return WebCore::FilterOperations::Grayscale { op.amount.value };
        },
        [&](const FilterOperations::Sepia& op) -> WebCore::FilterOperations::FilterOperation {
            return WebCore::FilterOperations::Sepia { op.amount.value };
        },
        [&](const FilterOperations::Saturate& op) -> WebCore::FilterOperations::FilterOperation {
            return WebCore::FilterOperations::Saturate { op.amount.value };
        },
        [&](const FilterOperations::HueRotate& op) -> WebCore::FilterOperations::FilterOperation {
            return WebCore::FilterOperations::HueRotate { op.amount.value };
        },
        [&](const FilterOperations::Invert& op) -> WebCore::FilterOperations::FilterOperation {
            return WebCore::FilterOperations::Invert { op.amount.value };
        },
        [&](const FilterOperations::Opacity& op) -> WebCore::FilterOperations::FilterOperation {
            return WebCore::FilterOperations::Opacity { op.amount.value };
        },
        [&](const FilterOperations::Brightness& op) -> WebCore::FilterOperations::FilterOperation {
            return WebCore::FilterOperations::Brightness { op.amount.value };
        },
        [&](const FilterOperations::Contrast& op) -> WebCore::FilterOperations::FilterOperation {
            return WebCore::FilterOperations::Contrast { op.amount.value };
        },
        [&](const FilterOperations::Blur& op) -> WebCore::FilterOperations::FilterOperation {
            return WebCore::FilterOperations::Blur { op.stdDeviation };
        },
        [&](const FilterOperations::DropShadow& op) -> WebCore::FilterOperations::FilterOperation {
            return WebCore::FilterOperations::DropShadow { op.location, op.stdDeviation, colorResolver(op.color) };
        },
        [&](const FilterOperations::AppleInvertLightness&) -> WebCore::FilterOperations::FilterOperation {
            return WebCore::FilterOperations::AppleInvertLightness { };
        }
    );
}

WebCore::FilterOperations::FilterOperation FilterOperations::resolve(const FilterOperation& operation, const RenderStyle& style)
{
    return resolveOperation(operation, [&](const StyleColor& color) {
        return style.colorResolvingCurrentColor(color);
    });
}

WebCore::FilterOperations::FilterOperation FilterOperations::resolve(const FilterOperation& operation, const CSSUnresolvedFilterResolutionContext& context)
{
    return resolveOperation(operation, [&](const StyleColor& color) {
        return context.color.resolveColor(color);
    });
}

WebCore::FilterOperations FilterOperations::resolve(const RenderStyle& style) const
{
    return WTF::map(m_operations, [&](const auto& op) {
        return FilterOperations::resolve(op, style);
    });
}

WebCore::FilterOperations FilterOperations::resolve(const CSSUnresolvedFilterResolutionContext& context) const
{
    return WTF::map(m_operations, [&](const auto& op) {
        return FilterOperations::resolve(op, context);
    });
}

Ref<CSSValue> FilterOperations::computedStyleValue(const RenderStyle& style) const
{
    return ComputedStyleExtractor::valueForFilter(style, *this);
}

bool FilterOperations::operator==(const FilterOperations& other) const
{
    return m_operations == other.m_operations;
}

bool FilterOperations::hasReferenceFilter() const
{
    return hasFilterOfType<Reference>();
}

bool FilterOperations::hasReferenceFilterOnly() const
{
    return m_operations.size() == 1 && std::holds_alternative<Reference>(m_operations[0]);
}

static bool filterAffectsOpacity(const FilterOperations::FilterOperation& operation)
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

static bool filterMovesPixels(const FilterOperations::FilterOperation& operation)
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

static bool filterShouldBeRestrictedBySecurityOrigin(const FilterOperations::FilterOperation& operation)
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

static bool filterUsesCurrentColor(const FilterOperations::FilterOperation& operation)
{
    return WTF::switchOn(operation,
        [](const FilterOperations::DropShadow& op) -> bool {
            return op.color.containsCurrentColor();
        },
        [](const auto&) -> bool {
            return false;
        }
    );
}

bool FilterOperations::hasFilterThatUsesCurrentColor() const
{
    return std::ranges::any_of(m_operations, [](auto& op) { return filterUsesCurrentColor(op); });
}

static RefPtr<SVGFilterElement> referenceFilterElement(const AtomString& fragment, RenderElement& renderer)
{
    RefPtr filterElement = ReferencedSVGResources::referencedFilterElement(renderer.treeScopeForSVGReferences(), fragment);

    if (!filterElement) {
        LOG_WITH_STREAM(Filters, stream << " buildReferenceFilter: failed to find filter renderer, adding pending resource " << fragment);
        // Although we did not find the referenced filter, it might exist later in the document.
        // FIXME: This skips anonymous RenderObjects. <https://webkit.org/b/131085>
        // FIXME: Unclear if this does anything.
        return nullptr;
    }

    return filterElement;
}

static bool isIdentity(const FilterOperations::FilterOperation& operation, RenderElement& renderer)
{
    return WTF::switchOn(operation,
        [&](const FilterOperations::Reference& op) -> bool {
            RefPtr filterElement = referenceFilterElement(op.fragment, renderer);
            if (!filterElement)
                return false;
            return SVGFilter::isIdentity(*filterElement);
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

bool FilterOperations::isIdentity(RenderElement& renderer) const
{
    if (hasFilterThatShouldBeRestrictedBySecurityOrigin())
        return false;

    return std::ranges::all_of(m_operations, [&](auto& op) { return WebCore::Style::isIdentity(op, renderer); });
}

static IntOutsets outsets(const FilterOperations::Reference& op, RenderElement& renderer, const FloatRect& targetBoundingBox)
{
    RefPtr filterElement = referenceFilterElement(op.fragment, renderer);
    if (!filterElement)
        return { };
    return SVGFilter::calculateOutsets(*filterElement, targetBoundingBox);
}

static IntOutsets outsets(const FilterOperations::Blur& op)
{
    auto stdDeviation = floatValueForLength(op.stdDeviation, 0);
    return FEGaussianBlur::calculateOutsets({ stdDeviation, stdDeviation });
}

static IntOutsets outsets(const FilterOperations::DropShadow& op)
{
    auto location = toFloatSize(floatPointForLengthPoint(op.location, { 0, 0 }));
    auto stdDeviation = floatValueForLength(op.stdDeviation, 0);
    return FEDropShadow::calculateOutsets(location, { stdDeviation, stdDeviation });
}

static IntOutsets outsets(const FilterOperations::FilterOperation& operation)
{
    return WTF::switchOn(operation,
        [&](const FilterOperations::Reference&) -> IntOutsets {
            // Answering this question requires access to the renderer and the referenced filterElement.
            ASSERT_NOT_REACHED();
            return { };
        },
        [&](const FilterOperations::Blur& op) -> IntOutsets {
            return outsets(op);
        },
        [&](const FilterOperations::DropShadow& op) -> IntOutsets {
            return outsets(op);
        },
        [&](const auto&) -> IntOutsets {
            return { };
        }
    );
}

IntOutsets FilterOperations::outsets() const
{
    IntOutsets result;
    for (const auto& op : m_operations)
        result += WebCore::Style::outsets(op);
    return result;
}

static IntOutsets outsets(const FilterOperations::FilterOperation& operation, RenderElement& renderer, const FloatRect& targetBoundingBox)
{
    return WTF::switchOn(operation,
        [&](const FilterOperations::Reference& op) -> IntOutsets {
            return outsets(op, renderer, targetBoundingBox);
        },
        [&](const FilterOperations::Blur& op) -> IntOutsets {
            return outsets(op);
        },
        [&](const FilterOperations::DropShadow& op) -> IntOutsets {
            return outsets(op);
        },
        [&](const auto&) -> IntOutsets {
            return { };
        }
    );
}

IntOutsets FilterOperations::outsets(RenderElement& renderer, const FloatRect& targetBoundingBox) const
{
    IntOutsets result;
    for (const auto& op : m_operations)
        result += WebCore::Style::outsets(op, renderer, targetBoundingBox);
    return result;
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

static FilterOperations::FilterOperation blendOperation(const FilterOperations::FilterOperation& from, const FilterOperations::FilterOperation& to, const RenderStyle& fromStyle, const RenderStyle& toStyle, const BlendingContext& context)
{
    ASSERT(from.index() == to.index());

    return std::visit(WTF::makeVisitor(
        [&](const FilterOperations::Reference& from, const FilterOperations::Reference&) -> FilterOperations::FilterOperation {
            return from;
        },
        [&](const FilterOperations::DropShadow& from, const FilterOperations::DropShadow& to) -> FilterOperations::FilterOperation {
            return FilterOperations::DropShadow {
                WebCore::blend(from.location, to.location, context),
                WebCore::blend(from.stdDeviation, to.stdDeviation, context, ValueRange::NonNegative),
                WebCore::blend(fromStyle.colorResolvingCurrentColor(from.color), toStyle.colorResolvingCurrentColor(to.color), context)
            };
        },
        [&](const FilterOperations::Blur& from, const FilterOperations::Blur& to) -> FilterOperations::FilterOperation {
            return FilterOperations::Blur { WebCore::blend(from.stdDeviation, to.stdDeviation, context, ValueRange::NonNegative) };
        },
        [&](const FilterOperations::HueRotate& from, const FilterOperations::HueRotate& to) -> FilterOperations::FilterOperation {
            return FilterOperations::HueRotate { { WebCore::blend(from.amount.value, to.amount.value, context) } };
        },
        [&](const FilterOperations::AppleInvertLightness&, const FilterOperations::AppleInvertLightness&) -> FilterOperations::FilterOperation {
            return FilterOperations::AppleInvertLightness { };
        },
        [&]<typename T>(const T& from, const T& to) -> FilterOperations::FilterOperation {
            double blendedAmount = [&] {
                if constexpr (filterFunctionInitialValueForInterpolation<T::functionID>().value == 1) {
                    if (context.compositeOperation == CompositeOperation::Accumulate)
                        return from.amount.value + to.amount.value - 1;
                }
                return WebCore::blend(from.amount.value, to.amount.value, context);
            }();

            if constexpr (!filterFunctionAllowsValuesGreaterThanOne<T::functionID>())
                blendedAmount = std::min(blendedAmount, 1.0);
            if constexpr (!filterFunctionAllowsValuesLessThanZero<T::functionID>())
                blendedAmount = std::max(blendedAmount, 0.0);

            return T { { blendedAmount } };
        },
        []<typename T, typename U>(const T& from, const U&) -> FilterOperations::FilterOperation {
            static_assert(!std::same_as<T, U>);
            RELEASE_ASSERT_NOT_REACHED();
            return from;
        }
    ), from, to);
}

static FilterOperations::FilterOperation initialValueForInterpolationMatchingType(const FilterOperations::FilterOperation& operation)
{
    return WTF::switchOn(operation,
        [](const FilterOperations::Reference& op) -> FilterOperations::FilterOperation {
            return op;
        },
        [](const FilterOperations::Blur&) -> FilterOperations::FilterOperation {
            using Descriptor = CSSFilterFunctionDescriptor<FilterOperations::Blur::functionID>;
            return FilterOperations::Blur {
                { Descriptor::initialValueForInterpolation.value, LengthType::Fixed },
            };
        },
        [](const FilterOperations::DropShadow&) -> FilterOperations::FilterOperation {
            using Descriptor = CSSFilterFunctionDescriptor<FilterOperations::DropShadow::functionID>;
            return FilterOperations::DropShadow {
                { { Descriptor::initialLengthValueForInterpolation.value, LengthType::Fixed }, { Descriptor::initialLengthValueForInterpolation.value, LengthType::Fixed } },
                { Descriptor::initialLengthValueForInterpolation.value, LengthType::Fixed },
                { Descriptor::initialColorValueForInterpolation }
            };
        },
        [](const FilterOperations::AppleInvertLightness&) -> FilterOperations::FilterOperation {
            return FilterOperations::AppleInvertLightness { };
        },
        []<typename T>(const T&) -> FilterOperations::FilterOperation {
            return T { filterFunctionInitialValueForInterpolation<T::functionID>() };
        }
    );
}

FilterOperations FilterOperations::blend(const FilterOperations& from, const FilterOperations& to, const RenderStyle& fromStyle, const RenderStyle& toStyle, const BlendingContext& context)
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
            return blendOperation(*fromOp, *toOp, fromStyle, toStyle, context);
        if (fromOp)
            return blendOperation(*fromOp, initialValueForInterpolationMatchingType(*fromOp), fromStyle, toStyle, context);
        return blendOperation(initialValueForInterpolationMatchingType(*toOp), *toOp, fromStyle, toStyle, context);
    } } };
}

bool FilterOperations::filtersProduceDifferentOutput(const FilterOperations& a, const FilterOperations& b, const RenderStyle& aStyle, const RenderStyle& bStyle)
{
    if (a != b)
        return true;

    // If the operations are the same, the output might still differ if `currentcolor` is used
    // by `DropShadow` and the `RenderStyles` have distinct `color()` values.
    if (a.hasFilterThatUsesCurrentColor() && aStyle.color() != bStyle.color())
        return true;
    return false;
}

// FIXME: This hueRotate code exists to allow InvertLightnessFilterOperation to perform hue rotation
// on color values outside of the non-extended SRGB value range (0-1) to maintain the behavior of colors
// prior to clamping being enforced. It should likely just use the existing hueRotateColorMatrix(amount)
// in ColorMatrix.h
static ColorComponents<float, 4> hueRotate(const ColorComponents<float, 4>& color, float amount)
{
    auto [r, g, b, alpha] = color;

    auto [min, max] = std::minmax({ r, g, b });
    float chroma = max - min;

    float lightness = 0.5f * (max + min);
    float saturation;
    if (!chroma)
        saturation = 0;
    else if (lightness <= 0.5f)
        saturation = (chroma / (max + min));
    else
        saturation = (chroma / (2.0f - (max + min)));

    if (!saturation)
        return { lightness, lightness, lightness, alpha };

    float hue;
    if (!chroma)
        hue = 0;
    else if (max == r)
        hue = (60.0f * ((g - b) / chroma)) + 360.0f;
    else if (max == g)
        hue = (60.0f * ((b - r) / chroma)) + 120.0f;
    else
        hue = (60.0f * ((r - g) / chroma)) + 240.0f;

    if (hue >= 360.0f)
        hue -= 360.0f;

    hue /= 360.0f;

    // Perform rotation.
    hue = std::fmod(hue + amount, 1.0f);

    float temp2 = lightness <= 0.5f ? lightness * (1.0f + saturation) : lightness + saturation - lightness * saturation;
    float temp1 = 2.0f * lightness - temp2;

    hue *= 6.0f; // calcHue() wants hue in the 0-6 range.

    // Hue is in the range 0-6, other args in 0-1.
    auto calcHue = [](float temp1, float temp2, float hueVal) {
        if (hueVal < 0.0f)
            hueVal += 6.0f;
        else if (hueVal >= 6.0f)
            hueVal -= 6.0f;
        if (hueVal < 1.0f)
            return temp1 + (temp2 - temp1) * hueVal;
        if (hueVal < 3.0f)
            return temp2;
        if (hueVal < 4.0f)
            return temp1 + (temp2 - temp1) * (4.0f - hueVal);
        return temp1;
    };

    return {
        calcHue(temp1, temp2, hue + 2.0f),
        calcHue(temp1, temp2, hue),
        calcHue(temp1, temp2, hue - 2.0f),
        alpha
    };
}

static bool transformColor(const FilterOperations::AppleInvertLightness&, SRGBA<float>& color)
{
    auto hueRotatedSRGBAComponents = hueRotate(asColorComponents(color.resolved()), 0.5f);

    // Apply the matrix. See rdar://problem/41146650 for how this matrix was derived.
    constexpr ColorMatrix<5, 3> toDarkModeMatrix {
       -0.770f,  0.059f, -0.089f, 0.0f, 1.0f,
        0.030f, -0.741f, -0.089f, 0.0f, 1.0f,
        0.030f,  0.059f, -0.890f, 0.0f, 1.0f
    };
    color = makeFromComponentsClamping<SRGBA<float>>(toDarkModeMatrix.transformedColorComponents(hueRotatedSRGBAComponents));
    return true;
}

static bool inverseTransformColor(const FilterOperations::AppleInvertLightness&, SRGBA<float>& color)
{
    // Apply the matrix.
    constexpr ColorMatrix<5, 3> toLightModeMatrix {
        -1.300f, -0.097f,  0.147f, 0.0f, 1.25f,
        -0.049f, -1.347f,  0.146f, 0.0f, 1.25f,
        -0.049f, -0.097f, -1.104f, 0.0f, 1.25f
    };
    auto convertedToLightModeComponents = toLightModeMatrix.transformedColorComponents(asColorComponents(color.resolved()));

    auto hueRotatedSRGBAComponents = hueRotate(convertedToLightModeComponents, 0.5f);

    color = makeFromComponentsClamping<SRGBA<float>>(hueRotatedSRGBAComponents);
    return true;
}

static bool transformColor(const FilterOperations::Opacity& op, SRGBA<float>& color)
{
    color = colorWithOverriddenAlpha(color, std::clamp<float>(color.resolved().alpha * op.amount.value, 0.0f, 1.0f));
    return true;
}

static bool transformColor(const FilterOperations::Invert& op, SRGBA<float>& color)
{
    float oneMinusAmount = 1.0f - op.amount.value;
    color = colorByModifyingEachNonAlphaComponent(color, [&](float component) {
        return 1.0f - (oneMinusAmount + component * (op.amount.value - oneMinusAmount));
    });
    return true;
}

static bool transformColor(const FilterOperations::Contrast& op, SRGBA<float>& color)
{
    float intercept = -(0.5f * op.amount.value) + 0.5f;
    color = colorByModifyingEachNonAlphaComponent(color, [&](float component) {
        return std::clamp<float>(intercept + op.amount.value * component, 0.0f, 1.0f);
    });
    return true;
}

static bool transformColor(const FilterOperations::Brightness& op, SRGBA<float>& color)
{
    color = colorByModifyingEachNonAlphaComponent(color, [&](float component) {
        return std::clamp<float>(op.amount.value * component, 0.0f, 1.0f);
    });
    return true;
}

template<typename T> static bool transformColorComponents(const T& matrix, SRGBA<float>& color)
{
    color = makeFromComponentsClamping<SRGBA<float>>(matrix.transformedColorComponents(asColorComponents(color.resolved())));
    return true;
}

static bool transformColor(const FilterOperations::FilterOperation& operation, SRGBA<float>& color)
{
    return WTF::switchOn(operation,
        [&](const FilterOperations::Grayscale& op) -> bool {
            return transformColorComponents(grayscaleColorMatrix(op.amount.value), color);
        },
        [&](const FilterOperations::Sepia& op) -> bool {
            return transformColorComponents(sepiaColorMatrix(op.amount.value), color);
        },
        [&](const FilterOperations::Saturate& op) -> bool {
            return transformColorComponents(saturationColorMatrix(op.amount.value), color);
        },
        [&](const FilterOperations::HueRotate& op) -> bool {
            return transformColorComponents(hueRotateColorMatrix(op.amount.value), color);
        },
        [&](const FilterOperations::Invert& op) -> bool {
            return transformColor(op, color);
        },
        [&](const FilterOperations::Opacity& op) -> bool {
            return transformColor(op, color);
        },
        [&](const FilterOperations::Brightness& op) -> bool {
            return transformColor(op, color);
        },
        [&](const FilterOperations::Contrast& op) -> bool {
            return transformColor(op, color);
        },
        [&](const FilterOperations::AppleInvertLightness& op) -> bool {
            return transformColor(op, color);
        },
        [](const auto&) -> bool {
            return false;
        }
    );
}

bool FilterOperations::transformColor(Color& color) const
{
    if (isEmpty() || !color.isValid())
        return false;
    // Color filter does not apply to semantic CSS colors (like "Windowframe").
    if (color.isSemantic())
        return false;

    auto sRGBAColor = color.toColorTypeLossy<SRGBA<float>>();

    for (auto& operation : m_operations) {
        if (!WebCore::Style::transformColor(operation, sRGBAColor))
            return false;
    }

    color = convertColor<SRGBA<uint8_t>>(sRGBAColor);
    return true;
}

static bool inverseTransformColor(const FilterOperations::FilterOperation& operation, SRGBA<float>& color)
{
    return WTF::switchOn(operation,
        [&](const FilterOperations::AppleInvertLightness& op) -> bool {
            return inverseTransformColor(op, color);
        },
        [](const auto&) -> bool {
            return false;
        }
    );
}

bool FilterOperations::inverseTransformColor(Color& color) const
{
    if (isEmpty() || !color.isValid())
        return false;
    // Color filter does not apply to semantic CSS colors (like "Windowframe").
    if (color.isSemantic())
        return false;

    auto sRGBAColor = color.toColorTypeLossy<SRGBA<float>>();

    for (auto& operation : m_operations) {
        if (!WebCore::Style::inverseTransformColor(operation, sRGBAColor))
            return false;
    }

    color = convertColor<SRGBA<uint8_t>>(sRGBAColor);
    return true;
}

static void load(FilterOperations::FilterOperation& operation, CachedResourceLoader& cachedResourceLoader, const ResourceLoaderOptions& options)
{
    WTF::switchOn(operation,
        [&](FilterOperations::Reference& op) {
            if (op.cachedSVGDocumentReference)
                return;
            if (!SVGURIReference::isExternalURIReference(op.url, *cachedResourceLoader.protectedDocument()))
                return;
            op.cachedSVGDocumentReference = makeUnique<CachedSVGDocumentReference>(op.url);
            op.cachedSVGDocumentReference->load(cachedResourceLoader, options);
        },
        [](const auto&) { }
    );
}

void FilterOperations::load(CachedResourceLoader& cachedResourceLoader, const ResourceLoaderOptions& options)
{
    for (auto& operation : m_operations)
        WebCore::Style::load(operation, cachedResourceLoader, options);
}

// MARK: - TextStream Support

TextStream& operator<<(TextStream& ts, const FilterOperations::FilterOperation& operation)
{
    WTF::switchOn(operation,
        [&](const FilterOperations::Reference&) {
            ts << "reference";
        },
        [&](const FilterOperations::Grayscale& op) {
            ts << "grayscale(" << op.amount.value << ")";
        },
        [&](const FilterOperations::Sepia& op) {
            ts << "sepia(" << op.amount.value << ")";
        },
        [&](const FilterOperations::Saturate& op) {
            ts << "saturate(" << op.amount.value << ")";
        },
        [&](const FilterOperations::HueRotate& op) {
            ts << "hue-rotate(" << op.amount.value << ")";
        },
        [&](const FilterOperations::Invert& op) {
            ts << "invert(" << op.amount.value << ")";
        },
        [&](const FilterOperations::Opacity& op) {
            ts << "opacity(" << op.amount.value << ")";
        },
        [&](const FilterOperations::Brightness& op) {
            ts << "brightness(" << op.amount.value << ")";
        },
        [&](const FilterOperations::Contrast& op) {
            ts << "contrast(" << op.amount.value << ")";
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

} // namespace Style
} // namespace WebCore
