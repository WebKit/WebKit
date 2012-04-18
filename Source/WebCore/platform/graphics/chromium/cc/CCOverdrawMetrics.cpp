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
#include <public/Platform.h>

namespace WebCore {

CCOverdrawMetrics::CCOverdrawMetrics(bool recordMetricsForFrame)
    : m_recordMetricsForFrame(recordMetricsForFrame)
    , m_pixelsPainted(0)
    , m_pixelsUploadedOpaque(0)
    , m_pixelsUploadedTranslucent(0)
    , m_tilesCulledForUpload(0)
    , m_pixelsDrawnOpaque(0)
    , m_pixelsDrawnTranslucent(0)
    , m_pixelsCulledForDrawing(0)
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

void CCOverdrawMetrics::didPaint(const IntRect& paintedRect)
{
    if (!m_recordMetricsForFrame)
        return;

    m_pixelsPainted += static_cast<float>(paintedRect.width()) * paintedRect.height();
}

void CCOverdrawMetrics::didCullTileForUpload()
{
    if (m_recordMetricsForFrame)
        ++m_tilesCulledForUpload;
}

void CCOverdrawMetrics::didUpload(const TransformationMatrix& transformToTarget, const IntRect& uploadRect, const IntRect& opaqueRect)
{
    if (!m_recordMetricsForFrame)
        return;

    float uploadArea = quadArea(transformToTarget.mapQuad(FloatQuad(uploadRect)));
    float uploadOpaqueArea = quadArea(transformToTarget.mapQuad(FloatQuad(intersection(opaqueRect, uploadRect))));

    m_pixelsUploadedOpaque += uploadOpaqueArea;
    m_pixelsUploadedTranslucent += uploadArea - uploadOpaqueArea;
}

void CCOverdrawMetrics::didCullForDrawing(const TransformationMatrix& transformToTarget, const IntRect& beforeCullRect, const IntRect& afterCullRect)
{
    if (!m_recordMetricsForFrame)
        return;

    float beforeCullArea = quadArea(transformToTarget.mapQuad(FloatQuad(beforeCullRect)));
    float afterCullArea = quadArea(transformToTarget.mapQuad(FloatQuad(afterCullRect)));

    m_pixelsCulledForDrawing += beforeCullArea - afterCullArea;
}

void CCOverdrawMetrics::didDraw(const TransformationMatrix& transformToTarget, const IntRect& afterCullRect, const IntRect& opaqueRect)
{
    if (!m_recordMetricsForFrame)
        return;

    float afterCullArea = quadArea(transformToTarget.mapQuad(FloatQuad(afterCullRect)));
    float afterCullOpaqueArea = quadArea(transformToTarget.mapQuad(FloatQuad(intersection(opaqueRect, afterCullRect))));

    m_pixelsDrawnOpaque += afterCullOpaqueArea;
    m_pixelsDrawnTranslucent += afterCullArea - afterCullOpaqueArea;
}

void CCOverdrawMetrics::recordMetrics(const CCLayerTreeHost* layerTreeHost) const
{
    if (m_recordMetricsForFrame)
        recordMetricsInternal<CCLayerTreeHost>(UpdateAndCommit, layerTreeHost);
}

void CCOverdrawMetrics::recordMetrics(const CCLayerTreeHostImpl* layerTreeHost) const
{
    if (m_recordMetricsForFrame)
        recordMetricsInternal<CCLayerTreeHostImpl>(DrawingToScreen, layerTreeHost);
}

template<typename LayerTreeHostType>
void CCOverdrawMetrics::recordMetricsInternal(MetricsType metricsType, const LayerTreeHostType* layerTreeHost) const
{
    // This gives approximately 10x the percentage of pixels to fill the viewport once.
    float normalization = 1000.f / (layerTreeHost->viewportSize().width() * layerTreeHost->viewportSize().height());
    // This gives approximately 100x the percentage of tiles to fill the viewport once, if all tiles were 256x256.
    float tileNormalization = 10000.f / (layerTreeHost->viewportSize().width() / 256.f * layerTreeHost->viewportSize().height() / 256.f);

    switch (metricsType) {
    case DrawingToScreen:
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.pixelCountOpaque_Draw", static_cast<int>(normalization * m_pixelsDrawnOpaque), 100, 1000000, 50);
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.pixelCountTranslucent_Draw", static_cast<int>(normalization * m_pixelsDrawnTranslucent), 100, 1000000, 50);
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.pixelCountCulled_Draw", static_cast<int>(normalization * m_pixelsCulledForDrawing), 100, 1000000, 50);

        {
            TRACE_COUNTER_ID1("webkit", "DrawPixelsCulled", layerTreeHost, m_pixelsCulledForDrawing);
            TRACE_EVENT2("webkit", "CCOverdrawMetrics", "PixelsDrawnOpaque", m_pixelsDrawnOpaque, "PixelsDrawnTranslucent", m_pixelsDrawnTranslucent);
        }
        break;
    case UpdateAndCommit:
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.pixelCountPainted", static_cast<int>(normalization * m_pixelsPainted), 100, 1000000, 50);
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.pixelCountOpaque_Upload", static_cast<int>(normalization * m_pixelsUploadedOpaque), 100, 1000000, 50);
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.pixelCountTranslucent_Upload", static_cast<int>(normalization * m_pixelsUploadedTranslucent), 100, 1000000, 50);
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.tileCountCulled_Upload", static_cast<int>(tileNormalization * m_tilesCulledForUpload), 100, 10000000, 50);

        {
            TRACE_COUNTER_ID1("webkit", "UploadTilesCulled", layerTreeHost, m_tilesCulledForUpload);
            TRACE_EVENT2("webkit", "CCOverdrawMetrics", "PixelsUploadedOpaque", m_pixelsUploadedOpaque, "PixelsUploadedTranslucent", m_pixelsUploadedTranslucent);
        }
        {
            // This must be in a different scope than the TRACE_EVENT2 above.
            TRACE_EVENT1("webkit", "CCOverdrawPaintMetrics", "PixelsPainted", m_pixelsPainted);
        }
        break;
    }
}

} // namespace WebCore

#endif
