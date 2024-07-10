/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PlatformCAFilters.h"

#import "FloatConversion.h"
#import "LengthFunctions.h"
#import "PlatformCALayerCocoa.h"
#import <QuartzCore/QuartzCore.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/MakeString.h>

namespace WebCore {

static CAColorMatrix caColorMatrixFromColorMatrix(const ColorMatrix<3, 3>& colorMatrix)
{
    return {
        colorMatrix.at(0, 0), colorMatrix.at(0, 1), colorMatrix.at(0, 2), 0, 0,
        colorMatrix.at(1, 0), colorMatrix.at(1, 1), colorMatrix.at(1, 2), 0, 0,
        colorMatrix.at(2, 0), colorMatrix.at(2, 1), colorMatrix.at(2, 2), 0, 0,
        0, 0, 0, 1, 0
    };
}

static CAColorMatrix caColorMatrixFromColorMatrix(const ColorMatrix<5, 4>& colorMatrix)
{
    return {
        colorMatrix.at(0, 0), colorMatrix.at(0, 1), colorMatrix.at(0, 2), colorMatrix.at(0, 3), colorMatrix.at(0, 4),
        colorMatrix.at(1, 0), colorMatrix.at(1, 1), colorMatrix.at(1, 2), colorMatrix.at(1, 3), colorMatrix.at(1, 4),
        colorMatrix.at(2, 0), colorMatrix.at(2, 1), colorMatrix.at(2, 2), colorMatrix.at(2, 3), colorMatrix.at(2, 4),
        colorMatrix.at(3, 0), colorMatrix.at(3, 1), colorMatrix.at(3, 2), colorMatrix.at(3, 3), colorMatrix.at(3, 4),
    };
}

static RetainPtr<NSValue> colorMatrixValueForFilter(const FilterOperations::Sepia& op)
{
    return [NSValue valueWithCAColorMatrix:caColorMatrixFromColorMatrix(sepiaColorMatrix(op.amount))];
}

static RetainPtr<NSValue> colorMatrixValueForFilter(const FilterOperations::Invert& op)
{
    return [NSValue valueWithCAColorMatrix:caColorMatrixFromColorMatrix(invertColorMatrix(op.amount))];
}

static RetainPtr<NSValue> colorMatrixValueForFilter(const FilterOperations::Opacity& op)
{
    return [NSValue valueWithCAColorMatrix:caColorMatrixFromColorMatrix(opacityColorMatrix(op.amount))];
}

static RetainPtr<NSValue> colorMatrixValueForFilter(const FilterOperations::Contrast& op)
{
    return [NSValue valueWithCAColorMatrix:caColorMatrixFromColorMatrix(contrastColorMatrix(op.amount))];
}

static RetainPtr<NSValue> colorMatrixValueForFilter(const FilterOperations::Brightness& op)
{
    return [NSValue valueWithCAColorMatrix:caColorMatrixFromColorMatrix(brightnessColorMatrix(op.amount))];
}

#if PLATFORM(MAC)
static unsigned keyValueCountForFilter(const FilterOperations::FilterOperation& filterOperation)
{
    return WTF::switchOn(filterOperation,
        [](const FilterOperations::Reference&) {
            ASSERT_NOT_REACHED();
            return 0;
        },
        [](const FilterOperations::AppleInvertLightness&) {
            ASSERT_NOT_REACHED(); // AppleInvertLightness is only used in -apple-color-filter.
            return 0;
        },
        [](const FilterOperations::DropShadow&) {
            return 3;
        },
        [](const auto&) {
            return 1;
        }
    );
}

size_t PlatformCAFilters::presentationModifierCount(const FilterOperations& filters)
{
    size_t count = 0;
    for (const auto& filter : filters)
        count += keyValueCountForFilter(filter);
    return count;
}

void PlatformCAFilters::presentationModifiers(const FilterOperations& initialFilters, const FilterOperations* canonicalFilters, Vector<TypedFilterPresentationModifier>& presentationModifiers, RetainPtr<CAPresentationModifierGroup>& group)
{
    if (!canonicalFilters || canonicalFilters->isEmpty())
        return;

    auto numberOfCanonicalFilters = canonicalFilters->size();
    auto numberOfInitialFilters = initialFilters.size();

    ASSERT(numberOfCanonicalFilters >= numberOfInitialFilters);
    ASSERT(presentationModifierCount(*canonicalFilters));

    for (size_t i = 0; i < numberOfCanonicalFilters; ++i) {
        const auto& canonicalFilterOperation = (*canonicalFilters)[i];
        const auto& initialFilterOperation = i < numberOfInitialFilters ? initialFilters[i] : FilterOperations::initialValueForInterpolationMatchingType(canonicalFilterOperation);
        ASSERT(canonicalFilterOperation.index() == initialFilterOperation.index());

        bool added = WTF::switchOn(initialFilterOperation,
            [](const FilterOperations::Reference&) {
                ASSERT_NOT_REACHED();
                return false;
            },
            [](const FilterOperations::AppleInvertLightness&) {
                ASSERT_NOT_REACHED(); // AppleInvertLightness is only used in -apple-color-filter.
                return false;
            },
            [&](const FilterOperations::DropShadow& op) {
                auto location = floatPointForLengthPoint(op.location, { 0, 0 });
                auto stdDeviation = floatValueForLength(op.stdDeviation, 0);
                auto size = CGSizeMake(location.x(), location.y());
                presentationModifiers.append({ op.type, adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"shadowOffset" initialValue:[NSValue value:&size withObjCType:@encode(CGSize)] additive:NO group:group.get()]) });
                presentationModifiers.append({ op.type, adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"shadowColor" initialValue:(id) cachedCGColor(op.color).autorelease() additive:NO group:group.get()]) });
                presentationModifiers.append({ op.type, adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"shadowRadius" initialValue:@(stdDeviation) additive:NO group:group.get()]) });
                return true;
            },
            [&](const auto& op) {
                auto keyValueName = makeString("filters.filter_"_s, i, '.', animatedFilterPropertyName(op.type));
                presentationModifiers.append({ op.type, adoptNS([[CAPresentationModifier alloc] initWithKeyPath:keyValueName initialValue:filterValueForOperation(op).get() additive:NO group:group.get()]) });
                return true;
            }
        );
        if (!added)
            return;
    }

    ASSERT(presentationModifierCount(*canonicalFilters) == presentationModifiers.size());
}

