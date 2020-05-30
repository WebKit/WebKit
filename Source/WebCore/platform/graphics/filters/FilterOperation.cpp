/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "FilterOperation.h"

#include "AnimationUtilities.h"
#include "CachedResourceLoader.h"
#include "CachedSVGDocumentReference.h"
#include "ColorMatrix.h"
#include "ColorUtilities.h"
#include "FilterEffect.h"
#include "SVGURIReference.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
    
bool DefaultFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    
    return representedType() == downcast<DefaultFilterOperation>(operation).representedType();
}

ReferenceFilterOperation::ReferenceFilterOperation(const String& url, const String& fragment)
    : FilterOperation(REFERENCE)
    , m_url(url)
    , m_fragment(fragment)
{
}

ReferenceFilterOperation::~ReferenceFilterOperation() = default;
    
bool ReferenceFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    
    return m_url == downcast<ReferenceFilterOperation>(operation).m_url;
}

void ReferenceFilterOperation::loadExternalDocumentIfNeeded(CachedResourceLoader& cachedResourceLoader, const ResourceLoaderOptions& options)
{
    if (m_cachedSVGDocumentReference)
        return;
    if (!SVGURIReference::isExternalURIReference(m_url, *cachedResourceLoader.document()))
        return;
    m_cachedSVGDocumentReference = makeUnique<CachedSVGDocumentReference>(m_url);
    m_cachedSVGDocumentReference->load(cachedResourceLoader, options);
}

RefPtr<FilterOperation> BasicColorMatrixFilterOperation::blend(const FilterOperation* from, double progress, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;
    
    if (blendToPassthrough)
        return BasicColorMatrixFilterOperation::create(WebCore::blend(m_amount, passthroughAmount(), progress), m_type);
        
    const BasicColorMatrixFilterOperation* fromOperation = downcast<BasicColorMatrixFilterOperation>(from);
    double fromAmount = fromOperation ? fromOperation->amount() : passthroughAmount();
    return BasicColorMatrixFilterOperation::create(WebCore::blend(fromAmount, m_amount, progress), m_type);
}

bool BasicColorMatrixFilterOperation::transformColor(ColorComponents<float>& colorComponents) const
{
    switch (m_type) {
    case GRAYSCALE: {
        auto matrix = grayscaleColorMatrix(m_amount);
        matrix.transformColorComponents(colorComponents);
        return true;
    }
    case SEPIA: {
        auto matrix = sepiaColorMatrix(m_amount);
        matrix.transformColorComponents(colorComponents);
        return true;
    }
    case HUE_ROTATE: {
        auto matrix = hueRotateColorMatrix(m_amount);
        matrix.transformColorComponents(colorComponents);
        return true;
    }
    case SATURATE: {
        auto matrix = saturationColorMatrix(m_amount);
        matrix.transformColorComponents(colorComponents);
        return true;
    }
    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    return false;
}

inline bool BasicColorMatrixFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    const BasicColorMatrixFilterOperation& other = downcast<BasicColorMatrixFilterOperation>(operation);
    return m_amount == other.m_amount;
}

