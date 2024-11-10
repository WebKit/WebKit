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
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/MakeString.h>

namespace WebCore {

#if PLATFORM(MAC)
static unsigned keyValueCountForFilter(const FilterOperation& filterOperation)
{
    switch (filterOperation.type()) {
    case FilterOperation::Type::Default:
    case FilterOperation::Type::Reference:
    case FilterOperation::Type::None:
        ASSERT_NOT_REACHED();
        return 0;
    case FilterOperation::Type::DropShadow:
        return 3;
    case FilterOperation::Type::Sepia:
    case FilterOperation::Type::Saturate:
    case FilterOperation::Type::HueRotate:
    case FilterOperation::Type::Invert:
    case FilterOperation::Type::Opacity:
    case FilterOperation::Type::Brightness:
    case FilterOperation::Type::Contrast:
    case FilterOperation::Type::Grayscale:
    case FilterOperation::Type::Blur:
        return 1;
    case FilterOperation::Type::AppleInvertLightness:
        ASSERT_NOT_REACHED(); // AppleInvertLightness is only used in -apple-color-filter.
        break;
    case FilterOperation::Type::Passthrough:
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

size_t PlatformCAFilters::presentationModifierCount(const FilterOperations& filters)
{
    size_t count = 0;
    for (const auto& filter : filters)
        count += keyValueCountForFilter(filter);
    return count;
}

static const FilterOperation& passthroughFilter(const FilterOperation::Type typeToMatch)
{
    switch (typeToMatch) {
    case FilterOperation::Type::DropShadow:
        static NeverDestroyed<Ref<DropShadowFilterOperation>> passthroughDropShadowFilter = DropShadowFilterOperation::create({ }, 0, { });
        return passthroughDropShadowFilter.get();
    case FilterOperation::Type::Grayscale:
        static NeverDestroyed<Ref<BasicColorMatrixFilterOperation>> passthroughGrayscaleFilter = BasicColorMatrixFilterOperation::create(0, typeToMatch);
        return passthroughGrayscaleFilter.get();
    case FilterOperation::Type::Sepia:
        static NeverDestroyed<Ref<BasicColorMatrixFilterOperation>> passthroughSepiaFilter = BasicColorMatrixFilterOperation::create(0, typeToMatch);
        return passthroughSepiaFilter.get();
    case FilterOperation::Type::Saturate:
        static NeverDestroyed<Ref<BasicColorMatrixFilterOperation>> passthroughSaturateFilter = BasicColorMatrixFilterOperation::create(0, typeToMatch);
        return passthroughSaturateFilter.get();
    case FilterOperation::Type::HueRotate:
        static NeverDestroyed<Ref<BasicColorMatrixFilterOperation>> passthroughHueRotateFilter = BasicColorMatrixFilterOperation::create(0, typeToMatch);
        return passthroughHueRotateFilter.get();
    case FilterOperation::Type::Invert:
        static NeverDestroyed<Ref<BasicColorMatrixFilterOperation>> passthroughInvertFilter = BasicColorMatrixFilterOperation::create(0, typeToMatch);
        return passthroughInvertFilter.get();
    case FilterOperation::Type::Opacity:
        static NeverDestroyed<Ref<BasicColorMatrixFilterOperation>> passthroughOpacityFilter = BasicColorMatrixFilterOperation::create(0, typeToMatch);
        return passthroughOpacityFilter.get();
    case FilterOperation::Type::Brightness:
        static NeverDestroyed<Ref<BasicColorMatrixFilterOperation>> passthroughBrightnessFilter = BasicColorMatrixFilterOperation::create(0, typeToMatch);
        return passthroughBrightnessFilter.get();
    case FilterOperation::Type::Contrast:
        static NeverDestroyed<Ref<BasicColorMatrixFilterOperation>> passthroughContrastFilter = BasicColorMatrixFilterOperation::create(0, typeToMatch);
        return passthroughContrastFilter.get();
    case FilterOperation::Type::Blur:
        static NeverDestroyed<Ref<BlurFilterOperation>> passthroughBlurFilter = BlurFilterOperation::create({ 0, LengthType::Fixed });
        return passthroughBlurFilter.get();
    default:
        ASSERT_NOT_REACHED();
        static NeverDestroyed<Ref<BasicColorMatrixFilterOperation>> passthroughDefaultFilter = BasicColorMatrixFilterOperation::create(0, FilterOperation::Type::Grayscale);
        return passthroughDefaultFilter.get();
    }
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
        auto& canonicalFilterOperation = (*canonicalFilters)[i].get();
        auto& initialFilterOperation = i < numberOfInitialFilters ? initialFilters[i].get() : passthroughFilter(canonicalFilterOperation.type());
        ASSERT(canonicalFilterOperation.type() == initialFilterOperation.type());

        auto filterName = makeString("filter_"_s, i);

        auto type = initialFilterOperation.type();
        switch (type) {
        case FilterOperation::Type::Default:
        case FilterOperation::Type::Reference:
        case FilterOperation::Type::None:
            ASSERT_NOT_REACHED();
            break;
        case FilterOperation::Type::DropShadow: {
            const auto& dropShadowOperation = downcast<DropShadowFilterOperation>(initialFilterOperation);
            auto size = CGSizeMake(dropShadowOperation.x(), dropShadowOperation.y());
            presentationModifiers.append({ type, adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"shadowOffset" initialValue:[NSValue value:&size withObjCType:@encode(CGSize)] additive:NO group:group.get()]) });
            presentationModifiers.append({ type, adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"shadowColor" initialValue:(id) cachedCGColor(dropShadowOperation.color()).autorelease() additive:NO group:group.get()]) });
            presentationModifiers.append({ type, adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"shadowRadius" initialValue:@(dropShadowOperation.stdDeviation()) additive:NO group:group.get()]) });
            continue;
        }
        case FilterOperation::Type::Grayscale:
        case FilterOperation::Type::Sepia:
        case FilterOperation::Type::Saturate:
        case FilterOperation::Type::HueRotate:
        case FilterOperation::Type::Invert:
        case FilterOperation::Type::Opacity:
        case FilterOperation::Type::Brightness:
        case FilterOperation::Type::Contrast:
        case FilterOperation::Type::Blur: {
            auto keyValueName = makeString("filters."_s, filterName, '.', animatedFilterPropertyName(initialFilterOperation.type()));
            presentationModifiers.append({ type, adoptNS([[CAPresentationModifier alloc] initWithKeyPath:keyValueName initialValue:filterValueForOperation(initialFilterOperation).get() additive:NO group:group.get()]) });
            continue;
        }
        case FilterOperation::Type::AppleInvertLightness:
            ASSERT_NOT_REACHED(); // AppleInvertLightness is only used in -apple-color-filter.
            break;
        case FilterOperation::Type::Passthrough:
            continue;
        }
        ASSERT_NOT_REACHED();
        break;
    }

    ASSERT(presentationModifierCount(*canonicalFilters) == presentationModifiers.size());
}

