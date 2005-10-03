/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
#include "replace_selection_command.h"

#include "visible_position.h"
#include "htmlnames.h"
#include "xml/dom_docimpl.h"
#include "html_interchange.h"
#include "xml/dom_textimpl.h"
#include "html/html_elementimpl.h"
#include "htmlediting.h"
#include "css/css_valueimpl.h"
#include "visible_units.h"
#include "xml/dom_position.h"
#include "khtml_part.h"
#include "xml/dom2_rangeimpl.h"
#include "rendering/render_object.h"
#include "css/css_computedstyle.h"
#include "css/cssproperties.h"

#if APPLE_CHANGES
#include <kxmlcore/Assertions.h>
#else
#define ASSERT(assertion) assert(assertion)
#endif

using namespace DOM::HTMLNames;

using DOM::CSSComputedStyleDeclarationImpl;
using DOM::DocumentImpl;
using DOM::NodeImpl;
using DOM::DocumentFragmentImpl;
using DOM::ElementImpl;
using DOM::TextImpl;
using DOM::DOMString;
using DOM::HTMLElementImpl;
using DOM::CSSMutableStyleDeclarationImpl;
using DOM::Position;

namespace khtml {

ReplacementFragment::ReplacementFragment(DocumentImpl *document, DocumentFragmentImpl *fragment, bool matchStyle)
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

    m_document->ref();
    m_fragment->ref();

    NodeImpl *firstChild = m_fragment->firstChild();
    NodeImpl *lastChild = m_fragment->lastChild();

    if (!firstChild) {
        m_type = EmptyFragment;
        return;
    }

    if (firstChild == lastChild && firstChild->isTextNode()) {
        m_type = SingleTextNodeFragment;
        return;
    }
    
    m_type = TreeFragment;

