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
        return offscreenCanvas;
#endif
    return &downcast<HTMLCanvasElement>(base.get());
}

ExceptionOr<void> ImageBitmapRenderingContext::transferFromImageBitmap(RefPtr<ImageBitmap> imageBitmap)
{
    if (!imageBitmap) {
        setBlank();
        canvasBase().setOriginClean();
        return { };
    }
    if (imageBitmap->isDetached())
        return Exception { ExceptionCode::InvalidStateError };
    if (imageBitmap->originClean())
        canvasBase().setOriginClean();
    else
        canvasBase().setOriginTainted();
    canvasBase().setImageBufferAndMarkDirty(imageBitmap->takeImageBuffer());
    return { };
}

void ImageBitmapRenderingContext::setBlank()
{
    // FIXME: What is the point of creating a full size transparent buffer that
    // can never be changed? Wouldn't a 1x1 buffer give the same rendering? The
    // only reason I can think of is toDataURL(), but that doesn't seem like
    // a good enough argument to waste memory.
    auto buffer = ImageBuffer::create(FloatSize(canvasBase().width(), canvasBase().height()), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    canvasBase().setImageBufferAndMarkDirty(WTF::move(buffer));
}

RefPtr<ImageBuffer> ImageBitmapRenderingContext::transferToImageBuffer()
{
    if (!canvasBase().hasCreatedImageBuffer())
        return canvasBase().allocateImageBuffer();
    RefPtr result = canvasBase().buffer();
    if (!result)
        return nullptr;
    setBlank();
    return result;
}

}
