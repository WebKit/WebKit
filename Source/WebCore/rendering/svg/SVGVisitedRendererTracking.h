/*
 * Copyright (c) 2024 Igalia S.L.
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

#include "RenderElement.h"
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class SVGVisitedRendererTracking {
public:
    using VisitedSet = SingleThreadWeakHashSet<RenderElement>;

    SVGVisitedRendererTracking(VisitedSet& visitedSet)
        : m_visitedRenderers(visitedSet)
    {
    }

    ~SVGVisitedRendererTracking() = default;

    bool isEmpty() const { return m_visitedRenderers.isEmptyIgnoringNullReferences(); }
    bool isVisiting(const RenderElement& renderer) { return m_visitedRenderers.contains(renderer); }

    class Scope {
    public:
        Scope(SVGVisitedRendererTracking& tracking, const RenderElement& renderer)
            : m_tracking(tracking)
            , m_renderer(renderer)
        {
            m_tracking.addUnique(renderer);
        }

        ~Scope()
        {
            if (m_renderer)
                m_tracking.removeUnique(*m_renderer);
        }

    private:
        SVGVisitedRendererTracking& m_tracking;
        SingleThreadWeakPtr<RenderElement> m_renderer;
    };

private:
    friend class Scope;

    void addUnique(const RenderElement& renderer)
    {
        auto result = m_visitedRenderers.add(renderer);
        ASSERT_UNUSED(result, result.isNewEntry);
    }

    void removeUnique(const RenderElement& renderer)
    {
        bool result = m_visitedRenderers.remove(renderer);
        ASSERT_UNUSED(result, result);
    }

    VisitedSet& m_visitedRenderers;
};

}; // namespace WebCore
