/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "Canvas2DLayerChromium.h"

#include "DrawingBuffer.h"
#include "GraphicsContext3D.h"

namespace WebCore {

PassRefPtr<Canvas2DLayerChromium> Canvas2DLayerChromium::create(DrawingBuffer* drawingBuffer, GraphicsLayerChromium* owner)
{
    return adoptRef(new Canvas2DLayerChromium(drawingBuffer, owner));
}

Canvas2DLayerChromium::Canvas2DLayerChromium(DrawingBuffer* drawingBuffer, GraphicsLayerChromium* owner)
    : CanvasLayerChromium(owner)
    , m_drawingBuffer(drawingBuffer)
{
}

Canvas2DLayerChromium::~Canvas2DLayerChromium()
{
    if (m_textureId)
        layerRendererContext()->deleteTexture(m_textureId);
}

void Canvas2DLayerChromium::updateContents()
{
    if (!m_drawingBuffer)
        return;
    if (m_textureChanged) { // We have to generate a new backing texture.
        GraphicsContext3D* context = layerRendererContext();
        if (m_textureId)
            context->deleteTexture(m_textureId);
        m_textureId = context->createTexture();
        context->activeTexture(GraphicsContext3D::TEXTURE0);
        context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_textureId);
        IntSize size = m_drawingBuffer->size();
        context->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, size.width(), size.height(), 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, 0);
        // Set the min-mag filters to linear and wrap modes to GraphicsContext3D::CLAMP_TO_EDGE
        // to get around NPOT texture limitations of GLES.
        context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
        context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
        context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
        context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
        m_textureChanged = false;
        // FIXME: The finish() here is required because we have to make sure that the texture created in this
        // context (the compositor context) is actually created by the service side before the child context
        // attempts to use it (in publishToPlatformLayer).  finish() is currently the only call with strong
        // enough semantics to promise this, but is actually much stronger.  Ideally we'd do something like
        // inserting a fence here and waiting for it before trying to publish.
        context->finish();
    }
    // Update the contents of the texture used by the compositor.
    if (m_contentsDirty) {
        m_drawingBuffer->publishToPlatformLayer();
        m_contentsDirty = false;
    }
}

void Canvas2DLayerChromium::setTextureChanged()
{
    m_textureChanged = true;
}

unsigned Canvas2DLayerChromium::textureId() const
{
    return m_textureId;
}

void Canvas2DLayerChromium::setDrawingBuffer(DrawingBuffer* drawingBuffer)
{
    if (drawingBuffer != m_drawingBuffer) {
        m_drawingBuffer = drawingBuffer;
        m_textureChanged = true;
    }
}

}
#endif // USE(ACCELERATED_COMPOSITING)
