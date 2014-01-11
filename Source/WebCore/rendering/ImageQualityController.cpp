/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageQualityController.h"

#include "Frame.h"
#include "GraphicsContext.h"
#include "Page.h"
#include "RenderBoxModelObject.h"
#include "RenderView.h"

namespace WebCore {

static const double cInterpolationCutoff = 800. * 800.;
static const double cLowQualityTimeThreshold = 0.500; // 500 ms

ImageQualityController::ImageQualityController(const RenderView& renderView)
    : m_renderView(renderView)
    , m_timer(this, &ImageQualityController::highQualityRepaintTimerFired)
    , m_animatedResizeIsActive(false)
    , m_liveResizeOptimizationIsActive(false)
{
}

void ImageQualityController::removeLayer(RenderBoxModelObject* object, LayerSizeMap* innerMap, const void* layer)
{
    if (!innerMap)
        return;
    innerMap->remove(layer);
    if (innerMap->isEmpty())
        removeObject(object);
}

void ImageQualityController::set(RenderBoxModelObject* object, LayerSizeMap* innerMap, const void* layer, const LayoutSize& size)
{
    if (innerMap)
        innerMap->set(layer, size);
    else {
        LayerSizeMap newInnerMap;
        newInnerMap.set(layer, size);
        m_objectLayerSizeMap.set(object, newInnerMap);
    }
}

void ImageQualityController::removeObject(RenderBoxModelObject* object)
{
    m_objectLayerSizeMap.remove(object);
    if (m_objectLayerSizeMap.isEmpty()) {
        m_animatedResizeIsActive = false;
        m_timer.stop();
    }
}

void ImageQualityController::highQualityRepaintTimerFired(Timer<ImageQualityController>&)
{
    if (m_renderView.documentBeingDestroyed())
        return;
    if (!m_animatedResizeIsActive && !m_liveResizeOptimizationIsActive)
        return;
    m_animatedResizeIsActive = false;

    // If the FrameView is in live resize, punt the timer and hold back for now.
    if (m_renderView.frameView().inLiveResize()) {
        restartTimer();
        return;
    }

    for (auto it = m_objectLayerSizeMap.begin(), end = m_objectLayerSizeMap.end(); it != end; ++it)
        it->key->repaint();

    m_liveResizeOptimizationIsActive = false;
}

void ImageQualityController::restartTimer()
{
    m_timer.startOneShot(cLowQualityTimeThreshold);
}

bool ImageQualityController::shouldPaintAtLowQuality(GraphicsContext* context, RenderBoxModelObject* object, Image* image, const void *layer, const LayoutSize& size)
{
    // If the image is not a bitmap image, then none of this is relevant and we just paint at high
    // quality.
    if (!image || !(image->isBitmapImage() || image->isPDFDocumentImage()) || context->paintingDisabled())
        return false;

    switch (object->style().imageRendering()) {
    case ImageRenderingOptimizeSpeed:
    case ImageRenderingCrispEdges:
        return true;
    case ImageRenderingOptimizeQuality:
        return false;
    case ImageRenderingAuto:
        break;
    }

    // Make sure to use the unzoomed image size, since if a full page zoom is in effect, the image
    // is actually being scaled.
    IntSize imageSize(image->width(), image->height());

    // Look ourselves up in the hashtables.
    auto i = m_objectLayerSizeMap.find(object);
    LayerSizeMap* innerMap = i != m_objectLayerSizeMap.end() ? &i->value : 0;
    LayoutSize oldSize;
    bool isFirstResize = true;
    if (innerMap) {
        LayerSizeMap::iterator j = innerMap->find(layer);
        if (j != innerMap->end()) {
            isFirstResize = false;
            oldSize = j->value;
        }
    }

    // If the containing FrameView is being resized, paint at low quality until resizing is finished.
    if (Frame* frame = object->document().frame()) {
        bool frameViewIsCurrentlyInLiveResize = frame->view() && frame->view()->inLiveResize();
        if (frameViewIsCurrentlyInLiveResize) {
            set(object, innerMap, layer, size);
            restartTimer();
            m_liveResizeOptimizationIsActive = true;
            return true;
        }
        if (m_liveResizeOptimizationIsActive)
            return false;
    }

    const AffineTransform& currentTransform = context->getCTM();
    bool contextIsScaled = !currentTransform.isIdentityOrTranslationOrFlipped();
    if (!contextIsScaled && size == imageSize) {
        // There is no scale in effect. If we had a scale in effect before, we can just remove this object from the list.
        removeLayer(object, innerMap, layer);
        return false;
    }

    // There is no need to hash scaled images that always use low quality mode when the page demands it. This is the iChat case.
    if (m_renderView.frame().page()->inLowQualityImageInterpolationMode()) {
        double totalPixels = static_cast<double>(image->width()) * static_cast<double>(image->height());
        if (totalPixels > cInterpolationCutoff)
            return true;
    }

    // If an animated resize is active, paint in low quality and kick the timer ahead.
    if (m_animatedResizeIsActive) {
        set(object, innerMap, layer, size);
        restartTimer();
        return true;
    }
    // If this is the first time resizing this image, or its size is the
    // same as the last resize, draw at high res, but record the paint
    // size and set the timer.
    if (isFirstResize || oldSize == size) {
        restartTimer();
        set(object, innerMap, layer, size);
        return false;
    }
    // If the timer is no longer active, draw at high quality and don't
    // set the timer.
    if (!m_timer.isActive()) {
        removeLayer(object, innerMap, layer);
        return false;
    }
    // This object has been resized to two different sizes while the timer
    // is active, so draw at low quality, set the flag for animated resizes and
    // the object to the list for high quality redraw.
    set(object, innerMap, layer, size);
    m_animatedResizeIsActive = true;
    restartTimer();
    return true;
}

}