void PlatformCAFilters::updatePresentationModifiers(const FilterOperations& filters, const Vector<TypedFilterPresentationModifier>& presentationModifiers)
{
    ASSERT(presentationModifierCount(filters) <= presentationModifiers.size());

    size_t filterIndex = 0;
    auto numberOfFilters = filters.size();
    for (size_t i = 0; i < presentationModifiers.size(); ++i) {
        auto filterOperation = filterIndex < numberOfFilters ? filters[filterIndex] : FilterOperations::initialValueForInterpolationMatchingType(presentationModifiers[i].first);
        ++filterIndex;

        bool added = WTF::switchOn(filterOperation,
            [](const FilterOperations::Reference&) {
                ASSERT_NOT_REACHED();
                return false;
            },
            [](const FilterOperations::AppleInvertLightness&) {
                ASSERT_NOT_REACHED(); // AppleInvertLightness is only used in -apple-color-filter.
                return false;
            },
            [&](const FilterOperations::DropShadow& op) {
                auto location = floatPointForLengthPoint(op.location, { 0, 0 });
                auto stdDeviation = floatValueForLength(op.stdDeviation, 0);
                auto size = CGSizeMake(location.x(), location.y());
                [presentationModifiers[i].second.get() setValue:[NSValue value:&size withObjCType:@encode(CGSize)]];
                [presentationModifiers[i + 1].second.get() setValue:(id)cachedCGColor(op.color).autorelease()];
                [presentationModifiers[i + 2].second.get() setValue:@(stdDeviation)];
                return true;
            },
            [&](const auto& op) {
                [presentationModifiers[i].second.get() setValue:filterValueForOperation(op).get()];
                return true;
            }
        );
        if (!added)
            return;
    }
}
#endif // PLATFORM(MAC)

