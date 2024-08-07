/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "LegacyRenderSVGResourceContainer.h"
#include "SVGUnitTypes.h"

#include <wtf/EnumeratedArray.h>
#include <wtf/HashMap.h>

namespace WebCore {

class GraphicsContext;
class ImageBuffer;
class SVGClipPathElement;

struct ClipperData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    struct Inputs {
        bool operator==(const Inputs& other) const = default;

        FloatRect objectBoundingBox;
        FloatRect clippedContentBounds;
        FloatSize scale;
        float usedZoom = 1;
        bool paintingDisabled { false };
    };

    bool invalidate(const Inputs& inputs)
    {
        if (this->inputs != inputs) {
            imageBuffer = nullptr;
            this->inputs = inputs;
        }
        return !imageBuffer;
    }

    RefPtr<ImageBuffer> imageBuffer;
    Inputs inputs;
};

class LegacyRenderSVGResourceClipper final : public LegacyRenderSVGResourceContainer {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(LegacyRenderSVGResourceClipper);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LegacyRenderSVGResourceClipper);
public:
    LegacyRenderSVGResourceClipper(SVGClipPathElement&, RenderStyle&&);
    virtual ~LegacyRenderSVGResourceClipper();

    inline SVGClipPathElement& clipPathElement() const;
    inline Ref<SVGClipPathElement> protectedClipPathElement() const;

    void removeAllClientsFromCacheIfNeeded(bool markForInvalidation, SingleThreadWeakHashSet<RenderObject>* visitedRenderers) override;
    void removeClientFromCache(RenderElement&, bool markForInvalidation = true) override;

    OptionSet<ApplyResult> applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>) override;

    // clipPath can be clipped too, but don't have a boundingBox or repaintRect. So we can't call
    // applyResource directly and use the rects from the object, since they are empty for RenderSVGResources
    // FIXME: We made applyClippingToContext public because we cannot call applyResource on HTML elements (it asserts on RenderObject::objectBoundingBox)
    // objectBoundingBox ia used to compute clip path geometry when clipPathUnits="objectBoundingBox".
    // clippedContentBounds is the bounds of the content to which clipping is being applied.
    OptionSet<ApplyResult> applyClippingToContext(GraphicsContext&, RenderElement&, const FloatRect& objectBoundingBox, const FloatRect& clippedContentBounds, float usedZoom = 1);
    FloatRect resourceBoundingBox(const RenderObject&, RepaintRectCalculation) override;

    RenderSVGResourceType resourceType() const override { return ClipperResourceType; }
    
    bool hitTestClipContent(const FloatRect&, const FloatPoint&);

    inline SVGUnitTypes::SVGUnitType clipPathUnits() const;

private:
    bool selfNeedsClientInvalidation() const override { return (everHadLayout() || m_clipperMap.size()) && selfNeedsLayout(); }

    void element() const = delete;

    ASCIILiteral renderName() const override { return "RenderSVGResourceClipper"_s; }

    ClipperData::Inputs computeInputs(const GraphicsContext&, const RenderElement&, const FloatRect& objectBoundingBox, const FloatRect& clippedContentBounds, float usedZoom);
    OptionSet<ApplyResult> pathOnlyClipping(GraphicsContext&, const RenderElement&, const AffineTransform&, const FloatRect&, float usedZoom);
    bool drawContentIntoMaskImage(ImageBuffer&, const FloatRect& objectBoundingBox, float usedZoom);
    void calculateClipContentRepaintRect(RepaintRectCalculation);

    EnumeratedArray<RepaintRectCalculation, FloatRect, RepaintRectCalculation::Accurate> m_clipBoundaries;
    HashMap<SingleThreadWeakRef<const RenderObject>, std::unique_ptr<ClipperData>> m_clipperMap;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::LegacyRenderSVGResourceClipper)
static bool isType(const WebCore::RenderObject& renderer) { return renderer.isLegacyRenderSVGResourceClipper(); }
static bool isType(const WebCore::LegacyRenderSVGResource& resource) { return resource.resourceType() == WebCore::ClipperResourceType; }
SPECIALIZE_TYPE_TRAITS_END()
