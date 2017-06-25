/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(FULLSCREEN_API)

#include "RenderFlexibleBox.h"

namespace WebCore {

class RenderFullScreen final : public RenderFlexibleBox {
public:
    RenderFullScreen(Document&, RenderStyle&&);

    const char* renderName() const override { return "RenderFullScreen"; }

    void setPlaceholder(RenderBlock*);
    RenderBlock* placeholder() { return m_placeholder; }
    void createPlaceholder(std::unique_ptr<RenderStyle>, const LayoutRect& frameRect);

    static RenderFullScreen* wrapRenderer(RenderObject*, RenderElement*, Document&);
    void unwrapRenderer(bool& requiresRenderTreeRebuild);

    ItemPosition selfAlignmentNormalBehavior(const RenderBox* = nullptr) const override { return ItemPositionCenter; }
    
private:
    bool isRenderFullScreen() const override { return true; }
    void willBeDestroyed() override;
    bool isFlexibleBoxImpl() const override { return true; }

protected:
    RenderBlock* m_placeholder;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderFullScreen, isRenderFullScreen())

#endif // ENABLE(FULLSCREEN_API)
