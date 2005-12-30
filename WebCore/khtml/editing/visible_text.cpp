/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov.
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
#include "visible_text.h"

#include "htmlnames.h"
#include "rendering/render_text.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_position.h"
#include "xml/dom2_rangeimpl.h"

using namespace DOM::HTMLNames;

using DOM::DocumentImpl;
using DOM::DOMString;
using DOM::Node;
using DOM::NodeImpl;
using DOM::offsetInCharacters;
using DOM::RangeImpl;

// FIXME: These classes should probably use the render tree and not the DOM tree, since elements could
// be hidden using CSS, or additional generated content could be added.  For now, we just make sure
// text objects walk their renderers' InlineTextBox objects, so that we at least get the whitespace 
// stripped out properly and obey CSS visibility for text runs.

namespace khtml {

const unsigned short nonBreakingSpace = 0xA0;

// Buffer that knows how to compare with a search target.
// Keeps enough of the previous text to be able to search in the future,
// but no more.
class CircularSearchBuffer {
public:
    CircularSearchBuffer(const QString &target, bool isCaseSensitive);
    ~CircularSearchBuffer() { fastFree(m_buffer); }

    void clear() { m_cursor = m_buffer; m_bufferFull = false; }
    void append(int length, const QChar *characters);
    void append(const QChar &);

    int neededCharacters() const;
    bool isMatch() const;
    int length() const { return m_target.length(); }

private:
    QString m_target;
    bool m_isCaseSensitive;

    QChar *m_buffer;
    QChar *m_cursor;
    bool m_bufferFull;

