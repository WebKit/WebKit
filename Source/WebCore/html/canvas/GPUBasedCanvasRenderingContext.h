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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ActiveDOMObject.h"
#include "CanvasRenderingContext.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class HTMLCanvasElement;

class GPUBasedCanvasRenderingContext : public CanvasRenderingContext, public ActiveDOMObject {
    WTF_MAKE_TZONE_OR_ISO_NON_HEAP_ALLOCATABLE(GPUBasedCanvasRenderingContext);
public:
    // ContextDestructionObserver.
    void ref() const final { CanvasRenderingContext::ref(); }
    void deref() const final { CanvasRenderingContext::deref(); }
    USING_CAN_MAKE_WEAKPTR(CanvasRenderingContext);

    virtual void reshape() = 0;
protected:
    explicit GPUBasedCanvasRenderingContext(CanvasBase&, CanvasRenderingContext::Type);

    HTMLCanvasElement* htmlCanvas() const;
    void markCanvasChanged();
};
    
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::GPUBasedCanvasRenderingContext, isGPUBased())
