/*
 * Copyright (C) 2020, 2021, 2022, 2026 Igalia S.L.
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

#include "RenderLayerModelObject.h"
#include <wtf/CheckedRef.h>

namespace WebCore {

class SVGTransformLayoutScope {
    WTF_MAKE_NONCOPYABLE(SVGTransformLayoutScope);
public:
    SVGTransformLayoutScope(RenderLayerModelObject& renderer)
        : m_renderer(renderer)
    {
        if (m_renderer->hasLayer() || m_renderer->transformReferenceBoxIsSVGViewport())
            m_transformReferenceBox = m_renderer->transformReferenceBoxRect();

        // Always call updateLayerTransform(), even without a layer. SVG renderers
        // (e.g. RenderSVGViewportContainer) compute supplemental transforms (viewBox,
        // zoom, pan) in their override, which are needed by applyTransform() during painting.
        m_renderer->updateLayerTransform();
    }

    ~SVGTransformLayoutScope()
    {
        if (m_renderer->transformReferenceBoxRect() == m_transformReferenceBox)
            return;

        m_renderer->updateLayerTransform();
    }

private:
    const CheckedRef<RenderLayerModelObject> m_renderer;
    std::optional<FloatRect> m_transformReferenceBox;
};

} // namespace WebCore

