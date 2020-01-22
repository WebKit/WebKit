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

#include "ImageBitmap.h"
#include "ImageBuffer.h"
#include "InspectorInstrumentation.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBitmapRenderingContext);

#if USE(IOSURFACE_CANVAS_BACKING_STORE) || ENABLE(ACCELERATED_2D_CANVAS)
static RenderingMode bufferRenderingMode = RenderingMode::Accelerated;
#else
static RenderingMode bufferRenderingMode = RenderingMode::Unaccelerated;
#endif

std::unique_ptr<ImageBitmapRenderingContext> ImageBitmapRenderingContext::create(CanvasBase& canvas, ImageBitmapRenderingContextSettings&& settings)
{
    auto renderingContext = std::unique_ptr<ImageBitmapRenderingContext>(new ImageBitmapRenderingContext(canvas, WTFMove(settings)));

    InspectorInstrumentation::didCreateCanvasRenderingContext(*renderingContext);

    return renderingContext;
}

ImageBitmapRenderingContext::ImageBitmapRenderingContext(CanvasBase& canvas, ImageBitmapRenderingContextSettings&& settings)
    : CanvasRenderingContext(canvas)
    , m_settings(WTFMove(settings))
{
    setOutputBitmap(nullptr);
}

ImageBitmapRenderingContext::~ImageBitmapRenderingContext() = default;

HTMLCanvasElement* ImageBitmapRenderingContext::canvas() const
{
    auto& base = canvasBase();
    if (!is<HTMLCanvasElement>(base))
        return nullptr;
    return &downcast<HTMLCanvasElement>(base);
}

bool ImageBitmapRenderingContext::isAccelerated() const
{
    return bufferRenderingMode == RenderingMode::Accelerated;
}

void ImageBitmapRenderingContext::setOutputBitmap(RefPtr<ImageBitmap> imageBitmap)
{
    // 1. If a bitmap argument was not provided, then:

    if (!imageBitmap) {

        // 1.1. Set context's bitmap mode to blank.

        m_bitmapMode = BitmapMode::Blank;

        // 1.2. Let canvas be the canvas element to which context is bound.

        // 1.3. Set context's output bitmap to be transparent black with an
        //      intrinsic width equal to the numeric value of canvas's width attribute
        //      and an intrinsic height equal to the numeric value of canvas's height
        //      attribute, those values being interpreted in CSS pixels.

        // FIXME: What is the point of creating a full size transparent buffer that
        // can never be changed? Wouldn't a 1x1 buffer give the same rendering? The
        // only reason I can think of is toDataURL(), but that doesn't seem like
        // a good enough argument to waste memory.

        canvas()->setImageBufferAndMarkDirty(ImageBuffer::create(FloatSize(canvas()->width(), canvas()->height()), bufferRenderingMode));

        // 1.4. Set the output bitmap's origin-clean flag to true.

        canvas()->setOriginClean();
        return;
    }

    // 2. If a bitmap argument was provided, then:

    // 2.1. Set context's bitmap mode to valid.

    m_bitmapMode = BitmapMode::Valid;

    // 2.2. Set context's output bitmap to refer to the same underlying
    //      bitmap data as bitmap, without making a copy.
    //      Note: the origin-clean flag of bitmap is included in the
    //      bitmap data to be referenced by context's output bitmap.

    if (imageBitmap->originClean())
        canvas()->setOriginClean();
    else
        canvas()->setOriginTainted();
    canvas()->setImageBufferAndMarkDirty(imageBitmap->transferOwnershipAndClose());
}

ExceptionOr<void> ImageBitmapRenderingContext::transferFromImageBitmap(RefPtr<ImageBitmap> imageBitmap)
{
    // 1. Let bitmapContext be the ImageBitmapRenderingContext object on which
    //    the transferFromImageBitmap() method was called.

    // 2. If imageBitmap is null, then run the steps to set an ImageBitmapRenderingContext's
    //    output bitmap, with bitmapContext as the context argument and no bitmap argument,
    //    then abort these steps.

    if (!imageBitmap) {
        setOutputBitmap(nullptr);
        return { };
    }

    // 3. If the value of imageBitmap's [[Detached]] internal slot is set to true,
    //    then throw an "InvalidStateError" DOMException and abort these steps.

    if (imageBitmap->isDetached())
        return Exception { InvalidStateError };

    // 4. Run the steps to set an ImageBitmapRenderingContext's output bitmap,
    //    with the context argument equal to bitmapContext, and the bitmap
    //    argument referring to imageBitmap's underlying bitmap data.

    setOutputBitmap(imageBitmap);

    // 5. Set the value of imageBitmap's [[Detached]] internal slot to true.
    // 6. Unset imageBitmap's bitmap data.

    // Note that the algorithm in the specification is currently a bit
    // muddy here. The setOutputBitmap step above had to transfer ownership
    // from the imageBitmap to this object, which requires a detach and unset,
    // so this step isn't necessary, but we'll do it anyway.

    imageBitmap->close();

    return { };
}

}
