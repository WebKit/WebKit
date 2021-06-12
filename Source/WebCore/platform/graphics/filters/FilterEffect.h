/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "AlphaPremultiplication.h"
#include "DestinationColorSpace.h"
#include "FloatRect.h"
#include "IntRect.h"
#include "IntRectExtent.h"
#include "PixelBuffer.h"
#include <JavaScriptCore/Uint8ClampedArray.h>
#include <wtf/MathExtras.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class Filter;
class FilterEffect;
class ImageBuffer;

typedef Vector<RefPtr<FilterEffect>> FilterEffectVector;

enum FilterEffectType {
    FilterEffectTypeUnknown,
    FilterEffectTypeImage,
    FilterEffectTypeTile,
    FilterEffectTypeSourceInput
};

class FilterEffect : public RefCounted<FilterEffect> {
public:
    enum class Type : uint8_t {
        Blend,
        ColorMatrix,
        ComponentTransfer,
        Composite,
        ConvolveMatrix,
        DiffuseLighting,
        DisplacementMap,
        DropShadow,
        Flood,
        GaussianBlur,
        Image,
        Lighting,
        Merge,
        Morphology,
        Offset,
        SpecularLighting,
        Tile,
        Turbulence,
        SourceAlpha,
        SourceGraphic
    };
    virtual ~FilterEffect();

    void clearResult();
    void clearResultsRecursive();

    ImageBuffer* imageBufferResult();
    RefPtr<Uint8ClampedArray> unmultipliedResult(const IntRect&, std::optional<DestinationColorSpace> = std::nullopt);
    RefPtr<Uint8ClampedArray> premultipliedResult(const IntRect&, std::optional<DestinationColorSpace> = std::nullopt);
    void copyUnmultipliedResult(Uint8ClampedArray& destination, const IntRect&, std::optional<DestinationColorSpace> = std::nullopt);
    void copyPremultipliedResult(Uint8ClampedArray& destination, const IntRect&, std::optional<DestinationColorSpace> = std::nullopt);
    FilterEffectVector& inputEffects() { return m_inputEffects; }
    FilterEffect* inputEffect(unsigned) const;
    unsigned numberOfEffectInputs() const { return m_inputEffects.size(); }
    unsigned totalNumberOfEffectInputs() const;
    
    inline bool hasResult() const
    {
        // This function needs platform specific checks, if the memory managment is not done by FilterEffect.
        return m_imageBufferResult
            || m_unmultipliedImageResult
            || m_premultipliedImageResult;
    }

    FloatRect drawingRegionOfInputImage(const IntRect&) const;
    IntRect requestedRegionOfInputPixelBuffer(const IntRect&) const;
    
    // Recurses on inputs.
    FloatRect determineFilterPrimitiveSubregion();

    // Solid black image with different alpha values.
    bool isAlphaImage() const { return m_alphaImage; }
    void setIsAlphaImage(bool alphaImage) { m_alphaImage = alphaImage; }

    IntRect absolutePaintRect() const { return m_absolutePaintRect; }
    void setAbsolutePaintRect(const IntRect& absolutePaintRect) { m_absolutePaintRect = absolutePaintRect; }

    FloatRect maxEffectRect() const { return m_maxEffectRect; }
    void setMaxEffectRect(const FloatRect& maxEffectRect) { m_maxEffectRect = maxEffectRect; } 

    void apply();

    // Correct any invalid pixels, if necessary, in the result of a filter operation.
    // This method is used to ensure valid pixel values on filter inputs and the final result.
    // Only the arithmetic composite filter ever needs to perform correction.
    virtual void correctFilterResultIfNeeded() { }

    virtual void determineAbsolutePaintRect();

    virtual FilterEffectType filterEffectType() const { return FilterEffectTypeUnknown; }

    virtual IntOutsets outsets() const { return IntOutsets(); }

    enum class RepresentationType { TestOutput, Debugging };
    virtual WTF::TextStream& externalRepresentation(WTF::TextStream&, RepresentationType = RepresentationType::TestOutput) const;

    // The following functions are SVG specific and will move to RenderSVGResourceFilterPrimitive.
    // See bug https://bugs.webkit.org/show_bug.cgi?id=45614.
    bool hasX() const { return m_hasX; }
    void setHasX(bool value) { m_hasX = value; }

    bool hasY() const { return m_hasY; }
    void setHasY(bool value) { m_hasY = value; }

    bool hasWidth() const { return m_hasWidth; }
    void setHasWidth(bool value) { m_hasWidth = value; }

    bool hasHeight() const { return m_hasHeight; }
    void setHasHeight(bool value) { m_hasHeight = value; }

    FloatRect filterPrimitiveSubregion() const { return m_filterPrimitiveSubregion; }
    void setFilterPrimitiveSubregion(const FloatRect& filterPrimitiveSubregion) { m_filterPrimitiveSubregion = filterPrimitiveSubregion; }

