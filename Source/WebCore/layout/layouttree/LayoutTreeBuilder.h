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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutBox.h"
#include <wtf/IsoMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class RenderBlockFlow;
class RenderElement;
class RenderObject;
class RenderTable;
class RenderView;

namespace Layout {

class Box;
class Container;
class LayoutState;

class LayoutTreeContent : public CanMakeWeakPtr<LayoutTreeContent> {
    WTF_MAKE_ISO_ALLOCATED(LayoutTreeContent);
public:
    LayoutTreeContent(const RenderBox&, std::unique_ptr<Container>);
    ~LayoutTreeContent();

    const Container& rootLayoutBox() const { return *m_rootLayoutBox; }
    Container& rootLayoutBox() { return *m_rootLayoutBox; }
    const RenderBox& rootRenderer() const { return m_rootRenderer; }

    void addBox(std::unique_ptr<Box> box) { m_boxes.add(WTFMove(box)); }

    Box* layoutBoxForRenderer(const RenderObject& renderer) { return m_renderObjectToLayoutBox.get(&renderer); }
    const Box* layoutBoxForRenderer(const RenderObject& renderer) const { return m_renderObjectToLayoutBox.get(&renderer); }

    const RenderObject* rendererForLayoutBox(const Box& box) const { return m_layoutBoxToRenderObject.get(&box); }

    void addLayoutBoxForRenderer(const RenderObject&, Box&);

private:
    const RenderBox& m_rootRenderer;
    std::unique_ptr<Container> m_rootLayoutBox;
    HashSet<std::unique_ptr<Box>> m_boxes;

    HashMap<const RenderObject*, Box*> m_renderObjectToLayoutBox;
    HashMap<const Box*, const RenderObject*> m_layoutBoxToRenderObject;
};

class TreeBuilder {
public:
    static std::unique_ptr<Layout::LayoutTreeContent> buildLayoutTree(const RenderView&);
    static std::unique_ptr<Layout::LayoutTreeContent> buildLayoutTreeForIntegration(const RenderBlockFlow&);

private:
    TreeBuilder(LayoutTreeContent&);

    void buildTree();
    void buildSubTree(const RenderElement& parentRenderer, Container& parentContainer);
    void buildTableStructure(const RenderTable& tableRenderer, Container& tableWrapperBox);
    std::unique_ptr<Box> createLayoutBox(const Container& parentContainer, const RenderObject& childRenderer);

    LayoutTreeContent& m_layoutTreeContent;
};

#if ENABLE(TREE_DEBUGGING)
void showLayoutTree(const Box&, const LayoutState*);
void showLayoutTree(const Box&);
void printLayoutTreeForLiveDocuments();
#endif

}
}

#endif
