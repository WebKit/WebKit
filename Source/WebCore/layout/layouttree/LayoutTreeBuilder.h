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

#include "LayoutContainerBox.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class RenderBlockFlow;
class RenderBox;
class RenderElement;
class RenderObject;
class RenderTable;
class RenderView;

namespace Layout {

class LayoutState;

class LayoutTree {
    WTF_MAKE_ISO_ALLOCATED(LayoutTree);
public:
    LayoutTree(std::unique_ptr<ContainerBox>);
    ~LayoutTree() = default;

    const ContainerBox& root() const { return *m_root; }

private:
    std::unique_ptr<ContainerBox> m_root;
};

class TreeBuilder {
public:
    static std::unique_ptr<Layout::LayoutTree> buildLayoutTree(const RenderView&);

private:
    TreeBuilder();

    void buildSubTree(const RenderElement& parentRenderer, ContainerBox& parentContainer);
    void buildTableStructure(const RenderTable& tableRenderer, ContainerBox& tableWrapperBox);
    std::unique_ptr<Box> createLayoutBox(const ContainerBox& parentContainer, const RenderObject& childRenderer);

    std::unique_ptr<Box> createReplacedBox(std::optional<Box::ElementAttributes>, RenderStyle&&);
    std::unique_ptr<Box> createTextBox(String text, bool canUseSimplifiedTextMeasuring, bool canUseSimpleFontCodePath, RenderStyle&&);
    std::unique_ptr<Box> createLineBreakBox(bool isOptional, RenderStyle&&);
    std::unique_ptr<ContainerBox> createContainer(std::optional<Box::ElementAttributes>, RenderStyle&&);
};

#if ENABLE(TREE_DEBUGGING)
String layoutTreeAsText(const Box&, const LayoutState*);
void showLayoutTree(const Box&, const LayoutState*);
void showLayoutTree(const Box&);
void showInlineTreeAndRuns(TextStream&, const LayoutState&, const ContainerBox& inlineFormattingRoot, size_t depth);
void printLayoutTreeForLiveDocuments();
#endif

}
}

#endif
