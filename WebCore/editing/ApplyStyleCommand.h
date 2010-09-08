/*
 * Copyright (C) 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef ApplyStyleCommand_h
#define ApplyStyleCommand_h

#include "CompositeEditCommand.h"

namespace WebCore {

class CSSPrimitiveValue;
class HTMLElement;
class StyleChange;

enum ShouldIncludeTypingStyle {
    IncludeTypingStyle,
    IgnoreTypingStyle
};

class ApplyStyleCommand : public CompositeEditCommand {
public:
    enum EPropertyLevel { PropertyDefault, ForceBlockProperties };
    enum InlineStyleRemovalMode { RemoveAttributesAndElements, RemoveNone };
    enum EAddStyledElement { AddStyledElement, DoNotAddStyledElement };

    static PassRefPtr<ApplyStyleCommand> create(Document* document, CSSStyleDeclaration* style, EditAction action = EditActionChangeAttributes, EPropertyLevel level = PropertyDefault)
    {
        return adoptRef(new ApplyStyleCommand(document, style, action, level));
    }
    static PassRefPtr<ApplyStyleCommand> create(Document* document, CSSStyleDeclaration* style, const Position& start, const Position& end, EditAction action = EditActionChangeAttributes, EPropertyLevel level = PropertyDefault)
    {
        return adoptRef(new ApplyStyleCommand(document, style, start, end, action, level));
    }
    static PassRefPtr<ApplyStyleCommand> create(PassRefPtr<Element> element, bool removeOnly = false, EditAction action = EditActionChangeAttributes)
    {
        return adoptRef(new ApplyStyleCommand(element, removeOnly, action));
    }
    
    static PassRefPtr<CSSMutableStyleDeclaration> editingStyleAtPosition(Position pos, ShouldIncludeTypingStyle shouldIncludeTypingStyle = IgnoreTypingStyle);

private:
    ApplyStyleCommand(Document*, CSSStyleDeclaration*, EditAction, EPropertyLevel);
    ApplyStyleCommand(Document*, CSSStyleDeclaration*, const Position& start, const Position& end, EditAction, EPropertyLevel);
    ApplyStyleCommand(PassRefPtr<Element>, bool removeOnly, EditAction);

    virtual void doApply();
    virtual EditAction editingAction() const;

    CSSMutableStyleDeclaration* style() const { return m_style.get(); }

    // style-removal helpers
    bool removeInlineStyleFromElement(CSSMutableStyleDeclaration*, HTMLElement*, InlineStyleRemovalMode = RemoveAttributesAndElements);
    inline bool shouldRemoveInlineStyleFromElement(CSSMutableStyleDeclaration* style, HTMLElement* element) {return removeInlineStyleFromElement(style, element, RemoveNone);}
    bool removeImplicitlyStyledElement(CSSMutableStyleDeclaration*, HTMLElement*, InlineStyleRemovalMode, CSSMutableStyleDeclaration* extractedStyle = 0);
    void replaceWithSpanOrRemoveIfWithoutAttributes(HTMLElement*&);
    bool removeCSSStyle(CSSMutableStyleDeclaration*, HTMLElement*, InlineStyleRemovalMode = RemoveAttributesAndElements);
    HTMLElement* highestAncestorWithConflictingInlineStyle(CSSMutableStyleDeclaration*, Node*);
    PassRefPtr<CSSMutableStyleDeclaration> extractInlineStyleToPushDown(CSSMutableStyleDeclaration*, Node*, bool isStyledElement);
    void applyInlineStyleToPushDown(Node*, CSSMutableStyleDeclaration *style);
    void pushDownInlineStyleAroundNode(CSSMutableStyleDeclaration*, Node*);
    void removeInlineStyle(PassRefPtr<CSSMutableStyleDeclaration>, const Position& start, const Position& end);
    bool nodeFullySelected(Node*, const Position& start, const Position& end) const;
    bool nodeFullyUnselected(Node*, const Position& start, const Position& end) const;

    // style-application helpers
    void applyBlockStyle(CSSMutableStyleDeclaration*);
    void applyRelativeFontStyleChange(CSSMutableStyleDeclaration*);
    void applyInlineStyle(CSSMutableStyleDeclaration*);
    void fixRangeAndApplyInlineStyle(CSSMutableStyleDeclaration*, const Position& start, const Position& end);
    void applyInlineStyleToNodeRange(CSSMutableStyleDeclaration*, Node* startNode, Node* pastEndNode);
    void addBlockStyle(const StyleChange&, HTMLElement*);
    void addInlineStyleIfNeeded(CSSMutableStyleDeclaration*, Node* start, Node* end, EAddStyledElement addStyledElement = AddStyledElement);
    void splitTextAtStart(const Position& start, const Position& end);
    void splitTextAtEnd(const Position& start, const Position& end);
    void splitTextElementAtStart(const Position& start, const Position& end);
    void splitTextElementAtEnd(const Position& start, const Position& end);
    bool shouldSplitTextElement(Element* elem, CSSMutableStyleDeclaration*);
    bool isValidCaretPositionInTextNode(const Position& position);
    bool mergeStartWithPreviousIfIdentical(const Position& start, const Position& end);
    bool mergeEndWithNextIfIdentical(const Position& start, const Position& end);
    void cleanupUnstyledAppleStyleSpans(Node* dummySpanAncestor);

    void surroundNodeRangeWithElement(Node* start, Node* end, PassRefPtr<Element>);
    float computedFontSize(const Node*);
    void joinChildTextNodes(Node*, const Position& start, const Position& end);

    HTMLElement* splitAncestorsWithUnicodeBidi(Node*, bool before, int allowedDirection);
    void removeEmbeddingUpToEnclosingBlock(Node* node, Node* unsplitAncestor);

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
RefPtr<CSSMutableStyleDeclaration> getPropertiesNotInComputedStyle(CSSStyleDeclaration* style, CSSComputedStyleDeclaration* computedStyle);

void prepareEditingStyleToApplyAt(CSSMutableStyleDeclaration*, Position);
void removeStylesAddedByNode(CSSMutableStyleDeclaration*, Node*);

} // namespace WebCore

#endif