    CircularSearchBuffer(const CircularSearchBuffer&);
    CircularSearchBuffer &operator=(const CircularSearchBuffer&);
};

TextIterator::TextIterator() : m_endContainer(0), m_endOffset(0), m_positionNode(0)
{
}

TextIterator::TextIterator(const RangeImpl *r, IteratorKind kind) : m_endContainer(0), m_endOffset(0), m_positionNode(0)
{
    if (!r)
        return;

    int exceptionCode = 0;

    // get and validate the range endpoints
    NodeImpl *startContainer = r->startContainer(exceptionCode);
    int startOffset = r->startOffset(exceptionCode);
    NodeImpl *endContainer = r->endContainer(exceptionCode);
    int endOffset = r->endOffset(exceptionCode);
    if (exceptionCode != 0)
        return;

    // remember ending place - this does not change
    m_endContainer = endContainer;
    m_endOffset = endOffset;

    // set up the current node for processing
    m_node = r->startNode();
    if (m_node == 0)
        return;
    m_offset = m_node == startContainer ? startOffset : 0;
    m_handledNode = false;
    m_handledChildren = false;

    // calculate first out of bounds node
    m_pastEndNode = r->pastEndNode();

    // initialize node processing state
    m_needAnotherNewline = false;
    m_textBox = 0;

    // initialize record of previous node processing
    m_lastTextNode = 0;
    m_lastTextNodeEndedWithCollapsedSpace = false;
    if (kind == RUNFINDER)
        m_lastCharacter = 0;
    else
        m_lastCharacter = '\n';

#ifndef NDEBUG
    // need this just because of the assert in advance()
    m_positionNode = m_node;
#endif

    // identify the first run
    advance();
}

void TextIterator::advance()
{
    // otherwise, where are we advancing from?
    assert(m_positionNode);

    // reset the run information
    m_positionNode = 0;
    m_textLength = 0;

    // handle remembered node that needed a newline after the text node's newline
    if (m_needAnotherNewline) {
        // emit the newline, with position a collapsed range at the end of current node.
        emitCharacter('\n', m_node->parentNode(), m_node, 1, 1);
        m_needAnotherNewline = false;
        return;
    }

    // handle remembered text box
    if (m_textBox) {
        handleTextBox();
        if (m_positionNode) {
            return;
        }
    }

    while (m_node && m_node != m_pastEndNode) {
        // handle current node according to its type
        if (!m_handledNode) {
            RenderObject *renderer = m_node->renderer();
            if (renderer && renderer->isText() && m_node->nodeType() == Node::TEXT_NODE) {
                // FIXME: What about CDATA_SECTION_NODE?
                if (renderer->style()->visibility() == VISIBLE) {
                    m_handledNode = handleTextNode();
                }
            } else if (renderer && (renderer->isImage() || renderer->isWidget())) {
                if (renderer->style()->visibility() == VISIBLE) {
                    m_handledNode = handleReplacedElement();
                }
            } else {
                m_handledNode = handleNonTextNode();
            }
            if (m_positionNode) {
                return;
            }
        }

        // find a new current node to handle in depth-first manner,
        // calling exitNode() as we come back thru a parent node
        NodeImpl *next = m_handledChildren ? 0 : m_node->firstChild();
        m_offset = 0;
        if (!next) {
            next = m_node->nextSibling();
            if (!next) {
                if (m_node->traverseNextNode() == m_pastEndNode)
                    break;
                while (!next && m_node->parentNode()) {
                    m_node = m_node->parentNode();
                    exitNode();
                    if (m_positionNode) {
                        m_handledNode = true;
                        m_handledChildren = true;
                        return;
                    }
                    next = m_node->nextSibling();
                }
            }
        }

        // set the new current node
        m_node = next;
        m_handledNode = false;
        m_handledChildren = false;

        // how would this ever be?
        if (m_positionNode) {
            return;
        }
    }
}

bool TextIterator::handleTextNode()
{
    m_lastTextNode = m_node;

    RenderText *renderer = static_cast<RenderText *>(m_node->renderer());
    DOMString str = renderer->string();

    // handle pre-formatted text
    if (!renderer->style()->collapseWhiteSpace()) {
        int runStart = m_offset;
        if (m_lastTextNodeEndedWithCollapsedSpace) {
            emitCharacter(' ', m_node, 0, runStart, runStart);
            return false;
        }
        int strLength = str.length();
        int end = (m_node == m_endContainer) ? m_endOffset : LONG_MAX;
        int runEnd = kMin(strLength, end);

        if (runStart >= runEnd)
            return true;

        m_positionNode = m_node;
        m_positionOffsetBaseNode = 0;
        m_positionStartOffset = runStart;
        m_positionEndOffset = runEnd;
        m_textCharacters = str.unicode() + runStart;
        m_textLength = runEnd - runStart;

        m_lastCharacter = str[runEnd - 1];

        return true;
    }

    if (!renderer->firstTextBox() && str.length() > 0) {
        m_lastTextNodeEndedWithCollapsedSpace = true; // entire block is collapsed space
        return true;
    }

    // Used when text boxes are out of order (Hebrew/Arabic w/ embeded LTR text)
    if (renderer->containsReversedText()) {
        m_sortedTextBoxes.clear();
        for (InlineTextBox * textBox = renderer->firstTextBox(); textBox; textBox = textBox->nextTextBox()) {
            m_sortedTextBoxes.append(textBox);
        }
        m_sortedTextBoxes.sort(); 
    }
    
    m_textBox = renderer->containsReversedText() ? m_sortedTextBoxes.first() : renderer->firstTextBox();
    handleTextBox();
    return true;
}

void TextIterator::handleTextBox()
{    
    RenderText *renderer = static_cast<RenderText *>(m_node->renderer());
    DOMString str = renderer->string();
    int start = m_offset;
    int end = (m_node == m_endContainer) ? m_endOffset : LONG_MAX;
    while (m_textBox) {
        int textBoxStart = m_textBox->m_start;
        int runStart = kMax(textBoxStart, start);

        // Check for collapsed space at the start of this run.
        InlineTextBox *firstTextBox = renderer->containsReversedText() ? m_sortedTextBoxes.getFirst() : renderer->firstTextBox();
        bool needSpace = m_lastTextNodeEndedWithCollapsedSpace
            || (m_textBox == firstTextBox && textBoxStart == runStart && runStart > 0);
        if (needSpace && !isCollapsibleWhitespace(m_lastCharacter) && !m_lastCharacter.isNull()) {
            emitCharacter(' ', m_node, 0, runStart, runStart);
            return;
        }
        int textBoxEnd = textBoxStart + m_textBox->m_len;
        int runEnd = kMin(textBoxEnd, end);
        
        // Determine what the next text box will be, but don't advance yet
        InlineTextBox *nextTextBox = renderer->containsReversedText() ? m_sortedTextBoxes.getNext() : m_textBox->nextTextBox();

        if (runStart < runEnd) {
            // Handle either a single newline character (which becomes a space),
            // or a run of characters that does not include a newline.
            // This effectively translates newlines to spaces without copying the text.
            if (str[runStart] == '\n') {
                emitCharacter(' ', m_node, 0, runStart, runStart + 1);
                m_offset = runStart + 1;
            } else {
                int subrunEnd = str.find('\n', runStart);
                if (subrunEnd == -1 || subrunEnd > runEnd) {
                    subrunEnd = runEnd;
                }

                m_offset = subrunEnd;

                m_positionNode = m_node;
                m_positionOffsetBaseNode = 0;
                m_positionStartOffset = runStart;
                m_positionEndOffset = subrunEnd;
                m_textCharacters = str.unicode() + runStart;
                m_textLength = subrunEnd - runStart;

                m_lastTextNodeEndedWithCollapsedSpace = false;
                m_lastCharacter = str[subrunEnd - 1];
            }

            // If we are doing a subrun that doesn't go to the end of the text box,
            // come back again to finish handling this text box; don't advance to the next one.
            if (m_positionEndOffset < textBoxEnd) {
                return;
            }

            // Advance and return
            int nextRunStart = nextTextBox ? nextTextBox->m_start : str.length();
            if (nextRunStart > runEnd) {
                m_lastTextNodeEndedWithCollapsedSpace = true; // collapsed space between runs or at the end
            }
            m_textBox = nextTextBox;
            if (renderer->containsReversedText())
                m_sortedTextBoxes.next();
            return;
        }
        // Advance and continue
        m_textBox = nextTextBox;
        if (renderer->containsReversedText())
            m_sortedTextBoxes.next();
    }
}

bool TextIterator::handleReplacedElement()
{
    if (m_lastTextNodeEndedWithCollapsedSpace) {
        emitCharacter(' ', m_lastTextNode->parentNode(), m_lastTextNode, 1, 1);
        return false;
    }

    m_positionNode = m_node->parentNode();
    m_positionOffsetBaseNode = m_node;
    m_positionStartOffset = 0;
    m_positionEndOffset = 1;

    m_textCharacters = 0;
    m_textLength = 0;

    m_lastCharacter = 0;

    return true;
}

bool TextIterator::handleNonTextNode()
{
    if (m_node->hasTagName(brTag)) {
        emitCharacter('\n', m_node->parentNode(), m_node, 0, 1);
    } else if (m_node->hasTagName(tdTag) || m_node->hasTagName(thTag)) {
        if (m_lastCharacter != '\n' && m_lastTextNode) {
            emitCharacter('\t', m_lastTextNode->parentNode(), m_lastTextNode, 0, 1);
        }
    } else if (m_node->hasTagName(blockquoteTag) || m_node->hasTagName(ddTag) ||
               m_node->hasTagName(divTag) ||
               m_node->hasTagName(dlTag) || m_node->hasTagName(dtTag) || 
               m_node->hasTagName(h1Tag) || m_node->hasTagName(h2Tag) ||
               m_node->hasTagName(h3Tag) || m_node->hasTagName(h4Tag) ||
               m_node->hasTagName(h5Tag) || m_node->hasTagName(h6Tag) ||
               m_node->hasTagName(hrTag) || m_node->hasTagName(liTag) ||
               m_node->hasTagName(olTag) || m_node->hasTagName(pTag) ||
               m_node->hasTagName(preTag) || m_node->hasTagName(trTag) ||
               m_node->hasTagName(ulTag)) {
        if (m_lastCharacter != '\n' && m_lastTextNode) {
            emitCharacter('\n', m_lastTextNode->parentNode(), m_lastTextNode, 0, 1);
        }
    }

    return true;
}

void TextIterator::exitNode()
{
    bool endLine = false;
    bool addNewline = false;

    if (m_node->hasTagName(blockquoteTag) || m_node->hasTagName(ddTag) ||
               m_node->hasTagName(divTag) ||
               m_node->hasTagName(dlTag) || m_node->hasTagName(dtTag) || 
               m_node->hasTagName(hrTag) || m_node->hasTagName(liTag) ||
               m_node->hasTagName(olTag) ||
               m_node->hasTagName(preTag) || m_node->hasTagName(trTag) ||
               m_node->hasTagName(ulTag))
        endLine = true;
    else if (m_node->hasTagName(h1Tag) || m_node->hasTagName(h2Tag) ||
             m_node->hasTagName(h3Tag) || m_node->hasTagName(h4Tag) ||
             m_node->hasTagName(h5Tag) || m_node->hasTagName(h6Tag) ||
             m_node->hasTagName(pTag)) {
        endLine = true;

        // In certain cases, emit a new newline character for this node
        // regardless of whether we emit another one.
        // FIXME: Some day we could do this for other tags.
        // However, doing it just for the tags above makes it more likely
        // we'll end up getting the right result without margin collapsing.
        // For example: <div><p>text</p></div> will work right even if both
        // the <div> and the <p> have bottom margins.
        RenderObject *renderer = m_node->renderer();
        if (renderer) {
            RenderStyle *style = renderer->style();
            if (style) {
                int bottomMargin = renderer->collapsedMarginBottom();
                int fontSize = style->htmlFont().getFontDef().computedPixelSize();
                if (bottomMargin * 2 >= fontSize) {
                    addNewline = true;
                }
            }
        }
    }

    // emit character(s) iff there is an earlier text node and we need at least one newline
    if (m_lastTextNode && endLine) {
        if (m_lastCharacter != '\n') {
            // insert a newline with a position following this block
            emitCharacter('\n', m_node->parentNode(), m_node, 1, 1);

            //...and, if needed, set flag to later add a newline for the current node
            assert(!m_needAnotherNewline);
            m_needAnotherNewline = addNewline;
        } else if (addNewline) {
            // insert a newline with a position following this block
            emitCharacter('\n', m_node->parentNode(), m_node, 1, 1);
        }
    }
}

void TextIterator::emitCharacter(QChar c, NodeImpl *textNode, NodeImpl *offsetBaseNode, int textStartOffset, int textEndOffset)
{
    // remember information with which to construct the TextIterator::range()
    // NOTE: textNode is often not a text node, so the range will specify child nodes of positionNode
    m_positionNode = textNode;
    m_positionOffsetBaseNode = offsetBaseNode;
    m_positionStartOffset = textStartOffset;
    m_positionEndOffset = textEndOffset;
 
    // remember information with which to construct the TextIterator::characters() and length()
    m_singleCharacterBuffer = c;
    m_textCharacters = &m_singleCharacterBuffer;
    m_textLength = 1;

    // remember some iteration state
    m_lastTextNodeEndedWithCollapsedSpace = false;
    m_lastCharacter = c;
}

RefPtr<RangeImpl> TextIterator::range() const
{
    // use the current run information, if we have it
    if (m_positionNode) {
        if (m_positionOffsetBaseNode) {
            int index = m_positionOffsetBaseNode->nodeIndex();
            m_positionStartOffset += index;
            m_positionEndOffset += index;
            m_positionOffsetBaseNode = 0;
        }
        return RefPtr<RangeImpl>(new RangeImpl(m_positionNode->getDocument(),
            m_positionNode, m_positionStartOffset, m_positionNode, m_positionEndOffset));
    }

    // otherwise, return the end of the overall range we were given
    if (m_endContainer)
        return RefPtr<RangeImpl>(new RangeImpl(m_endContainer->getDocument(), 
            m_endContainer, m_endOffset, m_endContainer, m_endOffset));
        
    return RefPtr<RangeImpl>();
}

SimplifiedBackwardsTextIterator::SimplifiedBackwardsTextIterator() : m_positionNode(0)
{
}

SimplifiedBackwardsTextIterator::SimplifiedBackwardsTextIterator(const RangeImpl *r)
{
    m_positionNode = 0;

    if (!r)
        return;

    int exception = 0;
    NodeImpl *startNode = r->startContainer(exception);
    if (exception)
        return;
    NodeImpl *endNode = r->endContainer(exception);
    if (exception)
        return;
    int startOffset = r->startOffset(exception);
    if (exception)
        return;
    int endOffset = r->endOffset(exception);
    if (exception)
        return;

    if (!offsetInCharacters(startNode->nodeType())) {
        if (startOffset >= 0 && startOffset < static_cast<int>(startNode->childNodeCount())) {
            startNode = startNode->childNode(startOffset);
            startOffset = 0;
        }
    }
    if (!offsetInCharacters(endNode->nodeType())) {
        if (endOffset > 0 && endOffset <= static_cast<int>(endNode->childNodeCount())) {
            endNode = endNode->childNode(endOffset - 1);
            endOffset = endNode->hasChildNodes() ? endNode->childNodeCount() : endNode->maxOffset();
        }
    }

    m_node = endNode;
    m_offset = endOffset;
    m_handledNode = false;
    m_handledChildren = endOffset == 0;

    m_startNode = startNode;
    m_startOffset = startOffset;

#ifndef NDEBUG
    // Need this just because of the assert.
    m_positionNode = endNode;
#endif

    m_lastTextNode = 0;
    m_lastCharacter = '\n';

    advance();
}

void SimplifiedBackwardsTextIterator::advance()
{
    assert(m_positionNode);

    m_positionNode = 0;
    m_textLength = 0;

    while (m_node) {
        if (!m_handledNode) {
            RenderObject *renderer = m_node->renderer();
            if (renderer && renderer->isText() && m_node->nodeType() == Node::TEXT_NODE) {
                // FIXME: What about CDATA_SECTION_NODE?
                if (renderer->style()->visibility() == VISIBLE && m_offset > 0) {
                    m_handledNode = handleTextNode();
                }
            } else if (renderer && (renderer->isImage() || renderer->isWidget())) {
                if (renderer->style()->visibility() == VISIBLE && m_offset > 0) {
                    m_handledNode = handleReplacedElement();
                }
            } else {
                m_handledNode = handleNonTextNode();
            }
            if (m_positionNode) {
                return;
            }
        }

        if (m_node == m_startNode)
            return;

        NodeImpl *next = 0;
        if (!m_handledChildren) {
            next = m_node->lastChild();
            while (next && next->lastChild())
                next = next->lastChild();
            m_handledChildren = true;
        }
        if (!next && m_node != m_startNode) {
            next = m_node->previousSibling();
            if (next) {
                exitNode();
                while (next->lastChild())
                    next = next->lastChild();
            }
            else if (m_node->parentNode()) {
                next = m_node->parentNode();
                exitNode();
            }
        }
        
        // Check for leaving a node and iterating backwards
        // into a different block that is an descendent of the
        // block containing the node (as in leaving
        // the "bar" node in this example: <p>foo</p>bar).
        // Must emit newline when leaving node containing "bar".
        if (next && m_node->renderer() && m_node->renderer()->style()->visibility() == VISIBLE) {
            NodeImpl *block = m_node->enclosingBlockFlowElement();
            if (block) {
                NodeImpl *nextBlock = next->enclosingBlockFlowElement();
                if (nextBlock && nextBlock->isAncestor(block))
                    emitNewlineForBROrText();
            }
        }
        
        m_node = next;
        if (m_node)
            m_offset = m_node->caretMaxOffset();
        else
            m_offset = 0;
        m_handledNode = false;
        
        if (m_positionNode) {
            return;
        }
    }
}

bool SimplifiedBackwardsTextIterator::handleTextNode()
{
    m_lastTextNode = m_node;

    RenderText *renderer = static_cast<RenderText *>(m_node->renderer());
    DOMString str = m_node->nodeValue();

    if (!renderer->firstTextBox() && str.length() > 0) {
        return true;
    }

    m_positionEndOffset = m_offset;

    m_offset = (m_node == m_startNode) ? m_startOffset : 0;
    m_positionNode = m_node;
    m_positionStartOffset = m_offset;
    m_textLength = m_positionEndOffset - m_positionStartOffset;
    m_textCharacters = str.unicode() + m_positionStartOffset;

    m_lastCharacter = str[m_positionEndOffset - 1];

    return true;
}

bool SimplifiedBackwardsTextIterator::handleReplacedElement()
{
    int offset = m_node->nodeIndex();

    m_positionNode = m_node->parentNode();
    m_positionStartOffset = offset;
    m_positionEndOffset = offset + 1;

    m_textCharacters = 0;
    m_textLength = 0;

    m_lastCharacter = 0;

    return true;
}

bool SimplifiedBackwardsTextIterator::handleNonTextNode()
{
    if (m_node->hasTagName(brTag))
        emitNewlineForBROrText();
    else if (m_node->hasTagName(tdTag) || m_node->hasTagName(thTag) ||
             m_node->hasTagName(blockquoteTag) || m_node->hasTagName(ddTag) ||
             m_node->hasTagName(divTag) ||
             m_node->hasTagName(dlTag) || m_node->hasTagName(dtTag) || 
             m_node->hasTagName(h1Tag) || m_node->hasTagName(h2Tag) ||
             m_node->hasTagName(h3Tag) || m_node->hasTagName(h4Tag) ||
             m_node->hasTagName(h5Tag) || m_node->hasTagName(h6Tag) ||
             m_node->hasTagName(hrTag) || m_node->hasTagName(liTag) ||
             m_node->hasTagName(olTag) || m_node->hasTagName(pTag) ||
             m_node->hasTagName(preTag) || m_node->hasTagName(trTag) ||
             m_node->hasTagName(ulTag)) {
        // Emit a space to "break up" content. Any word break
        // character will do.
        emitCharacter(' ', m_node, 0, 0);
    }

    return true;
}

void SimplifiedBackwardsTextIterator::exitNode()
{
    // Left this function in so that the name and usage patters remain similar to 
    // TextIterator. However, the needs of this iterator are much simpler, and
    // the handleNonTextNode() function does just what we want (i.e. insert a
    // space for certain node types to "break up" text so that it does not seem
    // like content is next to other text when it really isn't). 
    handleNonTextNode();
}

void SimplifiedBackwardsTextIterator::emitCharacter(QChar c, NodeImpl *node, int startOffset, int endOffset)
{
    m_singleCharacterBuffer = c;
    m_positionNode = node;
    m_positionStartOffset = startOffset;
    m_positionEndOffset = endOffset;
    m_textCharacters = &m_singleCharacterBuffer;
    m_textLength = 1;
    m_lastCharacter = c;
}

void SimplifiedBackwardsTextIterator::emitNewlineForBROrText()
{
    int offset;
    
    if (m_lastTextNode) {
        offset = m_lastTextNode->nodeIndex();
        emitCharacter('\n', m_lastTextNode->parentNode(), offset, offset + 1);
    } else {
        offset = m_node->nodeIndex();
        emitCharacter('\n', m_node->parentNode(), offset, offset + 1);
    }
}

RefPtr<RangeImpl> SimplifiedBackwardsTextIterator::range() const
{
    if (m_positionNode) {
        return RefPtr<RangeImpl>(new RangeImpl(m_positionNode->getDocument(), m_positionNode, m_positionStartOffset, m_positionNode, m_positionEndOffset));
    } else {
        return RefPtr<RangeImpl>(new RangeImpl(m_startNode->getDocument(), m_startNode, m_startOffset, m_startNode, m_startOffset));
    }
}

CharacterIterator::CharacterIterator()
    : m_offset(0), m_runOffset(0), m_atBreak(true)
{
}

CharacterIterator::CharacterIterator(const RangeImpl *r)
    : m_offset(0), m_runOffset(0), m_atBreak(true), m_textIterator(r, RUNFINDER)
{
    while (!atEnd() && m_textIterator.length() == 0) {
        m_textIterator.advance();
    }
}

RefPtr<RangeImpl> CharacterIterator::range() const
{
    RefPtr<RangeImpl> r = m_textIterator.range();
    if (!m_textIterator.atEnd()) {
        if (m_textIterator.length() <= 1) {
            assert(m_runOffset == 0);
        } else {
            int exception = 0;
            NodeImpl *n = r->startContainer(exception);
            assert(n == r->endContainer(exception));
            int offset = r->startOffset(exception) + m_runOffset;
            r->setStart(n, offset, exception);
            r->setEnd(n, offset + 1, exception);
        }
    }
    return r;
}

void CharacterIterator::advance(int count)
{
    assert(!atEnd());

    m_atBreak = false;

    // easy if there is enough left in the current m_textIterator run
    int remaining = m_textIterator.length() - m_runOffset;
    if (count < remaining) {
        m_runOffset += count;
        m_offset += count;
        return;
    }

    // exhaust the current m_textIterator run
    count -= remaining;
    m_offset += remaining;
    
    // move to a subsequent m_textIterator run
    for (m_textIterator.advance(); !atEnd(); m_textIterator.advance()) {
        int runLength = m_textIterator.length();
        if (runLength == 0) {
            m_atBreak = true;
        } else {
            // see whether this is m_textIterator to use
            if (count < runLength) {
                m_runOffset = count;
                m_offset += count;
                return;
            }
            
            // exhaust this m_textIterator run
            count -= runLength;
            m_offset += runLength;
        }
    }

    // ran to the end of the m_textIterator... no more runs left
    m_atBreak = true;
    m_runOffset = 0;
}

QString CharacterIterator::string(int numChars)
{
    QString result;
    result.reserve(numChars);
    while (numChars > 0 && !atEnd()) {
        int runSize = kMin(numChars, length());
        result.append(characters(), runSize);
        numChars -= runSize;
        advance(runSize);
    }
    return result;
}

WordAwareIterator::WordAwareIterator()
: m_previousText(0), m_didLookAhead(false)
{
}

WordAwareIterator::WordAwareIterator(const RangeImpl *r)
: m_previousText(0), m_didLookAhead(false), m_textIterator(r)
{
    m_didLookAhead = true;  // so we consider the first chunk from the text iterator
    advance();              // get in position over the first chunk of text
}

// We're always in one of these modes:
// - The current chunk in the text iterator is our current chunk
//      (typically its a piece of whitespace, or text that ended with whitespace)
// - The previous chunk in the text iterator is our current chunk
//      (we looked ahead to the next chunk and found a word boundary)
// - We built up our own chunk of text from many chunks from the text iterator

//FIXME: Perf could be bad for huge spans next to each other that don't fall on word boundaries

void WordAwareIterator::advance()
{
    m_previousText = 0;
    m_buffer = "";      // toss any old buffer we built up

    // If last time we did a look-ahead, start with that looked-ahead chunk now
    if (!m_didLookAhead) {
        assert(!m_textIterator.atEnd());
        m_textIterator.advance();
    }
    m_didLookAhead = false;

    // Go to next non-empty chunk 
    while (!m_textIterator.atEnd() && m_textIterator.length() == 0) {
        m_textIterator.advance();
    }
    m_range = m_textIterator.range();

    if (m_textIterator.atEnd()) {
        return;
    }
    
    while (1) {
        // If this chunk ends in whitespace we can just use it as our chunk.
        if (m_textIterator.characters()[m_textIterator.length()-1].isSpace()) {
            return;
        }

        // If this is the first chunk that failed, save it in previousText before look ahead
        if (m_buffer.isEmpty()) {
            m_previousText = m_textIterator.characters();
            m_previousLength = m_textIterator.length();
        }

        // Look ahead to next chunk.  If it is whitespace or a break, we can use the previous stuff
        m_textIterator.advance();
        if (m_textIterator.atEnd() || m_textIterator.length() == 0 || m_textIterator.characters()[0].isSpace()) {
            m_didLookAhead = true;
            return;
        }

        if (m_buffer.isEmpty()) {
            // Start gobbling chunks until we get to a suitable stopping point
            m_buffer.append(m_previousText, m_previousLength);
            m_previousText = 0;
        }
        m_buffer.append(m_textIterator.characters(), m_textIterator.length());
        int exception = 0;
        m_range->setEnd(m_textIterator.range()->endContainer(exception),
            m_textIterator.range()->endOffset(exception), exception);
    }
}

int WordAwareIterator::length() const
{
    if (!m_buffer.isEmpty()) {
        return m_buffer.length();
    } else if (m_previousText) {
        return m_previousLength;
    } else {
        return m_textIterator.length();
    }
}

const QChar *WordAwareIterator::characters() const
{
    if (!m_buffer.isEmpty()) {
        return m_buffer.unicode();
    } else if (m_previousText) {
        return m_previousText;
    } else {
        return m_textIterator.characters();
    }
}

CircularSearchBuffer::CircularSearchBuffer(const QString &s, bool isCaseSensitive)
    : m_target(s)
{
    assert(!s.isEmpty());

    if (!isCaseSensitive) {
        m_target = s.lower();
    }
    m_target.replace(nonBreakingSpace, ' ');
    m_isCaseSensitive = isCaseSensitive;

    m_buffer = static_cast<QChar *>(fastMalloc(s.length() * sizeof(QChar)));
    m_cursor = m_buffer;
    m_bufferFull = false;
}

void CircularSearchBuffer::append(const QChar &c)
{
    if (m_isCaseSensitive) {
        *m_cursor++ = c.unicode() == nonBreakingSpace ? ' ' : c.unicode();
    } else {
        *m_cursor++ = c.unicode() == nonBreakingSpace ? ' ' : c.lower().unicode();
    }
    if (m_cursor == m_buffer + length()) {
        m_cursor = m_buffer;
        m_bufferFull = true;
    }
}

// This function can only be used when the buffer is not yet full,
// and when then count is small enough to fit in the buffer.
// No need for a more general version for the search algorithm.
void CircularSearchBuffer::append(int count, const QChar *characters)
{
    int tailSpace = m_buffer + length() - m_cursor;

    assert(!m_bufferFull);
    assert(count <= tailSpace);

    if (m_isCaseSensitive) {
        for (int i = 0; i != count; ++i) {
            QChar c = characters[i];
            m_cursor[i] = c.unicode() == nonBreakingSpace ? ' ' : c.unicode();
        }
    } else {
        for (int i = 0; i != count; ++i) {
            QChar c = characters[i];
            m_cursor[i] = c.unicode() == nonBreakingSpace ? ' ' : c.lower().unicode();
        }
    }
    if (count < tailSpace) {
        m_cursor += count;
    } else {
        m_bufferFull = true;
        m_cursor = m_buffer;
    }
}

int CircularSearchBuffer::neededCharacters() const
{
    return m_bufferFull ? 0 : m_buffer + length() - m_cursor;
}

bool CircularSearchBuffer::isMatch() const
{
    assert(m_bufferFull);

    int headSpace = m_cursor - m_buffer;
    int tailSpace = length() - headSpace;
    return memcmp(m_cursor, m_target.unicode(), tailSpace * sizeof(QChar)) == 0
        && memcmp(m_buffer, m_target.unicode() + tailSpace, headSpace * sizeof(QChar)) == 0;
}

int TextIterator::rangeLength(const RangeImpl *r)
{
    int length = 0;
    for (TextIterator it(r); !it.atEnd(); it.advance()) {
        length += it.length();
    }
    return length;
}

RangeImpl *TextIterator::rangeFromLocationAndLength(DocumentImpl *doc, int rangeLocation, int rangeLength)
{
    RangeImpl *resultRange = doc->createRange();

    int docTextPosition = 0;
    int rangeEnd = rangeLocation + rangeLength;
    bool startRangeFound = false;

    RefPtr<RangeImpl> textRunRange;

    TextIterator it(rangeOfContents(doc).get());
    
    // FIXME: the atEnd() check shouldn't be necessary, workaround for <http://bugzilla.opendarwin.org/show_bug.cgi?id=6289>.
    if (rangeLocation == 0 && rangeLength == 0 && it.atEnd()) {
        int exception = 0;
        textRunRange = it.range();
        
        resultRange->setStart(textRunRange->startContainer(exception), 0, exception);
        assert(exception == 0);
        resultRange->setEnd(textRunRange->startContainer(exception), 0, exception);
        assert(exception == 0);
        
        return resultRange;
    }

    for (; !it.atEnd(); it.advance()) {
        int len = it.length();
        textRunRange = it.range();

        if (rangeLocation >= docTextPosition && rangeLocation <= docTextPosition + len) {
            startRangeFound = true;
            int exception = 0;
            if (textRunRange->startContainer(exception)->isTextNode()) {
                int offset = rangeLocation - docTextPosition;
                resultRange->setStart(textRunRange->startContainer(exception), offset + textRunRange->startOffset(exception), exception);
            } else {
                if (rangeLocation == docTextPosition) {
                    resultRange->setStart(textRunRange->startContainer(exception), textRunRange->startOffset(exception), exception);
                } else {
                    resultRange->setStart(textRunRange->endContainer(exception), textRunRange->endOffset(exception), exception);
                }
            }
        }

        if (rangeEnd >= docTextPosition && rangeEnd <= docTextPosition + len) {
            int exception = 0;
            if (textRunRange->startContainer(exception)->isTextNode()) {
                int offset = rangeEnd - docTextPosition;
                resultRange->setEnd(textRunRange->startContainer(exception), offset + textRunRange->startOffset(exception), exception);
            } else {
                if (rangeEnd == docTextPosition) {
                    resultRange->setEnd(textRunRange->startContainer(exception), textRunRange->startOffset(exception), exception);
                } else {
                    resultRange->setEnd(textRunRange->endContainer(exception), textRunRange->endOffset(exception), exception);
                }
            }
            if (!(rangeLength == 0 && rangeEnd == docTextPosition + len)) {
                docTextPosition += len;
                break;
            }
        }
        docTextPosition += len;
    }
    
    if (!startRangeFound) {
        delete resultRange;
        return 0;
    }
    
    if (rangeLength != 0 && rangeEnd > docTextPosition) { // rangeEnd is out of bounds
        int exception = 0;
        resultRange->setEnd(textRunRange->endContainer(exception), textRunRange->endOffset(exception), exception);
    }
    
    return resultRange;
}

QString plainText(const RangeImpl *r)
{
    QString result("");
    for (TextIterator it(r); !it.atEnd(); it.advance()) {
        result.append(it.characters(), it.length());
    }
    return result;
}

RefPtr<RangeImpl> findPlainText(const RangeImpl *r, const QString &s, bool forward, bool caseSensitive)
{
    // FIXME: Can we do Boyer-Moore or equivalent instead for speed?

    // FIXME: This code does not allow \n at the moment because of issues with <br>.
    // Once we fix those, we can remove this check.
    if (s.isEmpty() || s.find('\n') != -1) {
        int exception = 0;
        RangeImpl *result = r->cloneRange(exception);
        result->collapse(forward, exception);
        return RefPtr<RangeImpl>(result);
    }

    CircularSearchBuffer buffer(s, caseSensitive);

    bool found = false;
    CharacterIterator rangeEnd;

    {
        CharacterIterator it(r);
        while (1) {
            // Fill the buffer.
            while (int needed = buffer.neededCharacters()) {
                if (it.atBreak()) {
                    if (it.atEnd()) {
                        goto done;
                    }
                    buffer.clear();
                }
                int available = it.length();
                int runLength = kMin(needed, available);
                buffer.append(runLength, it.characters());
                it.advance(runLength);
            }

            // Do the search.
            while (1) {
                if (buffer.isMatch()) {
                    // Compute the range for the result.
                    found = true;
                    rangeEnd = it;
                    // If searching forward, stop on the first match.
                    // If searching backward, don't stop, so we end up with the last match.
                    if (forward) {
                        goto done;
                    }
                }
                if (it.atBreak())
                    break;
                buffer.append(it.characters()[0]);
                it.advance(1);
            }
            buffer.clear();
        }
    }

done:
    int exception = 0;
    RangeImpl *result = r->cloneRange(exception);
    if (!found) {
        result->collapse(!forward, exception);
    } else {
        CharacterIterator it(r);
        it.advance(rangeEnd.characterOffset() - buffer.length());
        result->setStart(it.range()->startContainer(exception), it.range()->startOffset(exception), exception);
        it.advance(buffer.length() - 1);
        result->setEnd(it.range()->endContainer(exception), it.range()->endOffset(exception), exception);
    }
    return RefPtr<RangeImpl>(result);
}

}
