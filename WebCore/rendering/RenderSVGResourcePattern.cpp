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
 *
 */

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGResourcePattern.h"

#include "GraphicsContext.h"
#include "PatternAttributes.h"
#include "SVGRenderSupport.h"

namespace WebCore {

RenderSVGResourceType RenderSVGResourcePattern::s_resourceType = PatternResourceType;

RenderSVGResourcePattern::RenderSVGResourcePattern(SVGPatternElement* node)
    : RenderSVGResourceContainer(node)
{
}

RenderSVGResourcePattern::~RenderSVGResourcePattern()
{
    deleteAllValues(m_pattern);
    m_pattern.clear();
}

void RenderSVGResourcePattern::invalidateClients()
{
    const HashMap<RenderObject*, PatternData*>::const_iterator end = m_pattern.end();
    for (HashMap<RenderObject*, PatternData*>::const_iterator it = m_pattern.begin(); it != end; ++it)
        markForLayoutAndResourceInvalidation(it->first);

    deleteAllValues(m_pattern);
    m_pattern.clear();
}

void RenderSVGResourcePattern::invalidateClient(RenderObject* object)
{
    ASSERT(object);

    // FIXME: The HashMap should always contain the object on calling invalidateClient. A race condition
    // during the parsing can causes a call of invalidateClient right before the call of applyResource.
    // We return earlier for the moment. This bug should be fixed in:
    // https://bugs.webkit.org/show_bug.cgi?id=35181
    if (!m_pattern.contains(object))
        return;

    delete m_pattern.take(object);
    markForLayoutAndResourceInvalidation(object);
}

bool RenderSVGResourcePattern::applyResource(RenderObject* object, RenderStyle* style, GraphicsContext*& context, unsigned short resourceMode)
{
    ASSERT(object);
    ASSERT(style);
    ASSERT(context);
    ASSERT(resourceMode != ApplyToDefaultMode);

    // Be sure to synchronize all SVG properties on the patternElement _before_ processing any further.
    // Otherwhise the call to collectPatternAttributes() in createTileImage(), may cause the SVG DOM property
    // synchronization to kick in, which causes invalidateClients() to be called, which in turn deletes our
    // PatternData object! Leaving out the line below will cause svg/dynamic-updates/SVGPatternElement-svgdom* to crash.
    SVGPatternElement* patternElement = static_cast<SVGPatternElement*>(node());
    if (!patternElement)
        return false;

    patternElement->updateAnimatedSVGAttribute(anyQName());

    if (!m_pattern.contains(object))
        m_pattern.set(object, new PatternData);

    PatternData* patternData = m_pattern.get(object);
    if (!patternData->pattern) {
        FloatRect patternBoundaries;
        AffineTransform patternTransform;

        // Create tile image
        OwnPtr<ImageBuffer> tileImage = createTileImage(patternBoundaries, patternTransform, patternElement, object);
        if (!tileImage)
            return false;

        // Create pattern object
        buildPattern(patternData, patternBoundaries, tileImage.release());

        if (!patternData->pattern)
            return false;

        // Compute pattern transformation
        AffineTransform transform;
        transform.translate(patternBoundaries.x(), patternBoundaries.y());
        transform.multiply(patternTransform);
        patternData->pattern->setPatternSpaceTransform(transform);
    }

    // Draw pattern
    context->save();

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    if (resourceMode & ApplyToFillMode) {
        context->setAlpha(svgStyle->fillOpacity());
        context->setFillPattern(patternData->pattern);
        context->setFillRule(svgStyle->fillRule());
    } else if (resourceMode & ApplyToStrokeMode) {
        context->setAlpha(svgStyle->strokeOpacity());
        context->setStrokePattern(patternData->pattern);
        applyStrokeStyleToContext(context, style, object);
    }

    if (resourceMode & ApplyToTextMode) {
        if (resourceMode & ApplyToFillMode) {
            context->setTextDrawingMode(cTextFill);

#if PLATFORM(CG)
            context->applyFillPattern();
#endif
        } else if (resourceMode & ApplyToStrokeMode) {
            context->setTextDrawingMode(cTextStroke);

#if PLATFORM(CG)
            context->applyStrokePattern();
#endif
        }
    }

    return true;
}

void RenderSVGResourcePattern::postApplyResource(RenderObject*, GraphicsContext*& context, unsigned short resourceMode)
{
    ASSERT(context);
    ASSERT(resourceMode != ApplyToDefaultMode);

    if (!(resourceMode & ApplyToTextMode)) {
        if (resourceMode & ApplyToFillMode)
            context->fillPath();
        else if (resourceMode & ApplyToStrokeMode)
            context->strokePath();
    }

    context->restore();
}

static inline FloatRect calculatePatternBoundaries(PatternAttributes& attributes,
                                                   const FloatRect& objectBoundingBox,
                                                   const SVGPatternElement* patternElement)
{
    if (attributes.boundingBoxMode())
        return FloatRect(attributes.x().valueAsPercentage() * objectBoundingBox.width(),
                         attributes.y().valueAsPercentage() * objectBoundingBox.height(),
                         attributes.width().valueAsPercentage() * objectBoundingBox.width(),
                         attributes.height().valueAsPercentage() * objectBoundingBox.height());

    return FloatRect(attributes.x().value(patternElement),
                     attributes.y().value(patternElement),
                     attributes.width().value(patternElement),
                     attributes.height().value(patternElement));
}

FloatRect RenderSVGResourcePattern::calculatePatternBoundariesIncludingOverflow(PatternAttributes& attributes,
                                                                                const FloatRect& objectBoundingBox,
                                                                                const AffineTransform& viewBoxCTM,
                                                                                const FloatRect& patternBoundaries) const
{
    // Eventually calculate the pattern content boundaries (only needed with overflow="visible").
    FloatRect patternContentBoundaries;

    const RenderStyle* style = this->style();
    if (style->overflowX() == OVISIBLE && style->overflowY() == OVISIBLE) {
        for (Node* node = attributes.patternContentElement()->firstChild(); node; node = node->nextSibling()) {
            if (!node->isSVGElement() || !static_cast<SVGElement*>(node)->isStyledTransformable() || !node->renderer())
                continue;
            patternContentBoundaries.unite(node->renderer()->repaintRectInLocalCoordinates());
        }
    }

    if (patternContentBoundaries.isEmpty())
        return patternBoundaries;

    FloatRect patternBoundariesIncludingOverflow = patternBoundaries;

    // Respect objectBoundingBoxMode for patternContentUnits, if viewBox is not set.
    if (!viewBoxCTM.isIdentity())
        patternContentBoundaries = viewBoxCTM.mapRect(patternContentBoundaries);
    else if (attributes.boundingBoxModeContent())
        patternContentBoundaries = FloatRect(patternContentBoundaries.x() * objectBoundingBox.width(),
                                             patternContentBoundaries.y() * objectBoundingBox.height(),
                                             patternContentBoundaries.width() * objectBoundingBox.width(),
                                             patternContentBoundaries.height() * objectBoundingBox.height());

    patternBoundariesIncludingOverflow.unite(patternContentBoundaries);
    return patternBoundariesIncludingOverflow;
}

PassOwnPtr<ImageBuffer> RenderSVGResourcePattern::createTileImage(FloatRect& patternBoundaries,
                                                                  AffineTransform& patternTransform,
                                                                  const SVGPatternElement* patternElement,
                                                                  RenderObject* object) const
{
    PatternAttributes attributes = patternElement->collectPatternProperties();

    // If we couldn't determine the pattern content element root, stop here.
    if (!attributes.patternContentElement())
        return 0;

    FloatRect objectBoundingBox = object->objectBoundingBox();    
    patternBoundaries = calculatePatternBoundaries(attributes, objectBoundingBox, patternElement); 
    patternTransform = attributes.patternTransform();

    AffineTransform viewBoxCTM = patternElement->viewBoxToViewTransform(patternElement->viewBox(),
                                                                        patternElement->preserveAspectRatio(),
                                                                        patternBoundaries.width(),
                                                                        patternBoundaries.height());

    FloatRect patternBoundariesIncludingOverflow = calculatePatternBoundariesIncludingOverflow(attributes,
                                                                                               objectBoundingBox,
                                                                                               viewBoxCTM,
                                                                                               patternBoundaries);

    IntSize imageSize(lroundf(patternBoundariesIncludingOverflow.width()), lroundf(patternBoundariesIncludingOverflow.height()));

    // FIXME: We should be able to clip this more, needs investigation
    clampImageBufferSizeToViewport(object->document()->view(), imageSize);

    // Don't create ImageBuffers with image size of 0
    if (imageSize.isEmpty())
        return 0;

    OwnPtr<ImageBuffer> tileImage = ImageBuffer::create(imageSize);

    GraphicsContext* context = tileImage->context();
    ASSERT(context);

    context->save();

    // Translate to pattern start origin
    if (patternBoundariesIncludingOverflow.location() != patternBoundaries.location()) {
        context->translate(patternBoundaries.x() - patternBoundariesIncludingOverflow.x(),
                           patternBoundaries.y() - patternBoundariesIncludingOverflow.y());

        patternBoundaries.setLocation(patternBoundariesIncludingOverflow.location());
    }

    // Process viewBox or boundingBoxModeContent correction
    if (!viewBoxCTM.isIdentity())
        context->concatCTM(viewBoxCTM);
    else if (attributes.boundingBoxModeContent()) {
        context->translate(objectBoundingBox.x(), objectBoundingBox.y());
        context->scale(FloatSize(objectBoundingBox.width(), objectBoundingBox.height()));
    }

    // Render subtree into ImageBuffer
    for (Node* node = attributes.patternContentElement()->firstChild(); node; node = node->nextSibling()) {
        if (!node->isSVGElement() || !static_cast<SVGElement*>(node)->isStyled() || !node->renderer())
            continue;
        renderSubtreeToImage(tileImage.get(), node->renderer());
    }

    context->restore();
    return tileImage.release();
}

void RenderSVGResourcePattern::buildPattern(PatternData* patternData, const FloatRect& patternBoundaries, PassOwnPtr<ImageBuffer> tileImage) const
{
    if (!tileImage->image()) {
        patternData->pattern = 0;
        return;
    }

    IntRect tileRect = tileImage->image()->rect();
    if (tileRect.width() <= patternBoundaries.width() && tileRect.height() <= patternBoundaries.height()) {
        patternData->pattern = Pattern::create(tileImage->image(), true, true);
        return;
    }

    // Draw the first cell of the pattern manually to support overflow="visible" on all platforms.
    int tileWidth = static_cast<int>(patternBoundaries.width() + 0.5f);
    int tileHeight = static_cast<int>(patternBoundaries.height() + 0.5f);

    // Don't create ImageBuffers with image size of 0
    if (!tileWidth || !tileHeight) {
        patternData->pattern = 0;
        return;
    }

    OwnPtr<ImageBuffer> newTileImage = ImageBuffer::create(IntSize(tileWidth, tileHeight));
    GraphicsContext* newTileImageContext = newTileImage->context();

    int numY = static_cast<int>(ceilf(tileRect.height() / tileHeight)) + 1;
    int numX = static_cast<int>(ceilf(tileRect.width() / tileWidth)) + 1;

    newTileImageContext->save();
    newTileImageContext->translate(-patternBoundaries.width() * numX, -patternBoundaries.height() * numY);
    for (int i = numY; i > 0; --i) {
        newTileImageContext->translate(0, patternBoundaries.height());
        for (int j = numX; j > 0; --j) {
            newTileImageContext->translate(patternBoundaries.width(), 0);
            newTileImageContext->drawImage(tileImage->image(), style()->colorSpace(), tileRect, tileRect);
        }
        newTileImageContext->translate(-patternBoundaries.width() * numX, 0);
    }
    newTileImageContext->restore();

    patternData->pattern = Pattern::create(newTileImage->image(), true, true);
}

}

#endif
