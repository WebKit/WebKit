/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if ENABLE(RESOURCE_USAGE)

#include "FloatRect.h"
#include "GraphicsLayer.h"
#include "IntRect.h"
#include "PageOverlay.h"
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>

#if PLATFORM(COCOA)
#include "PlatformCALayer.h"
#endif

namespace WebCore {

class FloatRect;
class IntPoint;
class IntRect;

class ResourceUsageOverlay final : public PageOverlay::Client {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ResourceUsageOverlay);
public:
    explicit ResourceUsageOverlay(Page&);
    ~ResourceUsageOverlay();

    PageOverlay& overlay() { return *m_overlay; }

#if PLATFORM(COCOA)
    void platformDraw(CGContextRef);
#endif

    static const int normalWidth = 570;
    static const int normalHeight = 180;

private:
    void willMoveToPage(PageOverlay&, Page*) override { }
    void didMoveToPage(PageOverlay&, Page*) override { }
    void drawRect(PageOverlay&, GraphicsContext&, const IntRect&) override { }
    bool mouseEvent(PageOverlay&, const PlatformMouseEvent&) override;
    void didScrollFrame(PageOverlay&, Frame&) override { }

    void initialize();

    void platformInitialize();
    void platformDestroy();

    Page& m_page;
    RefPtr<PageOverlay> m_overlay;
    bool m_dragging { false };
    IntPoint m_dragPoint;

#if PLATFORM(COCOA)
    RetainPtr<CALayer> m_layer;
    RetainPtr<CALayer> m_containerLayer;
#endif

#if OS(LINUX)
    RefPtr<GraphicsLayer> m_paintLayer;
    std::unique_ptr<GraphicsLayerClient> m_overlayPainter;
#endif
};

} // namespace WebCore

#endif // ENABLE(RESOURCE_USAGE)