void PlatformCAFilters::updatePresentationModifiers(const FilterOperations& filters, const Vector<TypedFilterPresentationModifier>& presentationModifiers)
{
    ASSERT(presentationModifierCount(filters) <= presentationModifiers.size());

    size_t filterIndex = 0;
    auto numberOfFilters = filters.size();
    for (size_t i = 0; i < presentationModifiers.size(); ++i) {
        auto& filterOperation = filterIndex < numberOfFilters ? *filters.at(filterIndex) : passthroughFilter(presentationModifiers[i].first);
        ++filterIndex;
        switch (filterOperation.type()) {
        case FilterOperation::Type::Default:
        case FilterOperation::Type::Reference:
        case FilterOperation::Type::None:
            ASSERT_NOT_REACHED();
            return;
        case FilterOperation::Type::DropShadow: {
            const auto& dropShadowOperation = downcast<DropShadowFilterOperation>(filterOperation);
            auto size = CGSizeMake(dropShadowOperation.x(), dropShadowOperation.y());
            [presentationModifiers[i].second.get() setValue:[NSValue value:&size withObjCType:@encode(CGSize)]];
            [presentationModifiers[i + 1].second.get() setValue:(id) cachedCGColor(dropShadowOperation.color()).autorelease()];
            [presentationModifiers[i + 2].second.get() setValue:@(dropShadowOperation.stdDeviation())];
            i += 2;
            continue;
        }
        case FilterOperation::Type::Grayscale:
        case FilterOperation::Type::Sepia:
        case FilterOperation::Type::Saturate:
        case FilterOperation::Type::HueRotate:
        case FilterOperation::Type::Invert:
        case FilterOperation::Type::Opacity:
        case FilterOperation::Type::Brightness:
        case FilterOperation::Type::Contrast:
        case FilterOperation::Type::Blur: {
            [presentationModifiers[i].second.get() setValue:filterValueForOperation(filterOperation).get()];
            continue;
        }
        case FilterOperation::Type::AppleInvertLightness:
            ASSERT_NOT_REACHED(); // AppleInvertLightness is only used in -apple-color-filter.
            return;
        case FilterOperation::Type::Passthrough:
            continue;
        }
        ASSERT_NOT_REACHED();
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

        auto& filterOperation = operation.get();
        switch (filterOperation.type()) {
        case FilterOperation::Type::Default:
        case FilterOperation::Type::Reference:
        case FilterOperation::Type::None:
            ASSERT_NOT_REACHED();
            return nil;
        case FilterOperation::Type::DropShadow: {
            // FIXME: For now assume drop shadow is the last filter, put it on the layer.
            // <rdar://problem/10959969> Handle case where drop-shadow is not the last filter.
            const auto& dropShadowOperation = downcast<DropShadowFilterOperation>(filterOperation);
            [layer setShadowOffset:CGSizeMake(dropShadowOperation.x(), dropShadowOperation.y())];
            [layer setShadowColor:cachedCGColor(dropShadowOperation.color()).get()];
            [layer setShadowRadius:dropShadowOperation.stdDeviation()];
            [layer setShadowOpacity:1];
            return nil;
        }
        case FilterOperation::Type::Grayscale: {
            const auto& colorMatrixOperation = downcast<BasicColorMatrixFilterOperation>(filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMonochrome];
            [filter setValue:[NSNumber numberWithFloat:colorMatrixOperation.amount()] forKey:@"inputAmount"];
            [filter setName:filterName];
            return filter;
        }
        case FilterOperation::Type::Sepia: {
            RetainPtr<NSValue> colorMatrixValue = PlatformCAFilters::colorMatrixValueForFilter(filterOperation.type(), &filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMatrix];
            [filter setValue:colorMatrixValue.get() forKey:@"inputColorMatrix"];
            [filter setName:filterName];
            return filter;
        }
        case FilterOperation::Type::Saturate: {
            const auto& colorMatrixOperation = downcast<BasicColorMatrixFilterOperation>(filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorSaturate];
            [filter setValue:[NSNumber numberWithFloat:colorMatrixOperation.amount()] forKey:@"inputAmount"];
            [filter setName:filterName];
            return filter;
        }
        case FilterOperation::Type::HueRotate: {
            const auto& colorMatrixOperation = downcast<BasicColorMatrixFilterOperation>(filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorHueRotate];
            [filter setValue:[NSNumber numberWithFloat:deg2rad(colorMatrixOperation.amount())] forKey:@"inputAngle"];
            [filter setName:filterName];
            return filter;
        }
        case FilterOperation::Type::Invert: {
            RetainPtr<NSValue> colorMatrixValue = PlatformCAFilters::colorMatrixValueForFilter(filterOperation.type(), &filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMatrix];
            [filter setValue:colorMatrixValue.get() forKey:@"inputColorMatrix"];
            [filter setName:filterName];
            return filter;
        }
        case FilterOperation::Type::AppleInvertLightness:
            ASSERT_NOT_REACHED(); // AppleInvertLightness is only used in -apple-color-filter.
            break;
        case FilterOperation::Type::Opacity: {
            RetainPtr<NSValue> colorMatrixValue = PlatformCAFilters::colorMatrixValueForFilter(filterOperation.type(), &filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMatrix];
            [filter setValue:colorMatrixValue.get() forKey:@"inputColorMatrix"];
            [filter setName:filterName];
            return filter;
        }
        case FilterOperation::Type::Brightness: {
            RetainPtr<NSValue> colorMatrixValue = PlatformCAFilters::colorMatrixValueForFilter(filterOperation.type(), &filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMatrix];
            [filter setValue:colorMatrixValue.get() forKey:@"inputColorMatrix"];
            [filter setName:filterName];
            return filter;
        }
        case FilterOperation::Type::Contrast: {
            RetainPtr<NSValue> colorMatrixValue = PlatformCAFilters::colorMatrixValueForFilter(filterOperation.type(), &filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMatrix];
            [filter setValue:colorMatrixValue.get() forKey:@"inputColorMatrix"];
            [filter setName:filterName];
            return filter;
        }
        case FilterOperation::Type::Blur: {
            const auto& blurOperation = downcast<BlurFilterOperation>(filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterGaussianBlur];
            [filter setValue:[NSNumber numberWithFloat:floatValueForLength(blurOperation.stdDeviation(), 0)] forKey:@"inputRadius"];
            if (is_objc<CABackdropLayer>(layer)) {
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
        case FilterOperation::Type::Passthrough:
            return nil;
        }
        ASSERT_NOT_REACHED();
        return nil;
    });

    if ([array count])
        [layer setFilters:array.get()];

    END_BLOCK_OBJC_EXCEPTIONS
}

RetainPtr<NSValue> PlatformCAFilters::filterValueForOperation(const FilterOperation& filterOperation)
{
    auto* operation = &filterOperation;
    auto type = filterOperation.type();

    if (auto* defaultOperation = dynamicDowncast<DefaultFilterOperation>(filterOperation)) {
        type = defaultOperation->representedType();
        operation = nullptr;
    }

    RetainPtr<id> value;

    switch (type) {
    case FilterOperation::Type::Default:
        ASSERT_NOT_REACHED();
        break;
    case FilterOperation::Type::Grayscale: {
        // CAFilter: inputAmount
        double amount = 0;
        if (operation)
            amount = downcast<BasicColorMatrixFilterOperation>(*operation).amount();

        value = @(amount);
        break;
    }
    case FilterOperation::Type::Sepia: {
        // CAFilter: inputColorMatrix
        value = PlatformCAFilters::colorMatrixValueForFilter(type, operation);
        break;
    }
    case FilterOperation::Type::Saturate: {
        // CAFilter: inputAmount
        double amount = 1;
        if (operation)
            amount = downcast<BasicColorMatrixFilterOperation>(*operation).amount();

        value = @(amount);
        break;
    }
    case FilterOperation::Type::HueRotate: {
        // Hue rotate CAFilter: inputAngle
        double amount = 0;
        if (operation)
            amount = downcast<BasicColorMatrixFilterOperation>(*operation).amount();

        amount = deg2rad(amount);
        value = @(amount);
        break;
    }
    case FilterOperation::Type::Invert: {
        // CAFilter: inputColorMatrix
        value = PlatformCAFilters::colorMatrixValueForFilter(type, operation);
        break;
    }
    case FilterOperation::Type::AppleInvertLightness:
        ASSERT_NOT_REACHED(); // AppleInvertLightness is only used in -apple-color-filter.
        break;
    case FilterOperation::Type::Opacity: {
        // Opacity CAFilter: inputColorMatrix
        value = PlatformCAFilters::colorMatrixValueForFilter(type, operation);
        break;
    }

    case FilterOperation::Type::Brightness: {
        // Brightness CAFilter: inputColorMatrix
        value = PlatformCAFilters::colorMatrixValueForFilter(type, operation);
        break;
    }

    case FilterOperation::Type::Contrast: {
        // Contrast CAFilter: inputColorMatrix
        value = PlatformCAFilters::colorMatrixValueForFilter(type, operation);
        break;
    }
    case FilterOperation::Type::Blur: {
        // CAFilter: inputRadius
        double amount = 0;
        if (operation)
            amount = floatValueForLength(downcast<BlurFilterOperation>(*operation).stdDeviation(), 0);

        value = @(amount);
        break;
    }
    default:
        break;
    }
    
    return value;
}

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

RetainPtr<NSValue> PlatformCAFilters::colorMatrixValueForFilter(FilterOperation::Type type, const FilterOperation* filterOperation)
{
    switch (type) {
    case FilterOperation::Type::Sepia: {
        float amount = filterOperation ? downcast<BasicColorMatrixFilterOperation>(*filterOperation).amount() : 0;
        auto sepiaMatrix = sepiaColorMatrix(amount);
        return [NSValue valueWithCAColorMatrix:caColorMatrixFromColorMatrix(sepiaMatrix)];
    }
    case FilterOperation::Type::Invert: {
        float amount = filterOperation ? downcast<BasicComponentTransferFilterOperation>(*filterOperation).amount() : 0;
        auto invertMatrix = invertColorMatrix(amount);
        return [NSValue valueWithCAColorMatrix:caColorMatrixFromColorMatrix(invertMatrix)];
    }
    case FilterOperation::Type::AppleInvertLightness:
        ASSERT_NOT_REACHED(); // AppleInvertLightness is only used in -apple-color-filter.
        return nullptr;
    case FilterOperation::Type::Opacity: {
        float amount = filterOperation ? downcast<BasicComponentTransferFilterOperation>(filterOperation)->amount() : 1;
        auto opacityMatrix = opacityColorMatrix(amount);
        return [NSValue valueWithCAColorMatrix:caColorMatrixFromColorMatrix(opacityMatrix)];
    }
    case FilterOperation::Type::Contrast: {
        float amount = filterOperation ? downcast<BasicComponentTransferFilterOperation>(filterOperation)->amount() : 1;
        auto contrastMatrix = contrastColorMatrix(amount);
        return [NSValue valueWithCAColorMatrix:caColorMatrixFromColorMatrix(contrastMatrix)];
    }
    case FilterOperation::Type::Brightness: {
        float amount = filterOperation ? downcast<BasicComponentTransferFilterOperation>(filterOperation)->amount() : 1;
        auto brightnessMatrix = brightnessColorMatrix(amount);
        return [NSValue valueWithCAColorMatrix:caColorMatrixFromColorMatrix(brightnessMatrix)];
    }
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
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
        filter = [CAFilter filterWithType:kCAFilterHueBlendMode];
        break;
    case BlendMode::Saturation:
        filter = [CAFilter filterWithType:kCAFilterSaturationBlendMode];
        break;
    case BlendMode::Color:
        filter = [CAFilter filterWithType:kCAFilterColorBlendMode];
        break;
    case BlendMode::Luminosity:
        filter = [CAFilter filterWithType:kCAFilterLuminosityBlendMode];
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    [layer setCompositingFilter:filter];

    END_BLOCK_OBJC_EXCEPTIONS
}

bool PlatformCAFilters::isAnimatedFilterProperty(FilterOperation::Type type)
{
    switch (type) {
    case FilterOperation::Type::Grayscale:
    case FilterOperation::Type::Sepia:
    case FilterOperation::Type::Saturate:
    case FilterOperation::Type::HueRotate:
    case FilterOperation::Type::Invert:
    case FilterOperation::Type::Opacity:
    case FilterOperation::Type::Brightness:
    case FilterOperation::Type::Contrast:
    case FilterOperation::Type::Blur:
        return true;
    default:
        return false;
    }
}

static constexpr auto inputAmountProperty = "inputAmount"_s;
static constexpr auto inputColorMatrixProperty = "inputColorMatrix"_s;
static constexpr auto inputAngleProperty = "inputAngle"_s;
static constexpr auto inputRadiusProperty = "inputRadius"_s;

String PlatformCAFilters::animatedFilterPropertyName(FilterOperation::Type type)
{
    switch (type) {
    case FilterOperation::Type::Grayscale:
    case FilterOperation::Type::Saturate:
        return inputAmountProperty;
    case FilterOperation::Type::Sepia:
    case FilterOperation::Type::Invert:
    case FilterOperation::Type::Opacity:
    case FilterOperation::Type::Brightness:
    case FilterOperation::Type::Contrast:
        return inputColorMatrixProperty;
    case FilterOperation::Type::HueRotate:
        return inputAngleProperty;
    case FilterOperation::Type::Blur:
        return inputRadiusProperty;
    default:
        return emptyString();
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
