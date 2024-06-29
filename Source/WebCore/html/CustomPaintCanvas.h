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

#include "AffineTransform.h"
#include "CanvasBase.h"
#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "ImageBuffer.h"
#include "IntSize.h"
#include "PaintRenderingContext2D.h"
#include "ScriptWrappable.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ImageBitmap;

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

    void didDraw(const std::optional<FloatRect>&, ShouldApplyPostProcessingToDirtyRect) final { }

    Image* copiedImage() const final;
    void clearCopiedImage() const final;

    void replayDisplayList(GraphicsContext&);

    void queueTaskKeepingObjectAlive(TaskSource, Function<void()>&&) final { };
    void dispatchEvent(Event&) final { }

    const CSSParserContext& cssParserContext() const final;

    using RefCounted::ref;
    using RefCounted::deref;

private:
    CustomPaintCanvas(ScriptExecutionContext&, unsigned width, unsigned height);

    void refCanvasBase() const final { ref(); }
    void derefCanvasBase() const final { deref(); }
    ScriptExecutionContext* canvasBaseScriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }

    std::unique_ptr<PaintRenderingContext2D> m_context;
    mutable RefPtr<Image> m_copiedImage;
    mutable std::unique_ptr<CSSParserContext> m_cssParserContext;
};

}
SPECIALIZE_TYPE_TRAITS_CANVAS(WebCore::CustomPaintCanvas, isCustomPaintCanvas())
