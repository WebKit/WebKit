/**
 * Copyright (C) 2021 Igalia S.L.
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
#include "RenderLayerModelObject.h"

#include <functional>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {

class SVGContainerLayout {
    WTF_MAKE_NONCOPYABLE(SVGContainerLayout);
public:
    explicit SVGContainerLayout(RenderLayerModelObject&);
    ~SVGContainerLayout() = default;

    // 'containerNeedsLayout' denotes if the container for which the
    // SVGContainerLayout object was created needs to be laid out or not.
    void layoutChildren(bool containerNeedsLayout);

    void positionChildrenRelativeToContainer();

    static void verifyLayoutLocationConsistency(const RenderLayerModelObject&);
    static bool transformToRootChanged(const RenderObject* ancestor);

private:
    bool layoutSizeOfNearestViewportChanged() const;

    RenderLayerModelObject& m_container;
    Vector<std::reference_wrapper<RenderLayerModelObject>> m_positionedChildren;
};

} // namespace WebCore

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
