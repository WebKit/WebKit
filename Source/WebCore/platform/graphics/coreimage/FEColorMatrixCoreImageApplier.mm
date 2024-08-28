/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#import "FEColorMatrixCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "FEColorMatrix.h"
#import "FilterImage.h"
#import <CoreImage/CIContext.h>
#import <CoreImage/CIFilter.h>
#import <CoreImage/CoreImage.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FEColorMatrixCoreImageApplier);

FEColorMatrixCoreImageApplier::FEColorMatrixCoreImageApplier(const FEColorMatrix& effect)
    : Base(effect)
{
    // FIXME: Implement the rest of FEColorMatrix types
    ASSERT(supportsCoreImageRendering(effect));
}

bool FEColorMatrixCoreImageApplier::supportsCoreImageRendering(const FEColorMatrix& effect)
{
    return effect.type() == ColorMatrixType::FECOLORMATRIX_TYPE_SATURATE
        || effect.type() == ColorMatrixType::FECOLORMATRIX_TYPE_HUEROTATE
        || effect.type() == ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX;
}

bool FEColorMatrixCoreImageApplier::apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const
{
    ASSERT(inputs.size() == 1);
    auto& input = inputs[0].get();

    auto inputImage = input.ciImage();
    if (!inputImage)
        return false;

    auto values = FEColorMatrix::normalizedFloats(m_effect.values());
    float components[9];

    switch (m_effect.type()) {
    case ColorMatrixType::FECOLORMATRIX_TYPE_SATURATE:
        FEColorMatrix::calculateSaturateComponents(components, values[0]);
        break;

    case ColorMatrixType::FECOLORMATRIX_TYPE_HUEROTATE:
        FEColorMatrix::calculateHueRotateComponents(components, values[0]);
        break;

    case ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX:
        break;

    case ColorMatrixType::FECOLORMATRIX_TYPE_UNKNOWN:
    case ColorMatrixType::FECOLORMATRIX_TYPE_LUMINANCETOALPHA: // FIXME: Add Luminance to Alpha Implementation
        return false;
    }

    auto *colorMatrixFilter = [CIFilter filterWithName:@"CIColorMatrix"];
    [colorMatrixFilter setValue:inputImage.get() forKey:kCIInputImageKey];

    switch (m_effect.type()) {
    case ColorMatrixType::FECOLORMATRIX_TYPE_SATURATE:
    case ColorMatrixType::FECOLORMATRIX_TYPE_HUEROTATE:
        [colorMatrixFilter setValue:[CIVector vectorWithX:components[0] Y:components[1] Z:components[2] W:0] forKey:@"inputRVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:components[3] Y:components[4] Z:components[5] W:0] forKey:@"inputGVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:components[6] Y:components[7] Z:components[8] W:0] forKey:@"inputBVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:0             Y:0             Z:0             W:1] forKey:@"inputAVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:0             Y:0             Z:0             W:0] forKey:@"inputBiasVector"];
        break;

    case ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX:
        [colorMatrixFilter setValue:[CIVector vectorWithX:values[0]  Y:values[1]  Z:values[2]  W:values[3]]  forKey:@"inputRVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:values[5]  Y:values[6]  Z:values[7]  W:values[8]]  forKey:@"inputGVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:values[10] Y:values[11] Z:values[12] W:values[13]] forKey:@"inputBVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:values[15] Y:values[16] Z:values[17] W:values[18]] forKey:@"inputAVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:values[4]  Y:values[9]  Z:values[14] W:values[19]] forKey:@"inputBiasVector"];
        break;

    case ColorMatrixType::FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
    case ColorMatrixType::FECOLORMATRIX_TYPE_UNKNOWN:
        return false;
    }

    result.setCIImage(colorMatrixFilter.outputImage);
    return true;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
