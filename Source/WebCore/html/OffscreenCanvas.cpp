/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "OffscreenCanvas.h"

#include "CanvasRenderingContext.h"
#include "ImageBitmap.h"
#include "WebGLRenderingContext.h"

namespace WebCore {

Ref<OffscreenCanvas> OffscreenCanvas::create(ScriptExecutionContext& context, unsigned width, unsigned height)
{
    return adoptRef(*new OffscreenCanvas(context, width, height));
}

OffscreenCanvas::OffscreenCanvas(ScriptExecutionContext& context, unsigned width, unsigned height)
    : CanvasBase(&context)
    , m_size(width, height)
{
}

OffscreenCanvas::~OffscreenCanvas()
{
    m_context = nullptr;
}

unsigned OffscreenCanvas::width() const
{
    return m_size.width();
}

void OffscreenCanvas::setWidth(unsigned newWidth)
{
    return m_size.setWidth(newWidth);
}

unsigned OffscreenCanvas::height() const
{
    return m_size.height();
}

void OffscreenCanvas::setHeight(unsigned newHeight)
{
    return m_size.setHeight(newHeight);
}

const IntSize& OffscreenCanvas::size() const
{
    return m_size;
}

void OffscreenCanvas::setSize(const IntSize& newSize)
{
    m_size = newSize;
}

#if ENABLE(WEBGL)
ExceptionOr<OffscreenRenderingContext> OffscreenCanvas::getContext(JSC::ExecState& state, RenderingContextType contextType, Vector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    if (m_context && contextType == RenderingContextType::Webgl)
        return { RefPtr<WebGLRenderingContext> { &downcast<WebGLRenderingContext>(*m_context) } };

    if (contextType == RenderingContextType::Webgl) {
        auto scope = DECLARE_THROW_SCOPE(state.vm());
        auto attributes = convert<IDLDictionary<WebGLContextAttributes>>(state, !arguments.isEmpty() ? arguments[0].get() : JSC::jsUndefined());
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

        m_context = WebGLRenderingContextBase::create(*this, attributes, "webgl");
        if (!m_context)
            return { nullptr };

        return { RefPtr<WebGLRenderingContext> { &downcast<WebGLRenderingContext>(*m_context) } };
    }

    return { nullptr };
}
#endif

RefPtr<ImageBitmap> OffscreenCanvas::transferToImageBitmap()
{
    if (!m_context)
        return nullptr;

#if ENABLE(WEBGL)
    if (!is<WebGLRenderingContext>(*m_context))
        return nullptr;

    auto webGLContext = &downcast<WebGLRenderingContext>(*m_context);

    // FIXME: We're supposed to create an ImageBitmap using the backing
    // store from this canvas (or its context), but for now we'll just
    // create a new bitmap and paint into it.

    auto imageBitmap = ImageBitmap::create(m_size);
    if (!imageBitmap->buffer())
        return nullptr;

    auto* gc3d = webGLContext->graphicsContext3D();
    gc3d->paintRenderingResultsToCanvas(imageBitmap->buffer());

    // FIXME: The transfer algorithm requires that the canvas effectively
    // creates a new backing store. Since we're not doing that yet, we
    // need to erase what's there.

    GC3Dfloat clearColor[4];
    gc3d->getFloatv(GraphicsContext3D::COLOR_CLEAR_VALUE, clearColor);
    gc3d->clearColor(0, 0, 0, 0);
    gc3d->clear(GraphicsContext3D::COLOR_BUFFER_BIT | GraphicsContext3D::DEPTH_BUFFER_BIT | GraphicsContext3D::STENCIL_BUFFER_BIT);
    gc3d->clearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

    return WTFMove(imageBitmap);
#else
    return nullptr;
#endif
}

}
