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
#include "LegacyRenderSVGResourceContainer.h"
#include "SVGGradientElement.h"
#include <memory>
#include <wtf/HashMap.h>

namespace WebCore {

class GraphicsContext;

struct GradientData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    struct Inputs {
        friend bool operator==(const Inputs&, const Inputs&) = default;

        std::optional<FloatRect> objectBoundingBox;
        float textPaintingScale = 1;
    };

    bool invalidate(const Inputs& inputs)
    {
        if (this->inputs != inputs) {
            gradient = nullptr;
            userspaceTransform = AffineTransform();
            this->inputs = inputs;
        }
        return !gradient;
    }

    RefPtr<Gradient> gradient;
    AffineTransform userspaceTransform;
    Inputs inputs;
};

class LegacyRenderSVGResourceGradient : public LegacyRenderSVGResourceContainer {
    WTF_MAKE_ISO_ALLOCATED(LegacyRenderSVGResourceGradient);
public:
    SVGGradientElement& gradientElement() const { return static_cast<SVGGradientElement&>(LegacyRenderSVGResourceContainer::element()); }

    void removeAllClientsFromCacheIfNeeded(bool markForInvalidation, SingleThreadWeakHashSet<RenderObject>* visitedRenderers) final;
    void removeClientFromCache(RenderElement&, bool markForInvalidation = true) final;

    bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>) final;
    void postApplyResource(RenderElement&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>, const Path*, const RenderElement*) final;
    FloatRect resourceBoundingBox(const RenderObject&, RepaintRectCalculation) final { return FloatRect(); }

protected:
    LegacyRenderSVGResourceGradient(Type, SVGGradientElement&, RenderStyle&&);

    static GradientColorStops stopsByApplyingColorFilter(const GradientColorStops&, const RenderStyle&);
    static GradientSpreadMethod platformSpreadMethodFromSVGType(SVGSpreadMethodType);

private:
    void element() const = delete;

    GradientData::Inputs computeInputs(RenderElement&, OptionSet<RenderSVGResourceMode>);

    virtual SVGUnitTypes::SVGUnitType gradientUnits() const = 0;
    virtual AffineTransform gradientTransform() const = 0;
    virtual bool collectGradientAttributes() = 0;
    virtual Ref<Gradient> buildGradient(const RenderStyle&) const = 0;

    HashMap<RenderObject*, std::unique_ptr<GradientData>> m_gradientMap;

#if USE(CG)
    GraphicsContext* m_savedContext { nullptr };
    RefPtr<ImageBuffer> m_imageBuffer;
#endif

    bool m_shouldCollectGradientAttributes { true };
};

} // namespace WebCore
