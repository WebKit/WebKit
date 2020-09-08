/*
* Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "FilterEffectRendererCoreImage.h"

#if USE(CORE_IMAGE)

#import "FEColorMatrix.h"
#import "FEComponentTransfer.h"
#import "Filter.h"
#import "FilterEffect.h"
#import "FilterOperation.h"
#import "FloatConversion.h"
#import "GraphicsContextCG.h"
#import "ImageBuffer.h"
#import "PlatformImageBuffer.h"
#import "SourceGraphic.h"
#import <CoreImage/CIContext.h>
#import <CoreImage/CIFilter.h>
#import <CoreImage/CoreImage.h>
#import <wtf/NeverDestroyed.h>

namespace WebCore {

std::unique_ptr<FilterEffectRendererCoreImage> FilterEffectRendererCoreImage::tryCreate(FilterEffect& lastEffect)
{
    if (canRenderUsingCIFilters(lastEffect))
        return makeUnique<FilterEffectRendererCoreImage>();
    return nullptr;
}

RetainPtr<CIContext> FilterEffectRendererCoreImage::sharedCIContext()
{
    static NeverDestroyed<RetainPtr<CIContext>> ciContext = [CIContext contextWithOptions: @{ kCIContextWorkingColorSpace: (__bridge id)CGColorSpaceCreateWithName(kCGColorSpaceSRGB)}];
    return ciContext;
}

static bool isNullOrLinearComponentTransferFunction(const FEComponentTransfer& effect)
{
    auto isNullOrLinear = [] (const ComponentTransferFunction& function) {
        return function.type == FECOMPONENTTRANSFER_TYPE_UNKNOWN
            || function.type == FECOMPONENTTRANSFER_TYPE_LINEAR;
    };
    return isNullOrLinear(effect.redFunction()) && isNullOrLinear(effect.greenFunction())
        && isNullOrLinear(effect.blueFunction()) && isNullOrLinear(effect.alphaFunction());
}

bool FilterEffectRendererCoreImage::supportsCoreImageRendering(FilterEffect& effect)
{
    // FIXME: change return value to true once they are implemented
    switch (effect.filterEffectClassType()) {
    case FilterEffect::Type::SourceGraphic:
        return true;
            
    case FilterEffect::Type::ColorMatrix: {
        switch (downcast<FEColorMatrix>(effect).type()) {
        case FECOLORMATRIX_TYPE_UNKNOWN:
        case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
            return false;
        case FECOLORMATRIX_TYPE_MATRIX:
        case FECOLORMATRIX_TYPE_SATURATE:
        case FECOLORMATRIX_TYPE_HUEROTATE:
            return true;
        }
    }

    case FilterEffect::Type::ComponentTransfer:
        return isNullOrLinearComponentTransferFunction(downcast<FEComponentTransfer>(effect));

    case FilterEffect::Type::Blend:
    case FilterEffect::Type::Composite:
    case FilterEffect::Type::ConvolveMatrix:
    case FilterEffect::Type::DiffuseLighting:
    case FilterEffect::Type::DisplacementMap:
    case FilterEffect::Type::DropShadow:
    case FilterEffect::Type::Flood:
    case FilterEffect::Type::GaussianBlur:
    case FilterEffect::Type::Image:
    case FilterEffect::Type::Lighting:
    case FilterEffect::Type::Merge:
    case FilterEffect::Type::Morphology:
    case FilterEffect::Type::Offset:
    case FilterEffect::Type::SpecularLighting:
    case FilterEffect::Type::Tile:
    case FilterEffect::Type::Turbulence:
    case FilterEffect::Type::SourceAlpha:
        return false;
    }
    return false;
}

void FilterEffectRendererCoreImage::applyEffects(FilterEffect& lastEffect)
{
    m_outputImage = connectCIFilters(lastEffect);
    if (!m_outputImage)
        return;
    renderToImageBuffer(lastEffect);
}

RetainPtr<CIImage> FilterEffectRendererCoreImage::connectCIFilters(FilterEffect& effect)
{
    Vector<RetainPtr<CIImage>> inputImages;
    
    for (auto in : effect.inputEffects()) {
        auto inputImage = connectCIFilters(*in);
        if (!inputImage)
            return nullptr;
        inputImages.append(inputImage);
    }
    effect.determineAbsolutePaintRect();
    effect.setResultColorSpace(effect.operatingColorSpace());
    
    if (effect.absolutePaintRect().isEmpty() || ImageBuffer::sizeNeedsClamping(effect.absolutePaintRect().size()))
        return nullptr;
    
    switch (effect.filterEffectClassType()) {
    case FilterEffect::Type::SourceGraphic:
        return imageForSourceGraphic(downcast<SourceGraphic>(effect));
    case FilterEffect::Type::ColorMatrix:
        return imageForFEColorMatrix(downcast<FEColorMatrix>(effect), inputImages);
    case FilterEffect::Type::ComponentTransfer:
        return imageForFEComponentTransfer(downcast<FEComponentTransfer>(effect), inputImages);

    // FIXME: Implement those filters using CIFilter so that the function returns a valid CIImage
    case FilterEffect::Type::Blend:
    case FilterEffect::Type::Composite:
    case FilterEffect::Type::ConvolveMatrix:
    case FilterEffect::Type::DiffuseLighting:
    case FilterEffect::Type::DisplacementMap:
    case FilterEffect::Type::DropShadow:
    case FilterEffect::Type::Flood:
    case FilterEffect::Type::GaussianBlur:
    case FilterEffect::Type::Image:
    case FilterEffect::Type::Lighting:
    case FilterEffect::Type::Merge:
    case FilterEffect::Type::Morphology:
    case FilterEffect::Type::Offset:
    case FilterEffect::Type::SpecularLighting:
    case FilterEffect::Type::Tile:
    case FilterEffect::Type::Turbulence:
    case FilterEffect::Type::SourceAlpha:
    default:
        return nullptr;
    }
    return nullptr;
}

RetainPtr<CIImage> FilterEffectRendererCoreImage::imageForSourceGraphic(SourceGraphic& effect)
{
    ImageBuffer* sourceImage = effect.filter().sourceImage();
    if (!sourceImage)
        return nullptr;
    
    if (is<AcceleratedImageBuffer>(*sourceImage))
        return [CIImage imageWithIOSurface:downcast<AcceleratedImageBuffer>(*sourceImage).surface().surface()];
    
    return [CIImage imageWithCGImage:sourceImage->copyNativeImage().get()];
}

RetainPtr<CIImage> FilterEffectRendererCoreImage::imageForFEColorMatrix(const FEColorMatrix& effect, const Vector<RetainPtr<CIImage>>& inputImages)
{
    auto inputImage = inputImages.at(0);

    auto values = FEColorMatrix::normalizedFloats(effect.values());
    float components[9];
    
    switch (effect.type()) {
    case FECOLORMATRIX_TYPE_SATURATE:
        FEColorMatrix::calculateSaturateComponents(components, values[0]);
        break;
    
    case FECOLORMATRIX_TYPE_HUEROTATE:
        FEColorMatrix::calculateHueRotateComponents(components, values[0]);
        break;
    
    case FECOLORMATRIX_TYPE_MATRIX:
        break;
        
    case FECOLORMATRIX_TYPE_UNKNOWN:
    case FECOLORMATRIX_TYPE_LUMINANCETOALPHA: // FIXME: Add Luminance to Alpha Implementation
        return nullptr;
    }
    
    auto *colorMatrixFilter = [CIFilter filterWithName:@"CIColorMatrix"];
    [colorMatrixFilter setValue:inputImage.get() forKey:kCIInputImageKey];
    
    switch (effect.type()) {
    case FECOLORMATRIX_TYPE_SATURATE:
    case FECOLORMATRIX_TYPE_HUEROTATE: {
        [colorMatrixFilter setValue:[CIVector vectorWithX:components[0] Y:components[1] Z:components[2] W:0] forKey:@"inputRVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:components[3] Y:components[4] Z:components[5] W:0] forKey:@"inputGVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:components[6] Y:components[7] Z:components[8] W:0] forKey:@"inputBVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:0             Y:0             Z:0             W:1] forKey:@"inputAVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:0             Y:0             Z:0             W:0] forKey:@"inputBiasVector"];
        break;
    }
    case FECOLORMATRIX_TYPE_MATRIX: {
        [colorMatrixFilter setValue:[CIVector vectorWithX:values[0]  Y:values[1]  Z:values[2]  W:values[3]]  forKey:@"inputRVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:values[5]  Y:values[6]  Z:values[7]  W:values[8]]  forKey:@"inputGVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:values[10] Y:values[11] Z:values[12] W:values[13]] forKey:@"inputBVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:values[15] Y:values[16] Z:values[17] W:values[18]] forKey:@"inputAVector"];
        [colorMatrixFilter setValue:[CIVector vectorWithX:values[4]  Y:values[9]  Z:values[14] W:values[19]] forKey:@"inputBiasVector"];
        break;
    }
    case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
    case FECOLORMATRIX_TYPE_UNKNOWN:
        return nullptr;
    }
    return colorMatrixFilter.outputImage;
}

RetainPtr<CIImage> FilterEffectRendererCoreImage::imageForFEComponentTransfer(const FEComponentTransfer& effect, Vector<RetainPtr<CIImage>>& inputImages)
{
    // FIXME: Implement the rest of FEComponentTransfer functions
    ASSERT(isNullOrLinearComponentTransferFunction(effect));

    auto inputImage = inputImages.at(0);
    auto filter = [CIFilter filterWithName:@"CIColorPolynomial"];
    
    [filter setValue:inputImage.get() forKey:kCIInputImageKey];
    
    auto setCoefficients = [&] (NSString *key, const ComponentTransferFunction& function) {
        if (function.type == FECOMPONENTTRANSFER_TYPE_LINEAR)
            [filter setValue:[CIVector vectorWithX:function.intercept Y:function.slope Z:0 W:0] forKey:key];
    };
    setCoefficients(@"inputRedCoefficients", effect.redFunction());
    setCoefficients(@"inputGreenCoefficients", effect.greenFunction());
    setCoefficients(@"inputBlueCoefficients", effect.blueFunction());
    setCoefficients(@"inputAlphaCoefficients", effect.alphaFunction());
    
    return filter.outputImage;
}

bool FilterEffectRendererCoreImage::canRenderUsingCIFilters(FilterEffect& effect)
{
    if (!supportsCoreImageRendering(effect))
        return false;
    
    for (auto in : effect.inputEffects()) {
        if (!supportsCoreImageRendering(*in) || !canRenderUsingCIFilters(*in))
            return false;
    }
    return true;
}

ImageBuffer* FilterEffectRendererCoreImage::output() const
{
    LOG_WITH_STREAM(Filters, stream << "Rendering " << this << " using CoreImage\n");
    return m_outputImageBuffer.get();
}
    
void FilterEffectRendererCoreImage::renderToImageBuffer(FilterEffect& lastEffect)
{
    FloatSize clampedSize = ImageBuffer::clampedSize(lastEffect.absolutePaintRect().size());
    m_outputImageBuffer = ImageBuffer::create(clampedSize, RenderingMode::Accelerated, lastEffect.filter().filterScale(), lastEffect.resultColorSpace());
    
    if (!m_outputImageBuffer) {
        clearResult();
        return;
    }
    
    // FIXME: In the situation where the Image is too large, an UnacceleratedImageBuffer will be created
    // in this case, special handling is needed to render to ImageBufferCGBackend instead
    if (!is<AcceleratedImageBuffer>(*m_outputImageBuffer))
        return;
    
    auto& surface = downcast<AcceleratedImageBuffer>(*m_outputImageBuffer).surface();
    [sharedCIContext().get() render: m_outputImage.get() toIOSurface: surface.surface() bounds:destRect(lastEffect) colorSpace:cachedCGColorSpace(lastEffect.resultColorSpace())];
}
    
FloatRect FilterEffectRendererCoreImage::destRect(const FilterEffect& lastEffect) const
{
    IntSize destSize = lastEffect.absolutePaintRect().size();
    destSize.scale(lastEffect.filter().filterScale());
    FloatRect destRect = FloatRect(FloatPoint(), destSize);
    return destRect;
}

void FilterEffectRendererCoreImage::clearResult()
{
    m_outputImageBuffer.reset();
    m_outputImage = nullptr;
}

FilterEffectRendererCoreImage::FilterEffectRendererCoreImage()
    : FilterEffectRenderer()
{
}
    
} // namespace WebCore

#endif
