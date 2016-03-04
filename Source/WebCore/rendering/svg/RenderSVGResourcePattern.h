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
 */

#ifndef RenderSVGResourcePattern_h
#define RenderSVGResourcePattern_h

#include "ImageBuffer.h"
#include "Pattern.h"
#include "PatternAttributes.h"
#include "RenderSVGResourceContainer.h"
#include "SVGPatternElement.h"
#include "SVGUnitTypes.h"
#include <memory>
#include <wtf/HashMap.h>

namespace WebCore {

struct PatternData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RefPtr<Pattern> pattern;
    AffineTransform transform;
};

class RenderSVGResourcePattern final : public RenderSVGResourceContainer {
public:
    RenderSVGResourcePattern(SVGPatternElement&, Ref<RenderStyle>&&);
    SVGPatternElement& patternElement() const;

    void removeAllClientsFromCache(bool markForInvalidation = true) override;
    void removeClientFromCache(RenderElement&, bool markForInvalidation = true) override;

    bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, unsigned short resourceMode) override;
    void postApplyResource(RenderElement&, GraphicsContext*&, unsigned short resourceMode, const Path*, const RenderSVGShape*) override;
    FloatRect resourceBoundingBox(const RenderObject&) override { return FloatRect(); }

    RenderSVGResourceType resourceType() const override { return PatternResourceType; }

    void collectPatternAttributes(PatternAttributes&) const;

private:
    void element() const = delete;
    const char* renderName() const override { return "RenderSVGResourcePattern"; }

    bool buildTileImageTransform(RenderElement&, const PatternAttributes&, const SVGPatternElement&, FloatRect& patternBoundaries, AffineTransform& tileImageTransform) const;

    std::unique_ptr<ImageBuffer> createTileImage(const PatternAttributes&, const FloatRect& tileBoundaries, const FloatRect& absoluteTileBoundaries, const AffineTransform& tileImageTransform, FloatRect& clampedAbsoluteTileBoundaries, RenderingMode) const;

    PatternData* buildPattern(RenderElement&, unsigned short resourceMode, GraphicsContext&);

    bool m_shouldCollectPatternAttributes : 1;
    PatternAttributes m_attributes;
    HashMap<RenderElement*, std::unique_ptr<PatternData>> m_patternMap;
};

}

SPECIALIZE_TYPE_TRAITS_RENDER_SVG_RESOURCE(RenderSVGResourcePattern, PatternResourceType)

#endif