    FloatRect effectBoundaries() const { return m_effectBoundaries; }
    void setEffectBoundaries(const FloatRect& effectBoundaries) { m_effectBoundaries = effectBoundaries; }
    
    void setUnclippedAbsoluteSubregion(const FloatRect& r) { m_absoluteUnclippedSubregion = r; }
    
    FloatPoint mapPointFromUserSpaceToBuffer(FloatPoint) const;
    
    Type filterEffectClassType() const { return m_filterEffectClassType; }

    Filter& filter() { return m_filter; }
    const Filter& filter() const { return m_filter; }

    bool clipsToBounds() const { return m_clipsToBounds; }
    void setClipsToBounds(bool value) { m_clipsToBounds = value; }

    const DestinationColorSpace& operatingColorSpace() const { return m_operatingColorSpace; }
    virtual void setOperatingColorSpace(const DestinationColorSpace& colorSpace) { m_operatingColorSpace = colorSpace; }
    const DestinationColorSpace& resultColorSpace() const { return m_resultColorSpace; }
    virtual void setResultColorSpace(const DestinationColorSpace& colorSpace) { m_resultColorSpace = colorSpace; }

    virtual void transformResultColorSpace(FilterEffect* in, const int) { in->transformResultColorSpace(m_operatingColorSpace); }
    void transformResultColorSpace(const DestinationColorSpace&);
    
    static Vector<float> normalizedFloats(const Vector<float>& values)
    {
        Vector<float> normalizedValues(values.size());
        for (size_t i = 0; i < values.size(); ++i)
            normalizedValues[i] = normalizedFloat(values[i]);
        return normalizedValues;
    }

protected:
    FilterEffect(Filter&, Type);
    
    virtual const char* filterName() const = 0;

    ImageBuffer* createImageBufferResult();
    std::optional<PixelBuffer>& createUnmultipliedImageResult();
    std::optional<PixelBuffer>& createPremultipliedImageResult();

    // Return true if the filter will only operate correctly on valid RGBA values, with
    // alpha in [0,255] and each color component in [0, alpha].
    virtual bool requiresValidPreMultipliedPixels() { return true; }

    // If a pre-multiplied image, check every pixel for validity and correct if necessary.
    void forceValidPreMultipliedPixels();

    void clipAbsolutePaintRect();

private:
    virtual void platformApplySoftware() = 0;

    void copyImageBytes(const Uint8ClampedArray& source, Uint8ClampedArray& destination, const IntRect&) const;
    void copyConvertedImageBufferToDestination(Uint8ClampedArray&, const DestinationColorSpace&, AlphaPremultiplication, const IntRect&);
    void copyConvertedPixelBufferToDestination(Uint8ClampedArray&, PixelBuffer&, const DestinationColorSpace&, const IntRect&);
    bool requiresPixelBufferColorSpaceConversion(std::optional<DestinationColorSpace>);
    std::optional<PixelBuffer> convertImageBufferToColorSpace(const DestinationColorSpace&, ImageBuffer&, const IntRect&, AlphaPremultiplication);
    std::optional<PixelBuffer> convertPixelBufferToColorSpace(const DestinationColorSpace&, PixelBuffer&);
    

    Filter& m_filter;
    FilterEffectVector m_inputEffects;

    RefPtr<ImageBuffer> m_imageBufferResult;
    std::optional<PixelBuffer> m_unmultipliedImageResult;
    std::optional<PixelBuffer> m_premultipliedImageResult;

    IntRect m_absolutePaintRect;
    
    // The maximum size of a filter primitive. In SVG this is the primitive subregion in absolute coordinate space.
    // The absolute paint rect should never be bigger than m_maxEffectRect.
    FloatRect m_maxEffectRect;
    
    // The subregion of a filter primitive according to the SVG Filter specification in local coordinates.
    // This is SVG specific and needs to move to RenderSVGResourceFilterPrimitive.
    FloatRect m_filterPrimitiveSubregion;

    // x, y, width and height of the actual SVGFE*Element. Is needed to determine the subregion of the
    // filter primitive on a later step.
    FloatRect m_effectBoundaries;
    
    // filterPrimitiveSubregion mapped to absolute coordinates before clipping.
    FloatRect m_absoluteUnclippedSubregion;

    bool m_alphaImage { false };
    bool m_hasX { false };
    bool m_hasY { false };
    bool m_hasWidth { false };
    bool m_hasHeight { false };

    // Should the effect clip to its primitive region, or expand to use the combined region of its inputs.
    bool m_clipsToBounds { true };

#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
    DestinationColorSpace m_operatingColorSpace { DestinationColorSpace::LinearSRGB() };
#else
    DestinationColorSpace m_operatingColorSpace { DestinationColorSpace::SRGB() };
#endif
    DestinationColorSpace m_resultColorSpace { DestinationColorSpace::SRGB() };
    
    const Type m_filterEffectClassType;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const FilterEffect&);

} // namespace WebCore