double BasicColorMatrixFilterOperation::passthroughAmount() const
{
    switch (m_type) {
    case GRAYSCALE:
    case SEPIA:
    case HUE_ROTATE:
        return 0;
    case SATURATE:
        return 1;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

RefPtr<FilterOperation> BasicComponentTransferFilterOperation::blend(const FilterOperation* from, double progress, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;
    
    if (blendToPassthrough)
        return BasicComponentTransferFilterOperation::create(WebCore::blend(m_amount, passthroughAmount(), progress), m_type);
        
    const BasicComponentTransferFilterOperation* fromOperation = downcast<BasicComponentTransferFilterOperation>(from);
    double fromAmount = fromOperation ? fromOperation->amount() : passthroughAmount();
    return BasicComponentTransferFilterOperation::create(WebCore::blend(fromAmount, m_amount, progress), m_type);
}

bool BasicComponentTransferFilterOperation::transformColor(ColorComponents<float>& colorComponents) const
{
    switch (m_type) {
    case OPACITY:
        colorComponents[3] *= m_amount;
        return true;
    case INVERT: {
        float oneMinusAmount = 1.f - m_amount;
        colorComponents[0] = 1 - (oneMinusAmount + colorComponents[0] * (m_amount - oneMinusAmount));
        colorComponents[1] = 1 - (oneMinusAmount + colorComponents[1] * (m_amount - oneMinusAmount));
        colorComponents[2] = 1 - (oneMinusAmount + colorComponents[2] * (m_amount - oneMinusAmount));
        return true;
    }
    case CONTRAST: {
        float intercept = -(0.5f * m_amount) + 0.5f;
        colorComponents[0] = clampTo<float>(intercept + m_amount * colorComponents[0], 0, 1);
        colorComponents[1] = clampTo<float>(intercept + m_amount * colorComponents[1], 0, 1);
        colorComponents[2] = clampTo<float>(intercept + m_amount * colorComponents[2], 0, 1);
        return true;
    }
    case BRIGHTNESS:
        colorComponents[0] = std::max<float>(m_amount * colorComponents[0], 0);
        colorComponents[1] = std::max<float>(m_amount * colorComponents[1], 0);
        colorComponents[2] = std::max<float>(m_amount * colorComponents[2], 0);
        return true;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
    return false;
}

inline bool BasicComponentTransferFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    const BasicComponentTransferFilterOperation& other = downcast<BasicComponentTransferFilterOperation>(operation);
    return m_amount == other.m_amount;
}

double BasicComponentTransferFilterOperation::passthroughAmount() const
{
    switch (m_type) {
    case OPACITY:
        return 1;
    case INVERT:
        return 0;
    case CONTRAST:
        return 1;
    case BRIGHTNESS:
        return 1;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}
    
bool InvertLightnessFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;

    return true;
}
    
RefPtr<FilterOperation> InvertLightnessFilterOperation::blend(const FilterOperation* from, double, bool)
{
    if (from && !from->isSameType(*this))
        return this;

    // This filter is not currently blendable.
    return InvertLightnessFilterOperation::create();
}

bool InvertLightnessFilterOperation::transformColor(ColorComponents<float>& sRGBColorComponents) const
{
    auto hslComponents = sRGBToHSL(sRGBColorComponents);
    
    // Rotate the hue 180deg.
    hslComponents[0] = fmod(hslComponents[0] + 0.5f, 1.0f);
    
    // Convert back to RGB.
    sRGBColorComponents = hslToSRGB(hslComponents);
    
    // Apply the matrix. See rdar://problem/41146650 for how this matrix was derived.
    constexpr ColorMatrix<5, 3> toDarkModeMatrix {
       -0.770f,  0.059f, -0.089f, 0.0f, 1.0f,
        0.030f, -0.741f, -0.089f, 0.0f, 1.0f,
        0.030f,  0.059f, -0.890f, 0.0f, 1.0f
    };
    toDarkModeMatrix.transformColorComponents(sRGBColorComponents);
    return true;
}

bool InvertLightnessFilterOperation::inverseTransformColor(ColorComponents<float>& sRGBColorComponents) const
{
    auto rgbComponents = sRGBColorComponents;

    // Apply the matrix.
    constexpr ColorMatrix<5, 3> toLightModeMatrix {
        -1.300f, -0.097f,  0.147f, 0.0f, 1.25f,
        -0.049f, -1.347f,  0.146f, 0.0f, 1.25f,
        -0.049f, -0.097f, -1.104f, 0.0f, 1.25f
    };
    toLightModeMatrix.transformColorComponents(rgbComponents);

    // Convert to HSL.
    auto hslComponents = sRGBToHSL(rgbComponents);
    // Hue rotate by 180deg.
    hslComponents[0] = fmod(hslComponents[0] + 0.5f, 1.0f);
    // And return RGB.
    sRGBColorComponents = hslToSRGB(hslComponents);
    return true;
}

bool BlurFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    
    return m_stdDeviation == downcast<BlurFilterOperation>(operation).stdDeviation();
}
    
RefPtr<FilterOperation> BlurFilterOperation::blend(const FilterOperation* from, double progress, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;

    LengthType lengthType = m_stdDeviation.type();

    if (blendToPassthrough)
        return BlurFilterOperation::create(WebCore::blend(m_stdDeviation, Length(lengthType), progress));

    const BlurFilterOperation* fromOperation = downcast<BlurFilterOperation>(from);
    Length fromLength = fromOperation ? fromOperation->m_stdDeviation : Length(lengthType);
    return BlurFilterOperation::create(WebCore::blend(fromLength, m_stdDeviation, progress));
}
    
bool DropShadowFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    const DropShadowFilterOperation& other = downcast<DropShadowFilterOperation>(operation);
    return m_location == other.m_location && m_stdDeviation == other.m_stdDeviation && m_color == other.m_color;
}
    
