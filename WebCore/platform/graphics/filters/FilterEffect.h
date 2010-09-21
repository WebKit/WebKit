/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef FilterEffect_h
#define FilterEffect_h

#if ENABLE(FILTERS)
#include "Filter.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "RenderTreeAsText.h"
#include "TextStream.h"

#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

typedef Vector<RefPtr<FilterEffect> > FilterEffectVector;

class FilterEffect : public RefCounted<FilterEffect> {
public:
    virtual ~FilterEffect();

    // The result is bounded to the size of the filter primitive to save resources.
    ImageBuffer* resultImage() const { return m_effectBuffer.get(); }
    void setEffectBuffer(PassOwnPtr<ImageBuffer> effectBuffer) { m_effectBuffer = effectBuffer; }

    // Creates the ImageBuffer for the current filter primitive result in the size of the
    // repaintRect. Gives back the GraphicsContext of the own ImageBuffer.
    GraphicsContext* effectContext();

    FilterEffectVector& inputEffects() { return m_inputEffects; }
    FilterEffect* inputEffect(unsigned) const;
    unsigned numberOfEffectInputs() const { return m_inputEffects.size(); }

    FloatRect drawingRegionOfInputImage(const FloatRect&) const;
    IntRect requestedRegionOfInputImageData(const FloatRect&) const;

    // Solid black image with different alpha values.
    bool isAlphaImage() const { return m_alphaImage; }
    void setIsAlphaImage(bool alphaImage) { m_alphaImage = alphaImage; }

    FloatRect repaintRectInLocalCoordinates() const { return m_repaintRectInLocalCoordinates; }
    void setRepaintRectInLocalCoordinates(const FloatRect& repaintRectInLocalCoordinates) { m_repaintRectInLocalCoordinates = repaintRectInLocalCoordinates; }

    virtual void apply(Filter*) = 0;
    virtual void dump() = 0;

    virtual bool isSourceInput() const { return false; }

    virtual TextStream& externalRepresentation(TextStream&, int indention = 0) const;

public:
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

    // FIXME: Pseudo primitives like SourceGraphic and SourceAlpha as well as FETile still need special handling.
    virtual FloatRect determineFilterPrimitiveSubregion(Filter*);

    FloatRect filterPrimitiveSubregion() const { return m_filterPrimitiveSubregion; }
    void setFilterPrimitiveSubregion(const FloatRect& filterPrimitiveSubregion) { m_filterPrimitiveSubregion = filterPrimitiveSubregion; }

    FloatRect effectBoundaries() const { return m_effectBoundaries; }
    void setEffectBoundaries(const FloatRect& effectBoundaries) { m_effectBoundaries = effectBoundaries; }

protected:
    FilterEffect();

private:
    OwnPtr<ImageBuffer> m_effectBuffer;
    FilterEffectVector m_inputEffects;

    bool m_alphaImage;

    // FIXME: Should be the paint region of the filter primitive, instead of the scaled subregion on use of filterRes.
    FloatRect m_repaintRectInLocalCoordinates;

private:
    // The following member variables are SVG specific and will move to RenderSVGResourceFilterPrimitive.
    // See bug https://bugs.webkit.org/show_bug.cgi?id=45614.

    // The subregion of a filter primitive according to the SVG Filter specification in local coordinates.
    // This is SVG specific and needs to move to RenderSVGResourceFilterPrimitive.
    FloatRect m_filterPrimitiveSubregion;

    // x, y, width and height of the actual SVGFE*Element. Is needed to determine the subregion of the
    // filter primitive on a later step.
    FloatRect m_effectBoundaries;
    bool m_hasX;
    bool m_hasY;
    bool m_hasWidth;
    bool m_hasHeight;
};

} // namespace WebCore

#endif // ENABLE(FILTERS)

#endif // FilterEffect_h
