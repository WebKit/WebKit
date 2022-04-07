/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (c) 2020, 2021, 2022 Igalia S.L.
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

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "RenderSVGContainer.h"

namespace WebCore {

class SVGGraphicsElement;

class RenderSVGTransformableContainer final : public RenderSVGContainer {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGTransformableContainer);
public:
    RenderSVGTransformableContainer(SVGGraphicsElement&, RenderStyle&&);

    bool isSVGTransformableContainer() const final { return true; }
    void setHadTransformUpdate(bool value = true) { m_hadTransformUpdate = value; }
    bool didTransformToRootUpdate() const { return m_didTransformToRootUpdate; }

private:
    SVGGraphicsElement& graphicsElement() const;

    void element() const = delete;

    void layoutChildren() final;
    void calculateViewport() final;
    FloatPoint additionalContainerTranslation() const;
    void applyTransform(TransformationMatrix&, const RenderStyle&, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> = RenderStyle::allTransformOperations) const final;
    void styleWillChange(StyleDifference, const RenderStyle& newStyle) final;
    void updateFromStyle() final;

    AffineTransform m_supplementalLocalToParentTransform;
    bool m_didTransformToRootUpdate { false };
    bool m_hadTransformUpdate { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGTransformableContainer, isSVGTransformableContainer())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
