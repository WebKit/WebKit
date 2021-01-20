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

#include "Color.h"
#include "RenderSVGResource.h"

namespace WebCore {

class RenderSVGResourceSolidColor final : public RenderSVGResource {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RenderSVGResourceSolidColor();
    virtual ~RenderSVGResourceSolidColor();

    void removeAllClientsFromCache(bool = true) override { }
    void removeClientFromCache(RenderElement&, bool = true) override { }

    bool applyResource(RenderElement&, const RenderStyle&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>) override;
    void postApplyResource(RenderElement&, GraphicsContext*&, OptionSet<RenderSVGResourceMode>, const Path*, const RenderSVGShape*) override;
    FloatRect resourceBoundingBox(const RenderObject&) override { return FloatRect(); }

    RenderSVGResourceType resourceType() const override { return SolidColorResourceType; }

    const Color& color() const { return m_color; }
    void setColor(const Color& color) { m_color = color; }

private:
    Color m_color;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_SVG_RESOURCE(RenderSVGResourceSolidColor, SolidColorResourceType)