    NodeImpl *node = m_fragment->firstChild();
    NodeImpl *newlineAtStartNode = 0;
    NodeImpl *newlineAtEndNode = 0;
    while (node) {
        NodeImpl *next = node->traverseNextNode();
        if (isInterchangeNewlineNode(node)) {
            if (next || node == m_fragment->firstChild()) {
                m_hasInterchangeNewlineAtStart = true;
                newlineAtStartNode = node;
            }
            else {
                m_hasInterchangeNewlineAtEnd = true;
                newlineAtEndNode = node;
            }
        }
        else if (isInterchangeConvertedSpaceSpan(node)) {
            NodeImpl *n = 0;
            while ((n = node->firstChild())) {
                n->ref();
                removeNode(n);
                insertNodeBefore(n, node);
                n->deref();
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
    
    NodeImpl *holder = insertFragmentForTestRendering();
    if (holder)
        holder->ref();
    if (!m_matchStyle) {
        computeStylesUsingTestRendering(holder);
    }
    removeUnrenderedNodesUsingTestRendering(holder);
    m_hasMoreThanOneBlock = countRenderedBlocks(holder) > 1;
    restoreTestRenderingNodesToFragment(holder);
    removeNode(holder);
    holder->deref();
    removeStyleNodes();
}

ReplacementFragment::~ReplacementFragment()
{
    if (m_document)
        m_document->deref();
    if (m_fragment)
        m_fragment->deref();
}

NodeImpl *ReplacementFragment::firstChild() const 
{ 
    return m_fragment->firstChild(); 
}

NodeImpl *ReplacementFragment::lastChild() const 
{ 
    return  m_fragment->lastChild(); 
}

static bool isProbablyBlock(const NodeImpl *node)
{
    if (!node)
        return false;
    
    // FIXME: This function seems really broken to me.  It isn't even including all the block-level elements.
    return (node->hasTagName(blockquoteTag) || node->hasTagName(ddTag) || node->hasTagName(divTag) ||
            node->hasTagName(dlTag) || node->hasTagName(dtTag) || node->hasTagName(h1Tag) ||
            node->hasTagName(h2Tag) || node->hasTagName(h3Tag) || node->hasTagName(h4Tag) ||
            node->hasTagName(h5Tag) || node->hasTagName(h6Tag) || node->hasTagName(hrTag) ||
            node->hasTagName(liTag) || node->hasTagName(olTag) || node->hasTagName(pTag) ||
            node->hasTagName(preTag) || node->hasTagName(tdTag) || node->hasTagName(thTag) ||
            node->hasTagName(ulTag));
}

static bool isMailPasteAsQuotationNode(const NodeImpl *node)
{
    if (!node)
        return false;
        
    return static_cast<const ElementImpl *>(node)->getAttribute("class") == ApplePasteAsQuotation;
}

NodeImpl *ReplacementFragment::mergeStartNode() const
{
    NodeImpl *node = m_fragment->firstChild();
    while (node && isProbablyBlock(node) && !isMailPasteAsQuotationNode(node))
        node = node->traverseNextNode();
    return node;
}

static bool isProbablyTableStructureNode(const NodeImpl *node)
{
    if (!node)
        return false;
    
    return (node->hasTagName(tableTag) || node->hasTagName(tbodyTag) || node->hasTagName(tdTag) ||
            node->hasTagName(tfootTag) || node->hasTagName(theadTag) || node->hasTagName(trTag));
}

void ReplacementFragment::pruneEmptyNodes()
{
    bool run = true;
    while (run) {
        run = false;
        NodeImpl *node = m_fragment->firstChild();
        while (node) {
            if ((node->isTextNode() && static_cast<TextImpl *>(node)->length() == 0) ||
                (isProbablyBlock(node) && !isProbablyTableStructureNode(node) && node->childNodeCount() == 0)) {
                NodeImpl *next = node->traverseNextSibling();
                removeNode(node);
                node = next;
                run = true;
            }
            else {
                node = node->traverseNextNode();
            }
         }
    }
}

bool ReplacementFragment::isInterchangeNewlineNode(const NodeImpl *node)
{
    static DOMString interchangeNewlineClassString(AppleInterchangeNewline);
    return node && node->hasTagName(brTag) && 
           static_cast<const ElementImpl *>(node)->getAttribute(classAttr) == interchangeNewlineClassString;
}

bool ReplacementFragment::isInterchangeConvertedSpaceSpan(const NodeImpl *node)
{
    static DOMString convertedSpaceSpanClassString(AppleConvertedSpace);
    return node->isHTMLElement() && 
           static_cast<const HTMLElementImpl *>(node)->getAttribute(classAttr) == convertedSpaceSpanClassString;
}

NodeImpl *ReplacementFragment::enclosingBlock(NodeImpl *node) const
{
    while (node && !isProbablyBlock(node))
        node = node->parentNode();    
    return node ? node : m_fragment;
}

void ReplacementFragment::removeNodePreservingChildren(NodeImpl *node)
{
    if (!node)
        return;

    while (NodeImpl *n = node->firstChild()) {
        n->ref();
        removeNode(n);
        insertNodeBefore(n, node);
        n->deref();
    }
    removeNode(node);
}

void ReplacementFragment::removeNode(NodeImpl *node)
{
    if (!node)
        return;
        
    NodeImpl *parent = node->parentNode();
    if (!parent)
        return;
        
    int exceptionCode = 0;
    node->ref();
    parent->removeChild(node, exceptionCode);
    node->deref();
    ASSERT(exceptionCode == 0);
}

void ReplacementFragment::insertNodeBefore(NodeImpl *node, NodeImpl *refNode)
{
    if (!node || !refNode)
        return;
        
    NodeImpl *parent = refNode->parentNode();
    if (!parent)
        return;
        
    int exceptionCode = 0;
    parent->insertBefore(node, refNode, exceptionCode);
    ASSERT(exceptionCode == 0);
}

NodeImpl *ReplacementFragment::insertFragmentForTestRendering()
{
    NodeImpl *body = m_document->body();
    if (!body)
        return 0;

    ElementImpl *holder = createDefaultParagraphElement(m_document);
    holder->ref();
    
    int exceptionCode = 0;
    holder->appendChild(m_fragment, exceptionCode);
    ASSERT(exceptionCode == 0);
    
    body->appendChild(holder, exceptionCode);
    ASSERT(exceptionCode == 0);
    holder->deref();
    
    m_document->updateLayout();
    
    return holder;
}

void ReplacementFragment::restoreTestRenderingNodesToFragment(NodeImpl *holder)
{
    if (!holder)
        return;

    int exceptionCode = 0;
    while (NodeImpl *node = holder->firstChild()) {
        node->ref();
        holder->removeChild(node, exceptionCode);
        ASSERT(exceptionCode == 0);
        m_fragment->appendChild(node, exceptionCode);
        ASSERT(exceptionCode == 0);
        node->deref();
    }
}

static DOMString &matchNearestBlockquoteColorString()
{
    static DOMString matchNearestBlockquoteColorString = "match";
    return matchNearestBlockquoteColorString;
}

void ReplaceSelectionCommand::fixupNodeStyles(const QValueList<NodeDesiredStyle> &list)
{
    // This function uses the mapped "desired style" to apply the additional style needed, if any,
    // to make the node have the desired style.

    document()->updateLayout();

    QValueListConstIterator<NodeDesiredStyle> it;
    for (it = list.begin(); it != list.end(); ++it) {
        NodeImpl *node = (*it).node();
        CSSMutableStyleDeclarationImpl *desiredStyle = (*it).style();
        ASSERT(desiredStyle);

        if (!node->inDocument())
            continue;

        // The desiredStyle declaration tells what style this node wants to be.
        // Compare that to the style that it is right now in the document.
        Position pos(node, 0);
        CSSComputedStyleDeclarationImpl *currentStyle = pos.computedStyle();
        currentStyle->ref();

        // Check for the special "match nearest blockquote color" property and resolve to the correct
        // color if necessary.
        DOMString matchColorCheck = desiredStyle->getPropertyValue(CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR);
        if (matchColorCheck == matchNearestBlockquoteColorString()) {
            NodeImpl *blockquote = nearestMailBlockquote(node);
            Position pos(blockquote ? blockquote : node->getDocument()->documentElement(), 0);
            CSSComputedStyleDeclarationImpl *style = pos.computedStyle();
            style->ref();
            DOMString desiredColor = desiredStyle->getPropertyValue(CSS_PROP_COLOR);
            DOMString nearestColor = style->getPropertyValue(CSS_PROP_COLOR);
            style->deref();
            if (desiredColor != nearestColor)
                desiredStyle->setProperty(CSS_PROP_COLOR, nearestColor);
        }
        desiredStyle->removeProperty(CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR);

        currentStyle->diff(desiredStyle);
        
        // Only add in block properties if the node is at the start of a 
        // paragraph. This matches AppKit.
        if (!isStartOfParagraph(VisiblePosition(pos, DOWNSTREAM)))
            desiredStyle->removeBlockProperties();
        
        // If the desiredStyle is non-zero length, that means the current style differs
        // from the desired by the styles remaining in the desiredStyle declaration.
        if (desiredStyle->length() > 0) {
            DOM::RangeImpl *rangeAroundNode = document()->createRange();
            rangeAroundNode->ref();
            int exceptionCode = 0;
            rangeAroundNode->selectNode(node, exceptionCode);
            ASSERT(exceptionCode == 0);
            // affinity is not really important since this is a temp selection
            // just for calling applyStyle
            setEndingSelection(SelectionController(rangeAroundNode, SEL_DEFAULT_AFFINITY, SEL_DEFAULT_AFFINITY));
            applyStyle(desiredStyle);
            rangeAroundNode->deref();
        }

        currentStyle->deref();
    }
}

static void computeAndStoreNodeDesiredStyle(DOM::NodeImpl *node, QValueList<NodeDesiredStyle> &list)
{
    if (!node || !node->inDocument())
        return;
        
    CSSComputedStyleDeclarationImpl *computedStyle = Position(node, 0).computedStyle();
    computedStyle->ref();
    CSSMutableStyleDeclarationImpl *style = computedStyle->copyInheritableProperties();
    list.append(NodeDesiredStyle(node, style));
    computedStyle->deref();

    // In either of the color-matching tests below, set the color to a pseudo-color that will
    // make the content take on the color of the nearest-enclosing blockquote (if any) after
    // being pasted in.
    if (NodeImpl *blockquote = nearestMailBlockquote(node)) {
        CSSComputedStyleDeclarationImpl *blockquoteStyle = Position(blockquote, 0).computedStyle();
        blockquoteStyle->ref();
        bool match = (blockquoteStyle->getPropertyValue(CSS_PROP_COLOR) == style->getPropertyValue(CSS_PROP_COLOR));
        blockquoteStyle->deref();
        if (match) {
            style->setProperty(CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR, matchNearestBlockquoteColorString());
            return;
        }
    }
    NodeImpl *documentElement = node->getDocument() ? node->getDocument()->documentElement() : 0;
    if (documentElement) {
        CSSComputedStyleDeclarationImpl *documentStyle = Position(documentElement, 0).computedStyle();
        documentStyle->ref();
        bool match = (documentStyle->getPropertyValue(CSS_PROP_COLOR) == style->getPropertyValue(CSS_PROP_COLOR));
        documentStyle->deref();
        if (match) {
            style->setProperty(CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR, matchNearestBlockquoteColorString());
        }
    }
}

void ReplacementFragment::computeStylesUsingTestRendering(NodeImpl *holder)
{
    if (!holder)
        return;

    m_document->updateLayout();

    for (NodeImpl *node = holder->firstChild(); node; node = node->traverseNextNode(holder))
        computeAndStoreNodeDesiredStyle(node, m_styles);
}

void ReplacementFragment::removeUnrenderedNodesUsingTestRendering(NodeImpl *holder)
{
    if (!holder)
        return;

    QPtrList<NodeImpl> unrendered;

    for (NodeImpl *node = holder->firstChild(); node; node = node->traverseNextNode(holder)) {
        if (!isNodeRendered(node) && !isTableStructureNode(node))
            unrendered.append(node);
    }

    for (QPtrListIterator<NodeImpl> it(unrendered); it.current(); ++it)
        removeNode(it.current());
}

int ReplacementFragment::countRenderedBlocks(NodeImpl *holder)
{
    if (!holder)
        return 0;
    
    int count = 0;
    NodeImpl *prev = 0;
    for (NodeImpl *node = holder->firstChild(); node; node = node->traverseNextNode(holder)) {
        if (node->isBlockFlow()) {
            if (!prev) {
                count++;
                prev = node;
            }
        }
        else {
            NodeImpl *block = node->enclosingBlockFlowElement();
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
    NodeImpl *node = m_fragment->firstChild();
    while (node) {
        NodeImpl *next = node->traverseNextNode();
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
            HTMLElementImpl *elem = static_cast<HTMLElementImpl *>(node);
            CSSMutableStyleDeclarationImpl *inlineStyleDecl = elem->inlineStyleDecl();
            if (inlineStyleDecl) {
                inlineStyleDecl->removeBlockProperties();
                inlineStyleDecl->removeInheritableProperties();
            }
        }
        node = next;
    }
}

NodeDesiredStyle::NodeDesiredStyle(NodeImpl *node, CSSMutableStyleDeclarationImpl *style) 
    : m_node(node), m_style(style)
{
    if (m_node)
        m_node->ref();
    if (m_style)
        m_style->ref();
}

NodeDesiredStyle::NodeDesiredStyle(const NodeDesiredStyle &other)
    : m_node(other.node()), m_style(other.style())
{
    if (m_node)
        m_node->ref();
    if (m_style)
        m_style->ref();
}

NodeDesiredStyle::~NodeDesiredStyle()
{
    if (m_node)
        m_node->deref();
    if (m_style)
        m_style->deref();
}

NodeDesiredStyle &NodeDesiredStyle::operator=(const NodeDesiredStyle &other)
{
    NodeImpl *oldNode = m_node;
    CSSMutableStyleDeclarationImpl *oldStyle = m_style;

    m_node = other.node();
    m_style = other.style();
    
    if (m_node)
        m_node->ref();
    if (m_style)
        m_style->ref();
    
    if (oldNode)
        oldNode->deref();
    if (oldStyle)
        oldStyle->deref();
        
    return *this;
}

ReplaceSelectionCommand::ReplaceSelectionCommand(DocumentImpl *document, DocumentFragmentImpl *fragment, bool selectReplacement, bool smartReplace, bool matchStyle) 
    : CompositeEditCommand(document), 
      m_fragment(document, fragment, matchStyle),
      m_firstNodeInserted(0),
      m_lastNodeInserted(0),
      m_lastTopNodeInserted(0),
      m_insertionStyle(0),
      m_selectReplacement(selectReplacement), 
      m_smartReplace(smartReplace),
      m_matchStyle(matchStyle)
{
}

ReplaceSelectionCommand::~ReplaceSelectionCommand()
{
    if (m_firstNodeInserted)
        m_firstNodeInserted->deref();
    if (m_lastNodeInserted)
        m_lastNodeInserted->deref();
    if (m_lastTopNodeInserted)
        m_lastTopNodeInserted->deref();
    if (m_insertionStyle)
        m_insertionStyle->deref();
}

static int maxRangeOffset(NodeImpl *n)
{
    if (DOM::offsetInCharacters(n->nodeType()))
        return n->maxOffset();

    if (n->isElementNode())
        return n->childNodeCount();

    return 1;
}

// This version of the function is meant to be called on positions in a document fragment,
// so it does not check for a root editable element, it is assumed these nodes will be put
// somewhere editable in the future
bool isFirstVisiblePositionInSpecialElementInFragment(const Position& pos)
{
    VisiblePosition vPos = VisiblePosition(pos, DOWNSTREAM);

    for (NodeImpl *n = pos.node(); n; n = n->parentNode()) {
        if (VisiblePosition(n, 0, DOWNSTREAM) != vPos)
            return false;
        if (isSpecialElement(n))
            return true;
    }

    return false;
}

void ReplaceSelectionCommand::doApply()
{
    // collect information about the current selection, prior to deleting the selection
    SelectionController selection = endingSelection();
    ASSERT(selection.isCaretOrRange());
    
    if (m_matchStyle) {
        m_insertionStyle = styleAtPosition(selection.start());
        m_insertionStyle->ref();
    }
    
    VisiblePosition visibleStart(selection.start(), selection.startAffinity());
    VisiblePosition visibleEnd(selection.end(), selection.endAffinity());
    bool startAtStartOfBlock = isStartOfBlock(visibleStart);
    bool startAtEndOfBlock = isEndOfBlock(visibleStart);
    bool startAtBlockBoundary = startAtStartOfBlock || startAtEndOfBlock;
    NodeImpl *startBlock = selection.start().node()->enclosingBlockFlowElement();
    NodeImpl *endBlock = selection.end().node()->enclosingBlockFlowElement();

    // decide whether to later merge content into the startBlock
    bool mergeStart = false;
    if (startBlock == startBlock->rootEditableElement() && startAtStartOfBlock && startAtEndOfBlock) {
        // empty editable subtree, need to mergeStart so that fragment ends up
        // inside the editable subtree rather than just before it
        mergeStart = false;
    } else {
        // merge if current selection starts inside a paragraph, or there is only one block and no interchange newline to add
        mergeStart = !m_fragment.hasInterchangeNewlineAtStart() && 
            (!isStartOfParagraph(visibleStart) || (!m_fragment.hasInterchangeNewlineAtEnd() && !m_fragment.hasMoreThanOneBlock())) &&
            !isLastVisiblePositionInSpecialElement(selection.start());
        
        // This is a workaround for this bug:
        // <rdar://problem/4013642> REGRESSION (Mail): Copied quoted word does not paste as a quote if pasted at the start of a line
        // We need more powerful logic in this whole mergeStart code for this case to come out right without
        // breaking other cases.
        if (isStartOfParagraph(visibleStart) && isMailBlockquote(m_fragment.firstChild()))
            mergeStart = false;
    }
    
    // decide whether to later append nodes to the end
    NodeImpl *beyondEndNode = 0;
    if (!isEndOfParagraph(visibleEnd) && !m_fragment.hasInterchangeNewlineAtEnd()) {
        Position beyondEndPos = selection.end().downstream();
        if (!isFirstVisiblePositionInSpecialElement(beyondEndPos))
            beyondEndNode = beyondEndPos.node();
    }
    bool moveNodesAfterEnd = beyondEndNode && (startBlock != endBlock || m_fragment.hasMoreThanOneBlock());

    Position startPos = selection.start();
    
    // delete the current range selection, or insert paragraph for caret selection, as needed
    if (selection.isRange()) {
        deleteSelection(false, !(m_fragment.hasInterchangeNewlineAtStart() || m_fragment.hasInterchangeNewlineAtEnd() || m_fragment.hasMoreThanOneBlock()));
        document()->updateLayout();
        visibleStart = VisiblePosition(endingSelection().start(), VP_DEFAULT_AFFINITY);
        if (m_fragment.hasInterchangeNewlineAtStart()) {
            if (isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart)) {
                if (!isEndOfDocument(visibleStart))
                    setEndingSelection(visibleStart.next());
            }
            else {
                insertParagraphSeparator();
                setEndingSelection(VisiblePosition(endingSelection().start(), VP_DEFAULT_AFFINITY));
            }
        }
        startPos = endingSelection().start();
    } 
    else {
        ASSERT(selection.isCaret());
        if (m_fragment.hasInterchangeNewlineAtStart()) {
            if (isEndOfParagraph(visibleStart) && !isStartOfParagraph(visibleStart)) {
                if (!isEndOfDocument(visibleStart))
                    setEndingSelection(visibleStart.next());
            }
            else {
                insertParagraphSeparator();
                setEndingSelection(VisiblePosition(endingSelection().start(), VP_DEFAULT_AFFINITY));
            }
        }
        if (!m_fragment.hasInterchangeNewlineAtEnd() && m_fragment.hasMoreThanOneBlock() && 
            !startAtBlockBoundary && !isEndOfParagraph(visibleEnd)) {
            // The start and the end need to wind up in separate blocks.
            // Insert a paragraph separator to make that happen.
            insertParagraphSeparator();
            setEndingSelection(VisiblePosition(endingSelection().start(), VP_DEFAULT_AFFINITY).previous());
        }
        startPos = endingSelection().start();
    }

    if (startAtStartOfBlock && startBlock->inDocument())
        startPos = Position(startBlock, 0);

    startPos = positionOutsideContainingSpecialElement(startPos);

    KHTMLPart *part = document()->part();
    
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    part->clearTypingStyle();
    setTypingStyle(0);    
    
    // done if there is nothing to add
    if (!m_fragment.firstChild())
        return;
    
    // check for a line placeholder, and store it away for possible removal later.
    NodeImpl *block = startPos.node()->enclosingBlockFlowElement();
    NodeImpl *linePlaceholder = findBlockPlaceholder(block);
    if (!linePlaceholder) {
        Position downstream = startPos.downstream();
        downstream = positionOutsideContainingSpecialElement(downstream);
        if (downstream.node()->hasTagName(brTag) && downstream.offset() == 0 && 
            m_fragment.hasInterchangeNewlineAtEnd() &&
            isStartOfLine(VisiblePosition(downstream, VP_DEFAULT_AFFINITY)))
            linePlaceholder = downstream.node();
    }
    
    // check whether to "smart replace" needs to add leading and/or trailing space
    bool addLeadingSpace = false;
    bool addTrailingSpace = false;
    // FIXME: We need the affinity for startPos and endPos, but Position::downstream
    // and Position::upstream do not give it
    if (m_smartReplace) {
        VisiblePosition visiblePos = VisiblePosition(startPos, VP_DEFAULT_AFFINITY);
        assert(visiblePos.isNotNull());
        addLeadingSpace = startPos.leadingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull() && !isStartOfLine(visiblePos);
        if (addLeadingSpace) {
            QChar previousChar = visiblePos.previous().character();
            if (!previousChar.isNull()) {
                addLeadingSpace = !part->isCharacterSmartReplaceExempt(previousChar, true);
            }
        }
        addTrailingSpace = startPos.trailingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull() && !isEndOfLine(visiblePos);
        if (addTrailingSpace) {
            QChar thisChar = visiblePos.character();
            if (!thisChar.isNull()) {
                addTrailingSpace = !part->isCharacterSmartReplaceExempt(thisChar, false);
            }
        }
    }
    
    // There are five steps to adding the content: merge blocks at start, add remaining blocks,
    // add "smart replace" space, handle trailing newline, clean up.
    
    // initially, we say the insertion point is the start of selection
    document()->updateLayout();
    Position insertionPos = startPos;

    // step 1: merge content into the start block, if that is needed
    if (mergeStart && !isFirstVisiblePositionInSpecialElementInFragment(Position(m_fragment.mergeStartNode(), 0))) {
        NodeImpl *refNode = m_fragment.mergeStartNode();
        if (refNode) {
            NodeImpl *node = refNode->nextSibling();
            insertNodeAtAndUpdateNodesInserted(refNode, startPos.node(), startPos.offset());
            while (node && !isProbablyBlock(node)) {
                NodeImpl *next = node->nextSibling();
                insertNodeAfterAndUpdateNodesInserted(node, refNode);
                refNode = node;
                node = next;
            }
        }
        
        // update insertion point to be at the end of the last block inserted
        if (m_lastNodeInserted) {
            document()->updateLayout();
            insertionPos = Position(m_lastNodeInserted, m_lastNodeInserted->caretMaxOffset());
        }
    }

    // prune empty nodes from fragment
    // NOTE: why was this not done earlier, before the mergeStart?
    m_fragment.pruneEmptyNodes();
    
    // step 2 : merge everything remaining in the fragment
    if (m_fragment.firstChild()) {
        NodeImpl *refNode = m_fragment.firstChild();
        NodeImpl *node = refNode ? refNode->nextSibling() : 0;
        NodeImpl *insertionBlock = insertionPos.node()->enclosingBlockFlowElement();
        bool insertionBlockIsRoot = insertionBlock == insertionBlock->rootEditableElement();
        VisiblePosition visiblePos(insertionPos, DOWNSTREAM);
        if (!insertionBlockIsRoot && isProbablyBlock(refNode) && isStartOfBlock(visiblePos))
            insertNodeBeforeAndUpdateNodesInserted(refNode, insertionBlock);
        else if (!insertionBlockIsRoot && isProbablyBlock(refNode) && isEndOfBlock(visiblePos)) {
            insertNodeAfterAndUpdateNodesInserted(refNode, insertionBlock);
        // Insert the rest of the fragment at the NEXT visible position ONLY IF part of the fragment was already merged AND !isProbablyBlock
        } else if (m_lastNodeInserted && !isProbablyBlock(refNode)) {
            Position pos = visiblePos.next().deepEquivalent().downstream();
            insertNodeAtAndUpdateNodesInserted(refNode, pos.node(), pos.offset());
        } else {
            insertNodeAtAndUpdateNodesInserted(refNode, insertionPos.node(), insertionPos.offset());
        }
        
        while (node) {
            NodeImpl *next = node->nextSibling();
            insertNodeAfterAndUpdateNodesInserted(node, refNode);
            refNode = node;
            node = next;
        }
        document()->updateLayout();
        insertionPos = Position(m_lastNodeInserted, m_lastNodeInserted->caretMaxOffset());
    }

    // step 3 : handle "smart replace" whitespace
    if (addTrailingSpace && m_lastNodeInserted) {
        document()->updateLayout();
        Position pos(m_lastNodeInserted, m_lastNodeInserted->caretMaxOffset());
        bool needsTrailingSpace = pos.trailingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull();
        if (needsTrailingSpace) {
            if (m_lastNodeInserted->isTextNode()) {
                TextImpl *text = static_cast<TextImpl *>(m_lastNodeInserted);
                insertTextIntoNode(text, text->length(), nonBreakingSpaceString());
                insertionPos = Position(text, text->length());
            }
            else {
                NodeImpl *node = document()->createEditingTextNode(nonBreakingSpaceString());
                insertNodeAfterAndUpdateNodesInserted(node, m_lastNodeInserted);
                insertionPos = Position(node, 1);
            }
        }
    }

    if (addLeadingSpace && m_firstNodeInserted) {
        document()->updateLayout();
        Position pos(m_firstNodeInserted, 0);
        bool needsLeadingSpace = pos.leadingWhitespacePosition(VP_DEFAULT_AFFINITY, true).isNull();
        if (needsLeadingSpace) {
            if (m_firstNodeInserted->isTextNode()) {
                TextImpl *text = static_cast<TextImpl *>(m_firstNodeInserted);
                insertTextIntoNode(text, 0, nonBreakingSpaceString());
            } else {
                NodeImpl *node = document()->createEditingTextNode(nonBreakingSpaceString());
                insertNodeBeforeAndUpdateNodesInserted(node, m_firstNodeInserted);
            }
        }
    }
    
    Position lastPositionToSelect;

    // step 4 : handle trailing newline
    if (m_fragment.hasInterchangeNewlineAtEnd()) {
        removeLinePlaceholderIfNeeded(linePlaceholder);

        if (!m_lastNodeInserted) {
            lastPositionToSelect = endingSelection().end().downstream();
        }
        else {
            bool insertParagraph = false;
            VisiblePosition pos(insertionPos, VP_DEFAULT_AFFINITY);

            if (startBlock == endBlock && !isProbablyBlock(m_lastTopNodeInserted)) {
                insertParagraph = true;
            } else {
                // Handle end-of-document case.
                document()->updateLayout();
                if (isEndOfDocument(pos))
                    insertParagraph = true;
            }
            if (insertParagraph) {
                setEndingSelection(insertionPos, DOWNSTREAM);
                insertParagraphSeparator();
                VisiblePosition next = pos.next();

                // Select up to the paragraph separator that was added.
                lastPositionToSelect = next.deepEquivalent().downstream();
                updateNodesInserted(lastPositionToSelect.node());
            } else {
                // Select up to the preexising paragraph separator.
                VisiblePosition next = pos.next();
                lastPositionToSelect = next.deepEquivalent().downstream();
            }
        }
    } 
    else {
        if (m_lastNodeInserted && m_lastNodeInserted->hasTagName(brTag) && !document()->inStrictMode()) {
            document()->updateLayout();
            VisiblePosition pos(Position(m_lastNodeInserted, 1), DOWNSTREAM);
            if (isEndOfBlock(pos)) {
                NodeImpl *next = m_lastNodeInserted->traverseNextNode();
                bool hasTrailingBR = next && next->hasTagName(brTag) && m_lastNodeInserted->enclosingBlockFlowElement() == next->enclosingBlockFlowElement();
                if (!hasTrailingBR) {
                    // Insert an "extra" BR at the end of the block. 
                    insertNodeBefore(createBreakElement(document()), m_lastNodeInserted);
                }
            }
        }

        if (moveNodesAfterEnd && !isLastVisiblePositionInSpecialElement(Position(m_lastNodeInserted, maxRangeOffset(m_lastNodeInserted)))) {
            document()->updateLayout();
            QValueList<NodeDesiredStyle> styles;
            QPtrList<NodeImpl> blocks;
            NodeImpl *node = beyondEndNode;
            NodeImpl *refNode = m_lastNodeInserted;
            while (node) {
                RenderObject *renderer = node->renderer();
                // Stop at the first table or block.
                if (renderer && (renderer->isBlockFlow() || renderer->isTable()))
                    break;
                NodeImpl *next = node->nextSibling();
                blocks.append(node->enclosingBlockFlowElement());
                computeAndStoreNodeDesiredStyle(node, styles);
                removeNode(node);
                // No need to update inserted node variables.
                insertNodeAfter(node, refNode);
                refNode = node;
                // We want to move the first BR we see, so check for that here.
                if (node->hasTagName(brTag))
                    break;
                node = next;
            }
            document()->updateLayout();
            for (QPtrListIterator<NodeImpl> it(blocks); it.current(); ++it) {
                NodeImpl *blockToRemove = it.current();
                if (!blockToRemove->inDocument())
                    continue;
                if (!blockToRemove->renderer() || !blockToRemove->renderer()->firstChild()) {
                    if (blockToRemove->parentNode())
                        blocks.append(blockToRemove->parentNode()->enclosingBlockFlowElement());
                    removeNode(blockToRemove);
                    document()->updateLayout();
                }
            }

            fixupNodeStyles(styles);
        }
    }
    
    if (!m_matchStyle)
        fixupNodeStyles(m_fragment.desiredStyles());
    completeHTMLReplacement(lastPositionToSelect);
    
    // step 5 : mop up
    removeLinePlaceholderIfNeeded(linePlaceholder);
}

void ReplaceSelectionCommand::removeLinePlaceholderIfNeeded(NodeImpl *linePlaceholder)
{
    if (!linePlaceholder)
        return;
        
    document()->updateLayout();
    if (linePlaceholder->inDocument()) {
        VisiblePosition placeholderPos(linePlaceholder, linePlaceholder->renderer()->caretMinOffset(), DOWNSTREAM);
        if (placeholderPos.next().isNull() ||
            !(isStartOfLine(placeholderPos) && isEndOfLine(placeholderPos))) {
            NodeImpl *block = linePlaceholder->enclosingBlockFlowElement();
            removeNode(linePlaceholder);
            document()->updateLayout();
            if (!block->renderer() || block->renderer()->height() == 0)
                removeNode(block);
        }
    }
}

void ReplaceSelectionCommand::completeHTMLReplacement(const Position &lastPositionToSelect)
{
    Position start;
    Position end;

    if (m_firstNodeInserted && m_firstNodeInserted->inDocument() &&
        m_lastNodeInserted && m_lastNodeInserted->inDocument()) {

        // Find the last leaf.
        NodeImpl *lastLeaf = m_lastNodeInserted;
        while (1) {
            NodeImpl *nextChild = lastLeaf->lastChild();
            if (!nextChild)
                break;
            lastLeaf = nextChild;
        }
    
        // Find the first leaf.
        NodeImpl *firstLeaf = m_firstNodeInserted;
        while (1) {
            NodeImpl *nextChild = firstLeaf->firstChild();
            if (!nextChild)
                break;
            firstLeaf = nextChild;
        }
        
        // Call updateLayout so caretMinOffset and caretMaxOffset return correct values.
        document()->updateLayout();
        start = Position(firstLeaf, firstLeaf->caretMinOffset());
        end = Position(lastLeaf, lastLeaf->caretMaxOffset());

        if (m_matchStyle) {
            assert(m_insertionStyle);
            setEndingSelection(SelectionController(start, SEL_DEFAULT_AFFINITY, end, SEL_DEFAULT_AFFINITY));
            applyStyle(m_insertionStyle);
        }    
        
        if (lastPositionToSelect.isNotNull())
            end = lastPositionToSelect;
    }
    else if (lastPositionToSelect.isNotNull()) {
        start = end = lastPositionToSelect;
    }
    else {
        return;
    }
    
    if (m_selectReplacement)
        setEndingSelection(SelectionController(start, SEL_DEFAULT_AFFINITY, end, SEL_DEFAULT_AFFINITY));
    else
        setEndingSelection(end, SEL_DEFAULT_AFFINITY);
    
    rebalanceWhitespace();
}

EditAction ReplaceSelectionCommand::editingAction() const
{
    return EditActionPaste;
}

void ReplaceSelectionCommand::insertNodeAfterAndUpdateNodesInserted(NodeImpl *insertChild, NodeImpl *refChild)
{
    insertNodeAfter(insertChild, refChild);
    updateNodesInserted(insertChild);
}

void ReplaceSelectionCommand::insertNodeAtAndUpdateNodesInserted(NodeImpl *insertChild, NodeImpl *refChild, int offset)
{
    insertNodeAt(insertChild, refChild, offset);
    updateNodesInserted(insertChild);
}

void ReplaceSelectionCommand::insertNodeBeforeAndUpdateNodesInserted(NodeImpl *insertChild, NodeImpl *refChild)
{
    insertNodeBefore(insertChild, refChild);
    updateNodesInserted(insertChild);
}

void ReplaceSelectionCommand::updateNodesInserted(NodeImpl *node)
{
    if (!node)
        return;

    // update m_lastTopNodeInserted
    node->ref();
    if (m_lastTopNodeInserted)
        m_lastTopNodeInserted->deref();
    m_lastTopNodeInserted = node;
    
    // update m_firstNodeInserted
    if (!m_firstNodeInserted) {
        m_firstNodeInserted = node;
        m_firstNodeInserted->ref();
    }
    
    if (node == m_lastNodeInserted)
        return;
    
    // update m_lastNodeInserted
    NodeImpl *old = m_lastNodeInserted;
    m_lastNodeInserted = node->lastDescendant();
    m_lastNodeInserted->ref();
    if (old)
        old->deref();
}

} // namespace khtml
