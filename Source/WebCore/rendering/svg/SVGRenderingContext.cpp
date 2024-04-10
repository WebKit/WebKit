/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2018 Adobe Systems Incorporated. All rights reserved.
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
#include "SVGRenderingContext.h"

#include "BasicShapes.h"
#include "LegacyRenderSVGImage.h"
#include "LegacyRenderSVGResourceClipper.h"
#include "LegacyRenderSVGResourceFilter.h"
#include "LegacyRenderSVGResourceMasker.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGGraphicsElement.h"
#include "SVGLengthContext.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"
#include <wtf/MathExtras.h>

namespace WebCore {

static inline bool isRenderingMaskImage(const RenderObject& object)
{
    return object.view().frameView().paintBehavior().contains(PaintBehavior::RenderingSVGClipOrMask);
}

SVGRenderingContext::~SVGRenderingContext()
{
    // Fast path if we don't need to restore anything.
    if (!(m_renderingFlags & ActionsNeeded))
        return;

    ASSERT(m_renderer && m_paintInfo);

    if (m_renderingFlags & EndFilterLayer) {
        ASSERT(m_filter);
        GraphicsContext* contextPtr = &m_paintInfo->context();
        m_filter->postApplyResource(*m_renderer, contextPtr, { }, nullptr, nullptr);
        m_paintInfo->setContext(*m_savedContext);
        m_paintInfo->rect = m_savedPaintRect;
    }

    if (m_renderingFlags & EndOpacityLayer)
        m_paintInfo->context().endTransparencyLayer();

    if (m_renderingFlags & RestoreGraphicsContext)
        m_paintInfo->context().restore();
}

void SVGRenderingContext::prepareToRenderSVGContent(RenderElement& renderer, PaintInfo& paintInfo, NeedsGraphicsContextSave needsGraphicsContextSave)
{
#ifndef NDEBUG
    // This function must not be called twice!
    ASSERT(!(m_renderingFlags & PrepareToRenderSVGContentWasCalled));
    m_renderingFlags |= PrepareToRenderSVGContentWasCalled;
#endif

    m_renderer = &renderer;
    m_paintInfo = &paintInfo;
    m_filter = nullptr;

    // We need to save / restore the context even if the initialization failed.
    if (needsGraphicsContextSave == SaveGraphicsContext) {
        m_paintInfo->context().save();
        m_renderingFlags |= RestoreGraphicsContext;
    }

    auto& style = m_renderer->style();

    // Setup transparency layers before setting up SVG resources!
    bool isRenderingMask = isRenderingMaskImage(*m_renderer);
    // RenderLayer takes care of root opacity.
    float opacity = (renderer.isLegacyRenderSVGRoot() || isRenderingMask) ? 1 : style.opacity();
    bool hasBlendMode = style.hasBlendMode();
    bool hasIsolation = style.hasIsolation();
    bool isolateMaskForBlending = false;

    if (style.hasPositionedMask()) {
        if (auto* graphicsElement = dynamicDowncast<SVGGraphicsElement>(*renderer.element()))
            isolateMaskForBlending = graphicsElement->shouldIsolateBlending();
    }

    if (opacity < 1 || hasBlendMode || isolateMaskForBlending || hasIsolation) {
        FloatRect repaintRect = m_renderer->repaintRectInLocalCoordinates();
        m_paintInfo->context().clip(repaintRect);

        if (opacity < 1 || hasBlendMode || isolateMaskForBlending || hasIsolation) {

            if (hasBlendMode)
                m_paintInfo->context().setCompositeOperation(m_paintInfo->context().compositeOperation(), style.blendMode());

            m_paintInfo->context().beginTransparencyLayer(opacity);

            if (hasBlendMode)
                m_paintInfo->context().setCompositeOperation(m_paintInfo->context().compositeOperation(), BlendMode::Normal);

            m_renderingFlags |= EndOpacityLayer;
        }
    }

    PathOperation* clipPathOperation = style.clipPath();
    bool hasCSSClipping = is<ShapePathOperation>(clipPathOperation) || is<BoxPathOperation>(clipPathOperation);
    if (hasCSSClipping)
        SVGRenderSupport::clipContextToCSSClippingArea(m_paintInfo->context(), renderer);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    // FIXME: Text painting under LBSE reaches this code path, since all text painting code is shared between legacy / LBSE.
    SVGResources* resources = nullptr;
    if (!renderer.document().settings().layerBasedSVGEngineEnabled())
        resources = SVGResourcesCache::cachedResourcesForRenderer(*m_renderer);
#else
    auto* resources = SVGResourcesCache::cachedResourcesForRenderer(*m_renderer);
#endif

    if (!resources) {
        if (style.hasReferenceFilterOnly())
            return;

        m_renderingFlags |= RenderingPrepared;
        return;
    }

    if (!isRenderingMask) {
        if (LegacyRenderSVGResourceMasker* masker = resources->masker()) {
            GraphicsContext* contextPtr = &m_paintInfo->context();
            bool result = masker->applyResource(*m_renderer, style, contextPtr, { });
            m_paintInfo->setContext(*contextPtr);
            if (!result)
                return;
        }
    }

    LegacyRenderSVGResourceClipper* clipper = resources->clipper();
    if (!hasCSSClipping && clipper) {
        GraphicsContext* contextPtr = &m_paintInfo->context();
        bool result = clipper->applyResource(*m_renderer, style, contextPtr, { });
        m_paintInfo->setContext(*contextPtr);
        if (!result)
            return;
    }

    if (!isRenderingMask) {
        m_filter = resources->filter();
        if (m_filter && !m_filter->isIdentity()) {
            m_savedContext = &m_paintInfo->context();
            m_savedPaintRect = m_paintInfo->rect;
            // Return with false here may mean that we don't need to draw the content
            // (because it was either drawn before or empty) but we still need to apply the filter.
            m_renderingFlags |= EndFilterLayer;
            GraphicsContext* contextPtr = &m_paintInfo->context();
            bool result = m_filter->applyResource(*m_renderer, style, contextPtr, { });
            m_paintInfo->setContext(*contextPtr);
            if (!result)
                return;

            // Since we're caching the resulting bitmap and do not invalidate it on repaint rect
            // changes, we need to paint the whole filter region. Otherwise, elements not visible
            // at the time of the initial paint (due to scrolling, window size, etc.) will never
            // be drawn.
            m_paintInfo->rect = IntRect(m_filter->drawingRegion(*m_renderer));
        }
    }

    m_renderingFlags |= RenderingPrepared;
}

static AffineTransform& currentContentTransformation()
{
    static NeverDestroyed<AffineTransform> s_currentContentTransformation;
    return s_currentContentTransformation;
}

float SVGRenderingContext::calculateScreenFontSizeScalingFactor(const RenderObject& renderer)
{
    AffineTransform ctm = calculateTransformationToOutermostCoordinateSystem(renderer);
    return narrowPrecisionToFloat(std::hypot(ctm.xScale(), ctm.yScale()) / sqrtOfTwoDouble);
}

AffineTransform SVGRenderingContext::calculateTransformationToOutermostCoordinateSystem(const RenderObject& renderer)
{
    AffineTransform absoluteTransform = currentContentTransformation();

    float deviceScaleFactor = renderer.document().deviceScaleFactor();
    // Walk up the render tree, accumulating SVG transforms.
    const RenderObject* ancestor = &renderer;
    while (ancestor) {
        absoluteTransform = ancestor->localToParentTransform() * absoluteTransform;
        if (ancestor->isRenderOrLegacyRenderSVGRoot())
            break;
        ancestor = ancestor->parent();
    }

    // Continue walking up the layer tree, accumulating CSS transforms.
    RenderLayer* layer = ancestor ? ancestor->enclosingLayer() : nullptr;
    while (layer) {
        if (TransformationMatrix* layerTransform = layer->transform())
            absoluteTransform = layerTransform->toAffineTransform() * absoluteTransform;

        // We can stop at compositing layers, to match the backing resolution.
        if (layer->isComposited())
            break;

        layer = layer->parent();
    }

    absoluteTransform.scale(deviceScaleFactor);
    return absoluteTransform;
}

void SVGRenderingContext::renderSubtreeToContext(GraphicsContext& context, RenderElement& item, const AffineTransform& subtreeContentTransformation)
{
    // Rendering into a buffer implies we're being used for masking, clipping, patterns or filters. In each of these
    // cases we don't want to paint the selection.
    PaintInfo info(context, LayoutRect::infiniteRect(), PaintPhase::Foreground, PaintBehavior::SkipSelectionHighlight);

    AffineTransform& contentTransformation = currentContentTransformation();
    AffineTransform savedContentTransformation = contentTransformation;
    contentTransformation = subtreeContentTransformation * contentTransformation;

    ASSERT(!item.needsLayout());
    item.paint(info, { });

    contentTransformation = savedContentTransformation;
}

void SVGRenderingContext::clipToImageBuffer(GraphicsContext& context, const FloatRect& targetRect, const FloatSize& scale, RefPtr<ImageBuffer>& imageBuffer, bool safeToClear)
{
    if (!imageBuffer)
        return;

    AffineTransform absoluteTransform;
    absoluteTransform.scale(scale.width(), scale.height());

    auto absoluteTargetRect = calculateImageBufferRect(targetRect, absoluteTransform);

    // The mask image has been created in the absolute coordinate space, as the image should not be scaled.
    // So the actual masking process has to be done in the absolute coordinate space as well.
    context.concatCTM(valueOrDefault(absoluteTransform.inverse()));
    context.clipToImageBuffer(*imageBuffer, absoluteTargetRect);
    context.concatCTM(absoluteTransform);

    // When nesting resources, with objectBoundingBox as content unit types, there's no use in caching the
    // resulting image buffer as the parent resource already caches the result.
    if (safeToClear && !currentContentTransformation().isIdentity())
        imageBuffer = nullptr;
}

bool SVGRenderingContext::bufferForeground(RefPtr<ImageBuffer>& imageBuffer)
{
    ASSERT(m_paintInfo);
    ASSERT(is<LegacyRenderSVGImage>(*m_renderer));
    FloatRect boundingBox = m_renderer->objectBoundingBox();

    // Invalidate an existing buffer if the scale is not correct.
    if (imageBuffer) {
        AffineTransform transform = m_paintInfo->context().getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
        IntSize expandedBoundingBox = expandedIntSize(boundingBox.size());
        IntSize bufferSize(static_cast<int>(ceil(expandedBoundingBox.width() * transform.xScale())), static_cast<int>(ceil(expandedBoundingBox.height() * transform.yScale())));
        if (bufferSize != imageBuffer->backendSize())
            imageBuffer = nullptr;
    }

    // Create a new buffer and paint the foreground into it.
    if (!imageBuffer) {
        imageBuffer = m_paintInfo->context().createAlignedImageBuffer(expandedIntSize(boundingBox.size()));
        if (!imageBuffer)
            return false;
    }

    GraphicsContext& bufferedRenderingContext = imageBuffer->context();
    bufferedRenderingContext.translate(-boundingBox.location());
    PaintInfo bufferedInfo(*m_paintInfo);
    bufferedInfo.setContext(bufferedRenderingContext);
    downcast<LegacyRenderSVGImage>(*m_renderer).paintForeground(bufferedInfo);

    m_paintInfo->context().drawImageBuffer(*imageBuffer, boundingBox);
    return true;
}

}
