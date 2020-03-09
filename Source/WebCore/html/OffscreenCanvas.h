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

#if ENABLE(OFFSCREEN_CANVAS)

#include "AffineTransform.h"
#include "CanvasBase.h"
#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "IDLTypes.h"
#include "IntSize.h"
#include "ScriptWrappable.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CanvasRenderingContext;
class CSSValuePool;
class DeferredPromise;
class ImageBitmap;
class OffscreenCanvasRenderingContext2D;
class WebGLRenderingContext;

#if ENABLE(WEBGL)
using OffscreenRenderingContext = Variant<RefPtr<OffscreenCanvasRenderingContext2D>, RefPtr<WebGLRenderingContext>>;
#else
using OffscreenRenderingContext = Variant<RefPtr<OffscreenCanvasRenderingContext2D>>;
#endif

class DetachedOffscreenCanvas {
    WTF_MAKE_NONCOPYABLE(DetachedOffscreenCanvas);
    WTF_MAKE_FAST_ALLOCATED;
public:
    DetachedOffscreenCanvas(std::unique_ptr<ImageBuffer>&&, const IntSize&, bool originClean);

    std::unique_ptr<ImageBuffer> takeImageBuffer();
    const IntSize& size() const { return m_size; }
    bool originClean() const { return m_originClean; }

private:
    std::unique_ptr<ImageBuffer> m_buffer;
    IntSize m_size;
    bool m_originClean;
};

class OffscreenCanvas final : public RefCounted<OffscreenCanvas>, public CanvasBase, public EventTargetWithInlineData, private ContextDestructionObserver {
    WTF_MAKE_ISO_ALLOCATED(OffscreenCanvas);
public:

    struct ImageEncodeOptions {
        String type = "image/png";
        double quality = 1.0;
    };

    enum class RenderingContextType {
        _2d,
        Webgl
    };

    static Ref<OffscreenCanvas> create(ScriptExecutionContext&, unsigned width, unsigned height);
    static Ref<OffscreenCanvas> create(ScriptExecutionContext&, std::unique_ptr<DetachedOffscreenCanvas>&&);
    virtual ~OffscreenCanvas();

    unsigned width() const final;
    unsigned height() const final;
    void setWidth(unsigned);
    void setHeight(unsigned);

    CanvasRenderingContext* renderingContext() const final { return m_context.get(); }

    ExceptionOr<OffscreenRenderingContext> getContext(JSC::JSGlobalObject&, RenderingContextType, Vector<JSC::Strong<JSC::Unknown>>&& arguments);
    ExceptionOr<RefPtr<ImageBitmap>> transferToImageBitmap();
    void convertToBlob(ImageEncodeOptions&&, Ref<DeferredPromise>&&);

    void didDraw(const FloatRect&) final;

    Image* copiedImage() const final { return nullptr; }
    bool hasCreatedImageBuffer() const final { return m_hasCreatedImageBuffer; }

    SecurityOrigin* securityOrigin() const final;

    bool canDetach() const;
    std::unique_ptr<DetachedOffscreenCanvas> detach();

    CSSValuePool& cssValuePool();

    using RefCounted::ref;
    using RefCounted::deref;

private:

    OffscreenCanvas(ScriptExecutionContext&, unsigned width, unsigned height);

    bool isOffscreenCanvas() const final { return true; }

    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }
    ScriptExecutionContext* canvasBaseScriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }

    EventTargetInterface eventTargetInterface() const final { return OffscreenCanvasEventTargetInterfaceType; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    void refCanvasBase() final { ref(); }
    void derefCanvasBase() final { deref(); }

    void setSize(const IntSize&) final;
    void createImageBuffer() const final;
    std::unique_ptr<ImageBuffer> takeImageBuffer() const;

    void reset();

    std::unique_ptr<CanvasRenderingContext> m_context;

    // m_hasCreatedImageBuffer means we tried to malloc the buffer. We didn't necessarily get it.
    mutable bool m_hasCreatedImageBuffer { false };

    bool m_detached { false };
};

}

SPECIALIZE_TYPE_TRAITS_CANVAS(WebCore::OffscreenCanvas, isOffscreenCanvas())

#endif
