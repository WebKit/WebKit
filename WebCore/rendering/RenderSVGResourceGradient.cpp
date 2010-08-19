/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *               2008 Eric Seidel <eric@webkit.org>
 *               2008 Dirk Schulze <krit@webkit.org>
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

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGResourceGradient.h"

#include "GradientAttributes.h"
#include "GraphicsContext.h"
#include "SVGImageBufferTools.h"
#include "SVGRenderSupport.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

RenderSVGResourceGradient::RenderSVGResourceGradient(SVGGradientElement* node)
    : RenderSVGResourceContainer(node)
#if PLATFORM(CG)
    , m_savedContext(0)
#endif
{
}

RenderSVGResourceGradient::~RenderSVGResourceGradient()
{
    if (m_gradient.isEmpty())
        return;

    deleteAllValues(m_gradient);
    m_gradient.clear();
}

void RenderSVGResourceGradient::removeAllClientsFromCache(bool markForInvalidation)
{
    if (!m_gradient.isEmpty()) {
        deleteAllValues(m_gradient);
        m_gradient.clear();
    }

    markAllClientsForInvalidation(markForInvalidation ? RepaintInvalidation : ParentOnlyInvalidation);
}

void RenderSVGResourceGradient::removeClientFromCache(RenderObject* client, bool markForInvalidation)
{
    ASSERT(client);

    if (m_gradient.contains(client))
        delete m_gradient.take(client);

    markClientForInvalidation(client, markForInvalidation ? RepaintInvalidation : ParentOnlyInvalidation);
}

#if PLATFORM(CG)
static inline bool createMaskAndSwapContextForTextGradient(GraphicsContext*& context,
                                                           GraphicsContext*& savedContext,
                                                           OwnPtr<ImageBuffer>& imageBuffer,
                                                           const RenderObject* object)
{
    const RenderObject* textRootBlock = SVGRenderSupport::findTextRootObject(object);
    ASSERT(textRootBlock);

    AffineTransform absoluteTransform(SVGImageBufferTools::transformationToOutermostSVGCoordinateSystem(textRootBlock));
    FloatRect clampedAbsoluteTargetRect = SVGImageBufferTools::clampedAbsoluteTargetRectForRenderer(textRootBlock, absoluteTransform, textRootBlock->repaintRectInLocalCoordinates());
    if (clampedAbsoluteTargetRect.isEmpty())
        return false;

    OwnPtr<ImageBuffer> maskImage;
    if (!SVGImageBufferTools::createImageBuffer(clampedAbsoluteTargetRect, maskImage, DeviceRGB))
        return false;

    GraphicsContext* maskImageContext = maskImage->context();
    ASSERT(maskImageContext);

    maskImageContext->translate(-clampedAbsoluteTargetRect.x(), -clampedAbsoluteTargetRect.y());
    maskImageContext->concatCTM(absoluteTransform);

    ASSERT(maskImage);
    savedContext = context;
    context = maskImageContext;
    imageBuffer = maskImage.release();
    return true;
}

static inline AffineTransform clipToTextMask(GraphicsContext* context,
                                             OwnPtr<ImageBuffer>& imageBuffer,
                                             FloatRect& targetRect,
                                             const RenderObject* object,
                                             GradientData* gradientData)
{
    const RenderObject* textRootBlock = SVGRenderSupport::findTextRootObject(object);
    ASSERT(textRootBlock);

    targetRect = textRootBlock->repaintRectInLocalCoordinates();

    AffineTransform absoluteTransform(SVGImageBufferTools::transformationToOutermostSVGCoordinateSystem(textRootBlock));
    FloatRect clampedAbsoluteTargetRect = SVGImageBufferTools::clampedAbsoluteTargetRectForRenderer(textRootBlock, absoluteTransform, targetRect);

    SVGImageBufferTools::clipToImageBuffer(context, absoluteTransform, clampedAbsoluteTargetRect, imageBuffer.get());

    AffineTransform matrix;
    if (gradientData->boundingBoxMode) {
        FloatRect maskBoundingBox = textRootBlock->objectBoundingBox();
        matrix.translate(maskBoundingBox.x(), maskBoundingBox.y());
        matrix.scaleNonUniform(maskBoundingBox.width(), maskBoundingBox.height());
    }
    matrix.multLeft(gradientData->transform);
    return matrix;
}
#endif

