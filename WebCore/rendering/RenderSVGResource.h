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
 *
 */

#ifndef RenderSVGResource_h
#define RenderSVGResource_h

#if ENABLE(SVG)
#include "SVGDocumentExtensions.h"

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

enum RenderSVGResourceMode {
    ApplyToDefaultMode = 1 << 0, // used for all resources except gradient/pattern
    ApplyToFillMode    = 1 << 1,
    ApplyToStrokeMode  = 1 << 2,
    ApplyToTextMode    = 1 << 3 // used in combination with ApplyTo{Fill|Stroke}Mode
};

class Color;
class FloatRect;
class GraphicsContext;
class RenderObject;
class RenderStyle;
class RenderSVGResourceSolidColor;

class RenderSVGResource {
public:
    RenderSVGResource() { }
    virtual ~RenderSVGResource() { }

    virtual void invalidateClients() = 0;
    virtual void invalidateClient(RenderObject*) = 0;

    virtual bool applyResource(RenderObject*, RenderStyle*, GraphicsContext*&, unsigned short resourceMode) = 0;
    virtual void postApplyResource(RenderObject*, GraphicsContext*&, unsigned short) { }
    virtual FloatRect resourceBoundingBox(const FloatRect&) const = 0;

    virtual RenderSVGResourceType resourceType() const = 0;

    template<class Renderer>
    Renderer* cast()
    {
        if (Renderer::s_resourceType == resourceType())
            return static_cast<Renderer*>(this);

        return 0;
    }

    // Helper utilities used in the render tree to access resources used for painting shapes/text (gradients & patterns only)
    static RenderSVGResource* fillPaintingResource(const RenderObject*, const RenderStyle*);
    static RenderSVGResource* strokePaintingResource(const RenderObject*, const RenderStyle*);
    static RenderSVGResourceSolidColor* sharedSolidPaintingResource();

private:
    static void adjustColorForPseudoRules(const RenderStyle*, bool useFillPaint, Color&);
    
protected:
    void markForLayoutAndResourceInvalidation(RenderObject*);
};

}

#endif
#endif
