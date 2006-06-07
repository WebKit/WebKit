/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

namespace WebCore {

class Document;
class Element;
class Node;
class Position;
class String;
class VisiblePosition;

const unsigned short NON_BREAKING_SPACE = 0xa0;

Position rangeCompliantEquivalent(const Position&);
Position rangeCompliantEquivalent(const VisiblePosition&);
int maxDeepOffset(const Node*);
bool isAtomicNode(const Node*);
bool editingIgnoresContent(const Node*);
bool canHaveChildrenForEditing(const Node*);

void rebalanceWhitespaceInTextNode(Node*, unsigned start, unsigned length);
const String& nonBreakingSpaceString();

//------------------------------------------------------------------------------------------

Position positionBeforeNode(const Node*);
Position positionAfterNode(const Node*);

bool isSpecialElement(const Node*);

PassRefPtr<Element> createDefaultParagraphElement(Document*);
PassRefPtr<Element> createBreakElement(Document*);
PassRefPtr<Element> createOrderedListElement(Document*);
PassRefPtr<Element> createUnorderedListElement(Document*);

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

//------------------------------------------------------------------------------------------

bool isTableStructureNode(const Node*);
PassRefPtr<Element> createBlockPlaceholderElement(Document*);

bool isFirstVisiblePositionInSpecialElement(const Position&);
Position positionBeforeContainingSpecialElement(const Position&, Node** containingSpecialElement=0);
bool isLastVisiblePositionInSpecialElement(const Position&);
Position positionAfterContainingSpecialElement(const Position&, Node** containingSpecialElement=0);
Position positionOutsideContainingSpecialElement(const Position&, Node** containingSpecialElement=0);

Node* enclosingTableCell(Node*);
bool isListElement(Node*);
Node* enclosingList(Node*);
Node* enclosingListChild(Node*);
bool isTableElement(Node*);
bool isFirstVisiblePositionAfterTableElement(const Position&);
Position positionBeforePrecedingTableElement(const Position&);
bool isLastVisiblePositionBeforeTableElement(const Position&);
Position positionAfterFollowingTableElement(const Position&);
Position positionAvoidingSpecialElementBoundary(const Position&);

}

#endif
