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

ReplacementFragment::ReplacementFragment(Document *document, DocumentFragment *fragment, bool matchStyle, Element* editableRoot)
    : m_document(document),
      m_fragment(fragment),
      m_matchStyle(matchStyle), 
      m_hasInterchangeNewlineAtStart(false), 
      m_hasInterchangeNewlineAtEnd(false), 
      m_hasMoreThanOneBlock(false)
{
    if (!m_document)
        return;

    if (!m_fragment) {
        m_type = EmptyFragment;
        return;
    }

    Node* firstChild = m_fragment->firstChild();
    Node* lastChild = m_fragment->lastChild();

    if (!firstChild) {
        m_type = EmptyFragment;
        return;
    }
    
    m_type = firstChild == lastChild && firstChild->isTextNode() ? SingleTextNodeFragment : TreeFragment;
    
    ASSERT(editableRoot);
    if (!editableRoot)
        return;
            
    RefPtr<Node> holder = insertFragmentForTestRendering();
    
    RefPtr<Range> range = Selection::selectionFromContentsOfNode(holder.get()).toRange();
    String text = plainText(range.get());
    // Give the root a chance to change the text.
    RefPtr<BeforeTextInsertedEvent> evt = new BeforeTextInsertedEvent(text);
    ExceptionCode ec = 0;
    editableRoot->dispatchEvent(evt, ec, true);
    ASSERT(ec == 0);
    if (text != evt->text() || !editableRoot->isContentRichlyEditable()) {
        m_fragment = createFragmentFromText(document, evt->text().deprecatedString());
        firstChild = m_fragment->firstChild();
        lastChild = m_fragment->firstChild();
        
        removeNode(holder);
        holder = insertFragmentForTestRendering();
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
    m_hasMoreThanOneBlock = renderedBlocks(holder.get()) > 1;
    restoreTestRenderingNodesToFragment(holder.get());
    removeNode(holder);
    removeStyleNodes();
}

ReplacementFragment::~ReplacementFragment()
{
}

Node *ReplacementFragment::firstChild() const 
{ 
    return m_fragment->firstChild(); 
}

Node *ReplacementFragment::lastChild() const 
{ 
    return m_fragment->lastChild(); 
}

static bool isMailPasteAsQuotationNode(const Node *node)
{
    return node && static_cast<const Element *>(node)->getAttribute("class") == ApplePasteAsQuotation;
}

Node *ReplacementFragment::mergeStartNode() const
{
    Node *node = m_fragment->firstChild();
    while (node && isBlockFlow(node) && !isMailPasteAsQuotationNode(node))
        node = node->traverseNextNode();
    return node;
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

Node *ReplacementFragment::enclosingBlock(Node *node) const
{
    while (node && !isBlockFlow(node))
        node = node->parentNode();    
    return node ? node : m_fragment.get();
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

PassRefPtr<Node> ReplacementFragment::insertFragmentForTestRendering()
{
    Node *body = m_document->body();
    if (!body)
        return 0;

    RefPtr<Node> holder = createDefaultParagraphElement(m_document.get());
    
    ExceptionCode ec = 0;
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

bool ReplacementFragment::isBlockFlow(Node* node) const
{
    RefPtr<RenderingInfo> info = m_renderingInfo.get(node);
    ASSERT(info);
    if (!info)
        return false;
    
    return info->isBlockFlow();
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
            m_renderingInfo.add(node, new RenderingInfo(0, node->isBlockFlow()));
    } else {
        for (Node *node = holder->firstChild(); node; node = node->traverseNextNode(holder)) {
            m_renderingInfo.add(node, new RenderingInfo(styleForNode(node), node->isBlockFlow()));
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

// FIXME: This counts two blocks for <span><div>foo</div></span>.  Get rid of uses of hasMoreThanOneBlock so that we can get rid of this function.
int ReplacementFragment::renderedBlocks(Node *holder)
{
    int count = 0;
    Node *prev = 0;
    for (Node *node = holder->firstChild(); node; node = node->traverseNextNode(holder)) {
        if (node->isBlockFlow()) {
            if (!prev) {
                count++;
                prev = node;
            }
        } else {
            Node *block = node->enclosingBlockFlowElement();
            if (block != prev) {
                count++;
                prev = block;
            }
        }
    }
    
    return count;
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

RenderingInfo::RenderingInfo(PassRefPtr<CSSMutableStyleDeclaration> style, bool isBlockFlow = false)
    : m_style(style), m_isBlockFlow(isBlockFlow)
{
}

ReplaceSelectionCommand::ReplaceSelectionCommand(Document *document, DocumentFragment *fragment, bool selectReplacement, bool smartReplace, bool matchStyle, bool forceMergeStart) 
    : CompositeEditCommand(document),
      m_selectReplacement(selectReplacement), 
      m_smartReplace(smartReplace),
      m_matchStyle(matchStyle),
      m_documentFragment(fragment),
      m_forceMergeStart(forceMergeStart)
{
}

ReplaceSelectionCommand::~ReplaceSelectionCommand()
{
}

// FIXME: This will soon operate on the fragment after it's been inserted so that it can check renderers and create visible positions.
bool ReplaceSelectionCommand::shouldMergeStart(const ReplacementFragment& incomingFragment, const Selection& destinationSelection)
{
    if (m_forceMergeStart)
        return true;
        
    VisiblePosition visibleStart = destinationSelection.visibleStart();
    Node* startBlock = destinationSelection.start().node()->enclosingBlockFlowElement();

    // <rdar://problem/4013642> Copied quoted word does not paste as a quote if pasted at the start of a line    
    if (isStartOfParagraph(visibleStart) && isMailBlockquote(incomingFragment.firstChild()))
        return false;
    
    // Don't pull content out of a list item.
    // FIXMEs: Don't pull content out of a table cell either.
    if (enclosingList(incomingFragment.mergeStartNode()))
        return false;
    
    // Merge if this is an empty editable subtree, to prevent an extra level of block nesting.
    if (startBlock == startBlock->rootEditableElement() && isStartOfBlock(visibleStart) && isEndOfBlock(visibleStart))
        return true;
    
    if (!incomingFragment.hasInterchangeNewlineAtStart() && 
        (!isStartOfParagraph(visibleStart) || !incomingFragment.hasInterchangeNewlineAtEnd() && !incomingFragment.hasMoreThanOneBlock()))
       return true;
    
    return false;
}

bool ReplaceSelectionCommand::shouldMergeEnd(const VisiblePosition& endOfInsertedContent, bool fragmentHadInterchangeNewlineAtEnd, bool selectionEndWasEndOfParagraph)
{
    Node* endNode = endOfInsertedContent.deepEquivalent().node();
    Node* nextNode = endOfInsertedContent.next().deepEquivalent().node();
    // FIXME: Unify the naming scheme for these enclosing element getters.
    return !selectionEndWasEndOfParagraph &&
           !fragmentHadInterchangeNewlineAtEnd && 
           isEndOfParagraph(endOfInsertedContent) && 
           nearestMailBlockquote(endNode) == nearestMailBlockquote(nextNode) &&
           enclosingListChild(endNode) == enclosingListChild(nextNode) &&
           enclosingTableCell(endNode) == enclosingTableCell(nextNode) &&
           !endNode->hasTagName(hrTag);
}

void ReplaceSelectionCommand::doApply()
{
    // collect information about the current selection, prior to deleting the selection
    Selection selection = endingSelection();
    ASSERT(selection.isCaretOrRange());
    ASSERT(selection.start().node());
    if (selection.isNone() || !selection.start().node())
        return;
    
    if (!selection.isContentRichlyEditable())
        m_matchStyle = true;
    
    Element* currentRoot = selection.rootEditableElement();
    ReplacementFragment fragment(document(), m_documentFragment.get(), m_matchStyle, currentRoot);
    
    if (fragment.type() == EmptyFragment)
        return;
    
    if (m_matchStyle)
        m_insertionStyle = styleAtPosition(selection.start());
    
    VisiblePosition visibleStart(selection.start(), selection.affinity());
    VisiblePosition visibleEnd(selection.end(), selection.affinity());
    bool startAtStartOfBlock = isStartOfBlock(visibleStart);
    Node* startBlock = selection.start().node()->enclosingBlockFlowElement();

    // Whether the first paragraph of the incoming fragment should be merged with content from visibleStart to startOfParagraph(visibleStart).
    bool mergeStart = shouldMergeStart(fragment, selection);
    
    bool endWasEndOfParagraph = isEndOfParagraph(visibleEnd);

    Position startPos = selection.start();
    
    // delete the current range selection, or insert paragraph for caret selection, as needed
    if (selection.isRange()) {
        // When the end of the selection to delete is at the end of a paragraph, and the selection
        // to delete spans multiple blocks, not merging will leave an empty line containing the
        // end of the selection to delete.
        bool mergeBlocksAfterDelete = !fragment.hasInterchangeNewlineAtEnd() && !fragment.hasInterchangeNewlineAtStart() && isEndOfParagraph(visibleEnd);
        deleteSelection(false, mergeBlocksAfterDelete);
        updateLayout();
        visibleStart = endingSelection().visibleStart();
        if (fragment.hasInterchangeNewlineAtStart()) {
            if (isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart)) {
                if (!isEndOfDocument(visibleStart))
                    setEndingSelection(visibleStart.next());
            } else
                insertParagraphSeparator();
        }
        startPos = endingSelection().start();
    } 
    else {
        ASSERT(selection.isCaret());
        if (fragment.hasInterchangeNewlineAtStart()) {
            if (isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart)) {
                if (!isEndOfDocument(visibleStart))
                    setEndingSelection(visibleStart.next());
            } else 
                insertParagraphSeparator();
        }
        // We split the current paragraph in two to avoid nesting the blocks from the fragment inside the current block.
        // For example paste <div>foo</div><div>bar</div><div>baz</div> into <div>x^x</div>, where ^ is the caret.  
        // As long as the  div styles are the same, visually you'd expect: <div>xbar</div><div>bar</div><div>bazx</div>, 
        // not <div>xbar<div>bar</div><div>bazx</div></div>
        // FIXME: If this code is really about preventing block nesting, then the check should be !isEndOfBlock(visibleStart) and we 
        // should split the block in two, instead of inserting a paragraph separator. In the meantime, it appears that code below 
        // depends on this split happening when the paste position is not the start or end of a paragraph.
        if (!isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart)) {
            insertParagraphSeparator();
            setEndingSelection(VisiblePosition(endingSelection().start(), VP_DEFAULT_AFFINITY).previous());
        }
        startPos = endingSelection().start();
    }
    
    // NOTE: This would be an incorrect usage of downstream() if downstream() were changed to mean the last position after 
    // p that maps to the same visible position as p (since in the case where a br is at the end of a block and collapsed 
    // away, there are positions after the br which map to the same visible position as [br, 0]).  
    Node* endBR = startPos.downstream().node()->hasTagName(brTag) ? startPos.downstream().node() : 0;
    
    if (startAtStartOfBlock && startBlock->inDocument())
        startPos = Position(startBlock, 0);

    // paste into run of tabs splits the tab span
    startPos = positionOutsideTabSpan(startPos);
    
    // paste at start or end of link goes outside of link
    startPos = positionAvoidingSpecialElementBoundary(startPos);

    Frame *frame = document()->frame();
    
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    frame->clearTypingStyle();
    setTypingStyle(0);    
    
    // done if there is nothing to add
    if (!fragment.firstChild())
        return;
    
    // 1) Add the first paragraph of the incoming fragment, merging it with the paragraph that contained the 
    // start of the selection that was pasted into.
    // 2) Add everything else.
    // 3) Restore the styles of inserted nodes (since styles were removed during the test insertion).
    // 4) Do one of the following: a) if there was a br at the end of the fragment, expand it if it collapsed 
    // because of quirks mode, b) merge the last paragraph of the incoming fragment with the paragraph that contained the 
    // end of the selection that was pasted into, or c) handle an interchange newline at the end of the 
    // incoming fragment.
    // 5) Add spaces for smart replace.
    // 6) Select the replacement if requested, and match style if requested.
        
    // initially, we say the insertion point is the start of selection
    updateLayout();
    Position insertionPos = startPos;

    // Add the first paragraph.
    if (mergeStart) {
        RefPtr<Node> refNode = fragment.mergeStartNode();
        if (refNode) {
            Node *parent = refNode->parentNode();
            RefPtr<Node> node = refNode->nextSibling();
            fragment.removeNode(refNode);
            insertNodeAtAndUpdateNodesInserted(refNode.get(), startPos.node(), startPos.offset());
            while (node && !fragment.isBlockFlow(node.get())) {
                Node *next = node->nextSibling();
                fragment.removeNode(node);
                insertNodeAfterAndUpdateNodesInserted(node.get(), refNode.get());
                refNode = node;
                node = next;
            }

            // remove any ancestors we emptied, except the root itself which cannot be removed
            while (parent && parent->parentNode() && parent->childNodeCount() == 0) {
                Node *nextParent = parent->parentNode();
                fragment.removeNode(parent);
                parent = nextParent;
            }
        }
        
        // update insertion point to be at the end of the last block inserted
        if (m_lastNodeInserted) {
            updateLayout();
            insertionPos = Position(m_lastNodeInserted.get(), m_lastNodeInserted->caretMaxOffset());
        }
    }
    
    // Add everything else.
    if (fragment.firstChild()) {
        RefPtr<Node> refNode = fragment.firstChild();
        RefPtr<Node> node = refNode ? refNode->nextSibling() : 0;
        Node* insertionBlock = insertionPos.node()->enclosingBlockFlowElement();
        Node* insertionRoot = insertionPos.node()->rootEditableElement();
        bool insertionBlockIsRoot = insertionBlock == insertionRoot;
        VisiblePosition visibleInsertionPos(insertionPos);
        fragment.removeNode(refNode);
        // FIXME: The first two cases need to be rethought.  They're about preventing the nesting of 
        // incoming blocks in the block where the paste is being performed.  But, avoiding nesting doesn't 
        // always produce the desired visual result, and the decisions are based on isBlockFlow, which 
        // we're getting rid of.
        if (!insertionBlockIsRoot && fragment.isBlockFlow(refNode.get()) && isStartOfBlock(visibleInsertionPos) && !m_lastNodeInserted)
            insertNodeBeforeAndUpdateNodesInserted(refNode.get(), insertionBlock);
        else if (!insertionBlockIsRoot && fragment.isBlockFlow(refNode.get()) && isEndOfBlock(visibleInsertionPos))
            insertNodeAfterAndUpdateNodesInserted(refNode.get(), insertionBlock);
        else if (m_lastNodeInserted && !fragment.isBlockFlow(refNode.get())) {
            // A non-null m_lastNodeInserted means we've done merging above.  That means everything in the first paragraph 
            // of the fragment has been merged with everything up to the start of the paragraph where the paste was performed.  
            // refNode is the first node in the second paragraph of the fragment to paste.  Since it's inline, we can't 
            // insert it at insertionPos, because it wouldn't end up in its own paragraph.

            // FIXME: Code above does paragraph splitting and so we are assured that visibleInsertionPos is the end of
            // a paragraph, but the above splitting should eventually be only about preventing nesting.
            ASSERT(isEndOfParagraph(visibleInsertionPos));
            VisiblePosition next = visibleInsertionPos.next();
            if (next.isNull() || next.rootEditableElement() != insertionRoot) {
                setEndingSelection(visibleInsertionPos);
                insertParagraphSeparator();
                next = visibleInsertionPos.next();
            }
            
            Position pos = next.deepEquivalent().downstream();
            insertNodeAtAndUpdateNodesInserted(refNode.get(), pos.node(), pos.offset());
        } else {
            insertNodeAtAndUpdateNodesInserted(refNode.get(), insertionPos.node(), insertionPos.offset());
        }
        
        while (node) {
            Node* next = node->nextSibling();
            fragment.removeNode(node);
            insertNodeAfterAndUpdateNodesInserted(node.get(), refNode.get());
            refNode = node;
            node = next;
        }
        updateLayout();
        insertionPos = Position(m_lastNodeInserted.get(), m_lastNodeInserted->caretMaxOffset());
    }
    
    removeEndBRIfNeeded(endBR);
    
    // Styles were removed during the test insertion.  Restore them.
    if (!m_matchStyle)
        fixupNodeStyles(fragment.nodes(), fragment.renderingInfo());
        
    VisiblePosition endOfInsertedContent(Position(m_lastNodeInserted.get(), maxDeepOffset(m_lastNodeInserted.get())));
    VisiblePosition startOfInsertedContent(Position(m_firstNodeInserted.get(), 0));    
    
    Position lastPositionToSelect;
    
    if (fragment.hasInterchangeNewlineAtEnd()) {
        VisiblePosition pos(insertionPos);
        VisiblePosition next = pos.next();
            
        if (endWasEndOfParagraph || !isEndOfParagraph(pos) || next.isNull() || next.rootEditableElement() != currentRoot) {
            setEndingSelection(insertionPos, DOWNSTREAM);
            insertParagraphSeparator();
            next = pos.next();

            // Select up to the paragraph separator that was added.
            lastPositionToSelect = next.deepEquivalent().downstream();
            updateNodesInserted(lastPositionToSelect.node());
        } else {
            // Select up to the beginning of the next paragraph.
            VisiblePosition next = pos.next();
            lastPositionToSelect = next.deepEquivalent().downstream();
        }
        
    } else if (m_lastNodeInserted->hasTagName(brTag)) {
        // We want to honor the last incoming line break, so, if it will collapse away because of quirks mode, 
        // add an extra one.
        // FIXME: This will expand a br inside a block: <div><br></div>
        // FIXME: Should we expand all incoming brs that collapse because of quirks mode?
        if (!document()->inStrictMode() && isEndOfBlock(VisiblePosition(Position(m_lastNodeInserted.get(), 0))))
            insertNodeBeforeAndUpdateNodesInserted(createBreakElement(document()).get(), m_lastNodeInserted.get());
            
    } else if (shouldMergeEnd(endOfInsertedContent, fragment.hasInterchangeNewlineAtEnd(), endWasEndOfParagraph)) {
        // Make sure that content after the end of the selection being pasted into is in the same paragraph as the 
        // last bit of content that was inserted.
        
        // Merging two paragraphs will destroy the moved one's block styles.  Always move forward to preserve
        // the block style of the paragraph already in the document, unless the paragraph to move would include the
        // what was the start of the selection that was pasted into.
        bool mergeForward = !inSameParagraph(startOfInsertedContent, endOfInsertedContent);
        
        VisiblePosition destination = mergeForward ? endOfInsertedContent.next() : endOfInsertedContent;
        VisiblePosition startOfParagraphToMove = mergeForward ? startOfParagraph(endOfInsertedContent) : endOfInsertedContent.next();

        moveParagraph(startOfParagraphToMove, endOfParagraph(startOfParagraphToMove), destination);
        // Merging forward will remove m_lastNodeInserted from the document.
        // FIXME: Maintain positions for the start and end of inserted content instead of keeping nodes.  The nodes are
        // only ever used to create positions where inserted content starts/ends.
        if (mergeForward)
            m_lastNodeInserted = destination.previous().deepEquivalent().node();
    }
    
    endOfInsertedContent = VisiblePosition(Position(m_lastNodeInserted.get(), maxDeepOffset(m_lastNodeInserted.get())));
    startOfInsertedContent = VisiblePosition(Position(m_firstNodeInserted.get(), 0));    
    
    // Add spaces for smart replace.
    if (m_smartReplace) {
        bool needsTrailingSpace = !isEndOfParagraph(endOfInsertedContent) &&
                                  !frame->isCharacterSmartReplaceExempt(endOfInsertedContent.characterAfter(), false);
        if (needsTrailingSpace) {
            if (m_lastNodeInserted->isTextNode()) {
                Text *text = static_cast<Text *>(m_lastNodeInserted.get());
                insertTextIntoNode(text, text->length(), nonBreakingSpaceString());
                insertionPos = Position(text, text->length());
            } else {
                RefPtr<Node> node = document()->createEditingTextNode(nonBreakingSpaceString());
                insertNodeAfterAndUpdateNodesInserted(node.get(), m_lastNodeInserted.get());
                insertionPos = Position(node.get(), 1);
            }
        }
    
        bool needsLeadingSpace = !isStartOfParagraph(startOfInsertedContent) &&
                                 !frame->isCharacterSmartReplaceExempt(startOfInsertedContent.previous().characterAfter(), true);
        if (needsLeadingSpace) {
            if (m_firstNodeInserted->isTextNode()) {
                Text *text = static_cast<Text *>(m_firstNodeInserted.get());
                insertTextIntoNode(text, 0, nonBreakingSpaceString());
            } else {
                RefPtr<Node> node = document()->createEditingTextNode(nonBreakingSpaceString());
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

void ReplaceSelectionCommand::removeEndBRIfNeeded(Node* endBR)
{
    if (!endBR || !endBR->inDocument())
        return;
        
    VisiblePosition visiblePos(Position(endBR, 0));
    
    if (// The br is collapsed away and so is unnecessary.
        !document()->inStrictMode() && isEndOfBlock(visiblePos) && !isStartOfParagraph(visiblePos) ||
        // A br that was originally holding a line open should be displaced by inserted content.
        // A br that was originally acting as a line break should still be acting as a line break, not as a placeholder.
        isStartOfParagraph(visiblePos) && isEndOfParagraph(visiblePos))
        removeNodeAndPruneAncestors(endBR);
}

void ReplaceSelectionCommand::completeHTMLReplacement(const Position &lastPositionToSelect)
{
    Position start;
    Position end;

    if (m_firstNodeInserted && m_firstNodeInserted->inDocument() && m_lastNodeInserted && m_lastNodeInserted->inDocument()) {
        // Find the last leaf.
        Node *lastLeaf = m_lastNodeInserted.get();
        while (1) {
            Node *nextChild = lastLeaf->lastChild();
            if (!nextChild)
                break;
            lastLeaf = nextChild;
        }
    
        // Find the first leaf.
        Node *firstLeaf = m_firstNodeInserted.get();
        while (1) {
            Node *nextChild = firstLeaf->firstChild();
            if (!nextChild)
                break;
            firstLeaf = nextChild;
        }
        
        // Call updateLayout so caretMinOffset and caretMaxOffset return correct values.
        updateLayout();
        start = Position(firstLeaf, firstLeaf->caretMinOffset());
        end = Position(lastLeaf, lastLeaf->caretMaxOffset());

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
    
    rebalanceWhitespace();
}

EditAction ReplaceSelectionCommand::editingAction() const
{
    return EditActionPaste;
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

    m_lastTopNodeInserted = node;
    if (!m_firstNodeInserted)
        m_firstNodeInserted = node;
    
    if (node == m_lastNodeInserted)
        return;
    
    m_lastNodeInserted = node->lastDescendant();
}

} // namespace WebCore
