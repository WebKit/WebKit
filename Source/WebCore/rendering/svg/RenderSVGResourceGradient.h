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

#pragma once

#include "ImageBuffer.h"
#include "RenderSVGResourceContainer.h"
#include "SVGGradientElement.h"
#include <memory>
#include <wtf/HashMap.h>

namespace WebCore {

class GraphicsContext;

class RenderSVGResourceGradient : public RenderSVGResourceContainer {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGResourceGradient);
public:
    SVGGradientElement& gradientElement() const { return static_cast<SVGGradientElement&>(RenderSVGResourceContainer::element()); }

    void removeAllClientsFromCache(bool markForInvalidation = true) final;
    void removeClientFromCache(RenderElement&, bool markForInvalidation = true) final;

    bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>) final;
    void postApplyResource(RenderElement&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>, const Path*, const RenderSVGShape*) final;
    FloatRect resourceBoundingBox(const RenderObject&) final { return FloatRect(); }

protected:
    RenderSVGResourceGradient(SVGGradientElement&, RenderStyle&&);

    static void addStops(Gradient&, const Gradient::ColorStopVector&, const RenderStyle&);
    static GradientSpreadMethod platformSpreadMethodFromSVGType(SVGSpreadMethodType);

private:
    void element() const = delete;

    virtual SVGUnitTypes::SVGUnitType gradientUnits() const = 0;
    virtual AffineTransform gradientTransform() const = 0;
    virtual bool collectGradientAttributes() = 0;
    virtual Ref<Gradient> buildGradient(const RenderStyle&) const = 0;

    struct GradientData {
        RefPtr<Gradient> gradient;
        AffineTransform userspaceTransform;
    };
    HashMap<RenderObject*, GradientData> m_gradientMap;

#if USE(CG)
    GraphicsContext* m_savedContext { nullptr };
    std::unique_ptr<ImageBuffer> m_imageBuffer;
#endif

    bool m_shouldCollectGradientAttributes { true };
};

} // namespace WebCore
