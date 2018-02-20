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

#include "RenderTreePosition.h"

namespace WebCore {

class RenderButton;
class RenderGrid;
class RenderMathMLFenced;
class RenderMenuList;
class RenderRubyAsBlock;
class RenderRubyAsInline;
class RenderRubyBase;
class RenderRubyRun;
class RenderSVGContainer;
class RenderSVGInline;
class RenderSVGRoot;
class RenderSVGText;
class RenderTable;
class RenderTableRow;
class RenderTableSection;
class RenderTreeUpdater;

class RenderTreeBuilder {
public:
    RenderTreeBuilder(RenderView&);
    ~RenderTreeBuilder();

    void insertChild(RenderElement& parent, RenderPtr<RenderObject>, RenderObject* beforeChild = nullptr);
    void insertChild(RenderTreePosition&, RenderPtr<RenderObject>);

    RenderPtr<RenderObject> takeChild(RenderElement&, RenderObject&) WARN_UNUSED_RETURN;

    void updateAfterDescendants(RenderElement&);

    // This avoids having to convert all sites that need RenderTreeBuilder in one go.
    // FIXME: Remove.
    static RenderTreeBuilder* current() { return s_current; }

    // These functions are temporary until after all block/inline/continuation code is moved over.
    void insertChildToRenderElement(RenderElement& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild = nullptr);
    void insertChildToRenderBlockIgnoringContinuation(RenderBlock& parent, RenderPtr<RenderObject>, RenderObject* beforeChild = nullptr);
    void insertChildToRenderInlineIgnoringContinuation(RenderInline& parent, RenderPtr<RenderObject>, RenderObject* beforeChild = nullptr);

    RenderPtr<RenderObject> takeChildFromRenderElement(RenderElement& parent, RenderObject& child) WARN_UNUSED_RETURN;

    void childFlowStateChangesAndAffectsParentBlock(RenderElement& child);
    void childFlowStateChangesAndNoLongerAffectsParentBlock(RenderElement& child);
    void removeFromParentAndDestroyCleaningUpAnonymousWrappers(RenderObject& child);
    void multiColumnDescendantInserted(RenderMultiColumnFlow&, RenderObject& newDescendant);

private:
    class FirstLetter;
    class List;
    class MultiColumn;
    class Table;
    class Ruby;
    class FormControls;
    class Block;
    class BlockFlow;
    class Inline;
    class SVG;
    class MathML;

    RenderObject* splitAnonymousBoxesAroundChild(RenderBox& parent, RenderObject* beforeChild);
    void makeChildrenNonInline(RenderBlock& parent, RenderObject* insertionPoint = nullptr);
    void removeAnonymousWrappersForInlineChildrenIfNeeded(RenderElement& parent);
    RenderPtr<RenderObject> takeChildFromRenderMenuList(RenderMenuList& parent, RenderObject& child);
    RenderPtr<RenderObject> takeChildFromRenderButton(RenderButton& parent, RenderObject& child);
    RenderPtr<RenderObject> takeChildFromRenderGrid(RenderGrid& parent, RenderObject& child);

    void insertChildToRenderGrid(RenderGrid& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild = nullptr);

    FirstLetter& firstLetterBuilder() { return *m_firstLetterBuilder; }
    List& listBuilder() { return *m_listBuilder; }
    MultiColumn& multiColumnBuilder() { return *m_multiColumnBuilder; }
    Table& tableBuilder() { return *m_tableBuilder; }
    Ruby& rubyBuilder() { return *m_rubyBuilder; }
    FormControls& formControlsBuilder() { return *m_formControlsBuilder; }
    Block& blockBuilder() { return *m_blockBuilder; }
    BlockFlow& blockFlowBuilder() { return *m_blockFlowBuilder; }
    Inline& inlineBuilder() { return *m_inlineBuilder; }
    SVG& svgBuilder() { return *m_svgBuilder; }
    MathML& mathMLBuilder() { return *m_mathMLBuilder; }

    RenderView& m_view;

    RenderTreeBuilder* m_previous { nullptr };
    static RenderTreeBuilder* s_current;

    std::unique_ptr<FirstLetter> m_firstLetterBuilder;
    std::unique_ptr<List> m_listBuilder;
    std::unique_ptr<MultiColumn> m_multiColumnBuilder;
    std::unique_ptr<Table> m_tableBuilder;
    std::unique_ptr<Ruby> m_rubyBuilder;
    std::unique_ptr<FormControls> m_formControlsBuilder;
    std::unique_ptr<Block> m_blockBuilder;
    std::unique_ptr<BlockFlow> m_blockFlowBuilder;
    std::unique_ptr<Inline> m_inlineBuilder;
    std::unique_ptr<SVG> m_svgBuilder;
    std::unique_ptr<MathML> m_mathMLBuilder;
};

}

