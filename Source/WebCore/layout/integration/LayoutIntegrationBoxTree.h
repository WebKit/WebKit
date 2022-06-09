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

#include "LayoutInitialContainingBlock.h"
#include <wtf/HashMap.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>

namespace WebCore {

class RenderBlockFlow;
class RenderBoxModelObject;

namespace LayoutIntegration {

#if ENABLE(TREE_DEBUGGING)
struct InlineContent;
#endif

class BoxTree {
public:
    BoxTree(RenderBlockFlow&);

    void updateStyle(const RenderBoxModelObject&);

    const RenderBlockFlow& flow() const { return m_flow; }
    RenderBlockFlow& flow() { return m_flow; }

    const Layout::ContainerBox& rootLayoutBox() const { return m_root; }
    Layout::ContainerBox& rootLayoutBox() { return m_root; }

    const Layout::Box& layoutBoxForRenderer(const RenderObject&) const;
    Layout::Box& layoutBoxForRenderer(const RenderObject&);

    const RenderObject& rendererForLayoutBox(const Layout::Box&) const;
    RenderObject& rendererForLayoutBox(const Layout::Box&);

    size_t boxCount() const { return m_boxes.size(); }

    struct BoxAndRenderer {
        CheckedRef<Layout::Box> box;
        RenderObject* renderer { nullptr };
    };
    const auto& boxAndRendererList() const { return m_boxes; }

private:
    void buildTree();
    void appendChild(UniqueRef<Layout::Box>, RenderObject&);

    RenderBlockFlow& m_flow;
    Layout::ContainerBox m_root;
    Vector<BoxAndRenderer, 1> m_boxes;

    HashMap<const RenderObject*, CheckedRef<Layout::Box>> m_rendererToBoxMap;
    HashMap<CheckedRef<const Layout::Box>, RenderObject*> m_boxToRendererMap;
};

#if ENABLE(TREE_DEBUGGING)
void showInlineContent(TextStream&, const InlineContent&, size_t depth);
#endif
}
}

#endif
