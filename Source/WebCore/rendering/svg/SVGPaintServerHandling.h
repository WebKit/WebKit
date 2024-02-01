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

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "RenderSVGResourceGradient.h"
#include "SVGRenderStyle.h"
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
            return op == Operation::Fill ? SVGRenderStyle::initialFillPaintColor().absoluteColor() : SVGRenderStyle::initialStrokePaintColor().absoluteColor();

        auto paintType = op == Operation::Fill ? style.svgStyle().fillPaintType() : style.svgStyle().strokePaintType();
        if (paintType == SVGPaintType::None)
            return { };

        if (paintType >= SVGPaintType::URINone) {
            if (allowPaintServerURIResolving == URIResolving::Disabled) {
                // If we found no paint server, and no fallback is desired, stop here.
                // We can only get here, if we previously requested a paint server, attempted to
                // prepare a fill or stroke operation, which failed. It can fail if, for example,
                // the paint sever is a gradient, gradientUnits are set to 'objectBoundingBox' and
                // the target is an one-dimensional object without a defined 'objectBoundingBox' (<line>).
                if (paintType == SVGPaintType::URI || paintType == SVGPaintType::URINone)
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
                if (paintType == SVGPaintType::URI || paintType == SVGPaintType::URINone)
                    return { };
            }
        }

        // SVGPaintType::CurrentColor / SVGPaintType::RGBColor / SVGPaintType::URICurrentColor / SVGPaintType::URIRGBColor handling.
        auto color = resolveColorFromStyle<op>(style);
        if (inheritColorFromParentStyleIfNeeded<op>(targetRenderer, color))
            return color;
        return { };
    }

private:
    inline void prepareFillOperation(const RenderLayerModelObject& renderer, const RenderStyle& style, const Color& fillColor) const
    {
        const auto& svgStyle = style.svgStyle();
        if (renderer.view().frameView().paintBehavior().contains(PaintBehavior::RenderingSVGClipOrMask)) {
            m_context.setAlpha(1);
            m_context.setFillRule(svgStyle.clipRule());
        } else {
            m_context.setAlpha(svgStyle.fillOpacity());
            m_context.setFillRule(svgStyle.fillRule());
        }

        m_context.setFillColor(style.colorByApplyingColorFilter(fillColor));
    }

    inline void prepareStrokeOperation(const RenderLayerModelObject& renderer, const RenderStyle& style, const Color& strokeColor) const
    {
        const auto& svgStyle = style.svgStyle();
        m_context.setAlpha(svgStyle.strokeOpacity());
        m_context.setStrokeColor(style.colorByApplyingColorFilter(strokeColor));
        SVGRenderSupport::applyStrokeStyleToContext(m_context, style, renderer);
    }

    template<Operation op>
    static inline Color resolveColorFromStyle(const RenderStyle& style)
    {
        const auto& svgStyle = style.svgStyle();
        if (op == Operation::Fill)
            return resolveColorFromStyle(style, svgStyle.fillPaintType(), svgStyle.fillPaintColor(), svgStyle.visitedLinkFillPaintType(), svgStyle.visitedLinkFillPaintColor());
        return resolveColorFromStyle(style, svgStyle.strokePaintType(), svgStyle.strokePaintColor(), svgStyle.visitedLinkStrokePaintType(), svgStyle.visitedLinkStrokePaintColor());
    }

    static inline Color resolveColorFromStyle(const RenderStyle& style, SVGPaintType paintType, const StyleColor& paintColor, SVGPaintType visitedLinkPaintType, const StyleColor& visitedLinkPaintColor)
    {
        // All paint types except None / URI / URINone handle solid colors.
        ASSERT_UNUSED(paintType, paintType != SVGPaintType::None);
        ASSERT(paintType != SVGPaintType::URI);
        ASSERT(paintType != SVGPaintType::URINone);

        auto color = style.colorResolvingCurrentColor(paintColor);
        if (style.insideLink() == InsideLink::InsideVisited) {
            // FIXME: This code doesn't support the uri component of the visited link paint, https://bugs.webkit.org/show_bug.cgi?id=70006
            if (visitedLinkPaintType == SVGPaintType::RGBColor) {
                const auto& visitedColor = style.colorResolvingCurrentColor(visitedLinkPaintColor);
                if (visitedColor.isValid())
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
        const auto& parentSVGStyle = renderer.parent()->style().svgStyle();
        color = renderer.style().colorResolvingCurrentColor(op == Operation::Fill ? parentSVGStyle.fillPaintColor() : parentSVGStyle.strokePaintColor());
        return true;
    }

private:
    GraphicsContext& m_context;
};

} // namespace WebCore

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
