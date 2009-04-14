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

namespace WebCore {

class Document;
class Element;
class HTMLElement;
class Node;
class Position;
class Range;
class VisibleSelection;
class String;
class VisiblePosition;

Position rangeCompliantEquivalent(const Position&);
Position rangeCompliantEquivalent(const VisiblePosition&);
int lastOffsetForEditing(const Node*);
bool isAtomicNode(const Node*);
bool editingIgnoresContent(const Node*);
bool canHaveChildrenForEditing(const Node*);
Node* highestEditableRoot(const Position&);
VisiblePosition firstEditablePositionAfterPositionInRoot(const Position&, Node*);
VisiblePosition lastEditablePositionBeforePositionInRoot(const Position&, Node*);
int comparePositions(const Position&, const Position&);
Node* lowestEditableAncestor(Node*);
bool isContentEditable(const Node*);
Position nextCandidate(const Position&);
Position nextVisuallyDistinctCandidate(const Position&);
Position previousCandidate(const Position&);
Position previousVisuallyDistinctCandidate(const Position&);
bool isEditablePosition(const Position&);
bool isRichlyEditablePosition(const Position&);
Element* editableRootForPosition(const Position&);
bool isBlock(const Node*);
Node* enclosingBlock(Node*);

String stringWithRebalancedWhitespace(const String&, bool, bool);
const String& nonBreakingSpaceString();

//------------------------------------------------------------------------------------------

Position positionBeforeNode(const Node*);
Position positionAfterNode(const Node*);

PassRefPtr<Range> avoidIntersectionWithNode(const Range*, Node*);
VisibleSelection avoidIntersectionWithNode(const VisibleSelection&, Node*);

bool isSpecialElement(const Node*);
bool validBlockTag(const String&);

PassRefPtr<HTMLElement> createDefaultParagraphElement(Document*);
PassRefPtr<HTMLElement> createBreakElement(Document*);
PassRefPtr<HTMLElement> createOrderedListElement(Document*);
PassRefPtr<HTMLElement> createUnorderedListElement(Document*);
PassRefPtr<HTMLElement> createListItemElement(Document*);
PassRefPtr<HTMLElement> createHTMLElement(Document*, const QualifiedName&);
PassRefPtr<HTMLElement> createHTMLElement(Document*, const AtomicString&);

bool isTabSpanNode(const Node*);
bool isTabSpanTextNode(const Node*);
Node* tabSpanNode(const Node*);
Position positionBeforeTabSpan(const Position&);
PassRefPtr<Element> createTabSpanElement(Document*);
PassRefPtr<Element> createTabSpanElement(Document*, PassRefPtr<Node> tabTextNode);
PassRefPtr<Element> createTabSpanElement(Document*, const String& tabText);

bool isNodeRendered(const Node*);
bool isMailBlockquote(const Node*);
Node* nearestMailBlockquote(const Node*);
unsigned numEnclosingMailBlockquotes(const Position&);
int caretMinOffset(const Node*);
int caretMaxOffset(const Node*);

//------------------------------------------------------------------------------------------

bool isTableStructureNode(const Node*);
PassRefPtr<Element> createBlockPlaceholderElement(Document*);

bool isFirstVisiblePositionInSpecialElement(const Position&);
Position positionBeforeContainingSpecialElement(const Position&, Node** containingSpecialElement=0);
bool isLastVisiblePositionInSpecialElement(const Position&);
Position positionAfterContainingSpecialElement(const Position&, Node** containingSpecialElement=0);
Position positionOutsideContainingSpecialElement(const Position&, Node** containingSpecialElement=0);
Node* isLastPositionBeforeTable(const VisiblePosition&);
Node* isFirstPositionAfterTable(const VisiblePosition&);

Node* enclosingNodeWithTag(const Position&, const QualifiedName&);
Node* enclosingNodeOfType(const Position&, bool (*nodeIsOfType)(const Node*), bool onlyReturnEditableNodes = true);
Node* highestEnclosingNodeOfType(const Position&, bool (*nodeIsOfType)(const Node*));
Node* enclosingTableCell(const Position&);
Node* enclosingEmptyListItem(const VisiblePosition&);
Node* enclosingAnchorElement(const Position&);
bool isListElement(Node*);
HTMLElement* enclosingList(Node*);
HTMLElement* outermostEnclosingList(Node*);
Node* enclosingListChild(Node*);
Node* highestAncestor(Node*);
bool isTableElement(Node*);
bool isTableCell(const Node*);

bool lineBreakExistsAtPosition(const Position&);
bool lineBreakExistsAtVisiblePosition(const VisiblePosition&);

VisibleSelection selectionForParagraphIteration(const VisibleSelection&);

int indexForVisiblePosition(VisiblePosition&);

}

#endif
