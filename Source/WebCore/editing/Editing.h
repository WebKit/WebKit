/*
 * Copyright (C) 2004, 2006, 2008, 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "Position.h"
#include <wtf/Forward.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

class Document;
class HTMLElement;
class HTMLSpanElement;
class HTMLTextFormControlElement;
class RenderBlock;
class VisiblePosition;
class VisibleSelection;

// -------------------------------------------------------------------------
// Node
// -------------------------------------------------------------------------

ContainerNode* highestEditableRoot(const Position&, EditableType = ContentIsEditable);

Node* highestEnclosingNodeOfType(const Position&, bool (*nodeIsOfType)(const Node*), EditingBoundaryCrossingRule = CannotCrossEditingBoundary, Node* stayWithin = nullptr);
Node* highestNodeToRemoveInPruning(Node*);
Element* lowestEditableAncestor(Node*);

Element* deprecatedEnclosingBlockFlowElement(Node*); // Use enclosingBlock instead.
Element* enclosingBlock(Node*, EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
Element* enclosingTableCell(const Position&);
Node* enclosingEmptyListItem(const VisiblePosition&);
Element* enclosingAnchorElement(const Position&);
Element* enclosingElementWithTag(const Position&, const QualifiedName&);
Node* enclosingNodeOfType(const Position&, bool (*nodeIsOfType)(const Node*), EditingBoundaryCrossingRule = CannotCrossEditingBoundary);
HTMLSpanElement* tabSpanNode(const Node*);
Element* isLastPositionBeforeTable(const VisiblePosition&); // FIXME: Strange to name this isXXX, but return an element.
Element* isFirstPositionAfterTable(const VisiblePosition&); // FIXME: Strange to name this isXXX, but return an element.

// These two deliver leaf nodes as if the whole DOM tree were a linear chain of its leaf nodes.
Node* nextLeafNode(const Node*);
Node* previousLeafNode(const Node*);

WEBCORE_EXPORT int lastOffsetForEditing(const Node&);
int caretMinOffset(const Node&);
int caretMaxOffset(const Node&);

bool hasEditableStyle(const Node&, EditableType);
bool isEditableNode(const Node&);

// FIXME: editingIgnoresContent, canHaveChildrenForEditing, and isAtomicNode should be named to clarify how they differ.

// Returns true for nodes that either have no content, or have content that is ignored (skipped over) while editing.
// There are no VisiblePositions inside these nodes.
bool editingIgnoresContent(const Node&);

bool canHaveChildrenForEditing(const Node&);
bool isAtomicNode(const Node*);

bool isBlock(const Node*);
bool isBlockFlowElement(const Node&);
bool isInline(const Node*);
bool isTabSpanNode(const Node*);
bool isTabSpanTextNode(const Node*);
bool isMailBlockquote(const Node*);
bool isRenderedTable(const Node*);
bool isTableCell(const Node*);
bool isEmptyTableCell(const Node*);
bool isTableStructureNode(const Node*);
bool isListHTMLElement(Node*);
bool isListItem(const Node*);
bool isNodeRendered(const Node&);
bool isRenderedAsNonInlineTableImageOrHR(const Node*);
bool isNonTableCellHTMLBlockElement(const Node*);

bool isNodeVisiblyContainedWithin(Node&, const Range&);

bool areIdenticalElements(const Node&, const Node&);

bool positionBeforeOrAfterNodeIsCandidate(Node&);

// -------------------------------------------------------------------------
// Position
// -------------------------------------------------------------------------

Position nextCandidate(const Position&);
Position previousCandidate(const Position&);

Position nextVisuallyDistinctCandidate(const Position&);
Position previousVisuallyDistinctCandidate(const Position&);

Position positionBeforeContainingSpecialElement(const Position&, HTMLElement** containingSpecialElement = nullptr);
Position positionAfterContainingSpecialElement(const Position&, HTMLElement** containingSpecialElement = nullptr);

Position firstPositionInOrBeforeNode(Node*);
Position lastPositionInOrAfterNode(Node*);

Position firstEditablePositionAfterPositionInRoot(const Position&, ContainerNode* root);
Position lastEditablePositionBeforePositionInRoot(const Position&, ContainerNode* root);

WEBCORE_EXPORT int comparePositions(const Position&, const Position&);

WEBCORE_EXPORT bool isEditablePosition(const Position&, EditableType = ContentIsEditable);
bool isRichlyEditablePosition(const Position&);
bool lineBreakExistsAtPosition(const Position&);
bool isAtUnsplittableElement(const Position&);

unsigned numEnclosingMailBlockquotes(const Position&);
void updatePositionForNodeRemoval(Position&, Node&);

WEBCORE_EXPORT TextDirection directionOfEnclosingBlock(const Position&);

// -------------------------------------------------------------------------
// VisiblePosition
// -------------------------------------------------------------------------

VisiblePosition visiblePositionBeforeNode(Node&);
VisiblePosition visiblePositionAfterNode(Node&);

bool lineBreakExistsAtVisiblePosition(const VisiblePosition&);

WEBCORE_EXPORT int comparePositions(const VisiblePosition&, const VisiblePosition&);

WEBCORE_EXPORT int indexForVisiblePosition(const VisiblePosition&, RefPtr<ContainerNode>& scope);
int indexForVisiblePosition(Node&, const VisiblePosition&, bool forSelectionPreservation);
WEBCORE_EXPORT VisiblePosition visiblePositionForPositionWithOffset(const VisiblePosition&, int offset);
WEBCORE_EXPORT VisiblePosition visiblePositionForIndex(int index, ContainerNode* scope);
VisiblePosition visiblePositionForIndexUsingCharacterIterator(Node&, int index); // FIXME: Why do we need this version?

// -------------------------------------------------------------------------
// HTMLElement
// -------------------------------------------------------------------------

WEBCORE_EXPORT Ref<HTMLElement> createDefaultParagraphElement(Document&);
Ref<HTMLElement> createHTMLElement(Document&, const QualifiedName&);
Ref<HTMLElement> createHTMLElement(Document&, const AtomString&);

WEBCORE_EXPORT HTMLElement* enclosingList(Node*);
HTMLElement* outermostEnclosingList(Node*, Node* rootList = nullptr);
Node* enclosingListChild(Node*);

// -------------------------------------------------------------------------
// Element
// -------------------------------------------------------------------------

Ref<Element> createTabSpanElement(Document&);
Ref<Element> createTabSpanElement(Document&, const String& tabText);
Ref<Element> createBlockPlaceholderElement(Document&);

Element* editableRootForPosition(const Position&, EditableType = ContentIsEditable);
Element* unsplittableElementForPosition(const Position&);

bool canMergeLists(Element* firstList, Element* secondList);

// -------------------------------------------------------------------------
// VisibleSelection
// -------------------------------------------------------------------------

VisibleSelection selectionForParagraphIteration(const VisibleSelection&);
Position adjustedSelectionStartForStyleComputation(const VisibleSelection&);

// -------------------------------------------------------------------------

// FIXME: This is only one of many definitions of whitespace. Possibly never the right one to use.
bool deprecatedIsEditingWhitespace(UChar);

// FIXME: Can't answer this question correctly without being passed the white-space mode.
bool deprecatedIsCollapsibleWhitespace(UChar);

bool isAmbiguousBoundaryCharacter(UChar);

String stringWithRebalancedWhitespace(const String&, bool startIsStartOfParagraph, bool endIsEndOfParagraph);
const String& nonBreakingSpaceString();

// Miscellaneous functions for caret rendering.

RenderBlock* rendererForCaretPainting(Node*);
LayoutRect localCaretRectInRendererForCaretPainting(const VisiblePosition&, RenderBlock*&);
LayoutRect localCaretRectInRendererForRect(LayoutRect&, Node*, RenderObject*, RenderBlock*&);
IntRect absoluteBoundsForLocalCaretRect(RenderBlock* rendererForCaretPainting, const LayoutRect&, bool* insideFixed = nullptr);

// -------------------------------------------------------------------------

inline bool deprecatedIsEditingWhitespace(UChar c)
{
    return c == noBreakSpace || c == ' ' || c == '\n' || c == '\t';
}

// FIXME: Can't really answer this question correctly without knowing the white-space mode.
inline bool deprecatedIsCollapsibleWhitespace(UChar c)
{
    return c == ' ' || c == '\n';
}

bool isAmbiguousBoundaryCharacter(UChar);

inline bool editingIgnoresContent(const Node& node)
{
    return !node.canContainRangeEndPoint();
}

inline bool positionBeforeOrAfterNodeIsCandidate(Node& node)
{
    return isRenderedTable(&node) || editingIgnoresContent(node);
}

inline Position firstPositionInOrBeforeNode(Node* node)
{
    if (!node)
        return { };
    return editingIgnoresContent(*node) ? positionBeforeNode(node) : firstPositionInNode(node);
}

inline Position lastPositionInOrAfterNode(Node* node)
{
    if (!node)
        return { };
    return editingIgnoresContent(*node) ? positionAfterNode(node) : lastPositionInNode(node);
}

}
