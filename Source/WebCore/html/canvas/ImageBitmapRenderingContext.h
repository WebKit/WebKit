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

#pragma once

#include "CanvasRenderingContext.h"

#include "ExceptionOr.h"
#include "ImageBitmapRenderingContextSettings.h"
#include <memory>
#include <wtf/RefPtr.h>

namespace WebCore {

class ImageBitmap;
class ImageBuffer;
class OffscreenCanvas;

#if ENABLE(OFFSCREEN_CANVAS)
using ImageBitmapCanvas = std::variant<RefPtr<HTMLCanvasElement>, RefPtr<OffscreenCanvas>>;
#else
using ImageBitmapCanvas = std::variant<RefPtr<HTMLCanvasElement>>;
#endif

class ImageBitmapRenderingContext final : public CanvasRenderingContext {
    WTF_MAKE_ISO_ALLOCATED(ImageBitmapRenderingContext);
public:
    static std::unique_ptr<ImageBitmapRenderingContext> create(CanvasBase&, ImageBitmapRenderingContextSettings&&);

    enum class BitmapMode {
        Valid,
        Blank
    };

    ~ImageBitmapRenderingContext();

    ImageBitmapCanvas canvas();

    ExceptionOr<void> transferFromImageBitmap(RefPtr<ImageBitmap>);

    BitmapMode bitmapMode() { return m_bitmapMode; }
    bool hasAlpha() { return m_settings.alpha; }

private:
    ImageBitmapRenderingContext(CanvasBase&, ImageBitmapRenderingContextSettings&&);

    bool isBitmapRenderer() const final { return true; }
    bool isAccelerated() const override;

    void setOutputBitmap(RefPtr<ImageBitmap>);

    BitmapMode m_bitmapMode { BitmapMode::Blank };
    ImageBitmapRenderingContextSettings m_settings;
};

}

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::ImageBitmapRenderingContext, isBitmapRenderer())
