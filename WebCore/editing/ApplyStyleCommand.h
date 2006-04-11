/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef apply_style_command_h__
#define apply_style_command_h__

#include "CompositeEditCommand.h"

namespace WebCore {

class HTMLElement;

class ApplyStyleCommand : public CompositeEditCommand
{
public:
    enum EPropertyLevel { PropertyDefault, ForceBlockProperties };

    ApplyStyleCommand(Document*, CSSStyleDeclaration*, EditAction = EditActionChangeAttributes, EPropertyLevel = PropertyDefault);
    ApplyStyleCommand(Document*, CSSStyleDeclaration*, const Position& start, const Position& end, EditAction = EditActionChangeAttributes, EPropertyLevel = PropertyDefault);
    ApplyStyleCommand(Document*, Element*, bool = false, EditAction = EditActionChangeAttributes);

    virtual void doApply();
    virtual EditAction editingAction() const;

    CSSMutableStyleDeclaration* style() const { return m_style.get(); }

private:
    // style-removal helpers
    bool isHTMLStyleNode(CSSMutableStyleDeclaration*, HTMLElement*);
    void removeHTMLStyleNode(HTMLElement *);
    void removeHTMLFontStyle(CSSMutableStyleDeclaration*, HTMLElement*);
    void removeCSSStyle(CSSMutableStyleDeclaration*, HTMLElement*);
    void removeBlockStyle(CSSMutableStyleDeclaration*, const Position& start, const Position& end);
    void removeInlineStyle(PassRefPtr<CSSMutableStyleDeclaration>, const Position& start, const Position& end);
    bool nodeFullySelected(Node*, const Position& start, const Position& end) const;
    bool nodeFullyUnselected(Node*, const Position& start, const Position& end) const;
    PassRefPtr<CSSMutableStyleDeclaration> extractTextDecorationStyle(Node*);
    PassRefPtr<CSSMutableStyleDeclaration> extractAndNegateTextDecorationStyle(Node*);
    void applyTextDecorationStyle(Node*, CSSMutableStyleDeclaration *style);
    void pushDownTextDecorationStyleAroundNode(Node*, const Position& start, const Position& end, bool force);
    void pushDownTextDecorationStyleAtBoundaries(const Position& start, const Position& end);
    
    // style-application helpers
    void applyBlockStyle(CSSMutableStyleDeclaration*);
    void applyRelativeFontStyleChange(CSSMutableStyleDeclaration*);
    void applyInlineStyle(CSSMutableStyleDeclaration*);
    void addBlockStyleIfNeeded(CSSMutableStyleDeclaration*, Node*);
    void addInlineStyleIfNeeded(CSSMutableStyleDeclaration*, Node* start, Node* end);
    bool splitTextAtStartIfNeeded(const Position& start, const Position& end);
    bool splitTextAtEndIfNeeded(const Position& start, const Position& end);
    bool splitTextElementAtStartIfNeeded(const Position& start, const Position& end);
    bool splitTextElementAtEndIfNeeded(const Position& start, const Position& end);
    bool mergeStartWithPreviousIfIdentical(const Position& start, const Position& end);
    bool mergeEndWithNextIfIdentical(const Position& start, const Position& end);
    void cleanUpEmptyStyleSpans(const Position& start, const Position& end);

    void surroundNodeRangeWithElement(Node* start, Node* end, Element* element);
    float computedFontSize(const Node*);
    void joinChildTextNodes(Node*, const Position& start, const Position& end);

    void updateStartEnd(const Position& newStart, const Position& newEnd);
    Position startPosition();
    Position endPosition();

    RefPtr<CSSMutableStyleDeclaration> m_style;
    EditAction m_editingAction;
    EPropertyLevel m_propertyLevel;
    Position m_start;
    Position m_end;
    bool m_useEndingSelection;
    RefPtr<Element> m_styledInlineElement;
    bool m_removeOnly;
};

bool isStyleSpan(const Node*);
PassRefPtr<HTMLElement> createStyleSpanElement(Document*);

} // namespace WebCore

#endif
