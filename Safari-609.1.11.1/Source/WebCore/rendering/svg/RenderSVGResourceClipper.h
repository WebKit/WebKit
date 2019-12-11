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

#include "RenderSVGResourceContainer.h"
#include "SVGClipPathElement.h"
#include "SVGUnitTypes.h"

#include <wtf/HashMap.h>

namespace WebCore {

class GraphicsContext;
class ImageBuffer;

typedef std::unique_ptr<ImageBuffer> ClipperMaskImage;

class RenderSVGResourceClipper final : public RenderSVGResourceContainer {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGResourceClipper);
public:
    RenderSVGResourceClipper(SVGClipPathElement&, RenderStyle&&);
    virtual ~RenderSVGResourceClipper();

    SVGClipPathElement& clipPathElement() const { return downcast<SVGClipPathElement>(nodeForNonAnonymous()); }

    void removeAllClientsFromCache(bool markForInvalidation = true) override;
    void removeClientFromCache(RenderElement&, bool markForInvalidation = true) override;

    bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>) override;
    // clipPath can be clipped too, but don't have a boundingBox or repaintRect. So we can't call
    // applyResource directly and use the rects from the object, since they are empty for RenderSVGResources
    // FIXME: We made applyClippingToContext public because we cannot call applyResource on HTML elements (it asserts on RenderObject::objectBoundingBox)
    bool applyClippingToContext(RenderElement&, const FloatRect&, const FloatRect&, GraphicsContext&);
    FloatRect resourceBoundingBox(const RenderObject&) override;

    RenderSVGResourceType resourceType() const override { return ClipperResourceType; }
    
    bool hitTestClipContent(const FloatRect&, const FloatPoint&);

    SVGUnitTypes::SVGUnitType clipPathUnits() const { return clipPathElement().clipPathUnits(); }

protected:
    bool selfNeedsClientInvalidation() const override { return (everHadLayout() || m_clipper.size()) && selfNeedsLayout(); }

private:
    void element() const = delete;

    const char* renderName() const override { return "RenderSVGResourceClipper"; }
    bool isSVGResourceClipper() const override { return true; }

    bool pathOnlyClipping(GraphicsContext&, const AffineTransform&, const FloatRect&);
    bool drawContentIntoMaskImage(const ClipperMaskImage&, const FloatRect& objectBoundingBox);
    void calculateClipContentRepaintRect();
    ClipperMaskImage& addRendererToClipper(const RenderObject&);

    FloatRect m_clipBoundaries;
    HashMap<const RenderObject*, ClipperMaskImage> m_clipper;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::RenderSVGResourceClipper)
static bool isType(const WebCore::RenderObject& renderer) { return renderer.isSVGResourceClipper(); }
static bool isType(const WebCore::RenderSVGResource& resource) { return resource.resourceType() == WebCore::ClipperResourceType; }
SPECIALIZE_TYPE_TRAITS_END()
