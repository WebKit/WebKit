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

#include "CanvasRenderingContext.h"
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class PlaceholderRenderingContext;

// Thread-safe interface to submit frames from worker to the placeholder rendering context.
class PlaceholderRenderingContextSource : public ThreadSafeRefCounted<PlaceholderRenderingContextSource> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(PlaceholderRenderingContextSource);
public:
    static Ref<PlaceholderRenderingContextSource> create(PlaceholderRenderingContext&);
    virtual ~PlaceholderRenderingContextSource() = default;

    // Called by the offscreen context to submit the frame.
    virtual void setPlaceholderBuffer(ImageBuffer&);

    // Called by the placeholder context to attach to compositor layer.
    virtual void setContentsToLayer(GraphicsLayer&) = 0;

protected:
    PlaceholderRenderingContextSource(PlaceholderRenderingContext&);
    WeakPtr<PlaceholderRenderingContext> m_placeholder; // For main thread use.
};

class PlaceholderRenderingContext final : public CanvasRenderingContext {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(PlaceholderRenderingContext);
public:
    static std::unique_ptr<PlaceholderRenderingContext> create(HTMLCanvasElement&);

    HTMLCanvasElement& canvas() const;
    IntSize size() const;
    void setPlaceholderBuffer(Ref<ImageBuffer>&&);

    Ref<PlaceholderRenderingContextSource> source() const { return m_source; }

private:
    PlaceholderRenderingContext(HTMLCanvasElement&);
    bool isPlaceholder() const final { return true; }
    bool delegatesDisplay() const final { return true; }
    void setContentsToLayer(GraphicsLayer&) final;

    Ref<PlaceholderRenderingContextSource> m_source;
};

}

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::PlaceholderRenderingContext, isPlaceholder())

#endif
