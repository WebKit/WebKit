/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
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

#include "config.h"
#include "RenderSVGResourceGradient.h"

#include "GradientAttributes.h"
#include "GraphicsContext.h"
#include "RenderSVGText.h"
#include "SVGRenderingContext.h"

namespace WebCore {

RenderSVGResourceGradient::RenderSVGResourceGradient(SVGGradientElement& node, PassRef<RenderStyle> style)
    : RenderSVGResourceContainer(node, WTF::move(style))
    , m_shouldCollectGradientAttributes(true)
#if USE(CG)
    , m_savedContext(0)
#endif
{
}

void RenderSVGResourceGradient::removeAllClientsFromCache(bool markForInvalidation)
{
    m_gradientMap.clear();
    m_shouldCollectGradientAttributes = true;
    markAllClientsForInvalidation(markForInvalidation ? RepaintInvalidation : ParentOnlyInvalidation);
}

void RenderSVGResourceGradient::removeClientFromCache(RenderElement& client, bool markForInvalidation)
{
    m_gradientMap.remove(&client);
    markClientForInvalidation(client, markForInvalidation ? RepaintInvalidation : ParentOnlyInvalidation);
}

#if USE(CG)
static inline bool createMaskAndSwapContextForTextGradient(GraphicsContext*& context, GraphicsContext*& savedContext, std::unique_ptr<ImageBuffer>& imageBuffer, RenderObject* object)
{
    auto* textRootBlock = RenderSVGText::locateRenderSVGTextAncestor(*object);
    ASSERT(textRootBlock);

    AffineTransform absoluteTransform;
    SVGRenderingContext::calculateTransformationToOutermostCoordinateSystem(*textRootBlock, absoluteTransform);

    FloatRect repaintRect = textRootBlock->repaintRectInLocalCoordinates();
    std::unique_ptr<ImageBuffer> maskImage;
    if (!SVGRenderingContext::createImageBuffer(repaintRect, absoluteTransform, maskImage, ColorSpaceDeviceRGB, Unaccelerated))
        return false;

    GraphicsContext* maskImageContext = maskImage->context();
    ASSERT(maskImageContext);
    ASSERT(maskImage);
    savedContext = context;
    context = maskImageContext;
    imageBuffer = WTF::move(maskImage);
    return true;
}

static inline AffineTransform clipToTextMask(GraphicsContext* context, std::unique_ptr<ImageBuffer>& imageBuffer, FloatRect& targetRect, RenderObject* object, bool boundingBoxMode, const AffineTransform& gradientTransform)
{
    auto* textRootBlock = RenderSVGText::locateRenderSVGTextAncestor(*object);
    ASSERT(textRootBlock);

    AffineTransform absoluteTransform;
    SVGRenderingContext::calculateTransformationToOutermostCoordinateSystem(*textRootBlock, absoluteTransform);

    targetRect = textRootBlock->repaintRectInLocalCoordinates();
    SVGRenderingContext::clipToImageBuffer(context, absoluteTransform, targetRect, imageBuffer, false);

    AffineTransform matrix;
    if (boundingBoxMode) {
        FloatRect maskBoundingBox = textRootBlock->objectBoundingBox();
        matrix.translate(maskBoundingBox.x(), maskBoundingBox.y());
        matrix.scaleNonUniform(maskBoundingBox.width(), maskBoundingBox.height());
    }
    matrix *= gradientTransform;
    return matrix;
}
#endif

bool RenderSVGResourceGradient::applyResource(RenderElement& renderer, const RenderStyle& style, GraphicsContext*& context, unsigned short resourceMode)
{
    ASSERT(context);
    ASSERT(resourceMode != ApplyToDefaultMode);

    // Be sure to synchronize all SVG properties on the gradientElement _before_ processing any further.
    // Otherwhise the call to collectGradientAttributes() in createTileImage(), may cause the SVG DOM property
    // synchronization to kick in, which causes removeAllClientsFromCache() to be called, which in turn deletes our
    // GradientData object! Leaving out the line below will cause svg/dynamic-updates/SVG*GradientElement-svgdom* to crash.
    if (m_shouldCollectGradientAttributes) {
        gradientElement().synchronizeAnimatedSVGAttribute(anyQName());
        if (!collectGradientAttributes())
            return false;

        m_shouldCollectGradientAttributes = false;
    }

    // Spec: When the geometry of the applicable element has no width or height and objectBoundingBox is specified,
    // then the given effect (e.g. a gradient or a filter) will be ignored.
    FloatRect objectBoundingBox = renderer.objectBoundingBox();
    if (gradientUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX && objectBoundingBox.isEmpty())
        return false;

    auto& gradientData = m_gradientMap.add(&renderer, nullptr).iterator->value;
    if (!gradientData)
        gradientData = std::make_unique<GradientData>();

    bool isPaintingText = resourceMode & ApplyToTextMode;

    // Create gradient object
    if (!gradientData->gradient) {
        buildGradient(gradientData.get());

        // CG platforms will handle the gradient space transform for text after applying the
        // resource, so don't apply it here. For non-CG platforms, we want the text bounding
        // box applied to the gradient space transform now, so the gradient shader can use it.
#if USE(CG)
        if (gradientUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX && !objectBoundingBox.isEmpty() && !isPaintingText) {
#else
        if (gradientUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX && !objectBoundingBox.isEmpty()) {
#endif
            gradientData->userspaceTransform.translate(objectBoundingBox.x(), objectBoundingBox.y());
            gradientData->userspaceTransform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        }

        AffineTransform gradientTransform;
        calculateGradientTransform(gradientTransform);

        gradientData->userspaceTransform *= gradientTransform;
        if (isPaintingText) {
            // Depending on font scaling factor, we may need to rescale the gradient here since
            // text painting removes the scale factor from the context.
            AffineTransform additionalTextTransform;
            if (shouldTransformOnTextPainting(renderer, additionalTextTransform))
                gradientData->userspaceTransform *= additionalTextTransform;
        }
        gradientData->gradient->setGradientSpaceTransform(gradientData->userspaceTransform);
    }

    if (!gradientData->gradient)
        return false;

    // Draw gradient
    context->save();

    if (isPaintingText) {
#if USE(CG)
        if (!createMaskAndSwapContextForTextGradient(context, m_savedContext, m_imageBuffer, &renderer)) {
            context->restore();
            return false;
        }
#endif

        context->setTextDrawingMode(resourceMode & ApplyToFillMode ? TextModeFill : TextModeStroke);
    }

    const SVGRenderStyle& svgStyle = style.svgStyle();

    if (resourceMode & ApplyToFillMode) {
        context->setAlpha(svgStyle.fillOpacity());
        context->setFillGradient(gradientData->gradient);
        context->setFillRule(svgStyle.fillRule());
    } else if (resourceMode & ApplyToStrokeMode) {
        if (svgStyle.vectorEffect() == VE_NON_SCALING_STROKE)
            gradientData->gradient->setGradientSpaceTransform(transformOnNonScalingStroke(&renderer, gradientData->userspaceTransform));
        context->setAlpha(svgStyle.strokeOpacity());
        context->setStrokeGradient(gradientData->gradient);
        SVGRenderSupport::applyStrokeStyleToContext(context, style, renderer);
    }

    return true;
}

void RenderSVGResourceGradient::postApplyResource(RenderElement& renderer, GraphicsContext*& context, unsigned short resourceMode, const Path* path, const RenderSVGShape* shape)
{
    ASSERT(context);
    ASSERT(resourceMode != ApplyToDefaultMode);

    if (resourceMode & ApplyToTextMode) {
#if USE(CG)
        // CG requires special handling for gradient on text
        GradientData* gradientData;
        if (m_savedContext && (gradientData = m_gradientMap.get(&renderer))) {
            // Restore on-screen drawing context
            context = m_savedContext;
            m_savedContext = 0;

            AffineTransform gradientTransform;
            calculateGradientTransform(gradientTransform);

            FloatRect targetRect;
            gradientData->gradient->setGradientSpaceTransform(clipToTextMask(context, m_imageBuffer, targetRect, &renderer, gradientUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX, gradientTransform));
            context->setFillGradient(gradientData->gradient);

            context->fillRect(targetRect);
            m_imageBuffer.reset();
        }
#else
        UNUSED_PARAM(renderer);
#endif
    } else {
        if (resourceMode & ApplyToFillMode) {
            if (path)
                context->fillPath(*path);
            else if (shape)
                shape->fillShape(context);
        }
        if (resourceMode & ApplyToStrokeMode) {
            if (path)
                context->strokePath(*path);
            else if (shape)
                shape->strokeShape(context);
        }
    }

    context->restore();
}

void RenderSVGResourceGradient::addStops(GradientData* gradientData, const Vector<Gradient::ColorStop>& stops) const
{
    ASSERT(gradientData->gradient);

    const Vector<Gradient::ColorStop>::const_iterator end = stops.end();
    for (Vector<Gradient::ColorStop>::const_iterator it = stops.begin(); it != end; ++it)
        gradientData->gradient->addColorStop(*it);
}

GradientSpreadMethod RenderSVGResourceGradient::platformSpreadMethodFromSVGType(SVGSpreadMethodType method) const
{
    switch (method) {
    case SVGSpreadMethodUnknown:
    case SVGSpreadMethodPad:
        return SpreadMethodPad;
    case SVGSpreadMethodReflect:
        return SpreadMethodReflect;
    case SVGSpreadMethodRepeat:
        return SpreadMethodRepeat;
    }

    ASSERT_NOT_REACHED();
    return SpreadMethodPad;
}

}