RefPtr<FilterOperation> DropShadowFilterOperation::blend(const FilterOperation* from, double progress, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;

    if (blendToPassthrough)
        return DropShadowFilterOperation::create(
            WebCore::blend(m_location, IntPoint(), progress),
            WebCore::blend(m_stdDeviation, 0, progress),
            WebCore::blend(m_color, Color(Color::transparent), progress));

    const DropShadowFilterOperation* fromOperation = downcast<DropShadowFilterOperation>(from);
    IntPoint fromLocation = fromOperation ? fromOperation->location() : IntPoint();
    int fromStdDeviation = fromOperation ? fromOperation->stdDeviation() : 0;
    Color fromColor = fromOperation ? fromOperation->color() : Color(Color::transparent);
    
    return DropShadowFilterOperation::create(
        WebCore::blend(fromLocation, m_location, progress),
        WebCore::blend(fromStdDeviation, m_stdDeviation, progress),
        WebCore::blend(fromColor, m_color, progress));
}

TextStream& operator<<(TextStream& ts, const FilterOperation& filter)
{
    switch (filter.type()) {
    case FilterOperation::REFERENCE:
        ts << "reference";
        break;
    case FilterOperation::GRAYSCALE: {
        const auto& colorMatrixFilter = downcast<BasicColorMatrixFilterOperation>(filter);
        ts << "grayscale(" << colorMatrixFilter.amount() << ")";
        break;
    }
    case FilterOperation::SEPIA: {
        const auto& colorMatrixFilter = downcast<BasicColorMatrixFilterOperation>(filter);
        ts << "sepia(" << colorMatrixFilter.amount() << ")";
        break;
    }
    case FilterOperation::SATURATE: {
        const auto& colorMatrixFilter = downcast<BasicColorMatrixFilterOperation>(filter);
        ts << "saturate(" << colorMatrixFilter.amount() << ")";
        break;
    }
    case FilterOperation::HUE_ROTATE: {
        const auto& colorMatrixFilter = downcast<BasicColorMatrixFilterOperation>(filter);
        ts << "hue-rotate(" << colorMatrixFilter.amount() << ")";
        break;
    }
    case FilterOperation::INVERT: {
        const auto& componentTransferFilter = downcast<BasicComponentTransferFilterOperation>(filter);
        ts << "invert(" << componentTransferFilter.amount() << ")";
        break;
    }
    case FilterOperation::APPLE_INVERT_LIGHTNESS: {
        ts << "apple-invert-lightness()";
        break;
    }
    case FilterOperation::OPACITY: {
        const auto& componentTransferFilter = downcast<BasicComponentTransferFilterOperation>(filter);
        ts << "opacity(" << componentTransferFilter.amount() << ")";
        break;
    }
    case FilterOperation::BRIGHTNESS: {
        const auto& componentTransferFilter = downcast<BasicComponentTransferFilterOperation>(filter);
        ts << "brightness(" << componentTransferFilter.amount() << ")";
        break;
    }
    case FilterOperation::CONTRAST: {
        const auto& componentTransferFilter = downcast<BasicComponentTransferFilterOperation>(filter);
        ts << "contrast(" << componentTransferFilter.amount() << ")";
        break;
    }
    case FilterOperation::BLUR: {
        const auto& blurFilter = downcast<BlurFilterOperation>(filter);
        ts << "blur(" << blurFilter.stdDeviation().value() << ")"; // FIXME: should call floatValueForLength() but that's outisde of platform/.
        break;
    }
    case FilterOperation::DROP_SHADOW: {
        const auto& dropShadowFilter = downcast<DropShadowFilterOperation>(filter);
        ts << "drop-shadow(" << dropShadowFilter.x() << " " << dropShadowFilter.y() << " " << dropShadowFilter.location() << " ";
        ts << dropShadowFilter.color() << ")";
        break;
    }
    case FilterOperation::PASSTHROUGH:
        ts << "passthrough";
        break;
    case FilterOperation::DEFAULT: {
        const auto& defaultFilter = downcast<DefaultFilterOperation>(filter);
        ts << "default type=" << (int)defaultFilter.representedType();
        break;
    }
    case FilterOperation::NONE:
        ts << "none";
        break;
    }
    return ts;
}

} // namespace WebCore
