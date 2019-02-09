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
#import "LengthFunctions.h" // This is a layering violation.
#import "PlatformCALayerCocoa.h"
#import <QuartzCore/QuartzCore.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

// FIXME: Should share these values with CSSFilter::build() (https://bugs.webkit.org/show_bug.cgi?id=76008).
static const double sepiaFullConstants[3][3] = {
    { 0.393, 0.769, 0.189 },
    { 0.349, 0.686, 0.168 },
    { 0.272, 0.534, 0.131 }
};

static const double sepiaNoneConstants[3][3] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
    { 0, 0, 1 }
};

void PlatformCAFilters::setFiltersOnLayer(PlatformLayer* layer, const FilterOperations& filters)
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
    
    RetainPtr<NSMutableArray> array = adoptNS([[NSMutableArray alloc] init]);

    for (unsigned i = 0; i < filters.size(); ++i) {
        String filterName = makeString("filter_", i);
        const FilterOperation& filterOperation = *filters.at(i);
        switch (filterOperation.type()) {
        case FilterOperation::DEFAULT:
            ASSERT_NOT_REACHED();
            break;
        case FilterOperation::DROP_SHADOW: {
            // FIXME: For now assume drop shadow is the last filter, put it on the layer.
            // <rdar://problem/10959969> Handle case where drop-shadow is not the last filter.
            const auto& dropShadowOperation = downcast<DropShadowFilterOperation>(filterOperation);
            [layer setShadowOffset:CGSizeMake(dropShadowOperation.x(), dropShadowOperation.y())];
            [layer setShadowColor:cachedCGColor(dropShadowOperation.color())];
            [layer setShadowRadius:dropShadowOperation.stdDeviation()];
            [layer setShadowOpacity:1];
            break;
        }
#if USE_CA_FILTERS
        case FilterOperation::GRAYSCALE: {
            const auto& colorMatrixOperation = downcast<BasicColorMatrixFilterOperation>(filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMonochrome];
            [filter setValue:[NSNumber numberWithFloat:colorMatrixOperation.amount()] forKey:@"inputAmount"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::SEPIA: {
            RetainPtr<NSValue> colorMatrixValue = PlatformCAFilters::colorMatrixValueForFilter(filterOperation.type(), &filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMatrix];
            [filter setValue:colorMatrixValue.get() forKey:@"inputColorMatrix"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::SATURATE: {
            const auto& colorMatrixOperation = downcast<BasicColorMatrixFilterOperation>(filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorSaturate];
            [filter setValue:[NSNumber numberWithFloat:colorMatrixOperation.amount()] forKey:@"inputAmount"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::HUE_ROTATE: {
            const auto& colorMatrixOperation = downcast<BasicColorMatrixFilterOperation>(filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorHueRotate];
            [filter setValue:[NSNumber numberWithFloat:deg2rad(colorMatrixOperation.amount())] forKey:@"inputAngle"];
            [filter setName:@"hueRotate"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::INVERT: {
            RetainPtr<NSValue> colorMatrixValue = PlatformCAFilters::colorMatrixValueForFilter(filterOperation.type(), &filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMatrix];
            [filter setValue:colorMatrixValue.get() forKey:@"inputColorMatrix"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::APPLE_INVERT_LIGHTNESS:
            ASSERT_NOT_REACHED(); // APPLE_INVERT_LIGHTNESS is only used in -apple-color-filter.
            break;
        case FilterOperation::OPACITY: {
            RetainPtr<NSValue> colorMatrixValue = PlatformCAFilters::colorMatrixValueForFilter(filterOperation.type(), &filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMatrix];
            [filter setValue:colorMatrixValue.get() forKey:@"inputColorMatrix"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::BRIGHTNESS: {
            RetainPtr<NSValue> colorMatrixValue = PlatformCAFilters::colorMatrixValueForFilter(filterOperation.type(), &filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMatrix];
            [filter setValue:colorMatrixValue.get() forKey:@"inputColorMatrix"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::CONTRAST: {
            RetainPtr<NSValue> colorMatrixValue = PlatformCAFilters::colorMatrixValueForFilter(filterOperation.type(), &filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterColorMatrix];
            [filter setValue:colorMatrixValue.get() forKey:@"inputColorMatrix"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::BLUR: {
            const auto& blurOperation = downcast<BlurFilterOperation>(filterOperation);
            CAFilter *filter = [CAFilter filterWithType:kCAFilterGaussianBlur];
            [filter setValue:[NSNumber numberWithFloat:floatValueForLength(blurOperation.stdDeviation(), 0)] forKey:@"inputRadius"];
#if ENABLE(FILTERS_LEVEL_2)
            if ([layer isKindOfClass:[CABackdropLayer class]])
                [filter setValue:@YES forKey:@"inputNormalizeEdges"];
#endif
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
#else
        case FilterOperation::GRAYSCALE: {
            const auto& colorMatrixOperation = downcast<BasicColorMatrixFilterOperation>(filterOperation);
            CIFilter* filter = [CIFilter filterWithName:@"CIColorMonochrome"];
            [filter setDefaults];
            [filter setValue:[NSNumber numberWithFloat:colorMatrixOperation.amount()] forKey:@"inputIntensity"];
            [filter setValue:[CIColor colorWithRed:0.67 green:0.67 blue:0.67] forKey:@"inputColor"]; // Color derived empirically to match zero saturation levels.
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::SEPIA: {
            const auto& colorMatrixOperation = downcast<BasicColorMatrixFilterOperation>(filterOperation);
            CIFilter* filter = [CIFilter filterWithName:@"CIColorMatrix"];
            [filter setDefaults];

            double t = colorMatrixOperation.amount();
            t = std::min(std::max(0.0, t), 1.0);
            // FIXME: results don't match the software filter.
            [filter setValue:[CIVector vectorWithX:WebCore::blend(sepiaNoneConstants[0][0], sepiaFullConstants[0][0], t)
                Y:WebCore::blend(sepiaNoneConstants[0][1], sepiaFullConstants[0][1], t)
                Z:WebCore::blend(sepiaNoneConstants[0][2], sepiaFullConstants[0][2], t) W:0] forKey:@"inputRVector"];
            [filter setValue:[CIVector vectorWithX:WebCore::blend(sepiaNoneConstants[1][0], sepiaFullConstants[1][0], t)
                Y:WebCore::blend(sepiaNoneConstants[1][1], sepiaFullConstants[1][1], t)
                Z:WebCore::blend(sepiaNoneConstants[1][2], sepiaFullConstants[1][2], t) W:0] forKey:@"inputGVector"];
            [filter setValue:[CIVector vectorWithX:WebCore::blend(sepiaNoneConstants[2][0], sepiaFullConstants[2][0], t)
                Y:WebCore::blend(sepiaNoneConstants[2][1], sepiaFullConstants[2][1], t)
                Z:WebCore::blend(sepiaNoneConstants[2][2], sepiaFullConstants[2][2], t) W:0] forKey:@"inputBVector"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::SATURATE: {
            const auto& colorMatrixOperation = downcast<BasicColorMatrixFilterOperation>(filterOperation);
            CIFilter* filter = [CIFilter filterWithName:@"CIColorControls"];
            [filter setDefaults];
            [filter setValue:[NSNumber numberWithFloat:colorMatrixOperation.amount()] forKey:@"inputSaturation"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::HUE_ROTATE: {
            const auto& colorMatrixOperation = downcast<BasicColorMatrixFilterOperation>(filterOperation);
            CIFilter* filter = [CIFilter filterWithName:@"CIHueAdjust"];
            [filter setDefaults];

            [filter setValue:[NSNumber numberWithFloat:deg2rad(colorMatrixOperation.amount())] forKey:@"inputAngle"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::INVERT: {
            const auto& componentTransferOperation = downcast<BasicComponentTransferFilterOperation>(filterOperation);
            CIFilter* filter = [CIFilter filterWithName:@"CIColorMatrix"];
            [filter setDefaults];

            double multiplier = 1 - componentTransferOperation.amount() * 2;

            // FIXME: the results of this filter look wrong.
            [filter setValue:[CIVector vectorWithX:multiplier Y:0 Z:0 W:0] forKey:@"inputRVector"];
            [filter setValue:[CIVector vectorWithX:0 Y:multiplier Z:0 W:0] forKey:@"inputGVector"];
            [filter setValue:[CIVector vectorWithX:0 Y:0 Z:multiplier W:0] forKey:@"inputBVector"];
            [filter setValue:[CIVector vectorWithX:0 Y:0 Z:0 W:1] forKey:@"inputAVector"];
            [filter setValue:[CIVector vectorWithX:op->amount() Y:op->amount() Z:op->amount() W:0] forKey:@"inputBiasVector"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::APPLE_INVERT_LIGHTNESS:
            ASSERT_NOT_REACHED(); // APPLE_INVERT_LIGHTNESS is only used in -apple-color-filter.
            break;
        case FilterOperation::OPACITY: {
            const auto& componentTransferOperation = downcast<BasicComponentTransferFilterOperation>(filterOperation);
            CIFilter* filter = [CIFilter filterWithName:@"CIColorMatrix"];
            [filter setDefaults];

            [filter setValue:[CIVector vectorWithX:1 Y:0 Z:0 W:0] forKey:@"inputRVector"];
            [filter setValue:[CIVector vectorWithX:0 Y:1 Z:0 W:0] forKey:@"inputGVector"];
            [filter setValue:[CIVector vectorWithX:0 Y:0 Z:1 W:0] forKey:@"inputBVector"];
            [filter setValue:[CIVector vectorWithX:0 Y:0 Z:0 W:componentTransferOperation.amount()] forKey:@"inputAVector"];
            [filter setValue:[CIVector vectorWithX:0 Y:0 Z:0 W:0] forKey:@"inputBiasVector"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::BRIGHTNESS: {
            const auto& componentTransferOperation = downcast<BasicComponentTransferFilterOperation>(filterOperation);
            CIFilter* filter = [CIFilter filterWithName:@"CIColorMatrix"];
            [filter setDefaults];
            double amount = componentTransferOperation.amount();

            [filter setValue:[CIVector vectorWithX:amount Y:0 Z:0 W:0] forKey:@"inputRVector"];
            [filter setValue:[CIVector vectorWithX:0 Y:amount Z:0 W:0] forKey:@"inputGVector"];
            [filter setValue:[CIVector vectorWithX:0 Y:0 Z:amount W:0] forKey:@"inputBVector"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::CONTRAST: {
            const auto& componentTransferOperation = downcast<BasicComponentTransferFilterOperation>(filterOperation);
            CIFilter* filter = [CIFilter filterWithName:@"CIColorControls"];
            [filter setDefaults];
            [filter setValue:[NSNumber numberWithFloat:componentTransferOperation.amount()] forKey:@"inputContrast"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
        case FilterOperation::BLUR: {
            // FIXME: For now we ignore stdDeviationY.
            const auto& blurOperation = downcast<BlurFilterOperation>(filterOperation);
            CIFilter* filter = [CIFilter filterWithName:@"CIGaussianBlur"];
            [filter setDefaults];
            [filter setValue:[NSNumber numberWithFloat:floatValueForLength(blurOperation.stdDeviation(), 0)] forKey:@"inputRadius"];
            [filter setName:filterName];
            [array.get() addObject:filter];
            break;
        }
#endif
        case FilterOperation::PASSTHROUGH:
            break;
        default:
            ASSERT(0);
            break;
        }
    }

    if ([array.get() count] > 0)
        [layer setFilters:array.get()];
    
    END_BLOCK_OBJC_EXCEPTIONS
}

RetainPtr<NSValue> PlatformCAFilters::filterValueForOperation(const FilterOperation* operation, int internalFilterPropertyIndex)
{
#if USE_CA_FILTERS
    UNUSED_PARAM(internalFilterPropertyIndex);
#endif
    FilterOperation::OperationType type = operation->type();
    RetainPtr<id> value;
    
    if (is<DefaultFilterOperation>(*operation)) {
        type = downcast<DefaultFilterOperation>(*operation).representedType();
        operation = nullptr;
    }
    
    switch (type) {
    case FilterOperation::DEFAULT:
        ASSERT_NOT_REACHED();
        break;
    case FilterOperation::GRAYSCALE: {
        // CIFilter: inputIntensity
        // CAFilter: inputAmount
        double amount = 0;
        if (operation)
            amount = downcast<BasicColorMatrixFilterOperation>(*operation).amount();
        
        value = [NSNumber numberWithDouble:amount];
        break;
    }
    case FilterOperation::SEPIA: {
#if USE_CA_FILTERS
        // CAFilter: inputColorMatrix
        value = PlatformCAFilters::colorMatrixValueForFilter(type, operation);
#else
        // CIFilter: inputRVector, inputGVector, inputBVector
        double amount = 0;
        if (operation)
            amount = downcast<BasicColorMatrixFilterOperation>(*operation).amount();

        CIVector* rowVector = nullptr;
        switch (internalFilterPropertyIndex) {
        case 0: rowVector = [[CIVector alloc] initWithX:WebCore::blend(sepiaNoneConstants[0][0], sepiaFullConstants[0][0], amount)
            Y:WebCore::blend(sepiaNoneConstants[0][1], sepiaFullConstants[0][1], amount)
            Z:WebCore::blend(sepiaNoneConstants[0][2], sepiaFullConstants[0][2], amount) W:0];
            break; // inputRVector
        case 1: rowVector = [[CIVector alloc] initWithX:WebCore::blend(sepiaNoneConstants[1][0], sepiaFullConstants[1][0], amount)
            Y:WebCore::blend(sepiaNoneConstants[1][1], sepiaFullConstants[1][1], amount)
            Z:WebCore::blend(sepiaNoneConstants[1][2], sepiaFullConstants[1][2], amount) W:0];
            break; // inputGVector
        case 2: rowVector = [[CIVector alloc] initWithX:WebCore::blend(sepiaNoneConstants[2][0], sepiaFullConstants[2][0], amount)
            Y:WebCore::blend(sepiaNoneConstants[2][1], sepiaFullConstants[2][1], amount)
            Z:WebCore::blend(sepiaNoneConstants[2][2], sepiaFullConstants[2][2], amount) W:0];
            break; // inputBVector
        }
        value = adoptNS(rowVector);
#endif
        break;
    }
    case FilterOperation::SATURATE: {
        // CIFilter: inputSaturation
        // CAFilter: inputAmount
        double amount = 1;
        if (operation)
            amount = downcast<BasicColorMatrixFilterOperation>(*operation).amount();
        
        value = [NSNumber numberWithDouble:amount];
        break;
    }
    case FilterOperation::HUE_ROTATE: {
        // Hue rotate CIFilter: inputAngle
        // Hue rotate CAFilter: inputAngle
        double amount = 0;
        if (operation)
            amount = downcast<BasicColorMatrixFilterOperation>(*operation).amount();
        
        amount = deg2rad(amount);
        value = [NSNumber numberWithDouble:amount];
        break;
    }
    case FilterOperation::INVERT: {
#if USE_CA_FILTERS
        // CAFilter: inputColorMatrix
        value = PlatformCAFilters::colorMatrixValueForFilter(type, operation);
#else
        // CIFilter: inputRVector, inputGVector, inputBVector, inputBiasVector
        double amount = 0;
        if (operation)
            amount = downcast<BasicComponentTransferFilterOperation>(*operation).amount();
        
        double multiplier = 1 - amount * 2;

        // The color matrix animation for invert does a scale of each color component by a value that goes from 
        // 1 (when amount is 0) to -1 (when amount is 1). Then the color values are offset by amount. This has the
        // effect of performing the operation: c' = c * -1 + 1, which inverts the color.
        CIVector* rowVector = 0;
        switch (internalFilterPropertyIndex) {
        case 0: rowVector = [[CIVector alloc] initWithX:multiplier Y:0 Z:0 W:0]; break; // inputRVector
        case 1: rowVector = [[CIVector alloc] initWithX:0 Y:multiplier Z:0 W:0]; break; // inputGVector
        case 2: rowVector = [[CIVector alloc] initWithX:0 Y:0 Z:multiplier W:0]; break; // inputBVector
        case 3: rowVector = [[CIVector alloc] initWithX:amount Y:amount Z:amount W:0]; break; // inputBiasVector
        }
        value = adoptNS(rowVector);
#endif
        break;
    }
    case FilterOperation::APPLE_INVERT_LIGHTNESS:
            ASSERT_NOT_REACHED(); // APPLE_INVERT_LIGHTNESS is only used in -apple-color-filter.
        break;
    case FilterOperation::OPACITY: {
#if USE_CA_FILTERS
        // Opacity CAFilter: inputColorMatrix
        value = PlatformCAFilters::colorMatrixValueForFilter(type, operation);
#else
        // Opacity CIFilter: inputAVector
        double amount = 1;
        if (operation)
            amount = downcast<BasicComponentTransferFilterOperation>(*operation).amount();
        
        value = adoptNS([[CIVector alloc] initWithX:0 Y:0 Z:0 W:amount]);
#endif
        break;
    }
    
    case FilterOperation::BRIGHTNESS: {
#if USE_CA_FILTERS
        // Brightness CAFilter: inputColorMatrix
        value = PlatformCAFilters::colorMatrixValueForFilter(type, operation);
#else
        // Brightness CIFilter: inputColorMatrix
        double amount = 1;
        if (operation)
            amount = downcast<BasicComponentTransferFilterOperation>(*operation).amount();
        
        CIVector* rowVector = nullptr;
        switch (internalFilterPropertyIndex) {
        case 0: rowVector = [[CIVector alloc] initWithX:amount Y:0 Z:0 W:0]; break; // inputRVector
        case 1: rowVector = [[CIVector alloc] initWithX:0 Y:amount Z:0 W:0]; break; // inputGVector
        case 2: rowVector = [[CIVector alloc] initWithX:0 Y:0 Z:amount W:0]; break; // inputBVector
        }
        value = adoptNS(rowVector);
#endif
        break;
    }

    case FilterOperation::CONTRAST: {
#if USE_CA_FILTERS
        // Contrast CAFilter: inputColorMatrix
        value = PlatformCAFilters::colorMatrixValueForFilter(type, operation);
#else
        // Contrast CIFilter: inputContrast
        double amount = 1;
        if (operation)
            amount = downcast<BasicComponentTransferFilterOperation>(*operation).amount();
        
        value = [NSNumber numberWithDouble:amount];
#endif
        break;
    }
    case FilterOperation::BLUR: {
        // CIFilter: inputRadius
        // CAFilter: inputRadius
        double amount = 0;
        if (operation)
            amount = floatValueForLength(downcast<BlurFilterOperation>(*operation).stdDeviation(), 0);
        
        value = [NSNumber numberWithDouble:amount];
        break;
    }
    default:
        break;
    }
    
    return value;
}

#if USE_CA_FILTERS
RetainPtr<NSValue> PlatformCAFilters::colorMatrixValueForFilter(FilterOperation::OperationType type, const FilterOperation* filterOperation)
{
    switch (type) {
    case FilterOperation::SEPIA: {
        double t = filterOperation ? downcast<BasicColorMatrixFilterOperation>(*filterOperation).amount() : 0;
        t = std::min(std::max(0.0, t), 1.0);
        CAColorMatrix colorMatrix = {
            static_cast<float>(WebCore::blend(sepiaNoneConstants[0][0], sepiaFullConstants[0][0], t)),
            static_cast<float>(WebCore::blend(sepiaNoneConstants[0][1], sepiaFullConstants[0][1], t)),
            static_cast<float>(WebCore::blend(sepiaNoneConstants[0][2], sepiaFullConstants[0][2], t)), 0, 0,
            
            static_cast<float>(WebCore::blend(sepiaNoneConstants[1][0], sepiaFullConstants[1][0], t)),
            static_cast<float>(WebCore::blend(sepiaNoneConstants[1][1], sepiaFullConstants[1][1], t)),
            static_cast<float>(WebCore::blend(sepiaNoneConstants[1][2], sepiaFullConstants[1][2], t)), 0, 0,
            
            static_cast<float>(WebCore::blend(sepiaNoneConstants[2][0], sepiaFullConstants[2][0], t)),
            static_cast<float>(WebCore::blend(sepiaNoneConstants[2][1], sepiaFullConstants[2][1], t)),
            static_cast<float>(WebCore::blend(sepiaNoneConstants[2][2], sepiaFullConstants[2][2], t)), 0, 0,
            0, 0, 0, 1, 0
        };
        return [NSValue valueWithCAColorMatrix:colorMatrix];
    }
    case FilterOperation::INVERT: {
        float amount = filterOperation ? downcast<BasicComponentTransferFilterOperation>(*filterOperation).amount() : 0;
        float multiplier = 1 - amount * 2;
        CAColorMatrix colorMatrix = {
            multiplier, 0, 0, 0, amount,
            0, multiplier, 0, 0, amount,
            0, 0, multiplier, 0, amount,
            0, 0, 0, 1, 0
        };
        return [NSValue valueWithCAColorMatrix:colorMatrix];
    }
    case FilterOperation::APPLE_INVERT_LIGHTNESS:
        ASSERT_NOT_REACHED(); // APPLE_INVERT_LIGHTNESS is only used in -apple-color-filter.
        return nullptr;
    case FilterOperation::OPACITY: {
        float amount = filterOperation ? downcast<BasicComponentTransferFilterOperation>(filterOperation)->amount() : 1;
        CAColorMatrix colorMatrix = {
            1, 0, 0, 0, 0,
            0, 1, 0, 0, 0,
            0, 0, 1, 0, 0,
            0, 0, 0, amount, 0
        };
        return [NSValue valueWithCAColorMatrix:colorMatrix];
    }
    case FilterOperation::CONTRAST: {
        float amount = filterOperation ? downcast<BasicComponentTransferFilterOperation>(filterOperation)->amount() : 1;
        float intercept = -0.5 * amount + 0.5;
        CAColorMatrix colorMatrix = {
            amount, 0, 0, 0, intercept,
            0, amount, 0, 0, intercept,
            0, 0, amount, 0, intercept,
            0, 0, 0, 1, 0
        };
        return [NSValue valueWithCAColorMatrix:colorMatrix];
    }
    case FilterOperation::BRIGHTNESS: {
        float amount = filterOperation ? downcast<BasicComponentTransferFilterOperation>(filterOperation)->amount() : 1;
        CAColorMatrix colorMatrix = {
            amount, 0, 0, 0, 0,
            0, amount, 0, 0, 0,
            0, 0, amount, 0, 0,
            0, 0, 0, 1, 0
        };
        return [NSValue valueWithCAColorMatrix:colorMatrix];
    }
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}
#endif

void PlatformCAFilters::setBlendingFiltersOnLayer(PlatformLayer* layer, const BlendMode blendMode)
{
#if USE_CA_FILTERS
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
#else
    UNUSED_PARAM(layer);
    UNUSED_PARAM(blendMode);
#endif
}

int PlatformCAFilters::numAnimatedFilterProperties(FilterOperation::OperationType type)
{
#if USE_CA_FILTERS
    switch (type) {
    case FilterOperation::GRAYSCALE: return 1;
    case FilterOperation::SEPIA: return 1;
    case FilterOperation::SATURATE: return 1;
    case FilterOperation::HUE_ROTATE: return 1;
    case FilterOperation::INVERT: return 1;
    case FilterOperation::OPACITY: return 1;
    case FilterOperation::BRIGHTNESS: return 1;
    case FilterOperation::CONTRAST: return 1;
    case FilterOperation::BLUR: return 1;
    default: return 0;
    }
#else
    switch (type) {
    case FilterOperation::GRAYSCALE: return 1;
    case FilterOperation::SEPIA: return 3;
    case FilterOperation::SATURATE: return 1;
    case FilterOperation::HUE_ROTATE: return 1;
    case FilterOperation::INVERT: return 4;
    case FilterOperation::OPACITY: return 1;
    case FilterOperation::BRIGHTNESS: return 3;
    case FilterOperation::CONTRAST: return 1;
    case FilterOperation::BLUR: return 1;
    default: return 0;
    }
#endif
}

const char* PlatformCAFilters::animatedFilterPropertyName(FilterOperation::OperationType type, int internalFilterPropertyIndex)
{
#if USE_CA_FILTERS
    UNUSED_PARAM(internalFilterPropertyIndex);
    switch (type) {
    case FilterOperation::GRAYSCALE: return "inputAmount";
    case FilterOperation::SEPIA:return "inputColorMatrix";
    case FilterOperation::SATURATE: return "inputAmount";
    case FilterOperation::HUE_ROTATE: return "inputAngle";
    case FilterOperation::INVERT: return "inputColorMatrix";
    case FilterOperation::OPACITY: return "inputColorMatrix";
    case FilterOperation::BRIGHTNESS: return "inputColorMatrix";
    case FilterOperation::CONTRAST: return "inputColorMatrix";
    case FilterOperation::BLUR: return "inputRadius";
    default: return "";
    }
#else
    switch (type) {
    case FilterOperation::GRAYSCALE: return "inputIntensity";
    case FilterOperation::SEPIA:
    case FilterOperation::BRIGHTNESS:
        switch (internalFilterPropertyIndex) {
        case 0: return "inputRVector";
        case 1: return "inputGVector";
        case 2: return "inputBVector";
        default: return "";
        }
    case FilterOperation::SATURATE: return "inputSaturation";
    case FilterOperation::HUE_ROTATE: return "inputAngle";
    case FilterOperation::INVERT:
        switch (internalFilterPropertyIndex) {
        case 0: return "inputRVector";
        case 1: return "inputGVector";
        case 2: return "inputBVector";
        case 3: return "inputBiasVector";
        default: return "";
        }
    case FilterOperation::OPACITY: return "inputAVector";
    case FilterOperation::CONTRAST: return "inputContrast";
    case FilterOperation::BLUR: return "inputRadius";
    default: return "";
    }
#endif
}

} // namespace WebCore
