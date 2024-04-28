/*
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

#include "RenderStyleConstants.h"
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/WeakRef.h>

namespace WebCore {

class LegacyRenderSVGResourceContainer;
class RenderElement;
class RenderObject;
class RenderStyle;
class SVGResources;

class SVGResourcesCache {
    WTF_MAKE_NONCOPYABLE(SVGResourcesCache); WTF_MAKE_FAST_ALLOCATED;
public:
    SVGResourcesCache();
    ~SVGResourcesCache();

    static SVGResources* cachedResourcesForRenderer(const RenderElement&);

    // Called from all SVG renderers addChild() methods.
    static void clientWasAddedToTree(RenderObject&);

    // Called from all SVG renderers removeChild() methods.
    static void clientWillBeRemovedFromTree(RenderObject&);

    // Called from all SVG renderers destroy() methods - except for LegacyRenderSVGResourceContainer.
    static void clientDestroyed(RenderElement&);

    // Called from all SVG renderers layout() methods.
    static void clientLayoutChanged(RenderElement&);

    // Called from all SVG renderers styleDidChange() methods.
    static void clientStyleChanged(RenderElement&, StyleDifference, const RenderStyle* oldStyle, const RenderStyle& newStyle);

    // Called from LegacyRenderSVGResourceContainer::willBeDestroyed().
    static void resourceDestroyed(LegacyRenderSVGResourceContainer&);

    class SetStyleForScope {
        WTF_MAKE_NONCOPYABLE(SetStyleForScope);
    public:
        SetStyleForScope(RenderElement&, const RenderStyle& scopedStyle, const RenderStyle& newStyle);
        ~SetStyleForScope();
    private:
        void setStyle(const RenderStyle&);

        RenderElement& m_renderer;
        const RenderStyle& m_scopedStyle;
        bool m_needsNewStyle { false };
    };

private:
    void addResourcesFromRenderer(RenderElement&, const RenderStyle&);
    void removeResourcesFromRenderer(RenderElement&);

    using CacheMap = HashMap<SingleThreadWeakRef<const RenderElement>, std::unique_ptr<SVGResources>>;
    CacheMap m_cache;
};

} // namespace WebCore
