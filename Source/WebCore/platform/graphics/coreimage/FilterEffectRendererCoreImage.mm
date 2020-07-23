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

#import "Filter.h"
#import "FilterEffect.h"
#import "GraphicsContextCG.h"
#import "ImageBuffer.h"
#import "PlatformImageBuffer.h"
#import <CoreImage/CIContext.h>
#import <CoreImage/CIFilter.h>
#import <CoreImage/CoreImage.h>

namespace WebCore {

std::unique_ptr<FilterEffectRendererCoreImage> FilterEffectRendererCoreImage::tryCreate(FilterEffect& lastEffect)
{
    if (canRenderUsingCIFilters(lastEffect))
        return makeUnique<FilterEffectRendererCoreImage>();
    return nullptr;
}

bool FilterEffectRendererCoreImage::supportsCoreImageRendering(FilterEffect& effect)
{
    // FIXME: change return value to true once they are implemented
    switch (effect.filterEffectClassType()) {
    case FilterEffect::Type::Blend:
    case FilterEffect::Type::ColorMatrix:
    case FilterEffect::Type::ComponentTransfer:
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
    case FilterEffect::Type::SourceGraphic:
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

CIImage* FilterEffectRendererCoreImage::connectCIFilters(FilterEffect& effect)
{
    Vector<CIImage*> inputImages;
    
    for (auto in : effect.inputEffects()) {
        CIImage* inputImage = connectCIFilters(*in);
        if (!inputImage)
            return nullptr;
        inputImages.append(inputImage);
    }
    effect.determineAbsolutePaintRect();
    effect.setResultColorSpace(effect.operatingColorSpace());
    
    if (effect.absolutePaintRect().isEmpty() || ImageBuffer::sizeNeedsClamping(effect.absolutePaintRect().size()))
        return nullptr;
    
    // FIXME: Implement those filters using CIFilter so that the function returns a valid CIImage
    switch (effect.filterEffectClassType()) {
    case FilterEffect::Type::Blend:
    case FilterEffect::Type::ColorMatrix:
    case FilterEffect::Type::ComponentTransfer:
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
    case FilterEffect::Type::SourceGraphic:
    default:
        return nullptr;
    }
    return nullptr;
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
    CGColorSpaceRef resultColorSpace = cachedCGColorSpace(lastEffect.resultColorSpace());
    [m_context.get() render: m_outputImage.get() toIOSurface: surface.surface() bounds:destRect(lastEffect) colorSpace:resultColorSpace];
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
    m_ciFilterStorageMap.clear();
    m_outputImage = nullptr;
    m_context = nullptr;
}

FilterEffectRendererCoreImage::FilterEffectRendererCoreImage()
    : FilterEffectRenderer()
{
    CGColorSpaceRef colorSpace = cachedCGColorSpace(ColorSpace::SRGB);
    m_context = [CIContext contextWithOptions: @{ kCIContextWorkingColorSpace: (__bridge id)colorSpace}];
}
    
} // namespace WebCore

#endif
