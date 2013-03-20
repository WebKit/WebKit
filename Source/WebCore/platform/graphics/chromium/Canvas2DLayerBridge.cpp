/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Canvas2DLayerBridge.h"

#include "Canvas2DLayerManager.h"
#include "GrContext.h"
#include "GraphicsContext3D.h"
#include "GraphicsContext3DPrivate.h"
#include "GraphicsLayerChromium.h"
#include "SkDevice.h"
#include "TraceEvent.h"
#include <public/Platform.h>
#include <public/WebCompositorSupport.h>
#include <public/WebGraphicsContext3D.h>

using WebKit::WebExternalTextureLayer;
using WebKit::WebGraphicsContext3D;
using WebKit::WebTextureUpdater;

namespace WebCore {

Canvas2DLayerBridge::Canvas2DLayerBridge(PassRefPtr<GraphicsContext3D> context, const IntSize& size, ThreadMode threadMode, unsigned textureId)
    : m_backBufferTexture(textureId)
    , m_size(size)
    , m_canvas(0)
    , m_context(context)
    , m_bytesAllocated(0)
    , m_didRecordDrawCommand(false)
    , m_framesPending(0)
    , m_next(0)
    , m_prev(0)
{
    // Used by browser tests to detect the use of a Canvas2DLayerBridge.
    TRACE_EVENT_INSTANT0("test_gpu", "Canvas2DLayerBridgeCreation");

    m_layer = adoptPtr(WebKit::Platform::current()->compositorSupport()->createExternalTextureLayer(this));
    m_layer->setTextureId(textureId);
    m_layer->setRateLimitContext(threadMode == SingleThread);
    GraphicsLayerChromium::registerContentsLayer(m_layer->layer());
}

Canvas2DLayerBridge::~Canvas2DLayerBridge()
{
    GraphicsLayerChromium::unregisterContentsLayer(m_layer->layer());
    Canvas2DLayerManager::get().layerToBeDestroyed(this);
    if (m_canvas)
        m_canvas->setNotificationClient(0);
    m_layer->setTextureId(0);
}

void Canvas2DLayerBridge::limitPendingFrames()
{
    if (m_didRecordDrawCommand) {
        m_framesPending++;
        m_didRecordDrawCommand = false;
        if (m_framesPending > 1)
            flush();
    }
}

void Canvas2DLayerBridge::prepareForDraw()
{
    m_layer->willModifyTexture();
    m_context->makeContextCurrent();
}

void Canvas2DLayerBridge::storageAllocatedForRecordingChanged(size_t bytesAllocated)
{
    intptr_t delta = (intptr_t)bytesAllocated - (intptr_t)m_bytesAllocated;
    m_bytesAllocated = bytesAllocated;
    Canvas2DLayerManager::get().layerAllocatedStorageChanged(this, delta);
}

size_t Canvas2DLayerBridge::storageAllocatedForRecording()
{
    return m_canvas ? m_canvas->storageAllocatedForRecording() : 0;
}

void Canvas2DLayerBridge::flushedDrawCommands()
{
    storageAllocatedForRecordingChanged(storageAllocatedForRecording());
    m_framesPending = 0;
}

void Canvas2DLayerBridge::skippedPendingDrawCommands()
{
    flushedDrawCommands();
}

size_t Canvas2DLayerBridge::freeMemoryIfPossible(size_t bytesToFree)
{
    size_t bytesFreed = m_canvas ? m_canvas->freeMemoryIfPossible(bytesToFree) : 0;
    if (bytesFreed)
        Canvas2DLayerManager::get().layerAllocatedStorageChanged(this, -((intptr_t)bytesFreed));
    m_bytesAllocated -= bytesFreed;
    return bytesFreed;
}

void Canvas2DLayerBridge::flush()
{
    if (m_canvas && m_canvas->hasPendingCommands())
        m_canvas->flush();
}

SkCanvas* Canvas2DLayerBridge::skCanvas(SkDevice* device)
{
    ASSERT(!m_canvas);
    m_canvas = new SkDeferredCanvas(device);
    m_canvas->setNotificationClient(this);
    return m_canvas;
}


unsigned Canvas2DLayerBridge::prepareTexture(WebTextureUpdater& updater)
{
    m_context->makeContextCurrent();

    if (m_canvas) {
        TRACE_EVENT0("cc", "Canvas2DLayerBridge::SkCanvas::flush");
        m_canvas->flush();
    }

    m_context->flush();

    if (m_canvas) {
        // Notify skia that the state of the backing store texture object will be touched by the compositor
        GrRenderTarget* renderTarget = reinterpret_cast<GrRenderTarget*>(m_canvas->getDevice()->accessRenderTarget());
        if (renderTarget)
            renderTarget->asTexture()->invalidateCachedState();
    }
    return m_backBufferTexture;
}

WebGraphicsContext3D* Canvas2DLayerBridge::context()
{
    return GraphicsContext3DPrivate::extractWebGraphicsContext3D(m_context.get());
}

WebKit::WebLayer* Canvas2DLayerBridge::layer()
{
    return m_layer->layer();
}

void Canvas2DLayerBridge::contextAcquired()
{
    Canvas2DLayerManager::get().layerDidDraw(this);
    m_didRecordDrawCommand = true;
}

unsigned Canvas2DLayerBridge::backBufferTexture()
{
    contextAcquired();
    if (m_canvas)
        m_canvas->flush();
    m_context->flush();
    return m_backBufferTexture;
}

}
