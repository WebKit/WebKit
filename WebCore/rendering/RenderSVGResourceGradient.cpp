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
    deleteAllValues(m_gradient);
    m_gradient.clear();
}

void RenderSVGResourceGradient::invalidateClients()
{
    const HashMap<RenderObject*, GradientData*>::const_iterator end = m_gradient.end();
    for (HashMap<RenderObject*, GradientData*>::const_iterator it = m_gradient.begin(); it != end; ++it)
        markForLayoutAndResourceInvalidation(it->first);

    deleteAllValues(m_gradient);
    m_gradient.clear();
}

void RenderSVGResourceGradient::invalidateClient(RenderObject* object)
{
    ASSERT(object);

    // FIXME: The HashMap should always contain the object on calling invalidateClient. A race condition
    // during the parsing can causes a call of invalidateClient right before the call of applyResource.
    // We return earlier for the moment. This bug should be fixed in:
    // https://bugs.webkit.org/show_bug.cgi?id=35181
    if (!m_gradient.contains(object))
        return;

    delete m_gradient.take(object);
    markForLayoutAndResourceInvalidation(object);
}

#if PLATFORM(CG)
static inline AffineTransform absoluteTransformForRenderer(const RenderObject* object)
{
    AffineTransform absoluteTransform;

    const RenderObject* currentObject = object;
    while (currentObject) {
        absoluteTransform = currentObject->localToParentTransform() * absoluteTransform;
        currentObject = currentObject->parent();
    }

    return absoluteTransform;
}

static inline bool createMaskAndSwapContextForTextGradient(GraphicsContext*& context,
                                                           GraphicsContext*& savedContext,
                                                           OwnPtr<ImageBuffer>& imageBuffer,
                                                           const RenderObject* object)
{
    const RenderObject* textRootBlock = findTextRootObject(object);

    AffineTransform transform = absoluteTransformForRenderer(textRootBlock);
    FloatRect maskAbsoluteBoundingBox = transform.mapRect(textRootBlock->repaintRectInLocalCoordinates());

    IntRect maskImageRect = enclosingIntRect(maskAbsoluteBoundingBox);
    if (maskImageRect.isEmpty())
        return false;

    // Allocate an image buffer as big as the absolute unclipped size of the object
    OwnPtr<ImageBuffer> maskImage = ImageBuffer::create(maskImageRect.size());
    if (!maskImage)
        return false;

    GraphicsContext* maskImageContext = maskImage->context();

    // Transform the mask image coordinate system to absolute screen coordinates
    maskImageContext->translate(-maskAbsoluteBoundingBox.x(), -maskAbsoluteBoundingBox.y());
    maskImageContext->concatCTM(transform);

    imageBuffer.set(maskImage.release());
    savedContext = context;
    context = maskImageContext;

    return true;
}

static inline AffineTransform clipToTextMask(GraphicsContext* context,
                                             OwnPtr<ImageBuffer>& imageBuffer,
                                             const RenderObject* object,
                                             GradientData* gradientData)
{
    const RenderObject* textRootBlock = findTextRootObject(object);
    context->clipToImageBuffer(textRootBlock->repaintRectInLocalCoordinates(), imageBuffer.get());

    AffineTransform matrix;
    if (gradientData->boundingBoxMode) {
        FloatRect maskBoundingBox = textRootBlock->objectBoundingBox();
        matrix.translate(maskBoundingBox.x(), maskBoundingBox.y());
        matrix.scaleNonUniform(maskBoundingBox.width(), maskBoundingBox.height());
    }
    matrix.multiply(gradientData->transform);
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
    // synchronization to kick in, which causes invalidateClients() to be called, which in turn deletes our
    // GradientData object! Leaving out the line below will cause svg/dynamic-updates/SVG*GradientElement-svgdom* to crash.
    SVGGradientElement* gradientElement = static_cast<SVGGradientElement*>(node());
    if (!gradientElement)
        return false;

    gradientElement->updateAnimatedSVGAttribute(anyQName());

    if (!m_gradient.contains(object))
        m_gradient.set(object, new GradientData);

    GradientData* gradientData = m_gradient.get(object);

    // Create gradient object
    if (!gradientData->gradient)
        buildGradient(gradientData, gradientElement);

    if (!gradientData->gradient)
        return false;

    // Draw gradient
    context->save();

    bool isPaintingText = resourceMode & ApplyToTextMode;
    if (isPaintingText) {
#if PLATFORM(CG)
        if (!createMaskAndSwapContextForTextGradient(context, m_savedContext, m_imageBuffer, object)) {
            context->restore();
            return false;
        }
#endif

        context->setTextDrawingMode(resourceMode & ApplyToFillMode ? cTextFill : cTextStroke);
    }

    AffineTransform transform;

    // CG platforms will handle the gradient space transform for text after applying the
    // resource, so don't apply it here. For non-CG platforms, we want the text bounding
    // box applied to the gradient space transform now, so the gradient shader can use it.
#if PLATFORM(CG)
    if (gradientData->boundingBoxMode && !isPaintingText) {
#else
    if (gradientData->boundingBoxMode) {
#endif
        FloatRect objectBoundingBox = object->objectBoundingBox();
        transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        transform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
    }

    transform.multiply(gradientData->transform);
    gradientData->gradient->setGradientSpaceTransform(transform);

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    if (resourceMode & ApplyToFillMode) {
        context->setAlpha(svgStyle->fillOpacity());
        context->setFillGradient(gradientData->gradient);
        context->setFillRule(svgStyle->fillRule());
    } else if (resourceMode & ApplyToStrokeMode) {
        context->setAlpha(svgStyle->strokeOpacity());
        context->setStrokeGradient(gradientData->gradient);
        applyStrokeStyleToContext(context, style, object);
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

            gradientData->gradient->setGradientSpaceTransform(clipToTextMask(context, m_imageBuffer, object, gradientData));
            context->setFillGradient(gradientData->gradient);

            const RenderObject* textRootBlock = findTextRootObject(object);
            context->fillRect(textRootBlock->repaintRectInLocalCoordinates());

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
