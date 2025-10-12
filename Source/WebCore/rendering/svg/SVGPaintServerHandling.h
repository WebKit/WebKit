/*
 * Copyright (C) 2023, 2024 Igalia S.L.
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
#include "RenderView.h"
#include "SVGRenderSupport.h"

namespace WebCore {

class SVGPaintServerHandling {
    WTF_MAKE_NONCOPYABLE(SVGPaintServerHandling);
public:
    SVGPaintServerHandling(GraphicsContext& context)
        : m_context(context)
    {
    }

    ~SVGPaintServerHandling() = default;

    GraphicsContext& context() const { return m_context; }

    enum class Operation : uint8_t {
        Fill,
        Stroke
    };

    template<Operation op>
    bool preparePaintOperation(const RenderLayerModelObject& renderer, const RenderStyle& style) const
    {
        auto paintServerResult = requestPaintServer<op>(renderer, style);
        if (std::holds_alternative<std::monostate>(paintServerResult))
            return false;

        if (std::holds_alternative<RenderSVGResourcePaintServer*>(paintServerResult)) {
            auto& paintServer = *std::get<RenderSVGResourcePaintServer*>(paintServerResult);
            if (op == Operation::Fill && paintServer.prepareFillOperation(m_context, renderer, style))
                return true;
            if (op == Operation::Stroke && paintServer.prepareStrokeOperation(m_context, renderer, style))
                return true;
            // Repeat the paint server request, but explicitly treating the paintServer as invalid/not-existant, to go through the fallback code path.
            paintServerResult = requestPaintServer<op, URIResolving::Disabled>(renderer, style);
            if (std::holds_alternative<std::monostate>(paintServerResult))
                return false;
        }

        ASSERT(std::holds_alternative<Color>(paintServerResult));
        const auto& color = std::get<Color>(paintServerResult);
        if (op == Operation::Fill)
            prepareFillOperation(renderer, style, color);
        else
            prepareStrokeOperation(renderer, style, color);

        return true;
    }

    enum class URIResolving : uint8_t {
        Enabled,
        Disabled
    };

    template<Operation op, URIResolving allowPaintServerURIResolving = URIResolving::Enabled>
    static SVGPaintServerOrColor requestPaintServer(const RenderLayerModelObject& targetRenderer, const RenderStyle& style)
    {
        // When rendering the mask for a RenderSVGResourceClipper, always use the initial fill paint server.
        if (targetRenderer.view().frameView().paintBehavior().contains(PaintBehavior::RenderingSVGClipOrMask))
            return op == Operation::Fill ? RenderStyle::initialFill().colorDisregardingType().resolvedColor() : RenderStyle::initialStroke().colorDisregardingType().resolvedColor();

        auto& paint = op == Operation::Fill ? style.fill() : style.stroke();
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
                auto paintServerForOperation = [&]() {
                    if (op == Operation::Fill)
                        return targetRenderer.svgFillPaintServerResourceFromStyle(style);
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

private:
    inline void prepareFillOperation(const RenderLayerModelObject& renderer, const RenderStyle& style, const Color& fillColor) const
    {
        if (renderer.view().frameView().paintBehavior().contains(PaintBehavior::RenderingSVGClipOrMask)) {
            m_context.setAlpha(1);
            m_context.setFillRule(style.clipRule());
        } else {
            m_context.setAlpha(style.fillOpacity().value.value);
            m_context.setFillRule(style.fillRule());
        }

        m_context.setFillColor(style.colorByApplyingColorFilter(fillColor));
    }

    inline void prepareStrokeOperation(const RenderLayerModelObject& renderer, const RenderStyle& style, const Color& strokeColor) const
    {
        m_context.setAlpha(style.strokeOpacity().value.value);
        m_context.setStrokeColor(style.colorByApplyingColorFilter(strokeColor));
        SVGRenderSupport::applyStrokeStyleToContext(m_context, style, renderer);
    }

    template<Operation op>
    static inline Color resolveColorFromStyle(const RenderStyle& style)
    {
        if (op == Operation::Fill)
            return resolveColorFromStyle(style, style.fill(), style.visitedLinkFill());
        return resolveColorFromStyle(style, style.stroke(), style.visitedLinkStroke());
    }

    static inline Color resolveColorFromStyle(const RenderStyle& style, const Style::SVGPaint& paint, const Style::SVGPaint& visitedLinkPaint)
    {
        // All paint types except `none` / `url` / `url none` handle solid colors.
        ASSERT(!paint.isNone());
        ASSERT(!paint.isURL());
        ASSERT(!paint.isURLNone());

        auto color = style.colorResolvingCurrentColor(paint.colorDisregardingType());
        if (style.insideLink() == InsideLink::InsideVisited) {
            // FIXME: This code doesn't support the uri component of the visited link paint, https://bugs.webkit.org/show_bug.cgi?id=70006
            if (auto visitedLinkPaintColor = visitedLinkPaint.tryColor()) {
                if (auto visitedColor = style.colorResolvingCurrentColor(*visitedLinkPaintColor); visitedColor.isValid())
                    color = visitedColor.colorWithAlpha(color.alphaAsFloat());
            }
        }

        return color;
    }

    template<Operation op>
    static inline bool inheritColorFromParentStyleIfNeeded(const RenderLayerModelObject& renderer, Color& color)
    {
        if (color.isValid())
            return true;
        if (!renderer.parent())
            return false;
        auto& parentStyle = renderer.parent()->style();
        color = renderer.style().colorResolvingCurrentColor(op == Operation::Fill ? parentStyle.fill().colorDisregardingType() : parentStyle.stroke().colorDisregardingType());
        return true;
    }

private:
    GraphicsContext& m_context;
};

} // namespace WebCore
