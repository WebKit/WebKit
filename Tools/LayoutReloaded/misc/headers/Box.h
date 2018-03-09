/*
 * Copyright (C) 2018 Apple Inc. All Rights Reserved.
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

namespace LayoutReloaded {

class Box {
    Box(Node*,  unsigned id, String name);
    virtual ~Box();
    
    unsigned id() const;
    String name() const;
    Node& node() const;

    Container* parent() const;
    Box* nextSibling() const;
    Box* nextInFlowSibling() const;
    Box* previousSibling() const;
    Box* previousInFlowSibling() const;
    
    void setParent(Container*);
    void setNextSibling(Box*);
    void setPreviousSibling(Box*);
    
    LayoutRect rect() const;
    LayoutPoint topLeft() const;
    LayoutPoint bottomRight() const;
    
    void setTopLeft(LayoutPoint);
    void setSize(LayoutSize);
    void setWidth(LayoutUnit);
    void setHeight(LayoutUnit);
    
    void setIsAnonymous(bool);
    bool isAnonymous() const;
    virtual bool isRootElement() const;
    virtual bool isContainer() const;

    bool isBlockLevelBox() const;
    bool isBlockContainerBox() const;
    bool isInlineLevelBox() const;

    bool establishesFormattingContext() const;
    bool establishesBlockFormattingContext() const;

    FormattingContext* establishedFormattingContext();

    bool isInFlow() const;
    bool isPositioned() const;
    bool isRelativePositioned() const;
    bool isAbsolutePositioned() const;
    bool isFixedPositioned() const;
    bool isOutOfFlowPositioned() const;
    bool isInFlowPositioned() const;
    bool isFloatingPositioned() const;
    bool isFloatingOrOutOfFlowPositioned() const;
    
    BlockContainer* containingBlock() const;

    LayoutRect& borderBox() const;
    LayoutRect& paddingBox() const;
    LayoutRect& contentBox() const;
    
private:
    unsigned m_id;
    String m_rendererName;
    Node* m_node { nullptr };
    Container* m_parent { nullptr };
    Box* m_nextSibling { nullptr };
    Box* m_previousSibling { nullptr };
    bool m_isAnonymous { false };
    LayoutRect m_rect;
    FormattingContext* m_establishedFormattingContext { nullptr };
};

}
