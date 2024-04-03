/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "ExceptionOr.h"
#include "GPUBasedCanvasRenderingContext.h"
#include <variant>
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#endif

namespace WebCore {

class CanvasBase;
class GPU;
struct GPUCanvasConfiguration;
class GPUTexture;

class GPUCanvasContext : public GPUBasedCanvasRenderingContext {
    WTF_MAKE_ISO_ALLOCATED(GPUCanvasContext);
public:
#if ENABLE(OFFSCREEN_CANVAS)
    using CanvasType = std::variant<RefPtr<HTMLCanvasElement>, RefPtr<OffscreenCanvas>>;
#else
    using CanvasType = std::variant<RefPtr<HTMLCanvasElement>>;
#endif

    static std::unique_ptr<GPUCanvasContext> create(CanvasBase&, GPU&);

    virtual CanvasType canvas() = 0;
    virtual ExceptionOr<void> configure(GPUCanvasConfiguration&&) = 0;
    virtual void unconfigure() = 0;
    virtual RefPtr<GPUTexture> getCurrentTexture() = 0;

    bool isWebGPU() const override { return true; }
    const char* activeDOMObjectName() const override
    {
        return "GPUCanvasElement";
    }

protected:
    GPUCanvasContext(CanvasBase&);
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::GPUCanvasContext, isWebGPU())
