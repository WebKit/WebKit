/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "LegacyRenderSVGResourceGradient.h"

#include "GradientAttributes.h"
#include "GraphicsContext.h"
#include "RenderSVGText.h"
#include "RenderStyleInlines.h"
#include "SVGRenderStyle.h"
#include "SVGRenderingContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyRenderSVGResourceGradient);

LegacyRenderSVGResourceGradient::LegacyRenderSVGResourceGradient(Type type, SVGGradientElement& node, RenderStyle&& style)
    : LegacyRenderSVGResourceContainer(type, node, WTFMove(style))
{
}

void LegacyRenderSVGResourceGradient::removeAllClientsFromCacheIfNeeded(bool markForInvalidation, SingleThreadWeakHashSet<RenderObject>* visitedRenderers)
{
    m_gradientMap.clear();
    m_shouldCollectGradientAttributes = true;
    markAllClientsForInvalidationIfNeeded(markForInvalidation ? RepaintInvalidation : ParentOnlyInvalidation, visitedRenderers);
}

void LegacyRenderSVGResourceGradient::removeClientFromCache(RenderElement& client, bool markForInvalidation)
{
    m_gradientMap.remove(&client);
    markClientForInvalidation(client, markForInvalidation ? RepaintInvalidation : ParentOnlyInvalidation);
}

#if USE(CG)
static inline bool createMaskAndSwapContextForTextGradient(GraphicsContext*& context, GraphicsContext*& savedContext, RefPtr<ImageBuffer>& imageBuffer, RenderElement& renderer)
{
    auto* textRootBlock = RenderSVGText::locateRenderSVGTextAncestor(renderer);
    ASSERT(textRootBlock);

    AffineTransform absoluteTransform = SVGRenderingContext::calculateTransformationToOutermostCoordinateSystem(*textRootBlock);
    FloatRect repaintRect = textRootBlock->repaintRectInLocalCoordinates();

    // Ignore 2D rotation, as it doesn't affect the size of the mask.
    FloatSize scale(absoluteTransform.xScale(), absoluteTransform.yScale());

    // Determine scale factor for the clipper. The size of intermediate ImageBuffers shouldn't be bigger than kMaxFilterSize.
    ImageBuffer::sizeNeedsClamping(repaintRect.size(), scale);

    auto maskImage = context->createScaledImageBuffer(repaintRect, scale);
    if (!maskImage)
        return false;

    GraphicsContext& maskImageContext = maskImage->context();
    ASSERT(maskImage);
    savedContext = context;
    context = &maskImageContext;
    imageBuffer = WTFMove(maskImage);
    return true;
}

static inline AffineTransform clipToTextMask(GraphicsContext& context, RefPtr<ImageBuffer>& imageBuffer, FloatRect& targetRect, RenderElement& renderer, bool boundingBoxMode, const AffineTransform& gradientTransform)
{
    auto* textRootBlock = RenderSVGText::locateRenderSVGTextAncestor(renderer);
    ASSERT(textRootBlock);

    AffineTransform absoluteTransform = SVGRenderingContext::calculateTransformationToOutermostCoordinateSystem(*textRootBlock);

    targetRect = textRootBlock->repaintRectInLocalCoordinates();
    
    // Ignore 2D rotation, as it doesn't affect the size of the mask.
    FloatSize scale(absoluteTransform.xScale(), absoluteTransform.yScale());

    // Determine scale factor for the clipper. The size of intermediate ImageBuffers shouldn't be bigger than kMaxFilterSize.
    ImageBuffer::sizeNeedsClamping(targetRect.size(), scale);

    SVGRenderingContext::clipToImageBuffer(context, targetRect, scale, imageBuffer, false);

    AffineTransform matrix;
    if (boundingBoxMode) {
        FloatRect maskBoundingBox = textRootBlock->objectBoundingBox();
        matrix.translate(maskBoundingBox.location());
        matrix.scale(maskBoundingBox.size());
    }
    matrix *= gradientTransform;
    return matrix;
}
#endif

