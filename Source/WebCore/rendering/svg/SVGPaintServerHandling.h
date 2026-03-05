/*
 * Copyright (C) 2023, 2024 Igalia S.L.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "RenderSVGResourcePaintServer.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class GraphicsContext;
class RenderLayerModelObject;
class RenderStyle;
class Color;

namespace Style {
struct SVGPaint;
}

class SVGPaintServerHandling {
    WTF_MAKE_NONCOPYABLE(SVGPaintServerHandling);
public:
    SVGPaintServerHandling(GraphicsContext& context)
        : m_context(context)
    {
    }

    ~SVGPaintServerHandling() = default;

    GraphicsContext& context() const { return m_context; }

    enum class Operation : bool { Fill, Stroke };
    enum class URIResolving : bool { Disabled, Enabled };

    template<Operation op>
    bool preparePaintOperation(const RenderLayerModelObject&, const RenderStyle&) const;

    template<Operation op, URIResolving allowPaintServerURIResolving = URIResolving::Enabled>
    static SVGPaintServerOrColor requestPaintServer(const RenderLayerModelObject&, const RenderStyle&);

private:
    inline void prepareFillOperation(const RenderLayerModelObject&, const RenderStyle&, const Color& fillColor) const;
    inline void prepareStrokeOperation(const RenderLayerModelObject&, const RenderStyle&, const Color& strokeColor) const;

    static inline Color resolveColorFromStyle(const RenderStyle&, const Style::SVGPaint& paint, const Style::SVGPaint& visitedLinkPaint);
    template<Operation op>
    static Color resolveColorFromStyle(const RenderStyle&);

    template<Operation op>
    static bool inheritColorFromParentStyleIfNeeded(const RenderLayerModelObject&, Color&);

    GraphicsContext& m_context;
};

} // namespace WebCore