void PlatformCAFilters::setFiltersOnLayer(PlatformLayer* layer, const FilterOperations& filters, bool backdropIsOpaque)
{
    if (!filters.size()) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [layer setFilters:nil];
        // FIXME: this adds shadow properties to the layer even when it had none.
        [layer setShadowOffset:CGSizeZero];
        [layer setShadowColor:nil];
        [layer setShadowRadius:0];
        [layer setShadowOpacity:0];
        END_BLOCK_OBJC_EXCEPTIONS
        return;
    }

    // Assume filtersCanBeComposited was called and it returned true.
    ASSERT(PlatformCALayerCocoa::filtersCanBeComposited(filters));

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    unsigned i = 0;
    auto array = createNSArray(filters, [&](auto& operation) -> id {
        auto filterName = makeString("filter_"_s, i++);

        return WTF::switchOn(operation,
            [](const FilterOperations::Reference&) -> id {
                ASSERT_NOT_REACHED();
                return nil;
            },
            [](const FilterOperations::AppleInvertLightness&) -> id {
                ASSERT_NOT_REACHED(); // AppleInvertLightness is only used in -apple-color-filter.
                return nil;
            },
            [&](const FilterOperations::DropShadow& op) -> id {
                // FIXME: For now assume drop shadow is the last filter, put it on the layer.
                // <rdar://problem/10959969> Handle case where drop-shadow is not the last filter.

                auto location = floatPointForLengthPoint(op.location, { 0, 0 });
                auto stdDeviation = floatValueForLength(op.stdDeviation, 0);
                auto size = CGSizeMake(location.x(), location.y());
                [layer setShadowOffset:size];
                [layer setShadowColor:cachedCGColor(op.color).get()];
                [layer setShadowRadius:stdDeviation];
                [layer setShadowOpacity:1];
                return nil;
            },
            [&](const FilterOperations::Grayscale& op) -> id {
                CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMonochrome];
                [filter setValue:[NSNumber numberWithFloat:op.amount] forKey:@"inputAmount"];
                [filter setName:filterName];
                return filter;
            },
            [&](const FilterOperations::Sepia& op) -> id {
                CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMatrix];
                [filter setValue:colorMatrixValueForFilter(op).autorelease() forKey:@"inputColorMatrix"];
                [filter setName:filterName];
                return filter;
            },
            [&](const FilterOperations::Saturate& op) -> id {
                CAFilter *filter = [CAFilter filterWithType:kCAFilterColorSaturate];
                [filter setValue:[NSNumber numberWithFloat:op.amount] forKey:@"inputAmount"];
                [filter setName:filterName];
                return filter;
            },
            [&](const FilterOperations::HueRotate& op) -> id {
                CAFilter *filter = [CAFilter filterWithType:kCAFilterColorHueRotate];
                [filter setValue:[NSNumber numberWithFloat:deg2rad(op.amount)] forKey:@"inputAngle"];
                [filter setName:filterName];
                return filter;
            },
            [&](const FilterOperations::Invert& op) -> id {
                CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMatrix];
                [filter setValue:colorMatrixValueForFilter(op).autorelease() forKey:@"inputColorMatrix"];
                [filter setName:filterName];
                return filter;
            },
            [&](const FilterOperations::Opacity& op) -> id {
                CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMatrix];
                [filter setValue:colorMatrixValueForFilter(op).autorelease() forKey:@"inputColorMatrix"];
                [filter setName:filterName];
                return filter;
            },
            [&](const FilterOperations::Brightness& op) -> id {
                CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMatrix];
                [filter setValue:colorMatrixValueForFilter(op).autorelease() forKey:@"inputColorMatrix"];
                [filter setName:filterName];
                return filter;
            },
            [&](const FilterOperations::Contrast& op) -> id {
                CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMatrix];
                [filter setValue:colorMatrixValueForFilter(op).autorelease() forKey:@"inputColorMatrix"];
                [filter setName:filterName];
                return filter;
            },
            [&](const FilterOperations::Blur& op) -> id {
                CAFilter *filter = [CAFilter filterWithType:kCAFilterGaussianBlur];
                [filter setValue:[NSNumber numberWithFloat:floatValueForLength(op.stdDeviation, 0)] forKey:@"inputRadius"];
                if ([layer isKindOfClass:[CABackdropLayer class]]) {
#if PLATFORM(VISION)
                    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=275965
                    UNUSED_PARAM(backdropIsOpaque);
                    [filter setValue:@YES forKey:@"inputNormalizeEdgesTransparent"];
#else
                    // If the backdrop is displayed inside a transparent web view over
                    // a material background, we need `normalizeEdgesTransparent`
                    // in order to render correctly.
                    if (backdropIsOpaque)
                        [filter setValue:@YES forKey:@"inputNormalizeEdges"];
                    else
                        [filter setValue:@YES forKey:@"inputHardEdges"];
#endif
                }
                [filter setName:filterName];
                return filter;
            }
        );
    });

    if ([array count])
        [layer setFilters:array.get()];

    END_BLOCK_OBJC_EXCEPTIONS
}

