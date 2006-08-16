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

#include "config.h"
#include "ReplaceSelectionCommand.h"

#include "ApplyStyleCommand.h"
#include "BeforeTextInsertedEvent.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "EditingText.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLElement.h"
#include "HTMLInterchange.h"
#include "HTMLNames.h"
#include "SelectionController.h"
#include "TextIterator.h"
#include "htmlediting.h"
#include "markup.h"
#include "visible_units.h"

namespace WebCore {

using namespace HTMLNames;

ReplacementFragment::ReplacementFragment(Document* document, DocumentFragment* fragment, bool matchStyle, const Selection& selection)
    : m_document(document),
      m_fragment(fragment),
      m_matchStyle(matchStyle), 
      m_hasInterchangeNewlineAtStart(false), 
      m_hasInterchangeNewlineAtEnd(false)
{
    if (!m_document)
        return;
    if (!m_fragment)
        return;
    Node* firstChild = m_fragment->firstChild();
    if (!firstChild)
        return;
    
    Element* editableRoot = selection.rootEditableElement();
    ASSERT(editableRoot);
    if (!editableRoot)
        return;

    Node* styleNode = selection.base().node();
    RefPtr<Node> holder = insertFragmentForTestRendering(styleNode);
    
    RefPtr<Range> range = Selection::selectionFromContentsOfNode(holder.get()).toRange();
    String text = plainText(range.get());
    // Give the root a chance to change the text.
    RefPtr<BeforeTextInsertedEvent> evt = new BeforeTextInsertedEvent(text);
    ExceptionCode ec = 0;
    editableRoot->dispatchEvent(evt, ec, true);
    ASSERT(ec == 0);
    if (text != evt->text() || !editableRoot->isContentRichlyEditable()) {
        restoreTestRenderingNodesToFragment(holder.get());
        removeNode(holder);

        m_fragment = createFragmentFromText(selection.toRange().get(), evt->text());
        firstChild = m_fragment->firstChild();
        if (!firstChild)
            return;
        holder = insertFragmentForTestRendering(styleNode);
    }

    Node *node = firstChild;
    Node *newlineAtStartNode = 0;
    Node *newlineAtEndNode = 0;
    while (node) {
        Node *next = node->traverseNextNode();
        if (isInterchangeNewlineNode(node)) {
            if (next || node == firstChild) {
                m_hasInterchangeNewlineAtStart = true;
                newlineAtStartNode = node;
            }
            else {
                m_hasInterchangeNewlineAtEnd = true;
                newlineAtEndNode = node;
            }
        }
        else if (isInterchangeConvertedSpaceSpan(node)) {
            RefPtr<Node> n = 0;
            while ((n = node->firstChild())) {
                removeNode(n);
                insertNodeBefore(n.get(), node);
            }
            removeNode(node);
            if (n)
                next = n->traverseNextNode();
        }
        node = next;
    }

    if (newlineAtStartNode)
        removeNode(newlineAtStartNode);
    if (newlineAtEndNode)
        removeNode(newlineAtEndNode);
    
    saveRenderingInfo(holder.get());
    removeUnrenderedNodes(holder.get());
    restoreTestRenderingNodesToFragment(holder.get());
    removeNode(holder);
    removeStyleNodes();
}

bool ReplacementFragment::isEmpty() const
{
    return (!m_fragment || !m_fragment->firstChild()) && !m_hasInterchangeNewlineAtStart && !m_hasInterchangeNewlineAtEnd;
}

Node *ReplacementFragment::firstChild() const 
{ 
    return m_fragment->firstChild(); 
}

Node *ReplacementFragment::lastChild() const 
{ 
    return m_fragment->lastChild(); 
}

bool ReplacementFragment::isInterchangeNewlineNode(const Node *node)
{
    static String interchangeNewlineClassString(AppleInterchangeNewline);
    return node && node->hasTagName(brTag) && 
           static_cast<const Element *>(node)->getAttribute(classAttr) == interchangeNewlineClassString;
}

bool ReplacementFragment::isInterchangeConvertedSpaceSpan(const Node *node)
{
    static String convertedSpaceSpanClassString(AppleConvertedSpace);
    return node->isHTMLElement() && 
           static_cast<const HTMLElement *>(node)->getAttribute(classAttr) == convertedSpaceSpanClassString;
}

void ReplacementFragment::removeNodePreservingChildren(Node *node)
{
    if (!node)
        return;

    while (RefPtr<Node> n = node->firstChild()) {
        removeNode(n);
        insertNodeBefore(n.get(), node);
    }
    removeNode(node);
}

void ReplacementFragment::removeNode(PassRefPtr<Node> node)
{
    if (!node)
        return;
    
    Node *parent = node->parentNode();
    if (!parent)
        return;
    
    ExceptionCode ec = 0;
    parent->removeChild(node.get(), ec);
    ASSERT(ec == 0);
}

void ReplacementFragment::insertNodeBefore(Node *node, Node *refNode)
{
    if (!node || !refNode)
        return;
        
    Node *parent = refNode->parentNode();
    if (!parent)
        return;
        
    ExceptionCode ec = 0;
    parent->insertBefore(node, refNode, ec);
    ASSERT(ec == 0);
}

PassRefPtr<Node> ReplacementFragment::insertFragmentForTestRendering(Node* context)
{
    Node* body = m_document->body();
    if (!body)
        return 0;

    RefPtr<StyledElement> holder = static_pointer_cast<StyledElement>(createDefaultParagraphElement(m_document.get()));
    
    ExceptionCode ec = 0;

    // Copy the whitespace style from the context onto this element.
    Node* n = context;
    while (n && !n->isElementNode())
        n = n->parentNode();
    if (n) {
        RefPtr<CSSComputedStyleDeclaration> contextStyle = new CSSComputedStyleDeclaration(static_cast<Element*>(n));
        CSSStyleDeclaration* style = holder->style();
        style->setProperty(CSS_PROP_WHITE_SPACE, contextStyle->getPropertyValue(CSS_PROP_WHITE_SPACE), false, ec);
        ASSERT(ec == 0);
    }
    
    holder->appendChild(m_fragment, ec);
    ASSERT(ec == 0);
    
    body->appendChild(holder.get(), ec);
    ASSERT(ec == 0);
    
    m_document->updateLayoutIgnorePendingStylesheets();
    
    return holder.release();
}

void ReplacementFragment::restoreTestRenderingNodesToFragment(Node *holder)
{
    if (!holder)
        return;
    
    ExceptionCode ec = 0;
    while (RefPtr<Node> node = holder->firstChild()) {
        holder->removeChild(node.get(), ec);
        ASSERT(ec == 0);
        m_fragment->appendChild(node.get(), ec);
        ASSERT(ec == 0);
    }
}

static String &matchNearestBlockquoteColorString()
{
    static String matchNearestBlockquoteColorString = "match";
    return matchNearestBlockquoteColorString;
}

void ReplaceSelectionCommand::fixupNodeStyles(const NodeVector& nodes, const RenderingInfoMap& renderingInfo)
{
    // This function uses the mapped "desired style" to apply the additional style needed, if any,
    // to make the node have the desired style.

    updateLayout();
    
    NodeVector::const_iterator e = nodes.end();
    for (NodeVector::const_iterator it = nodes.begin(); it != e; ++it) {
        Node *node = (*it).get();
        RefPtr<RenderingInfo> info = renderingInfo.get(node);
        ASSERT(info);
        if (!info)
            continue;
        CSSMutableStyleDeclaration *desiredStyle = info->style();
        ASSERT(desiredStyle);

        if (!node->inDocument())
            continue;

        // The desiredStyle declaration tells what style this node wants to be.
        // Compare that to the style that it is right now in the document.
        Position pos(node, 0);
        RefPtr<CSSComputedStyleDeclaration> currentStyle = pos.computedStyle();

        // Check for the special "match nearest blockquote color" property and resolve to the correct
        // color if necessary.
        String matchColorCheck = desiredStyle->getPropertyValue(CSS_PROP__WEBKIT_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR);
        if (matchColorCheck == matchNearestBlockquoteColorString()) {
            Node *blockquote = nearestMailBlockquote(node);
            Position pos(blockquote ? blockquote : node->document()->documentElement(), 0);
            RefPtr<CSSComputedStyleDeclaration> style = pos.computedStyle();
            String desiredColor = desiredStyle->getPropertyValue(CSS_PROP_COLOR);
            String nearestColor = style->getPropertyValue(CSS_PROP_COLOR);
            if (desiredColor != nearestColor)
                desiredStyle->setProperty(CSS_PROP_COLOR, nearestColor);
        }
        desiredStyle->removeProperty(CSS_PROP__WEBKIT_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR);

        currentStyle->diff(desiredStyle);
        
        // Only add in block properties if the node is at the start of a 
        // paragraph. This matches AppKit.
        if (!isStartOfParagraph(VisiblePosition(pos, DOWNSTREAM)))
            desiredStyle->removeBlockProperties();
        
        // If the desiredStyle is non-zero length, that means the current style differs
        // from the desired by the styles remaining in the desiredStyle declaration.
        if (desiredStyle->length() > 0)
            applyStyle(desiredStyle, Position(node, 0), Position(node, maxDeepOffset(node)));
    }
}

static PassRefPtr<CSSMutableStyleDeclaration> styleForNode(Node *node)
{
    if (!node || !node->inDocument())
        return 0;
        
    RefPtr<CSSComputedStyleDeclaration> computedStyle = Position(node, 0).computedStyle();
    RefPtr<CSSMutableStyleDeclaration> style = computedStyle->copyInheritableProperties();

    // In either of the color-matching tests below, set the color to a pseudo-color that will
    // make the content take on the color of the nearest-enclosing blockquote (if any) after
    // being pasted in.
    if (Node *blockquote = nearestMailBlockquote(node)) {
        RefPtr<CSSComputedStyleDeclaration> blockquoteStyle = Position(blockquote, 0).computedStyle();
        bool match = (blockquoteStyle->getPropertyValue(CSS_PROP_COLOR) == style->getPropertyValue(CSS_PROP_COLOR));
        if (match) {
            style->setProperty(CSS_PROP__WEBKIT_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR, matchNearestBlockquoteColorString());
            return style.release();
        }
    }
    Node *documentElement = node->document()->documentElement();
    RefPtr<CSSComputedStyleDeclaration> documentStyle = Position(documentElement, 0).computedStyle();
    bool match = (documentStyle->getPropertyValue(CSS_PROP_COLOR) == style->getPropertyValue(CSS_PROP_COLOR));
    if (match)
        style->setProperty(CSS_PROP__WEBKIT_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR, matchNearestBlockquoteColorString());
        
    return style.release();
}

void ReplacementFragment::saveRenderingInfo(Node *holder)
{
    m_document->updateLayoutIgnorePendingStylesheets();
    
    if (m_matchStyle) {
        // No style restoration will be done, so we don't need to save styles or keep a node vector.
        for (Node *node = holder->firstChild(); node; node = node->traverseNextNode(holder))
            m_renderingInfo.add(node, new RenderingInfo(0));
    } else {
        for (Node *node = holder->firstChild(); node; node = node->traverseNextNode(holder)) {
            m_renderingInfo.add(node, new RenderingInfo(styleForNode(node)));
            m_nodes.append(node);
        }
    }
}

void ReplacementFragment::removeUnrenderedNodes(Node *holder)
{
    DeprecatedPtrList<Node> unrendered;

    for (Node *node = holder->firstChild(); node; node = node->traverseNextNode(holder)) {
        if (!isNodeRendered(node) && !isTableStructureNode(node))
            unrendered.append(node);
    }

    for (DeprecatedPtrListIterator<Node> it(unrendered); it.current(); ++it)
        removeNode(it.current());
}

void ReplacementFragment::removeStyleNodes()
{
    // Since style information has been computed and cached away in
    // computeStylesUsingTestRendering(), these style nodes can be removed, since
    // the correct styles will be added back in fixupNodeStyles().
    Node *node = m_fragment->firstChild();
    while (node) {
        Node *next = node->traverseNextNode();
        // This list of tags change the appearance of content
        // in ways we can add back on later with CSS, if necessary.
        //  FIXME: This list is incomplete
        if (node->hasTagName(bTag) || 
            node->hasTagName(bigTag) || 
            node->hasTagName(centerTag) || 
            node->hasTagName(fontTag) || 
            node->hasTagName(iTag) || 
            node->hasTagName(sTag) || 
            node->hasTagName(smallTag) || 
            node->hasTagName(strikeTag) || 
            node->hasTagName(subTag) || 
            node->hasTagName(supTag) || 
            node->hasTagName(ttTag) || 
            node->hasTagName(uTag) || 
            isStyleSpan(node)) {
            removeNodePreservingChildren(node);
        }
        // need to skip tab span because fixupNodeStyles() is not called
        // when replace is matching style
        else if (node->isHTMLElement() && !isTabSpanNode(node)) {
            HTMLElement *elem = static_cast<HTMLElement *>(node);
            CSSMutableStyleDeclaration *inlineStyleDecl = elem->inlineStyleDecl();
            if (inlineStyleDecl) {
                inlineStyleDecl->removeBlockProperties();
                inlineStyleDecl->removeInheritableProperties();
            }
        }
        node = next;
    }
}

RenderingInfo::RenderingInfo(PassRefPtr<CSSMutableStyleDeclaration> style)
    : m_style(style)
{
}

ReplaceSelectionCommand::ReplaceSelectionCommand(Document* document, DocumentFragment* fragment, bool selectReplacement, bool smartReplace, bool matchStyle, bool preventNesting, EditAction editAction) 
    : CompositeEditCommand(document),
      m_selectReplacement(selectReplacement), 
      m_smartReplace(smartReplace),
      m_matchStyle(matchStyle),
      m_documentFragment(fragment),
      m_preventNesting(preventNesting),
      m_editAction(editAction)
{
}

bool ReplaceSelectionCommand::shouldMergeStart(bool selectionStartWasStartOfParagraph, bool fragmentHasInterchangeNewlineAtStart)
{
    VisiblePosition startOfInsertedContent(Position(m_firstNodeInserted.get(), 0));
    VisiblePosition prev = startOfInsertedContent.previous(true);
    if (prev.isNull())
        return false;
        
    return !selectionStartWasStartOfParagraph && 
           !fragmentHasInterchangeNewlineAtStart &&
           isStartOfParagraph(startOfInsertedContent) && 
           !startOfInsertedContent.deepEquivalent().node()->hasTagName(brTag) &&
           shouldMerge(startOfInsertedContent, prev);
}

bool ReplaceSelectionCommand::shouldMergeEnd(bool selectionEndWasEndOfParagraph)
{
    VisiblePosition endOfInsertedContent(Position(m_lastNodeInserted.get(), maxDeepOffset(m_lastNodeInserted.get())));
    VisiblePosition next = endOfInsertedContent.next(true);
    if (next.isNull())
        return false;

    return !selectionEndWasEndOfParagraph &&
           isEndOfParagraph(endOfInsertedContent) && 
           !endOfInsertedContent.deepEquivalent().node()->hasTagName(brTag) &&
           shouldMerge(endOfInsertedContent, next);
}

bool ReplaceSelectionCommand::shouldMerge(const VisiblePosition& from, const VisiblePosition& to)
{
    if (from.isNull() || to.isNull())
        return false;
        
    Node* fromNode = from.deepEquivalent().node();
    Node* toNode = to.deepEquivalent().node();
    
    return nearestMailBlockquote(fromNode) == nearestMailBlockquote(toNode) &&
           !enclosingBlock(fromNode)->hasTagName(blockquoteTag) &&
           enclosingListChild(fromNode) == enclosingListChild(toNode) &&
           enclosingTableCell(fromNode) == enclosingTableCell(toNode) &&
           !(fromNode->renderer() && fromNode->renderer()->isTable()) &&
           !(toNode->renderer() && toNode->renderer()->isTable()) && 
           !fromNode->hasTagName(hrTag) && !toNode->hasTagName(hrTag);
}

void ReplaceSelectionCommand::doApply()
{
    Selection selection = endingSelection();
    ASSERT(selection.isCaretOrRange());
    ASSERT(selection.start().node());
    if (selection.isNone() || !selection.start().node())
        return;
    
    if (!selection.isContentRichlyEditable())
        m_matchStyle = true;
    
    Element* currentRoot = selection.rootEditableElement();
    ReplacementFragment fragment(document(), m_documentFragment.get(), m_matchStyle, selection);
    
    if (fragment.isEmpty())
        return;
    
    if (m_matchStyle)
        m_insertionStyle = styleAtPosition(selection.start());
    
    VisiblePosition visibleStart = selection.visibleStart();
    VisiblePosition visibleEnd = selection.visibleEnd();
    
    bool selectionEndWasEndOfParagraph = isEndOfParagraph(visibleEnd);
    bool selectionStartWasStartOfParagraph = isStartOfParagraph(visibleStart);
    
    if (selectionStartWasStartOfParagraph && selectionEndWasEndOfParagraph ||
        enclosingBlock(visibleStart.deepEquivalent().node()) == currentRoot)
        m_preventNesting = false;
    
    Position insertionPos = selection.start();
    
    if (selection.isRange()) {
        // When the end of the selection being pasted into is at the end of a paragraph, and that selection
        // spans multiple blocks, not merging may leave an empty line.
        // When the start of the selection being pasted into is at the start of a block, not merging 
        // will leave hanging block(s).
        bool mergeBlocksAfterDelete = isEndOfParagraph(visibleEnd) || isStartOfBlock(visibleStart);
        deleteSelection(false, mergeBlocksAfterDelete, true);
        visibleStart = endingSelection().visibleStart();
        if (fragment.hasInterchangeNewlineAtStart()) {
            if (isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart)) {
                if (!isEndOfDocument(visibleStart))
                    setEndingSelection(visibleStart.next());
            } else
                insertParagraphSeparator();
        }
        insertionPos = endingSelection().start();
    } 
    else {
        ASSERT(selection.isCaret());
        if (fragment.hasInterchangeNewlineAtStart()) {
            VisiblePosition next = visibleStart.next(true);
            if (isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart) && next.isNotNull())
                setEndingSelection(next);
            else 
                insertParagraphSeparator();
        }
        // We split the current paragraph in two to avoid nesting the blocks from the fragment inside the current block.
        // For example paste <div>foo</div><div>bar</div><div>baz</div> into <div>x^x</div>, where ^ is the caret.  
        // As long as the  div styles are the same, visually you'd expect: <div>xbar</div><div>bar</div><div>bazx</div>, 
        // not <div>xbar<div>bar</div><div>bazx</div></div>
        if (m_preventNesting && !isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart)) {
            insertParagraphSeparator();
            setEndingSelection(endingSelection().visibleStart().previous());
        }
        insertionPos = endingSelection().start();
    }
    
    // NOTE: This would be an incorrect usage of downstream() if downstream() were changed to mean the last position after 
    // p that maps to the same visible position as p (since in the case where a br is at the end of a block and collapsed 
    // away, there are positions after the br which map to the same visible position as [br, 0]).  
    Node* endBR = insertionPos.downstream().node()->hasTagName(brTag) ? insertionPos.downstream().node() : 0;
    
    Node* startBlock = enclosingBlock(insertionPos.node());
    
    // Adjust insertionPos to prevent nesting.
    if (m_preventNesting) {
        ASSERT(startBlock != currentRoot);
        VisiblePosition visibleInsertionPos(insertionPos);
        if (isEndOfBlock(visibleInsertionPos) && !(isStartOfBlock(visibleInsertionPos) && fragment.hasInterchangeNewlineAtEnd()))
            insertionPos = positionAfterNode(startBlock);
        else if (isStartOfBlock(visibleInsertionPos))
            insertionPos = positionBeforeNode(startBlock);
    }

    // Paste into run of tabs splits the tab span.
    insertionPos = positionOutsideTabSpan(insertionPos);
    
    // Paste at start or end of link goes outside of link.
    insertionPos = positionAvoidingSpecialElementBoundary(insertionPos);

    Frame *frame = document()->frame();
    
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    frame->clearTypingStyle();
    setTypingStyle(0);    
    
    // We're finished if there is nothing to add.
    if (!fragment.firstChild())
        return;
    
    // 1) Insert the content.
    // 2) Restore the styles of inserted nodes (since styles were removed during the test insertion).
    // 3) Merge the start of the added content with the content before the position being pasted into.
    // 4) Do one of the following: a) expand the last br if the fragment ends with one and it collapsed,
    // b) merge the last paragraph of the incoming fragment with the paragraph that contained the 
    // end of the selection that was pasted into, or c) handle an interchange newline at the end of the 
    // incoming fragment.
    // 5) Add spaces for smart replace.
    // 6) Select the replacement if requested, and match style if requested.
    
    VisiblePosition startOfInsertedContent, endOfInsertedContent;
    
    RefPtr<Node> refNode = fragment.firstChild();
    RefPtr<Node> node = refNode->nextSibling();
    
    fragment.removeNode(refNode);
    insertNodeAtAndUpdateNodesInserted(refNode.get(), insertionPos.node(), insertionPos.offset());
    
    while (node) {
        Node* next = node->nextSibling();
        fragment.removeNode(node);
        insertNodeAfterAndUpdateNodesInserted(node.get(), refNode.get());
        refNode = node;
        node = next;
    }
    
    endOfInsertedContent = VisiblePosition(Position(m_lastNodeInserted.get(), maxDeepOffset(m_lastNodeInserted.get())));
    startOfInsertedContent = VisiblePosition(Position(m_firstNodeInserted.get(), 0));
    
    // We inserted before the startBlock to prevent nesting, and the content before the startBlock wasn't in its own block and
    // didn't have a br after it, so the inserted content ended up in the same paragraph.
    if (insertionPos.node() == startBlock->parentNode() && (unsigned)insertionPos.offset() < startBlock->nodeIndex() && !isStartOfParagraph(startOfInsertedContent))
        insertNodeAt(createBreakElement(document()).get(), startOfInsertedContent.deepEquivalent().node(), startOfInsertedContent.deepEquivalent().offset());
    
    Position lastPositionToSelect;
    
    bool interchangeNewlineAtEnd = fragment.hasInterchangeNewlineAtEnd();

    if (shouldRemoveEndBR(endBR)) {
        if (interchangeNewlineAtEnd) {
            interchangeNewlineAtEnd = false;
            m_lastNodeInserted = endBR;
            lastPositionToSelect = VisiblePosition(Position(m_lastNodeInserted.get(), 0)).deepEquivalent();
        } else
            removeNodeAndPruneAncestors(endBR);
    }
    
    // Styles were removed during the test insertion.  Restore them.
    if (!m_matchStyle)
        fixupNodeStyles(fragment.nodes(), fragment.renderingInfo());
        
    if (shouldMergeStart(selectionStartWasStartOfParagraph, fragment.hasInterchangeNewlineAtStart())) {
        VisiblePosition destination = startOfInsertedContent.previous();
        VisiblePosition startOfParagraphToMove = startOfInsertedContent;

        moveParagraph(startOfParagraphToMove, endOfParagraph(startOfParagraphToMove), destination);
        m_firstNodeInserted = endingSelection().visibleStart().deepEquivalent().downstream().node();
        if (!m_lastNodeInserted->inDocument())
            m_lastNodeInserted = endingSelection().visibleEnd().deepEquivalent().downstream().node();
    
    }
            
    endOfInsertedContent = VisiblePosition(Position(m_lastNodeInserted.get(), maxDeepOffset(m_lastNodeInserted.get())));
    startOfInsertedContent = VisiblePosition(Position(m_firstNodeInserted.get(), 0));
    
    if (interchangeNewlineAtEnd) {
        VisiblePosition next = endOfInsertedContent.next(true);

        if (selectionEndWasEndOfParagraph || !isEndOfParagraph(endOfInsertedContent) || next.isNull()) {
            if (!isStartOfParagraph(endOfInsertedContent)) {
                setEndingSelection(endOfInsertedContent);
                insertParagraphSeparator();

                // Select up to the paragraph separator that was added.
                lastPositionToSelect = endingSelection().visibleStart().deepEquivalent();
                updateNodesInserted(lastPositionToSelect.node());
            }
        } else {
            // Select up to the beginning of the next paragraph.
            lastPositionToSelect = next.deepEquivalent().downstream();
        }

    } else if (m_lastNodeInserted->hasTagName(brTag)) {
        // We want to honor the last incoming line break, so, if it will collapse away because of quirks mode, 
        // add an extra one.
        // FIXME: This will expand a br inside a block: <div><br></div>
        // FIXME: Should we expand all incoming brs that collapse because of quirks mode?
        if (!document()->inStrictMode() && isEndOfBlock(endOfInsertedContent) && !isStartOfParagraph(endOfInsertedContent))
            insertNodeBeforeAndUpdateNodesInserted(createBreakElement(document()).get(), m_lastNodeInserted.get());
            
    } else if (shouldMergeEnd(selectionEndWasEndOfParagraph)) {
    
        // Merging two paragraphs will destroy the moved one's block styles.  Always move forward to preserve
        // the block style of the paragraph already in the document, unless the paragraph to move would include the
        // what was the start of the selection that was pasted into.
        bool mergeForward = !inSameParagraph(startOfInsertedContent, endOfInsertedContent) || isStartOfParagraph(startOfInsertedContent);
        
        VisiblePosition destination = mergeForward ? endOfInsertedContent.next() : endOfInsertedContent;
        VisiblePosition startOfParagraphToMove = mergeForward ? startOfParagraph(endOfInsertedContent) : endOfInsertedContent.next();

        moveParagraph(startOfParagraphToMove, endOfParagraph(startOfParagraphToMove), destination);
        // Merging forward will remove m_lastNodeInserted from the document.
        // FIXME: Maintain positions for the start and end of inserted content instead of keeping nodes.  The nodes are
        // only ever used to create positions where inserted content starts/ends.
        if (mergeForward) {
            m_lastNodeInserted = destination.previous().deepEquivalent().node();
            if (!m_firstNodeInserted->inDocument())
                m_firstNodeInserted = endingSelection().visibleStart().deepEquivalent().node();
        }
    }
    
    endOfInsertedContent = VisiblePosition(Position(m_lastNodeInserted.get(), maxDeepOffset(m_lastNodeInserted.get())));
    startOfInsertedContent = VisiblePosition(Position(m_firstNodeInserted.get(), 0));    
    
    // Add spaces for smart replace.
    if (m_smartReplace) {
        bool needsTrailingSpace = !isEndOfParagraph(endOfInsertedContent) &&
                                  !frame->isCharacterSmartReplaceExempt(endOfInsertedContent.characterAfter(), false);
        if (needsTrailingSpace) {
            RenderObject* renderer = m_lastNodeInserted->renderer();
            bool collapseWhiteSpace = !renderer || renderer->style()->collapseWhiteSpace();
            if (m_lastNodeInserted->isTextNode()) {
                Text* text = static_cast<Text*>(m_lastNodeInserted.get());
                insertTextIntoNode(text, text->length(), collapseWhiteSpace ? nonBreakingSpaceString() : " ");
            } else {
                RefPtr<Node> node = document()->createEditingTextNode(collapseWhiteSpace ? nonBreakingSpaceString() : " ");
                insertNodeAfterAndUpdateNodesInserted(node.get(), m_lastNodeInserted.get());
            }
        }
    
        bool needsLeadingSpace = !isStartOfParagraph(startOfInsertedContent) &&
                                 !frame->isCharacterSmartReplaceExempt(startOfInsertedContent.previous().characterAfter(), true);
        if (needsLeadingSpace) {
            RenderObject* renderer = m_lastNodeInserted->renderer();
            bool collapseWhiteSpace = !renderer || renderer->style()->collapseWhiteSpace();
            if (m_firstNodeInserted->isTextNode()) {
                Text* text = static_cast<Text*>(m_firstNodeInserted.get());
                insertTextIntoNode(text, 0, collapseWhiteSpace ? nonBreakingSpaceString() : " ");
            } else {
                RefPtr<Node> node = document()->createEditingTextNode(collapseWhiteSpace ? nonBreakingSpaceString() : " ");
                // Don't updateNodesInserted.  Doing so would set m_lastNodeInserted to be the node containing the 
                // leading space, but m_lastNodeInserted is supposed to mark the end of pasted content.
                insertNodeBefore(node.get(), m_firstNodeInserted.get());
                // FIXME: Use positions to track the start/end of inserted content.
                m_firstNodeInserted = node;
            }
        }
    }
    
    completeHTMLReplacement(lastPositionToSelect);
}

