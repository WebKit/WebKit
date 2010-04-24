/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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
 *
 */

#ifndef RenderSVGResourcePattern_h
#define RenderSVGResourcePattern_h

#if ENABLE(SVG)
#include "AffineTransform.h"
#include "FloatRect.h"
#include "ImageBuffer.h"
#include "Pattern.h"
#include "RenderSVGResourceContainer.h"
#include "SVGPatternElement.h"
#include "SVGUnitTypes.h"

#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

struct PatternData {
    RefPtr<Pattern> pattern;
};

struct PatternAttributes;

class RenderSVGResourcePattern : public RenderSVGResourceContainer {
public:
    RenderSVGResourcePattern(SVGPatternElement*);
    virtual ~RenderSVGResourcePattern();

    virtual const char* renderName() const { return "RenderSVGResourcePattern"; }

    virtual void invalidateClients();
    virtual void invalidateClient(RenderObject*);

    virtual bool applyResource(RenderObject*, RenderStyle*, GraphicsContext*&, unsigned short resourceMode);
    virtual void postApplyResource(RenderObject*, GraphicsContext*&, unsigned short resourceMode);
    virtual FloatRect resourceBoundingBox(const FloatRect&) const { return FloatRect(); }

    virtual RenderSVGResourceType resourceType() const { return s_resourceType; }
    static RenderSVGResourceType s_resourceType;

private:
    PassOwnPtr<ImageBuffer> createTileImage(FloatRect& patternBoundaries, AffineTransform& patternTransform, const SVGPatternElement*, RenderObject*) const;
    void buildPattern(PatternData*, const FloatRect& patternBoundaries, PassOwnPtr<ImageBuffer> tileImage) const;
    FloatRect calculatePatternBoundariesIncludingOverflow(PatternAttributes&, const FloatRect& objectBoundingBox,
                                                          const AffineTransform& viewBoxCTM, const FloatRect& patternBoundaries) const;

    HashMap<RenderObject*, PatternData*> m_pattern;
};

}

#endif
#endif
