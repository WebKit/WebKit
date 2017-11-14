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

#include "EventTarget.h"
#include "ExceptionOr.h"
#include "IntSize.h"
#include "JSDOMPromiseDeferred.h"
#include "ScriptWrappable.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ImageBitmap;
class WebGLRenderingContext;

// using OffscreenRenderingContext = Variant<
// #if ENABLE(WEBGL)
// RefPtr<WebGLRenderingContext>,
// #endif
// RefPtr<OffscreenCanvasRenderingContext2D>
// >;

class OffscreenCanvas : public RefCounted<OffscreenCanvas>, public EventTargetWithInlineData {
    WTF_MAKE_FAST_ALLOCATED;
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
    ~OffscreenCanvas();

    unsigned width() const;
    void setWidth(unsigned);
    unsigned height() const;
    void setHeight(unsigned);

    // FIXME: Should be optional<OffscreenRenderingContext> from above.
    ExceptionOr<RefPtr<WebGLRenderingContext>> getContext(JSC::ExecState&, RenderingContextType, Vector<JSC::Strong<JSC::Unknown>>&& arguments);
    RefPtr<ImageBitmap> transferToImageBitmap();
    // void convertToBlob(ImageEncodeOptions options);

    using RefCounted::ref;
    using RefCounted::deref;

private:

    OffscreenCanvas(ScriptExecutionContext&, unsigned width, unsigned height);

    // EventTargetWithInlineData.
    ScriptExecutionContext* scriptExecutionContext() const final { return &m_scriptExecutionContext; }
    EventTargetInterface eventTargetInterface() const final { return OffscreenCanvasEventTargetInterfaceType; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    IntSize m_size;
    ScriptExecutionContext& m_scriptExecutionContext;
};

}
