/*
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef htmlediting_h
#define htmlediting_h

#include <wtf/Forward.h>
#include "HTMLNames.h"
#include "ExceptionCode.h"
#include "Position.h"

namespace WebCore {

class Document;
class Element;
class HTMLElement;
class Node;
class Position;
class Range;
class VisiblePosition;
class VisibleSelection;


// This file contains a set of helper functions used by the editing commands

// -------------------------------------------------------------------------
// Node
// -------------------------------------------------------------------------

// Functions returning Node

Node* highestAncestor(Node*);
Node* highestEditableRoot(const Position&);
Node* highestEnclosingNodeOfType(const Position&, bool (*nodeIsOfType)(const Node*));
Node* lowestEditableAncestor(Node*);   

Node* enclosingBlock(Node*);
Node* enclosingTableCell(const Position&);
Node* enclosingEmptyListItem(const VisiblePosition&);
Node* enclosingAnchorElement(const Position&);
Node* enclosingNodeWithTag(const Position&, const QualifiedName&);
Node* enclosingNodeOfType(const Position&, bool (*nodeIsOfType)(const Node*), bool onlyReturnEditableNodes = true);

Node* tabSpanNode(const Node*);
Node* nearestMailBlockquote(const Node*);
Node* isLastPositionBeforeTable(const VisiblePosition&);
Node* isFirstPositionAfterTable(const VisiblePosition&);

// offset functions on Node

int lastOffsetForEditing(const Node*);
int caretMinOffset(const Node*);
int caretMaxOffset(const Node*);

// boolean functions on Node

bool editingIgnoresContent(const Node*);
bool canHaveChildrenForEditing(const Node*);
bool isAtomicNode(const Node*);
bool isBlock(const Node*);
bool isSpecialElement(const Node*);
bool isTabSpanNode(const Node*);
bool isTabSpanTextNode(const Node*);
bool isMailBlockquote(const Node*);
bool isTableElement(Node*);
bool isTableCell(const Node*);
bool isEmptyTableCell(const Node*);
bool isTableStructureNode(const Node*);
bool isListElement(Node*);
bool isListItem(Node*);
bool isNodeRendered(const Node*);
bool isNodeVisiblyContainedWithin(Node*, const Range*);
bool isRenderedAsNonInlineTableImageOrHR(const Node*);
bool isNodeInTextFormControl(Node* node);
    
// -------------------------------------------------------------------------
// Position
// -------------------------------------------------------------------------
    
// Functions returning Position
    
Position rangeCompliantEquivalent(const Position&);
Position rangeCompliantEquivalent(const VisiblePosition&);
    
Position nextCandidate(const Position&);
Position previousCandidate(const Position&);
    
Position nextVisuallyDistinctCandidate(const Position&);
Position previousVisuallyDistinctCandidate(const Position&);

Position positionBeforeTabSpan(const Position&);
Position positionBeforeContainingSpecialElement(const Position&, Node** containingSpecialElement=0);
Position positionAfterContainingSpecialElement(const Position&, Node** containingSpecialElement=0);
Position positionOutsideContainingSpecialElement(const Position&, Node** containingSpecialElement=0);
    
// Position creation functions are inline to prevent ref-churn.
// Other Position creation functions are in Position.h
// but these depend on lastOffsetForEditing which is defined in htmlediting.h.

// NOTE: first/lastDeepEditingPositionForNode return legacy editing positions (like [img, 0])
// for elements which editing ignores.  The rest of the editing code will treat [img, 0]
// as "the last position before the img".
// New code should use the creation functions in Position.h instead.
inline Position firstDeepEditingPositionForNode(Node* anchorNode)
{
    ASSERT(anchorNode);
    return Position(anchorNode, 0);
}

inline Position lastDeepEditingPositionForNode(Node* anchorNode)
{
    ASSERT(anchorNode);
    return Position(anchorNode, lastOffsetForEditing(anchorNode));
}
       
// comparision functions on Position
    
int comparePositions(const Position&, const Position&);

// boolean functions on Position
    
bool isEditablePosition(const Position&);
bool isRichlyEditablePosition(const Position&);
bool isFirstVisiblePositionInSpecialElement(const Position&);
bool isLastVisiblePositionInSpecialElement(const Position&);
bool lineBreakExistsAtPosition(const Position&);
bool isVisiblyAdjacent(const Position& first, const Position& second);
bool isAtUnsplittableElement(const Position&);

// miscellaneous functions on Position

unsigned numEnclosingMailBlockquotes(const Position&);

// -------------------------------------------------------------------------
// VisiblePosition
// -------------------------------------------------------------------------
    
// Functions returning VisiblePosition
    
VisiblePosition firstEditablePositionAfterPositionInRoot(const Position&, Node*);
VisiblePosition lastEditablePositionBeforePositionInRoot(const Position&, Node*);
VisiblePosition visiblePositionBeforeNode(Node*);
VisiblePosition visiblePositionAfterNode(Node*);

bool lineBreakExistsAtVisiblePosition(const VisiblePosition&);
    
int comparePositions(const VisiblePosition&, const VisiblePosition&);
int indexForVisiblePosition(const VisiblePosition&);

// -------------------------------------------------------------------------
// Range
// -------------------------------------------------------------------------

// Functions returning Range

PassRefPtr<Range> createRange(PassRefPtr<Document>, const VisiblePosition& start, const VisiblePosition& end, ExceptionCode&);
PassRefPtr<Range> extendRangeToWrappingNodes(PassRefPtr<Range> rangeToExtend, const Range* maximumRange, const Node* rootNode);
PassRefPtr<Range> avoidIntersectionWithNode(const Range*, Node*);

// -------------------------------------------------------------------------
// HTMLElement
// -------------------------------------------------------------------------
    
// Functions returning HTMLElement
    
PassRefPtr<HTMLElement> createDefaultParagraphElement(Document*);
PassRefPtr<HTMLElement> createBreakElement(Document*);
PassRefPtr<HTMLElement> createOrderedListElement(Document*);
PassRefPtr<HTMLElement> createUnorderedListElement(Document*);
PassRefPtr<HTMLElement> createListItemElement(Document*);
PassRefPtr<HTMLElement> createHTMLElement(Document*, const QualifiedName&);
PassRefPtr<HTMLElement> createHTMLElement(Document*, const AtomicString&);

HTMLElement* enclosingList(Node*);
HTMLElement* outermostEnclosingList(Node*, Node* rootList = 0);
HTMLElement* enclosingListChild(Node*);

// -------------------------------------------------------------------------
// Element
// -------------------------------------------------------------------------
    
// Functions returning Element
    
PassRefPtr<Element> createTabSpanElement(Document*);
PassRefPtr<Element> createTabSpanElement(Document*, PassRefPtr<Node> tabTextNode);
PassRefPtr<Element> createTabSpanElement(Document*, const String& tabText);
PassRefPtr<Element> createBlockPlaceholderElement(Document*);

Element* editableRootForPosition(const Position&);
Element* unsplittableElementForPosition(const Position&);

// Boolean functions on Element
    
bool canMergeLists(Element* firstList, Element* secondList);
    
// -------------------------------------------------------------------------
// VisibleSelection
// -------------------------------------------------------------------------

// Functions returning VisibleSelection
VisibleSelection avoidIntersectionWithNode(const VisibleSelection&, Node*);
VisibleSelection selectionForParagraphIteration(const VisibleSelection&);
    

// Miscellaneous functions on String
    
String stringWithRebalancedWhitespace(const String&, bool, bool);
const String& nonBreakingSpaceString();

}

#endif
