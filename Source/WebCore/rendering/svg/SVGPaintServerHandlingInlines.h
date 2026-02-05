/*
 * Copyright (C) 2023, 2024 Igalia S.L.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "RenderSVGResourceGradient.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderView.h"
#include "SVGPaintServerHandling.h"
#include "SVGRenderSupport.h"
#include "StyleComputedStyle+InitialInlines.h"

namespace WebCore {

template<SVGPaintServerHandling::Operation op>
bool SVGPaintServerHandling::preparePaintOperation(const RenderLayerModelObject& renderer, const RenderStyle& style) const
{
    auto paintServerResult = requestPaintServer<op>(renderer, style);
    if (std::holds_alternative<std::monostate>(paintServerResult))
        return false;

    if (std::holds_alternative<RenderSVGResourcePaintServer*>(paintServerResult)) {
        auto& paintServer = *std::get<RenderSVGResourcePaintServer*>(paintServerResult);

        if constexpr (op == Operation::Fill) {
            if (paintServer.prepareFillOperation(m_context, renderer, style))
                return true;
        } else if constexpr (op == Operation::Stroke) {
            if (paintServer.prepareStrokeOperation(m_context, renderer, style))
                return true;
        }

        // Repeat the paint server request, but explicitly treating the paintServer as invalid/not-existent, to go through the fallback code path.
        paintServerResult = requestPaintServer<op, URIResolving::Disabled>(renderer, style);
        if (std::holds_alternative<std::monostate>(paintServerResult))
            return false;
    }

    ASSERT(std::holds_alternative<Color>(paintServerResult));
    const auto& color = std::get<Color>(paintServerResult);

    if constexpr (op == Operation::Fill)
        prepareFillOperation(renderer, style, color);
    else
        prepareStrokeOperation(renderer, style, color);

    return true;
}

template<SVGPaintServerHandling::Operation op, SVGPaintServerHandling::URIResolving allowPaintServerURIResolving>
SVGPaintServerOrColor SVGPaintServerHandling::requestPaintServer(const RenderLayerModelObject& targetRenderer, const RenderStyle& style)
{
    // When rendering the mask for a RenderSVGResourceClipper, always use the initial fill paint server.
    if (targetRenderer.view().frameView().paintBehavior().contains(PaintBehavior::RenderingSVGClipOrMask)) {
        if constexpr (op == Operation::Fill)
            return Style::ComputedStyle::initialFill().colorDisregardingType().resolvedColor();
        else
            return Style::ComputedStyle::initialStroke().colorDisregardingType().resolvedColor();
    }

    auto& paint = [&] -> const Style::SVGPaint& {
        if constexpr (op == Operation::Fill)
            return style.fill();
        else
            return style.stroke();
    }();

    if (paint.isNone())
        return { };

    if (!paint.isColor()) {
        if (allowPaintServerURIResolving == URIResolving::Disabled) {
            // If we found no paint server, and no fallback is desired, stop here.
            // We can only get here, if we previously requested a paint server, attempted to
            // prepare a fill or stroke operation, which failed. It can fail if, for example,
            // the paint sever is a gradient, gradientUnits are set to 'objectBoundingBox' and
            // the target is an one-dimensional object without a defined 'objectBoundingBox' (<line>).
            if (paint.isURL() || paint.isURLNone())
                return { };
        } else {
            auto paintServerForOperation = [&] {
                if constexpr (op == Operation::Fill)
                    return targetRenderer.svgFillPaintServerResourceFromStyle(style);
                else
                    return targetRenderer.svgStrokePaintServerResourceFromStyle(style);
            };

            // Try resolving URI first.
            if (auto* paintServer = paintServerForOperation())
                return paintServer;

            // If we found no paint server, and no fallback is desired, stop here.
            if (paint.isURL() || paint.isURLNone())
                return { };
        }
    }

    // Color and SVGPaint::URLColor handling.
    auto color = resolveColorFromStyle<op>(style);
    if (inheritColorFromParentStyleIfNeeded<op>(targetRenderer, color))
        return color;
    return { };
}

inline void SVGPaintServerHandling::prepareFillOperation(const RenderLayerModelObject& renderer, const RenderStyle& style, const Color& fillColor) const
{
    if (renderer.view().frameView().paintBehavior().contains(PaintBehavior::RenderingSVGClipOrMask)) {
        m_context.setAlpha(1);
        m_context.setFillRule(style.clipRule());
    } else {
        m_context.setAlpha(style.fillOpacity().value.value);
        m_context.setFillRule(style.fillRule());
    }

    Style::ColorResolver colorResolver { style };
    m_context.setFillColor(colorResolver.colorApplyingColorFilter(fillColor));
}

inline void SVGPaintServerHandling::prepareStrokeOperation(const RenderLayerModelObject& renderer, const RenderStyle& style, const Color& strokeColor) const
{
    m_context.setAlpha(style.strokeOpacity().value.value);

    Style::ColorResolver colorResolver { style };
    m_context.setStrokeColor(colorResolver.colorApplyingColorFilter(strokeColor));
    SVGRenderSupport::applyStrokeStyleToContext(m_context, style, renderer);
}

template<SVGPaintServerHandling::Operation op>
Color SVGPaintServerHandling::resolveColorFromStyle(const RenderStyle& style)
{
    if constexpr (op == Operation::Fill)
        return resolveColorFromStyle(style, style.fill(), style.visitedLinkFill());
    else
        return resolveColorFromStyle(style, style.stroke(), style.visitedLinkStroke());
}

inline Color SVGPaintServerHandling::resolveColorFromStyle(const RenderStyle& style, const Style::SVGPaint& paint, const Style::SVGPaint& visitedLinkPaint)
{
    // All paint types except `none` / `url` / `url none` handle solid colors.
    ASSERT(!paint.isNone());
    ASSERT(!paint.isURL());
    ASSERT(!paint.isURLNone());

    Style::ColorResolver colorResolver { style };

    auto color = colorResolver.colorResolvingCurrentColor(paint.colorDisregardingType());
    if (style.insideLink() == InsideLink::InsideVisited) {
        // FIXME: This code doesn't support the uri component of the visited link paint, https://bugs.webkit.org/show_bug.cgi?id=70006
        if (auto visitedLinkPaintColor = visitedLinkPaint.tryColor()) {
            if (visitedLinkPaintColor->isCurrentColor())
                color = style.visitedLinkColor();
            else if (auto visitedColor = colorResolver.colorResolvingCurrentColor(*visitedLinkPaintColor); visitedColor.isValid())
                color = visitedColor.colorWithAlpha(color.alphaAsFloat());
        }
    }

    return color;
}

template<SVGPaintServerHandling::Operation op>
bool SVGPaintServerHandling::inheritColorFromParentStyleIfNeeded(const RenderLayerModelObject& renderer, Color& color)
{
    if (color.isValid())
        return true;
    if (!renderer.parent())
        return false;

    // FIXME: If this is intentionally using the `renderer` currentColor to resolve colors from `renderer.parent()`, we should document that, otherwise, this should probably use the corresponding style's current color.

    Style::ColorResolver colorResolver { renderer.style() };
    auto& parentStyle = renderer.parent()->style();

    if constexpr (op == Operation::Fill)
        color = colorResolver.colorResolvingCurrentColor(parentStyle.fill().colorDisregardingType());
    else
        color = colorResolver.colorResolvingCurrentColor(parentStyle.stroke().colorDisregardingType());

    return true;
}

} // namespace WebCore
