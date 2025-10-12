/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(GradientData);

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

class GradientApplier {
public:
    GradientApplier() = default;
    virtual ~GradientApplier() = default;

    virtual bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, const GradientData&, OptionSet<RenderSVGResourceMode>) = 0;
    virtual void postApplyResource(RenderElement&, GraphicsContext*&, const GradientData&, SVGUnitTypes::SVGUnitType gradientUnits, const AffineTransform& gradientTransform, OptionSet<RenderSVGResourceMode>, const Path*, const RenderElement*) = 0;
};

class LegacyRenderSVGResourceGradient : public LegacyRenderSVGResourceContainer {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(LegacyRenderSVGResourceGradient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LegacyRenderSVGResourceGradient);
public:
    virtual ~LegacyRenderSVGResourceGradient();

    SVGGradientElement& gradientElement() const { return static_cast<SVGGradientElement&>(LegacyRenderSVGResourceContainer::element()); }

    void removeAllClientsFromCache() final;
    void removeAllClientsFromCacheAndMarkForInvalidationIfNeeded(bool markForInvalidation, SingleThreadWeakHashSet<RenderObject>* visitedRenderers) override;
    void removeClientFromCache(RenderElement&) final;

    OptionSet<ApplyResult> applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>) final;
    void postApplyResource(RenderElement&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>, const Path*, const RenderElement*) final;
    FloatRect resourceBoundingBox(const RenderObject&, RepaintRectCalculation) final { return FloatRect(); }

protected:
    LegacyRenderSVGResourceGradient(Type, SVGGradientElement&, RenderStyle&&);

    static GradientColorStops stopsByApplyingColorFilter(const GradientColorStops&, const RenderStyle&);
    static GradientSpreadMethod platformSpreadMethodFromSVGType(SVGSpreadMethodType);

private:
    void element() const = delete;

    GradientData::Inputs computeInputs(RenderElement&, OptionSet<RenderSVGResourceMode>);
    GradientData* gradientDataForRenderer(RenderElement&, const RenderStyle&, OptionSet<RenderSVGResourceMode>);

    virtual SVGUnitTypes::SVGUnitType gradientUnits() const = 0;
    virtual AffineTransform gradientTransform() const = 0;
    virtual bool collectGradientAttributes() = 0;
    virtual Ref<Gradient> buildGradient(const RenderStyle&) const = 0;

    HashMap<RenderObject*, std::unique_ptr<GradientData>> m_gradientMap;

    std::unique_ptr<GradientApplier> m_gradientApplier;
    bool m_shouldCollectGradientAttributes { true };
};

} // namespace WebCore
