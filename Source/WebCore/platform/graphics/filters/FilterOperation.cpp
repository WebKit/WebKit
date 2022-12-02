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
#include "ColorBlending.h"
#include "ColorConversion.h"
#include "ColorMatrix.h"
#include "ColorTypes.h"
#include "FEDropShadow.h"
#include "FEGaussianBlur.h"
#include "FilterEffect.h"
#include "ImageBuffer.h"
#include "LengthFunctions.h"
#include "SVGURIReference.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
    
bool DefaultFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    
    return representedType() == downcast<DefaultFilterOperation>(operation).representedType();
}

ReferenceFilterOperation::ReferenceFilterOperation(const String& url, AtomString&& fragment)
    : FilterOperation(REFERENCE)
    , m_url(url)
    , m_fragment(WTFMove(fragment))
{
}

ReferenceFilterOperation::~ReferenceFilterOperation() = default;
    
bool ReferenceFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    
    return m_url == downcast<ReferenceFilterOperation>(operation).m_url;
}

bool ReferenceFilterOperation::isIdentity() const
{
    // Answering this question requires access to the renderer and the referenced filterElement.
    ASSERT_NOT_REACHED();
    return false;
}

IntOutsets ReferenceFilterOperation::outsets() const
{
    // Answering this question requires access to the renderer and the referenced filterElement.
    ASSERT_NOT_REACHED();
    return { };
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

double FilterOperation::blendAmounts(double from, double to, const BlendingContext& context) const
{
    auto blendedAmount = [&]() {
        if (context.compositeOperation == CompositeOperation::Accumulate) {
            // The "initial value for interpolation" is 1 for brightness, contrast, opacity and saturate.
            // Accumulation works differently for such operations per https://drafts.fxtf.org/filter-effects/#accumulation.
            switch (m_type) {
            case BRIGHTNESS:
            case CONTRAST:
            case OPACITY:
            case SATURATE:
                return from + to - 1;
            default:
                break;
            }
        }
        return WebCore::blend(from, to, context);
    }();

    // Make sure blended values remain within bounds as specified by
    // https://drafts.fxtf.org/filter-effects/#supported-filter-functions
    switch (m_type) {
    case GRAYSCALE:
    case INVERT:
    case OPACITY:
    case SEPIA:
        return std::clamp(blendedAmount, 0.0, 1.0);
    case BRIGHTNESS:
    case CONTRAST:
    case SATURATE:
        return std::max(blendedAmount, 0.0);
    default:
        return blendedAmount;
    }
}

RefPtr<FilterOperation> BasicColorMatrixFilterOperation::blend(const FilterOperation* from, const BlendingContext& context, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;

    if (blendToPassthrough)
        return BasicColorMatrixFilterOperation::create(blendAmounts(m_amount, passthroughAmount(), context), m_type);

    const BasicColorMatrixFilterOperation* fromOperation = downcast<BasicColorMatrixFilterOperation>(from);
    double fromAmount = fromOperation ? fromOperation->amount() : passthroughAmount();
    auto blendedAmount = blendAmounts(fromAmount, m_amount, context);
    return BasicColorMatrixFilterOperation::create(blendedAmount, m_type);
}

bool BasicColorMatrixFilterOperation::isIdentity() const
{
    return m_type == SATURATE ? (m_amount == 1) : !m_amount;
}

bool BasicColorMatrixFilterOperation::transformColor(SRGBA<float>& color) const
{
    switch (m_type) {
    case GRAYSCALE: {
        color = makeFromComponentsClamping<SRGBA<float>>(grayscaleColorMatrix(m_amount).transformedColorComponents(asColorComponents(color.resolved())));
        return true;
    }
    case SEPIA: {
        color = makeFromComponentsClamping<SRGBA<float>>(sepiaColorMatrix(m_amount).transformedColorComponents(asColorComponents(color.resolved())));
        return true;
    }
    case HUE_ROTATE: {
        color = makeFromComponentsClamping<SRGBA<float>>(hueRotateColorMatrix(m_amount).transformedColorComponents(asColorComponents(color.resolved())));
        return true;
    }
    case SATURATE: {
        color = makeFromComponentsClamping<SRGBA<float>>(saturationColorMatrix(m_amount).transformedColorComponents(asColorComponents(color.resolved())));
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

RefPtr<FilterOperation> BasicComponentTransferFilterOperation::blend(const FilterOperation* from, const BlendingContext& context, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;
    
    if (blendToPassthrough)
        return BasicComponentTransferFilterOperation::create(blendAmounts(m_amount, passthroughAmount(), context), m_type);
        
    const BasicComponentTransferFilterOperation* fromOperation = downcast<BasicComponentTransferFilterOperation>(from);
    double fromAmount = fromOperation ? fromOperation->amount() : passthroughAmount();
    auto blendedAmount = blendAmounts(fromAmount, m_amount, context);
    return BasicComponentTransferFilterOperation::create(blendedAmount, m_type);
}

bool BasicComponentTransferFilterOperation::isIdentity() const
{
    return m_type == INVERT ? !m_amount : (m_amount == 1);
}

bool BasicComponentTransferFilterOperation::transformColor(SRGBA<float>& color) const
{
    switch (m_type) {
    case OPACITY:
        color = colorWithOverriddenAlpha(color, std::clamp<float>(color.resolved().alpha * m_amount, 0.0f, 1.0f));
        return true;
    case INVERT: {
        float oneMinusAmount = 1.0f - m_amount;
        color = colorByModifingEachNonAlphaComponent(color, [&](float component) {
            return 1.0f - (oneMinusAmount + component * (m_amount - oneMinusAmount));
        });
        return true;
    }
    case CONTRAST: {
        float intercept = -(0.5f * m_amount) + 0.5f;
        color = colorByModifingEachNonAlphaComponent(color, [&](float component) {
            return std::clamp<float>(intercept + m_amount * component, 0.0f, 1.0f);
        });
        return true;
    }
    case BRIGHTNESS:
        color = colorByModifingEachNonAlphaComponent(color, [&](float component) {
            return std::clamp<float>(m_amount * component, 0.0f, 1.0f);
        });
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
    
RefPtr<FilterOperation> InvertLightnessFilterOperation::blend(const FilterOperation* from, const BlendingContext&, bool)
{
    if (from && !from->isSameType(*this))
        return this;

    // This filter is not currently blendable.
    return InvertLightnessFilterOperation::create();
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

bool InvertLightnessFilterOperation::transformColor(SRGBA<float>& color) const
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

bool InvertLightnessFilterOperation::inverseTransformColor(SRGBA<float>& color) const
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

bool BlurFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    
    return m_stdDeviation == downcast<BlurFilterOperation>(operation).stdDeviation();
}
    
RefPtr<FilterOperation> BlurFilterOperation::blend(const FilterOperation* from, const BlendingContext& context, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;

    LengthType lengthType = m_stdDeviation.type();

    if (blendToPassthrough)
        return BlurFilterOperation::create(WebCore::blend(m_stdDeviation, Length(lengthType), context));

    const BlurFilterOperation* fromOperation = downcast<BlurFilterOperation>(from);
    Length fromLength = fromOperation ? fromOperation->m_stdDeviation : Length(lengthType);
    return BlurFilterOperation::create(WebCore::blend(fromLength, m_stdDeviation, context, ValueRange::NonNegative));
}

bool BlurFilterOperation::isIdentity() const
{
    return floatValueForLength(m_stdDeviation, 0) <= 0;
}

IntOutsets BlurFilterOperation::outsets() const
{
    float stdDeviation = floatValueForLength(m_stdDeviation, 0);
    return FEGaussianBlur::calculateOutsets({ stdDeviation, stdDeviation });
}

bool DropShadowFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    const DropShadowFilterOperation& other = downcast<DropShadowFilterOperation>(operation);
    return m_location == other.m_location && m_stdDeviation == other.m_stdDeviation && m_color == other.m_color;
}
    
RefPtr<FilterOperation> DropShadowFilterOperation::blend(const FilterOperation* from, const BlendingContext& context, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;

    if (blendToPassthrough)
        return DropShadowFilterOperation::create(
            WebCore::blend(m_location, IntPoint(), context),
            WebCore::blend(m_stdDeviation, 0, context),
            WebCore::blend(m_color, Color::transparentBlack, context));

    const DropShadowFilterOperation* fromOperation = downcast<DropShadowFilterOperation>(from);
    IntPoint fromLocation = fromOperation ? fromOperation->location() : IntPoint();
    int fromStdDeviation = fromOperation ? fromOperation->stdDeviation() : 0;
    Color fromColor = fromOperation ? fromOperation->color() : Color::transparentBlack;
    
    return DropShadowFilterOperation::create(
        WebCore::blend(fromLocation, m_location, context),
        std::max(WebCore::blend(fromStdDeviation, m_stdDeviation, context), 0),
        WebCore::blend(fromColor, m_color, context));
}

bool DropShadowFilterOperation::isIdentity() const
{
    return m_stdDeviation < 0 || (!m_stdDeviation && m_location.isZero());
}

IntOutsets DropShadowFilterOperation::outsets() const
{
    return FEDropShadow::calculateOutsets(FloatSize(x(), y()), FloatSize(m_stdDeviation, m_stdDeviation));
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
