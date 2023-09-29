/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(CSS_PAINTING_API)

#include "AffineTransform.h"
#include "CanvasBase.h"
#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "ImageBuffer.h"
#include "IntSize.h"
#include "ScriptWrappable.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CanvasRenderingContext;
class ImageBitmap;
class PaintRenderingContext2D;

namespace DisplayList {
class DrawingContext;
}

class CustomPaintCanvas final : public RefCounted<CustomPaintCanvas>, public CanvasBase, private ContextDestructionObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:

    static Ref<CustomPaintCanvas> create(ScriptExecutionContext&, unsigned width, unsigned height);
    virtual ~CustomPaintCanvas();
    bool isCustomPaintCanvas() const final { return true; }

    RefPtr<PaintRenderingContext2D> getContext();

    CanvasRenderingContext* renderingContext() const final { return m_context.get(); }
    GraphicsContext* drawingContext() const final;
    GraphicsContext* existingDrawingContext() const final;

    void didDraw(const std::optional<FloatRect>&, ShouldApplyPostProcessingToDirtyRect) final { }

    AffineTransform baseTransform() const final;
    Image* copiedImage() const final;
    void clearCopiedImage() const final;

    void replayDisplayList(GraphicsContext&);

    void queueTaskKeepingObjectAlive(TaskSource, Function<void()>&&) final { };
    void dispatchEvent(Event&) final { }

    using RefCounted::ref;
    using RefCounted::deref;

private:
    CustomPaintCanvas(ScriptExecutionContext&, unsigned width, unsigned height);

    void refCanvasBase() final { ref(); }
    void derefCanvasBase() final { deref(); }
    ScriptExecutionContext* canvasBaseScriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }
    void replayDisplayListImpl(GraphicsContext& target) const;

    std::unique_ptr<CanvasRenderingContext> m_context;
    mutable std::unique_ptr<DisplayList::DrawingContext> m_recordingContext;
    mutable RefPtr<Image> m_copiedImage;
};

}
SPECIALIZE_TYPE_TRAITS_CANVAS(WebCore::CustomPaintCanvas, isCustomPaintCanvas())
#endif
