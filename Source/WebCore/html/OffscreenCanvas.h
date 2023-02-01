/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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
#include "ImageBuffer.h"
#include "ImageBufferPipe.h"
#include "IntSize.h"
#include "ScriptWrappable.h"
#include <wtf/FixedVector.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBGL)
#include "WebGLContextAttributes.h"
#endif

namespace WebCore {

class CanvasRenderingContext;
class DeferredPromise;
class HTMLCanvasElement;
class ImageBitmap;
class ImageBitmapRenderingContext;
class ImageData;
class OffscreenCanvasRenderingContext2D;
class WebGL2RenderingContext;
class WebGLRenderingContext;
class WebGLRenderingContextBase;

using OffscreenRenderingContext = std::variant<
#if ENABLE(WEBGL)
    RefPtr<WebGLRenderingContext>,
    RefPtr<WebGL2RenderingContext>,
#endif
    RefPtr<ImageBitmapRenderingContext>,
    RefPtr<OffscreenCanvasRenderingContext2D>
>;

class DetachedOffscreenCanvas {
    WTF_MAKE_NONCOPYABLE(DetachedOffscreenCanvas);
    WTF_MAKE_FAST_ALLOCATED;
    friend class OffscreenCanvas;

public:
    DetachedOffscreenCanvas(std::unique_ptr<SerializedImageBuffer>, const IntSize&, bool originClean);

    RefPtr<ImageBuffer> takeImageBuffer(ScriptExecutionContext&);
    const IntSize& size() const { return m_size; }
    bool originClean() const { return m_originClean; }
    size_t memoryCost() const
    {
        auto* buffer = m_buffer.get();
        if (buffer)
            return buffer->memoryCost();
        return 0;
    }
    WeakPtr<HTMLCanvasElement, WeakPtrImplWithEventTargetData> takePlaceholderCanvas();

private:
    std::unique_ptr<SerializedImageBuffer> m_buffer;
    IntSize m_size;
    bool m_originClean;
    WeakPtr<HTMLCanvasElement, WeakPtrImplWithEventTargetData> m_placeholderCanvas;
};

class OffscreenCanvas final : public RefCounted<OffscreenCanvas>, public CanvasBase, public EventTarget, private ContextDestructionObserver {
    WTF_MAKE_ISO_ALLOCATED(OffscreenCanvas);
public:

    struct ImageEncodeOptions {
        String type = "image/png"_s;
        double quality = 1.0;
    };

    enum class RenderingContextType {
        _2d,
        Webgl,
        Webgl2,
        Bitmaprenderer
    };

    static bool enabledForContext(ScriptExecutionContext&);

    static Ref<OffscreenCanvas> create(ScriptExecutionContext&, unsigned width, unsigned height);
    static Ref<OffscreenCanvas> create(ScriptExecutionContext&, std::unique_ptr<DetachedOffscreenCanvas>&&);
    static Ref<OffscreenCanvas> create(ScriptExecutionContext&, HTMLCanvasElement&);
    virtual ~OffscreenCanvas();

    unsigned width() const final;
    unsigned height() const final;
    void setWidth(unsigned);
    void setHeight(unsigned);

    void setImageBufferAndMarkDirty(RefPtr<ImageBuffer>&&) final;

    CanvasRenderingContext* renderingContext() const final { return m_context.get(); }

    ExceptionOr<std::optional<OffscreenRenderingContext>> getContext(JSC::JSGlobalObject&, RenderingContextType, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments);
    ExceptionOr<RefPtr<ImageBitmap>> transferToImageBitmap();
    void convertToBlob(ImageEncodeOptions&&, Ref<DeferredPromise>&&);

    void didDraw(const std::optional<FloatRect>&) final;

    Image* copiedImage() const final;
    void clearCopiedImage() const final;

    bool hasCreatedImageBuffer() const final { return m_hasCreatedImageBuffer; }

    SecurityOrigin* securityOrigin() const final;

    bool canDetach() const;
    std::unique_ptr<DetachedOffscreenCanvas> detach();

    void commitToPlaceholderCanvas();

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

#if ENABLE(WEBGL)
    void createContextWebGL(RenderingContextType, WebGLContextAttributes&& = { });
#endif

    void createImageBuffer() const final;
    std::unique_ptr<SerializedImageBuffer> takeImageBuffer() const;

    void reset();

    void setPlaceholderCanvas(HTMLCanvasElement&);
    void pushBufferToPlaceholder();
    void scheduleCommitToPlaceholderCanvas();

    std::unique_ptr<CanvasRenderingContext> m_context;

    // m_hasCreatedImageBuffer means we tried to malloc the buffer. We didn't necessarily get it.
    mutable bool m_hasCreatedImageBuffer { false };

    bool m_detached { false };

    mutable RefPtr<Image> m_copiedImage;

    bool m_hasScheduledCommit { false };

    class PlaceholderData : public ThreadSafeRefCounted<PlaceholderData> {
    public:
        static Ref<PlaceholderData> create()
        {
            return adoptRef(*new PlaceholderData);
        }

        WeakPtr<HTMLCanvasElement, WeakPtrImplWithEventTargetData> canvas;
        RefPtr<ImageBufferPipe::Source> bufferPipeSource;
        std::unique_ptr<SerializedImageBuffer> pendingCommitBuffer;
        mutable Lock bufferLock;
    };

    RefPtr<PlaceholderData> m_placeholderData;
};

}

SPECIALIZE_TYPE_TRAITS_CANVAS(WebCore::OffscreenCanvas, isOffscreenCanvas())

#endif
