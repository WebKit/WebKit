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
#include "ImageBitmapRenderingContext.h"

#include "HTMLCanvasElement.h"
#include "ImageBitmap.h"
#include "ImageBuffer.h"
#include "InspectorInstrumentation.h"
#include "OffscreenCanvas.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ImageBitmapRenderingContext);

std::unique_ptr<ImageBitmapRenderingContext> ImageBitmapRenderingContext::create(CanvasBase& canvas, ImageBitmapRenderingContextSettings&& settings)
{
    auto renderingContext = std::unique_ptr<ImageBitmapRenderingContext>(new ImageBitmapRenderingContext(canvas, WTF::move(settings)));

    InspectorInstrumentation::didCreateCanvasRenderingContext(*renderingContext);

    return renderingContext;
}

ImageBitmapRenderingContext::ImageBitmapRenderingContext(CanvasBase& canvas, ImageBitmapRenderingContextSettings&& settings)
    : CanvasRenderingContext(canvas, Type::BitmapRenderer)
    , m_settings(WTF::move(settings))
{
}

ImageBitmapRenderingContext::~ImageBitmapRenderingContext() = default;

ImageBitmapCanvas ImageBitmapRenderingContext::canvas()
{
    WeakRef base = canvasBase();
#if ENABLE(OFFSCREEN_CANVAS)
    if (RefPtr offscreenCanvas = dynamicDowncast<OffscreenCanvas>(base.get()))
        return offscreenCanvas.releaseNonNull();
#endif
    return downcast<HTMLCanvasElement>(base.get());
}

ExceptionOr<void> ImageBitmapRenderingContext::transferFromImageBitmap(RefPtr<ImageBitmap> imageBitmap)
{
    RefPtr<ImageBuffer> newBuffer;
    bool originClean = true;
    if (imageBitmap) {
        if (imageBitmap->isDetached())
            return Exception { ExceptionCode::InvalidStateError };
        originClean = imageBitmap->originClean();
        newBuffer = imageBitmap->takeImageBuffer();
    } else if (!m_buffer)
        return { };

    Ref canvasBase = this->canvasBase();
    if (originClean)
        canvasBase->setOriginClean();
    else
        canvasBase->setOriginTainted();
    if (newBuffer) {
        IntSize newSize = newBuffer->truncatedLogicalSize();
        updateMemoryCost(newBuffer->memoryCost());
        m_buffer = newBuffer.releaseNonNull();
        canvasBase->setSizeForControllingContext(newSize);
    } else {
        m_buffer = nullptr;
        updateMemoryCost(0);
    }
    canvasBase->didDraw(FloatRect { { }, canvasBase->size() });
    return { };
}

RefPtr<ImageBuffer> ImageBitmapRenderingContext::transferToImageBuffer()
{
    Ref canvasBase = this->canvasBase();
    auto size = canvasBase->size();
    if (!m_buffer)
        return ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    RefPtr result = std::exchange(m_buffer, { });
    updateMemoryCost(0);
    canvasBase->setOriginClean();
    canvasBase->didDraw(FloatRect { { }, size });
    return result;
}

RefPtr<ImageBuffer> ImageBitmapRenderingContext::surfaceBufferToImageBuffer(SurfaceBuffer)
{
    if (!m_buffer) {
        RefPtr buffer = ImageBuffer::create(protect(canvasBase())->size(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
        if (buffer) {
            updateMemoryCost(buffer->memoryCost());
            m_buffer = WTF::move(buffer);
        }
    }
    return m_buffer;
}

}
