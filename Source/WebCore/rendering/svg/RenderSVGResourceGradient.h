/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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

#ifndef RenderSVGResourceGradient_h
#define RenderSVGResourceGradient_h

#if ENABLE(SVG)
#include "Gradient.h"
#include "ImageBuffer.h"
#include "RenderSVGResourceContainer.h"
#include "SVGGradientElement.h"

#include <wtf/HashMap.h>

namespace WebCore {

struct GradientData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RefPtr<Gradient> gradient;
    AffineTransform userspaceTransform;
};

class GraphicsContext;

class RenderSVGResourceGradient : public RenderSVGResourceContainer {
public:
    SVGGradientElement& gradientElement() const { return static_cast<SVGGradientElement&>(RenderSVGResourceContainer::element()); }

    virtual void removeAllClientsFromCache(bool markForInvalidation = true) override final;
    virtual void removeClientFromCache(RenderElement&, bool markForInvalidation = true) override final;

    virtual bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, unsigned short resourceMode) override final;
    virtual void postApplyResource(RenderElement&, GraphicsContext*&, unsigned short resourceMode, const Path*, const RenderSVGShape*) override final;
    virtual FloatRect resourceBoundingBox(const RenderObject&) override final { return FloatRect(); }

protected:
    RenderSVGResourceGradient(SVGGradientElement&, PassRef<RenderStyle>);

    void element() const = delete;

    void addStops(GradientData*, const Vector<Gradient::ColorStop>&) const;

    virtual SVGUnitTypes::SVGUnitType gradientUnits() const = 0;
    virtual void calculateGradientTransform(AffineTransform&) = 0;
    virtual bool collectGradientAttributes() = 0;
    virtual void buildGradient(GradientData*) const = 0;

    GradientSpreadMethod platformSpreadMethodFromSVGType(SVGSpreadMethodType) const;

private:
    bool m_shouldCollectGradientAttributes : 1;
    HashMap<RenderObject*, OwnPtr<GradientData>> m_gradientMap;

#if USE(CG)
    GraphicsContext* m_savedContext;
    std::unique_ptr<ImageBuffer> m_imageBuffer;
#endif
};

}

#endif
#endif
