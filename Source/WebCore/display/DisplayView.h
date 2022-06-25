/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayLayerController.h"
#include "LayoutUnits.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class Frame;
class FrameView;
class Page;

namespace Layout {
class LayoutState;
class LayoutTree;
}

namespace Display {

// The root object which renders a display tree for a single document.
class View {
    WTF_MAKE_ISO_ALLOCATED(View);
public:
    explicit View(FrameView&);
    ~View();

    const Tree* tree() const { return m_displayTree.get(); }

    void prepareForDisplay();
    void flushLayers();
    
    void setIsInWindow(bool);

    Page* page() const;
    FrameView& frameView() const { return m_frameView; }
    Frame& frame() const;

    // FIXME: Temporary.
    void setNeedsDisplay();

    float deviceScaleFactor() const;

private:
    const Layout::LayoutState* layoutState() const;

    FrameView& m_frameView;
    LayerController m_layerController;
    std::unique_ptr<Display::Tree> m_displayTree;
};

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
