/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "Color.h"
#include "LayoutRect.h"
#include "PageOverlay.h"
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class Frame;
class GraphicsContext;
class IntRect;
class FloatQuad;
class Page;
class RenderElement;
struct GapRects;

class ImageOverlayController final : private PageOverlay::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ImageOverlayController(Page&);

    void selectionQuadsDidChange(Frame&, const Vector<FloatQuad>&);
    void documentDetached(const Document&);

private:
    void willMoveToPage(PageOverlay&, Page*) final;
    void didMoveToPage(PageOverlay&, Page*) final { }
    void drawRect(PageOverlay&, GraphicsContext&, const IntRect& dirtyRect) final;
    bool mouseEvent(PageOverlay&, const PlatformMouseEvent&) final { return false; }

    bool shouldUsePageOverlayToPaintSelection(const RenderElement&);

    PageOverlay& installPageOverlayIfNeeded();
    void uninstallPageOverlayIfNeeded();

    WeakPtr<Page> m_page;
    RefPtr<PageOverlay> m_overlay;
    WeakPtr<Document> m_currentOverlayDocument;
    Vector<FloatQuad> m_overlaySelectionQuads;
    LayoutRect m_selectionOverlayBounds;
    Color m_selectionBackgroundColor { Color::transparentBlack };
};

} // namespace WebCore
