/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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
#include <wtf/OptionSet.h>
#include <wtf/TypeCasts.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

enum RenderSVGResourceType {
    MaskerResourceType,
    MarkerResourceType,
    PatternResourceType,
    LinearGradientResourceType,
    RadialGradientResourceType,
    SolidColorResourceType,
    FilterResourceType,
    ClipperResourceType
};

// If this enum changes change the unsigned bitfields using it.
enum class RenderSVGResourceMode {
    ApplyToFill    = 1 << 0,
    ApplyToStroke  = 1 << 1,
    ApplyToText    = 1 << 2 // used in combination with ApplyTo{Fill|Stroke}Mode
};

enum class RepaintRectCalculation : bool;

class Color;
class FloatRect;
class GraphicsContext;
class LegacyRenderSVGResourceSolidColor;
class Path;
class RenderElement;
class RenderObject;
class RenderStyle;

class LegacyRenderSVGResource {
public:
    LegacyRenderSVGResource() = default;
    virtual ~LegacyRenderSVGResource() = default;

    void removeAllClientsFromCache(bool markForInvalidation = true);
    virtual void removeAllClientsFromCacheIfNeeded(bool markForInvalidation, SingleThreadWeakHashSet<RenderObject>* visitedRenderers) = 0;
    virtual void removeClientFromCache(RenderElement&, bool markForInvalidation = true) = 0;

    virtual bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>) = 0;
    virtual void postApplyResource(RenderElement&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>, const Path*, const RenderElement* /* shape */) { }
    virtual FloatRect resourceBoundingBox(const RenderObject&, RepaintRectCalculation) = 0;

    virtual RenderSVGResourceType resourceType() const = 0;

    // Helper utilities used in the render tree to access resources used for painting shapes/text (gradients & patterns & solid colors only)
    static LegacyRenderSVGResource* fillPaintingResource(RenderElement&, const RenderStyle&, Color& fallbackColor);
    static LegacyRenderSVGResource* strokePaintingResource(RenderElement&, const RenderStyle&, Color& fallbackColor);
    static LegacyRenderSVGResourceSolidColor* sharedSolidPaintingResource();

    static void markForLayoutAndParentResourceInvalidation(RenderObject&, bool needsLayout = true);
    static void markForLayoutAndParentResourceInvalidationIfNeeded(RenderObject&, bool needsLayout, SingleThreadWeakHashSet<RenderObject>* visitedRenderers);

protected:
    void fillAndStrokePathOrShape(GraphicsContext&, OptionSet<RenderSVGResourceMode>, const Path*, const RenderElement* shape) const;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_LEGACY_RENDER_SVG_RESOURCE(ToValueTypeName, ResourceType) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::LegacyRenderSVGResource& resource) { return resource.resourceType() == WebCore::ResourceType; } \
SPECIALIZE_TYPE_TRAITS_END()