bool RenderSVGResourceGradient::applyResource(RenderObject* object, RenderStyle* style, GraphicsContext*& context, unsigned short resourceMode)
{
    ASSERT(object);
    ASSERT(style);
    ASSERT(context);
    ASSERT(resourceMode != ApplyToDefaultMode);

    // Be sure to synchronize all SVG properties on the gradientElement _before_ processing any further.
    // Otherwhise the call to collectGradientAttributes() in createTileImage(), may cause the SVG DOM property
    // synchronization to kick in, which causes removeAllClientsFromCache() to be called, which in turn deletes our
    // GradientData object! Leaving out the line below will cause svg/dynamic-updates/SVG*GradientElement-svgdom* to crash.
    SVGGradientElement* gradientElement = static_cast<SVGGradientElement*>(node());
    if (!gradientElement)
        return false;

    gradientElement->updateAnimatedSVGAttribute(anyQName());

    if (!m_gradient.contains(object))
        m_gradient.set(object, new GradientData);

    GradientData* gradientData = m_gradient.get(object);

    bool isPaintingText = resourceMode & ApplyToTextMode;

    // Create gradient object
    if (!gradientData->gradient) {
        buildGradient(gradientData, gradientElement);

        // CG platforms will handle the gradient space transform for text after applying the
        // resource, so don't apply it here. For non-CG platforms, we want the text bounding
        // box applied to the gradient space transform now, so the gradient shader can use it.
#if PLATFORM(CG)
        if (gradientData->boundingBoxMode && !isPaintingText) {
#else
        if (gradientData->boundingBoxMode) {
#endif
            FloatRect objectBoundingBox = object->objectBoundingBox();
            gradientData->userspaceTransform.translate(objectBoundingBox.x(), objectBoundingBox.y());
            gradientData->userspaceTransform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        }

        gradientData->userspaceTransform.multLeft(gradientData->transform);
        gradientData->gradient->setGradientSpaceTransform(gradientData->userspaceTransform);
    }

    if (!gradientData->gradient)
        return false;

    // Draw gradient
    context->save();

    if (isPaintingText) {
#if PLATFORM(CG)
        if (!createMaskAndSwapContextForTextGradient(context, m_savedContext, m_imageBuffer, object)) {
            context->restore();
            return false;
        }
#endif

        context->setTextDrawingMode(resourceMode & ApplyToFillMode ? cTextFill : cTextStroke);
    }

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    if (resourceMode & ApplyToFillMode) {
        context->setAlpha(svgStyle->fillOpacity());
        context->setFillGradient(gradientData->gradient);
        context->setFillRule(svgStyle->fillRule());
    } else if (resourceMode & ApplyToStrokeMode) {
        if (svgStyle->vectorEffect() == VE_NON_SCALING_STROKE)
            gradientData->gradient->setGradientSpaceTransform(transformOnNonScalingStroke(object, gradientData->userspaceTransform));
        context->setAlpha(svgStyle->strokeOpacity());
        context->setStrokeGradient(gradientData->gradient);
        SVGRenderSupport::applyStrokeStyleToContext(context, style, object);
    }

    return true;
}

void RenderSVGResourceGradient::postApplyResource(RenderObject* object, GraphicsContext*& context, unsigned short resourceMode)
{
    ASSERT(context);
    ASSERT(resourceMode != ApplyToDefaultMode);

    if (resourceMode & ApplyToTextMode) {
#if PLATFORM(CG)
        // CG requires special handling for gradient on text
        if (m_savedContext && m_gradient.contains(object)) {
            GradientData* gradientData = m_gradient.get(object);

            // Restore on-screen drawing context
            context = m_savedContext;
            m_savedContext = 0;

            FloatRect targetRect;
            gradientData->gradient->setGradientSpaceTransform(clipToTextMask(context, m_imageBuffer, targetRect, object, gradientData));
            context->setFillGradient(gradientData->gradient);

            context->fillRect(targetRect);
            m_imageBuffer.clear();
        }
#else
        UNUSED_PARAM(object);
#endif
    } else {
        if (resourceMode & ApplyToFillMode)
            context->fillPath();
        else if (resourceMode & ApplyToStrokeMode)
            context->strokePath();
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

}

#endif