RetainPtr<NSValue> PlatformCAFilters::filterValueForOperation(const FilterOperations::FilterOperation& filterOperation)
{
    return WTF::switchOn(filterOperation,
        [&](const FilterOperations::Reference&) -> RetainPtr<NSValue> {
            return nil;
        },
        [&](const FilterOperations::Grayscale& op) -> RetainPtr<NSValue> {
            // CAFilter: inputAmount
            return @(op.amount);
        },
        [&](const FilterOperations::Sepia& op) -> RetainPtr<NSValue> {
            // CAFilter: inputColorMatrix
            return colorMatrixValueForFilter(op);
        },
        [&](const FilterOperations::Saturate& op) -> RetainPtr<NSValue> {
            // CAFilter: inputAmount
            return @(op.amount);
        },
        [&](const FilterOperations::HueRotate& op) -> RetainPtr<NSValue> {
            // Hue rotate CAFilter: inputAngle
            return @(deg2rad(op.amount));
        },
        [&](const FilterOperations::Invert& op) -> RetainPtr<NSValue> {
            // CAFilter: inputColorMatrix
            return colorMatrixValueForFilter(op);
        },
        [&](const FilterOperations::Opacity& op) -> RetainPtr<NSValue> {
            // Opacity CAFilter: inputColorMatrix
            return colorMatrixValueForFilter(op);
        },
        [&](const FilterOperations::Brightness& op) -> RetainPtr<NSValue> {
            // Brightness CAFilter: inputColorMatrix
            return colorMatrixValueForFilter(op);
        },
        [&](const FilterOperations::Contrast& op) -> RetainPtr<NSValue> {
            // Contrast CAFilter: inputColorMatrix
            return colorMatrixValueForFilter(op);
        },
        [&](const FilterOperations::Blur& op) -> RetainPtr<NSValue> {
            // CAFilter: inputRadius
            return @(floatValueForLength(op.stdDeviation, 0));
        },
        [&](const FilterOperations::DropShadow&) -> RetainPtr<NSValue> {
            return nil;
        },
        [&](const FilterOperations::AppleInvertLightness&) -> RetainPtr<NSValue> {
            ASSERT_NOT_REACHED(); // AppleInvertLightness is only used in -apple-color-filter.
            return nil;
        }
    );
}