GradientData::Inputs LegacyRenderSVGResourceGradient::computeInputs(RenderElement& renderer, OptionSet<RenderSVGResourceMode> resourceMode)
{
    std::optional<FloatRect> objectBoundingBox;
    if (gradientUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        objectBoundingBox = renderer.objectBoundingBox();

    float textPaintingScale = 1;
    if (resourceMode.contains(RenderSVGResourceMode::ApplyToText))
        textPaintingScale = computeTextPaintingScale(renderer);

    return { objectBoundingBox, textPaintingScale };
}

bool LegacyRenderSVGResourceGradient::applyResource(RenderElement& renderer, const RenderStyle& style, GraphicsContext*& context, OptionSet<RenderSVGResourceMode> resourceMode)
{
    ASSERT(context);
    ASSERT(!resourceMode.isEmpty());

    // Be sure to synchronize all SVG properties on the gradientElement _before_ processing any further.
    // Otherwhise the call to collectGradientAttributes() in createTileImage(), may cause the SVG DOM property
    // synchronization to kick in, which causes removeAllClientsFromCache() to be called, which in turn deletes our
    // GradientData object! Leaving out the line below will cause svg/dynamic-updates/SVG*GradientElement-svgdom* to crash.
    if (m_shouldCollectGradientAttributes) {
        gradientElement().synchronizeAllAttributes();
        if (!collectGradientAttributes())
            return false;

        m_shouldCollectGradientAttributes = false;
    }

    // Spec: When the geometry of the applicable element has no width or height and objectBoundingBox is specified,
    // then the given effect (e.g. a gradient or a filter) will be ignored.
    auto inputs = computeInputs(renderer, resourceMode);
    if (inputs.objectBoundingBox && inputs.objectBoundingBox->isEmpty())
        return false;

    bool isPaintingText = resourceMode.contains(RenderSVGResourceMode::ApplyToText);

    auto& gradientData = *m_gradientMap.ensure(&renderer, [&]() {
        return makeUnique<GradientData>();
    }).iterator->value;

    if (gradientData.invalidate(inputs)) {
        gradientData.gradient = buildGradient(style);
        ASSERT(gradientData.userspaceTransform.isIdentity());

        // CG platforms will handle the gradient space transform for text after applying the
        // resource, so don't apply it here. For non-CG platforms, we want the text bounding
        // box applied to the gradient space transform now, so the gradient shader can use it.
        if (gradientData.inputs.objectBoundingBox
#if USE(CG)
            && !isPaintingText
#endif
        ) {
            gradientData.userspaceTransform.translate(gradientData.inputs.objectBoundingBox->location());
            gradientData.userspaceTransform.scale(gradientData.inputs.objectBoundingBox->size());
        }

        gradientData.userspaceTransform *= gradientTransform();

        // Depending on font scaling factor, we may need to rescale the gradient here since
        // text painting removes the scale factor from the context.
        if (gradientData.inputs.textPaintingScale != 1)
            gradientData.userspaceTransform.scale(gradientData.inputs.textPaintingScale);
    }

    // Draw gradient
    context->save();

    if (isPaintingText) {
#if USE(CG)
        if (!createMaskAndSwapContextForTextGradient(context, m_savedContext, m_imageBuffer, renderer)) {
            context->restore();
            return false;
        }
#endif
        context->setTextDrawingMode(resourceMode.contains(RenderSVGResourceMode::ApplyToFill) ? TextDrawingMode::Fill : TextDrawingMode::Stroke);
    }

    auto& svgStyle = style.svgStyle();
    auto userspaceTransform = gradientData.userspaceTransform;

    if (resourceMode.contains(RenderSVGResourceMode::ApplyToFill)) {
        context->setAlpha(svgStyle.fillOpacity());
        context->setFillGradient(*gradientData.gradient, userspaceTransform);
        context->setFillRule(svgStyle.fillRule());
    } else if (resourceMode.contains(RenderSVGResourceMode::ApplyToStroke)) {
        if (svgStyle.vectorEffect() == VectorEffect::NonScalingStroke)
            userspaceTransform = transformOnNonScalingStroke(&renderer, gradientData.userspaceTransform);
        context->setAlpha(svgStyle.strokeOpacity());
        context->setStrokeGradient(*gradientData.gradient, userspaceTransform);
        SVGRenderSupport::applyStrokeStyleToContext(*context, style, renderer);
    }

    return true;
}

void LegacyRenderSVGResourceGradient::postApplyResource(RenderElement& renderer, GraphicsContext*& context, OptionSet<RenderSVGResourceMode> resourceMode, const Path* path, const RenderElement* shape)
{
    ASSERT(context);
    ASSERT(!resourceMode.isEmpty());

    if (resourceMode.contains(RenderSVGResourceMode::ApplyToText)) {
#if USE(CG)
        // CG requires special handling for gradient on text
        if (m_savedContext) {
            auto gradientData = m_gradientMap.find(&renderer);
            if (gradientData != m_gradientMap.end()) {
                auto& gradient = *gradientData->value->gradient;

                // Restore on-screen drawing context
                context = std::exchange(m_savedContext, nullptr);

                FloatRect targetRect;
                AffineTransform userspaceTransform = clipToTextMask(*context, m_imageBuffer, targetRect, renderer, gradientUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX, gradientTransform());

                context->setFillGradient(gradient, userspaceTransform);
                context->fillRect(targetRect);

                m_imageBuffer = nullptr;
            }
        }
#else
        UNUSED_PARAM(renderer);
#endif
    } else
        fillAndStrokePathOrShape(*context, resourceMode, path, shape);

    context->restore();
}

GradientColorStops LegacyRenderSVGResourceGradient::stopsByApplyingColorFilter(const GradientColorStops& stops, const RenderStyle& style)
{
    if (!style.hasAppleColorFilter())
        return stops;

    return stops.mapColors([&] (auto& color) { return style.colorByApplyingColorFilter(color); });
}

GradientSpreadMethod LegacyRenderSVGResourceGradient::platformSpreadMethodFromSVGType(SVGSpreadMethodType method)
{
    switch (method) {
    case SVGSpreadMethodUnknown:
    case SVGSpreadMethodPad:
        return GradientSpreadMethod::Pad;
    case SVGSpreadMethodReflect:
        return GradientSpreadMethod::Reflect;
    case SVGSpreadMethodRepeat:
        return GradientSpreadMethod::Repeat;
    }

    ASSERT_NOT_REACHED();
    return GradientSpreadMethod::Pad;
}

}
