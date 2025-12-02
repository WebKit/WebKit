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

#include <WebCore/ActiveDOMObject.h>
#include <WebCore/AffineTransform.h>
#include <WebCore/CanvasBase.h>
#include <WebCore/ContextDestructionObserver.h>
#include <WebCore/EventTarget.h>
#include <WebCore/EventTargetInterfaces.h>
#include <WebCore/IDLTypes.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/IntSize.h>
#include <WebCore/ScriptWrappable.h>
#include <wtf/FixedVector.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBGL)
#include <WebCore/WebGLContextAttributes.h>
#endif

namespace WebCore {

class CanvasRenderingContext;
class DeferredPromise;
class GPU;
class GPUCanvasContext;
class HTMLCanvasElement;
class ImageBitmap;
class ImageBitmapRenderingContext;
class ImageData;
class OffscreenCanvasRenderingContext2D;
class WebGL2RenderingContext;
class WebGLRenderingContext;
class WebGLRenderingContextBase;

template<typename> class ExceptionOr;

using OffscreenRenderingContext = Variant<
#if ENABLE(WEBGL)
    RefPtr<WebGLRenderingContext>,
    RefPtr<WebGL2RenderingContext>,
#endif
    RefPtr<GPUCanvasContext>,
    RefPtr<ImageBitmapRenderingContext>,
    RefPtr<OffscreenCanvasRenderingContext2D>
>;

class PlaceholderRenderingContext;
class PlaceholderRenderingContextSource;

class DetachedOffscreenCanvas {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(DetachedOffscreenCanvas, WEBCORE_EXPORT);
    WTF_MAKE_NONCOPYABLE(DetachedOffscreenCanvas);
    friend class OffscreenCanvas;

public:
    DetachedOffscreenCanvas(const IntSize&, bool originClean, RefPtr<PlaceholderRenderingContextSource>&&);
    WEBCORE_EXPORT ~DetachedOffscreenCanvas();
    const IntSize& size() const { return m_size; }
    bool originClean() const { return m_originClean; }
    RefPtr<PlaceholderRenderingContextSource> takePlaceholderSource();

private:
    RefPtr<PlaceholderRenderingContextSource> m_placeholderSource;
    IntSize m_size;
    bool m_originClean;
};

class OffscreenCanvas final : public ActiveDOMObject, public CanvasBase, public RefCounted<OffscreenCanvas>, public EventTarget {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED_EXPORT(OffscreenCanvas, WEBCORE_EXPORT);
public:
    struct ImageEncodeOptions {
        String type = "image/png"_s;
        double quality = 1.0;
    };

    enum class RenderingContextType {
        _2d,
        Webgl,
        Webgl2,
        Bitmaprenderer,
        Webgpu
    };

    static bool enabledForContext(ScriptExecutionContext&);

    static Ref<OffscreenCanvas> create(ScriptExecutionContext&, unsigned width, unsigned height);
    static Ref<OffscreenCanvas> create(ScriptExecutionContext&, std::unique_ptr<DetachedOffscreenCanvas>&&);
    static Ref<OffscreenCanvas> create(ScriptExecutionContext&, PlaceholderRenderingContext&);
    WEBCORE_EXPORT virtual ~OffscreenCanvas();

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
    USING_CAN_MAKE_WEAKPTR(EventTarget);

    void setWidth(unsigned);
    void setHeight(unsigned);

    void setImageBufferAndMarkDirty(RefPtr<ImageBuffer>&&) final;

    CanvasRenderingContext* renderingContext() const final { return m_context.get(); }

    std::unique_ptr<CSSParserContext> createCSSParserContext() const final;

    ExceptionOr<std::optional<OffscreenRenderingContext>> getContext(JSC::JSGlobalObject&, RenderingContextType, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments);
    ExceptionOr<RefPtr<ImageBitmap>> transferToImageBitmap();
    void convertToBlob(ImageEncodeOptions&&, Ref<DeferredPromise>&&);

    void didDraw(const std::optional<FloatRect>&, ShouldApplyPostProcessingToDirtyRect) final;

    Image* copiedImage() const final;
    void clearCopiedImage() const final;

    SecurityOrigin* securityOrigin() const final;

    bool canDetach() const;
    std::unique_ptr<DetachedOffscreenCanvas> detach();

    void commitToPlaceholderCanvas();

    void queueTaskKeepingObjectAlive(TaskSource, Function<void(CanvasBase&)>&&) final;
    void dispatchEvent(Event&) final;
    bool isDetached() const { return m_detached; };

private:
    OffscreenCanvas(ScriptExecutionContext&, IntSize, RefPtr<PlaceholderRenderingContextSource>&&);

    bool isOffscreenCanvas() const final { return true; }

    ScriptExecutionContext* scriptExecutionContext() const final;
    ScriptExecutionContext* canvasBaseScriptExecutionContext() const final;

    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::OffscreenCanvas; }
    void refEventTarget() final { RefCounted::ref(); }
    void derefEventTarget() final { RefCounted::deref(); }

    void didUpdateSizeProperties();
    void createImageBuffer() const final;

    void scheduleCommitToPlaceholderCanvas();

    std::unique_ptr<CanvasRenderingContext> m_context;
    RefPtr<PlaceholderRenderingContextSource> m_placeholderSource;
    mutable RefPtr<Image> m_copiedImage;
    bool m_detached { false };
    bool m_hasScheduledCommit { false };
};

}

SPECIALIZE_TYPE_TRAITS_CANVAS(WebCore::OffscreenCanvas, isOffscreenCanvas())

#endif