void PlatformCAFilters::setBlendingFiltersOnLayer(PlatformLayer* layer, const BlendMode blendMode)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    CAFilter* filter = nil;

    switch (blendMode) {
    case BlendMode::Normal:
        // No need to set an actual filter object in this case.
        break;
    case BlendMode::Overlay:
        filter = [CAFilter filterWithType:kCAFilterOverlayBlendMode];
        break;
    case BlendMode::ColorDodge:
        filter = [CAFilter filterWithType:kCAFilterColorDodgeBlendMode];
        break;
    case BlendMode::ColorBurn:
        filter = [CAFilter filterWithType:kCAFilterColorBurnBlendMode];
        break;
    case BlendMode::Darken:
        filter = [CAFilter filterWithType:kCAFilterDarkenBlendMode];
        break;
    case BlendMode::Difference:
        filter = [CAFilter filterWithType:kCAFilterDifferenceBlendMode];
        break;
    case BlendMode::Exclusion:
        filter = [CAFilter filterWithType:kCAFilterExclusionBlendMode];
        break;
    case BlendMode::HardLight:
        filter = [CAFilter filterWithType:kCAFilterHardLightBlendMode];
        break;
    case BlendMode::Multiply:
        filter = [CAFilter filterWithType:kCAFilterMultiplyBlendMode];
        break;
    case BlendMode::Lighten:
        filter = [CAFilter filterWithType:kCAFilterLightenBlendMode];
        break;
    case BlendMode::SoftLight:
        filter = [CAFilter filterWithType:kCAFilterSoftLightBlendMode];
        break;
    case BlendMode::Screen:
        filter = [CAFilter filterWithType:kCAFilterScreenBlendMode];
        break;
    case BlendMode::PlusDarker:
        filter = [CAFilter filterWithType:kCAFilterPlusD];
        break;
    case BlendMode::PlusLighter:
        filter = [CAFilter filterWithType:kCAFilterPlusL];
        break;
    case BlendMode::Hue:
    case BlendMode::Saturation:
    case BlendMode::Color:
    case BlendMode::Luminosity:
        // FIXME: CA does't support non-separable blend modes on compositing filters.
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    [layer setCompositingFilter:filter];

    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCAFilters::isAnimatedFilterProperty(FilterOperations::Type type)
{
    switch (type) {
    case FilterOperations::Type::Grayscale:
    case FilterOperations::Type::Sepia:
    case FilterOperations::Type::Saturate:
    case FilterOperations::Type::HueRotate:
    case FilterOperations::Type::Invert:
    case FilterOperations::Type::Opacity:
    case FilterOperations::Type::Brightness:
    case FilterOperations::Type::Contrast:
    case FilterOperations::Type::Blur:
        return true;
    default:
        return false;
    }
}

static constexpr auto inputAmountProperty = "inputAmount"_s;
static constexpr auto inputColorMatrixProperty = "inputColorMatrix"_s;
static constexpr auto inputAngleProperty = "inputAngle"_s;
static constexpr auto inputRadiusProperty = "inputRadius"_s;

ASCIILiteral PlatformCAFilters::animatedFilterPropertyName(FilterOperations::Type type)
{
    switch (type) {
    case FilterOperations::Type::Grayscale:
    case FilterOperations::Type::Saturate:
        return inputAmountProperty;
    case FilterOperations::Type::Sepia:
    case FilterOperations::Type::Invert:
    case FilterOperations::Type::Opacity:
    case FilterOperations::Type::Brightness:
    case FilterOperations::Type::Contrast:
        return inputColorMatrixProperty;
    case FilterOperations::Type::HueRotate:
        return inputAngleProperty;
    case FilterOperations::Type::Blur:
        return inputRadiusProperty;
    default:
        return ""_s;
    }
}

bool PlatformCAFilters::isValidAnimatedFilterPropertyName(const String& animatedFilterPropertyName)
{
    return animatedFilterPropertyName == inputAmountProperty
        || animatedFilterPropertyName == inputColorMatrixProperty
        || animatedFilterPropertyName == inputAngleProperty
        || animatedFilterPropertyName == inputRadiusProperty;
}

} // namespace WebCore
