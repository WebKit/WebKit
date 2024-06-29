/*
 * Copyright (C) 2020, 2021, 2022 Igalia S.L.
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

#include "RenderElementInlines.h"
#include "RenderLayerModelObject.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class SVGLayerTransformUpdater {
    WTF_MAKE_NONCOPYABLE(SVGLayerTransformUpdater);
public:
    SVGLayerTransformUpdater(RenderLayerModelObject& renderer)
        : m_renderer(renderer)
    {
        if (!m_renderer->hasLayer())
            return;

        m_transformReferenceBox = m_renderer->transformReferenceBoxRect();
        m_layerTransform = m_renderer->layerTransform();

        m_renderer->updateLayerTransform();
    }

    ~SVGLayerTransformUpdater()
    {
        if (!m_renderer->hasLayer())
            return;
        if (m_renderer->transformReferenceBoxRect() == m_transformReferenceBox)
            return;

        m_renderer->updateLayerTransform();
    }

    bool layerTransformChanged() const
    {
        auto* layerTransform = m_renderer->layerTransform();

        bool hasTransform = !!layerTransform;
        bool hadTransform = !!m_layerTransform;
        if (hasTransform != hadTransform)
            return true;

        return hasTransform && (*layerTransform != *m_layerTransform);
    }

private:
    SingleThreadWeakRef<RenderLayerModelObject> m_renderer;
    FloatRect m_transformReferenceBox;
    TransformationMatrix* m_layerTransform { nullptr };
};

} // namespace WebCore

