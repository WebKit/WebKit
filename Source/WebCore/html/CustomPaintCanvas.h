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
#include "ImageBuffer.h"
#include "IntSize.h"
#include "PaintRenderingContext2D.h"
#include "ScriptWrappable.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ImageBitmap;

namespace DisplayList {
class DrawingContext;
}

class CustomPaintCanvas final : public CanvasBase, public RefCounted<CustomPaintCanvas>, private ContextDestructionObserver {
    WTF_MAKE_TZONE_ALLOCATED(CustomPaintCanvas);
public:

    static Ref<CustomPaintCanvas> create(ScriptExecutionContext&, unsigned width, unsigned height);
    virtual ~CustomPaintCanvas();
    bool isCustomPaintCanvas() const final { return true; }

    RefPtr<PaintRenderingContext2D> getContext();

    CanvasRenderingContext* renderingContext() const final { return m_context.get(); }

    void didDraw(const std::optional<FloatRect>&, ShouldApplyPostProcessingToDirtyRect) final { }
    void setSizeForControllingContext(IntSize) { };

    Image* copiedImage() const final;
    void clearCopiedImage() const final;

    void replayDisplayList(GraphicsContext&);

    void queueTaskKeepingObjectAlive(TaskSource, Function<void(CanvasBase&)>&&) final { };
    void dispatchEvent(Event&) final { }

    std::unique_ptr<CSSParserContext> createCSSParserContext() const final;

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    CustomPaintCanvas(ScriptExecutionContext&, unsigned width, unsigned height);

    ScriptExecutionContext* canvasBaseScriptExecutionContext() const final;

    std::unique_ptr<PaintRenderingContext2D> m_context;
    mutable RefPtr<Image> m_copiedImage;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CustomPaintCanvas)
    static bool isType(const WebCore::CanvasBase& base) { return base.isCustomPaintCanvas(); }
SPECIALIZE_TYPE_TRAITS_END()
