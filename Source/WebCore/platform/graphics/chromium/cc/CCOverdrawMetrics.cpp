/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "cc/CCOverdrawMetrics.h"

#include "FloatQuad.h"
#include "IntRect.h"
#include "TraceEvent.h"
#include "TransformationMatrix.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCLayerTreeHostImpl.h"

namespace WebCore {

CCOverdrawMetrics::CCOverdrawMetrics()
    : m_pixelsDrawnOpaque(0)
    , m_pixelsDrawnTranslucent(0)
    , m_pixelsCulled(0)
{
}

static inline float wedgeProduct(const FloatPoint& p1, const FloatPoint& p2)
{
    return p1.x() * p2.y() - p1.y() * p2.x();
}

// Computes area of quads that are possibly non-rectangular. Can be easily extended to polygons.
static inline float quadArea(const FloatQuad& quad)
{
    return fabs(0.5 * (wedgeProduct(quad.p1(), quad.p2()) +
                       wedgeProduct(quad.p2(), quad.p3()) +
                       wedgeProduct(quad.p3(), quad.p4()) +
                       wedgeProduct(quad.p4(), quad.p1())));
}

void CCOverdrawMetrics::didCull(const TransformationMatrix& transformToTarget, const IntRect& beforeCullRect, const IntRect& afterCullRect)
{
    float beforeCullArea = quadArea(transformToTarget.mapQuad(FloatQuad(beforeCullRect)));
    float afterCullArea = quadArea(transformToTarget.mapQuad(FloatQuad(afterCullRect)));

    m_pixelsCulled += beforeCullArea - afterCullArea;
}

void CCOverdrawMetrics::didDraw(const TransformationMatrix& transformToTarget, const IntRect& afterCullRect, const IntRect& opaqueRect)
{
    float afterCullArea = quadArea(transformToTarget.mapQuad(FloatQuad(afterCullRect)));
    float afterCullOpaqueArea = quadArea(transformToTarget.mapQuad(FloatQuad(intersection(opaqueRect, afterCullRect))));

    m_pixelsDrawnOpaque += afterCullOpaqueArea;
    m_pixelsDrawnTranslucent += afterCullArea - afterCullOpaqueArea;
}

void CCOverdrawMetrics::recordMetrics(const CCLayerTreeHost* layerTreeHost) const
{
    recordMetricsInternal<CCLayerTreeHost>(UPLOADING, layerTreeHost);
}

void CCOverdrawMetrics::recordMetrics(const CCLayerTreeHostImpl* layerTreeHost) const
{
    recordMetricsInternal<CCLayerTreeHostImpl>(DRAWING, layerTreeHost);
}

template<typename LayerTreeHostType>
void CCOverdrawMetrics::recordMetricsInternal(MetricsType metricsType, const LayerTreeHostType* layerTreeHost) const
{
    const char* histogramOpaqueName = 0;
    const char* histogramTranslucentName = 0;
    const char* histogramCulledName = 0;
    const char* cullCounterName = 0;
    const char* opaqueCounterName = 0;
    const char* translucentCounterName = 0;
    switch (metricsType) {
    case DRAWING:
        histogramOpaqueName = "Renderer4.drawPixelCountOpaque";
        histogramTranslucentName = "Renderer4.drawPixelCountTranslucent";
        histogramCulledName = "Renderer4.drawPixelCountCulled";
        cullCounterName = "DrawPixelsCulled";
        opaqueCounterName = "PixelsDrawnOpaque";
        translucentCounterName = "PixelsDrawnTranslucent";
        break;
    case UPLOADING:
        histogramOpaqueName = "Renderer4.uploadPixelCountOpaque";
        histogramTranslucentName = "Renderer4.uploadPixelCountTranslucent";
        histogramCulledName = "Renderer4.uploadPixelCountCulled";
        cullCounterName = "UploadPixelsCulled";
        opaqueCounterName = "PixelsUploadedOpaque";
        translucentCounterName = "PixelsUploadedTranslucent";
        break;
    }
    ASSERT(histogramOpaqueName);

    float normalization = 1000.f / (layerTreeHost->viewportSize().width() * layerTreeHost->viewportSize().height());
    PlatformSupport::histogramCustomCounts(histogramOpaqueName, static_cast<int>(normalization * m_pixelsDrawnOpaque), 100, 1000000, 50);
    PlatformSupport::histogramCustomCounts(histogramTranslucentName, static_cast<int>(normalization * m_pixelsDrawnTranslucent), 100, 1000000, 50);
    PlatformSupport::histogramCustomCounts(histogramCulledName, static_cast<int>(normalization * m_pixelsCulled), 100, 1000000, 50);

    TRACE_COUNTER_ID1("webkit", cullCounterName, layerTreeHost, m_pixelsCulled);
    TRACE_EVENT2("webkit", "CCOverdrawMetrics", opaqueCounterName, m_pixelsDrawnOpaque, translucentCounterName, m_pixelsDrawnTranslucent);
}

} // namespace WebCore

#endif
