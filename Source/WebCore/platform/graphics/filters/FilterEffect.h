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

#include "DestinationColorSpace.h"
#include "FilterEffectVector.h"
#include "FilterFunction.h"
#include "FilterImage.h"
#include "FilterImageVector.h"
#include <wtf/Vector.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class Filter;
class FilterEffectApplier;

class FilterEffect : public FilterFunction {
public:
    void clearResult() override;
    void clearResultsRecursive();
    bool hasResult() const { return m_filterImage; }

    FilterImage* filterImage() const { return m_filterImage.get(); }
    FilterImageVector inputFilterImages() const;

    void correctPremultipliedResultIfNeeded();

    FilterEffectVector& inputEffects() { return m_inputEffects; }
    FilterEffect* inputEffect(unsigned) const;
    unsigned numberOfEffectInputs() const { return m_inputEffects.size(); }

    // Recurses on inputs.
    FloatRect determineFilterPrimitiveSubregion(const Filter&);

    bool apply(const Filter&) override;

    // Correct any invalid pixels, if necessary, in the result of a filter operation.
    // This method is used to ensure valid pixel values on filter inputs and the final result.
    // Only the arithmetic composite filter ever needs to perform correction.
    virtual void correctFilterResultIfNeeded() { }

    enum class RepresentationType { TestOutput, Debugging };
    virtual WTF::TextStream& externalRepresentation(WTF::TextStream&, RepresentationType = RepresentationType::TestOutput) const;

    FloatRect filterPrimitiveSubregion() const { return m_filterPrimitiveSubregion; }
    void setFilterPrimitiveSubregion(const FloatRect& filterPrimitiveSubregion) { m_filterPrimitiveSubregion = filterPrimitiveSubregion; }

    virtual FloatRect calculateImageRect(const Filter&, const FilterImageVector& inputs, const FloatRect& primitiveSubregion) const;

    const DestinationColorSpace& operatingColorSpace() const { return m_operatingColorSpace; }
    virtual void setOperatingColorSpace(const DestinationColorSpace& colorSpace) { m_operatingColorSpace = colorSpace; }

    // Solid black image with different alpha values.
    virtual bool resultIsAlphaImage(const FilterImageVector&) const { return false; }
    virtual const DestinationColorSpace& resultColorSpace() const { return m_operatingColorSpace; }

    virtual void transformResultColorSpace(FilterEffect* in, const int) { in->transformResultColorSpace(m_operatingColorSpace); }
    void transformResultColorSpace(const DestinationColorSpace&);

protected:
    using FilterFunction::FilterFunction;

    virtual bool mayProduceInvalidPremultipliedPixels() const { return false; }
    
    virtual std::unique_ptr<FilterEffectApplier> createApplier(const Filter&) const = 0;

private:
    FilterEffectVector m_inputEffects;

    RefPtr<FilterImage> m_filterImage;

    // The subregion of a filter primitive according to the SVG Filter specification in local coordinates.
    // This is SVG specific and needs to move to RenderSVGResourceFilterPrimitive.
    FloatRect m_filterPrimitiveSubregion;

    DestinationColorSpace m_operatingColorSpace { DestinationColorSpace::SRGB() };
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const FilterEffect&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::FilterEffect)
    static bool isType(const WebCore::FilterFunction& function) { return function.isFilterEffect(); }
SPECIALIZE_TYPE_TRAITS_END()

#define SPECIALIZE_TYPE_TRAITS_FILTER_EFFECT(ClassName) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ClassName) \
    static bool isType(const WebCore::FilterEffect& effect) { return effect.filterType() == WebCore::FilterEffect::Type::ClassName; } \
SPECIALIZE_TYPE_TRAITS_END()
