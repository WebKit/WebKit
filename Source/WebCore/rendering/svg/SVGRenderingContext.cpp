/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#if ENABLE(SVG)
#include "SVGRenderingContext.h"

#include "Frame.h"
#include "FrameView.h"
#include "RenderSVGResource.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceFilter.h"
#include "RenderSVGResourceMasker.h"
#include "SVGImageBufferTools.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"

namespace WebCore {

static inline bool isRenderingMaskImage(RenderObject* object)
{
    if (object->frame() && object->frame()->view())
        return object->frame()->view()->paintBehavior() & PaintBehaviorRenderingSVGMask;
    return false;
}

SVGRenderingContext::~SVGRenderingContext()
{
    // Fast path if we don't need to restore anything.
    if (!(m_renderingFlags & ActionsNeeded))
        return;

    ASSERT(m_object && m_paintInfo);

#if ENABLE(FILTERS)
    if (m_renderingFlags & EndFilterLayer) {
        ASSERT(m_filter);
        m_filter->postApplyResource(static_cast<RenderSVGShape*>(m_object), m_paintInfo->context, ApplyToDefaultMode, 0, 0);
        m_paintInfo->context = m_savedContext;
    }
#endif

    if (m_renderingFlags & EndOpacityLayer)
        m_paintInfo->context->endTransparencyLayer();

    if (m_renderingFlags & EndShadowLayer)
        m_paintInfo->context->endTransparencyLayer();

    if (m_renderingFlags & RestoreGraphicsContext)
        m_paintInfo->context->restore();
}

void SVGRenderingContext::prepareToRenderSVGContent(RenderObject* object, PaintInfo& paintInfo, NeedsGraphicsContextSave needsGraphicsContextSave)
{
    ASSERT(object);

#ifndef NDEBUG
    // This function must not be called twice!
    ASSERT(!(m_renderingFlags & PrepareToRenderSVGContentWasCalled));
    m_renderingFlags |= PrepareToRenderSVGContentWasCalled;
#endif

    m_object = object;
    m_paintInfo = &paintInfo;
#if ENABLE(FILTERS)
    m_filter = 0;
#endif

    // We need to save / restore the context even if the initialization failed.
    if (needsGraphicsContextSave == SaveGraphicsContext) {
        m_paintInfo->context->save();
        m_renderingFlags |= RestoreGraphicsContext;
    }

    RenderStyle* style = m_object->style();
    ASSERT(style);

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    // Setup transparency layers before setting up SVG resources!
    bool isRenderingMask = isRenderingMaskImage(m_object);
    float opacity = isRenderingMask ? 1 : style->opacity();
    const ShadowData* shadow = svgStyle->shadow();
    if (opacity < 1 || shadow) {
        FloatRect repaintRect = m_object->repaintRectInLocalCoordinates();

        if (opacity < 1) {
            m_paintInfo->context->clip(repaintRect);
            m_paintInfo->context->beginTransparencyLayer(opacity);
            m_renderingFlags |= EndOpacityLayer;
        }

        if (shadow) {
            m_paintInfo->context->clip(repaintRect);
            m_paintInfo->context->setShadow(IntSize(roundToInt(shadow->x()), roundToInt(shadow->y())), shadow->blur(), shadow->color(), style->colorSpace());
            m_paintInfo->context->beginTransparencyLayer(1);
            m_renderingFlags |= EndShadowLayer;
        }
    }

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(m_object);
    if (!resources) {
#if ENABLE(FILTERS)
        if (svgStyle->hasFilter())
            return;
#endif
        m_renderingFlags |= RenderingPrepared;
        return;
    }

    if (!isRenderingMask) {
        if (RenderSVGResourceMasker* masker = resources->masker()) {
            if (!masker->applyResource(m_object, style, m_paintInfo->context, ApplyToDefaultMode))
                return;
        }
    }

    if (RenderSVGResourceClipper* clipper = resources->clipper()) {
        if (!clipper->applyResource(m_object, style, m_paintInfo->context, ApplyToDefaultMode))
            return;
    }

#if ENABLE(FILTERS)
    if (!isRenderingMask) {
        m_filter = resources->filter();
        if (m_filter) {
            m_savedContext = m_paintInfo->context;
            // Return with false here may mean that we don't need to draw the content
            // (because it was either drawn before or empty) but we still need to apply the filter.
            m_renderingFlags |= EndFilterLayer;
            if (!m_filter->applyResource(m_object, style, m_paintInfo->context, ApplyToDefaultMode))
                return;
        }
    }
#endif

    m_renderingFlags |= RenderingPrepared;
}

}

#endif