bool ReplaceSelectionCommand::shouldRemoveEndBR(Node* endBR)
{
    if (!endBR || !endBR->inDocument())
        return false;
        
    VisiblePosition visiblePos(Position(endBR, 0));
    
    return
        // The br is collapsed away and so is unnecessary.
        !document()->inStrictMode() && isEndOfBlock(visiblePos) && !isStartOfParagraph(visiblePos) ||
        // A br that was originally holding a line open should be displaced by inserted content or turned into a line break.
        // A br that was originally acting as a line break should still be acting as a line break, not as a placeholder.
        isStartOfParagraph(visiblePos) && isEndOfParagraph(visiblePos) && !m_lastNodeInserted->hasTagName(brTag);
}

void ReplaceSelectionCommand::completeHTMLReplacement(const Position &lastPositionToSelect)
{
    Position start;
    Position end;

    if (m_firstNodeInserted && m_firstNodeInserted->inDocument() && m_lastNodeInserted && m_lastNodeInserted->inDocument()) {
        
        Node* lastLeaf = m_lastNodeInserted->lastDescendant();
        Node* firstLeaf = m_firstNodeInserted->firstDescendant();
        
        start = Position(firstLeaf, 0);
        end = Position(lastLeaf, maxDeepOffset(lastLeaf));
        
        // FIXME: Should we treat all spaces in incoming content as having been rendered?
        rebalanceWhitespaceAt(start);
        rebalanceWhitespaceAt(end);

        if (m_matchStyle) {
            assert(m_insertionStyle);
            applyStyle(m_insertionStyle.get(), start, end);
        }    
        
        if (lastPositionToSelect.isNotNull())
            end = lastPositionToSelect;
    } else if (lastPositionToSelect.isNotNull())
        start = end = lastPositionToSelect;
    else
        return;
    
    if (m_selectReplacement)
        setEndingSelection(Selection(start, end, SEL_DEFAULT_AFFINITY));
    else
        setEndingSelection(end, SEL_DEFAULT_AFFINITY);
}

EditAction ReplaceSelectionCommand::editingAction() const
{
    return m_editAction;
}

void ReplaceSelectionCommand::insertNodeAfterAndUpdateNodesInserted(Node *insertChild, Node *refChild)
{
    insertNodeAfter(insertChild, refChild);
    updateNodesInserted(insertChild);
}

void ReplaceSelectionCommand::insertNodeAtAndUpdateNodesInserted(Node *insertChild, Node *refChild, int offset)
{
    insertNodeAt(insertChild, refChild, offset);
    updateNodesInserted(insertChild);
}

void ReplaceSelectionCommand::insertNodeBeforeAndUpdateNodesInserted(Node *insertChild, Node *refChild)
{
    insertNodeBefore(insertChild, refChild);
    updateNodesInserted(insertChild);
}

void ReplaceSelectionCommand::updateNodesInserted(Node *node)
{
    if (!node)
        return;

    if (!m_firstNodeInserted)
        m_firstNodeInserted = node;
    
    if (node == m_lastNodeInserted)
        return;
    
    m_lastNodeInserted = node->lastDescendant();
}

} // namespace WebCore
