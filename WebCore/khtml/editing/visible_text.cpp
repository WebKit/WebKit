/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#include "khtml_text_operations.h"

#include <misc/htmltags.h>
#include <rendering/render_text.h>
#include <xml/dom_nodeimpl.h>

using DOM::DOMString;
using DOM::Node;
using DOM::NodeImpl;
using DOM::Range;

namespace khtml {

const unsigned short nonBreakingSpace = 0xA0;

// Iterates through the DOM range, returning all the text, and 0-length boundaries
// at points where replaced elements break up the text flow.
class TextIterator
{
public:
    TextIterator();
    explicit TextIterator(const DOM::Range &);

    bool atEnd() const { return !m_positionNode; }
    void advance();

    long textLength() const { return m_textLength; }
    const QChar *textCharacters() const { return m_textCharacters; }

    DOM::Range position() const;

private:
    void exitNode();
    bool handleTextNode();
    bool handleReplacedElement();
    bool handleNonTextNode();
    void handleTextBox();
    void emitCharacter(QChar, DOM::NodeImpl *textNode, long textStartOffset, long textEndOffset);

    // Current position, not necessarily of the text being returned, but position
    // as we walk through the DOM tree.
    DOM::NodeImpl *m_node;
    long m_offset;
    bool m_handledNode;
    bool m_handledChildren;

    // End of the range.
    DOM::NodeImpl *m_endNode;
    long m_endOffset;

    // The current text and its position, in the form to be returned from the iterator.
    DOM::NodeImpl *m_positionNode;
    long m_positionStartOffset;
    long m_positionEndOffset;
    const QChar *m_textCharacters;
    long m_textLength;

    // Used when there is still some pending text from the current node; when these
    // are false and 0, we go back to normal iterating.
    bool m_needAnotherNewline;
    InlineTextBox *m_textBox;

    // Used to do the whitespace collapsing logic.
    DOM::NodeImpl *m_lastTextNode;    
    bool m_lastTextNodeEndedWithCollapsedSpace;
    QChar m_lastCharacter;

    // Used for whitespace characters that aren't in the DOM, so we can point at them.
    QChar m_singleCharacterBuffer;
};

// Builds on the text iterator, adding a character position so we can walk one
// character at a time, or faster, as needed. Useful for searching.
class CharacterIterator {
public:
    CharacterIterator();
    explicit CharacterIterator(const DOM::Range &r);

    void advance(long numCharacters);

    bool atBreak() const { return m_atBreak; }
    bool atEnd() const { return m_textIterator.atEnd(); }

    long numCharacters() const { return m_textIterator.textLength() - m_runOffset; }
    const QChar *characters() const { return m_textIterator.textCharacters() + m_runOffset; }

    long characterOffset() const { return m_offset; }
    Range position() const;

private:
    long m_offset;
    long m_runOffset;
    bool m_atBreak;

    TextIterator m_textIterator;
};

// Buffer that knows how to compare with a search target.
// Keeps enough of the previous text to be able to search in the future,
// but no more.
class CircularSearchBuffer {
public:
    CircularSearchBuffer(const QString &target, bool isCaseSensitive);
    ~CircularSearchBuffer() { free(m_buffer); }

    void clear() { m_cursor = m_buffer; m_bufferFull = false; }
    void append(long length, const QChar *characters);
    void append(const QChar &);

    long neededCharacters() const;
    bool isMatch() const;
    long length() const { return m_target.length(); }

private:
    QString m_target;
    bool m_isCaseSensitive;

    QChar *m_buffer;
    QChar *m_cursor;
    bool m_bufferFull;

    CircularSearchBuffer(const CircularSearchBuffer&);
    CircularSearchBuffer &operator=(const CircularSearchBuffer&);
};

TextIterator::TextIterator() : m_positionNode(0)
{
}

inline bool offsetInCharacters(unsigned short type)
{
    switch (type) {
        case Node::ATTRIBUTE_NODE:
        case Node::DOCUMENT_FRAGMENT_NODE:
        case Node::DOCUMENT_NODE:
        case Node::ELEMENT_NODE:
        case Node::ENTITY_REFERENCE_NODE:
            return false;

        case Node::CDATA_SECTION_NODE:
        case Node::COMMENT_NODE:
        case Node::PROCESSING_INSTRUCTION_NODE:
        case Node::TEXT_NODE:
            return true;

        case Node::DOCUMENT_TYPE_NODE:
        case Node::ENTITY_NODE:
        case Node::NOTATION_NODE:
            assert(false); // should never be reached
            return false;
    }

    assert(false); // should never be reached
    return false;
}

TextIterator::TextIterator(const Range &r)
{
    if (r.isNull()) {
        m_positionNode = 0;
        return;
    }

    NodeImpl *startNode = r.startContainer().handle();
    NodeImpl *endNode = r.endContainer().handle();
    long startOffset = r.startOffset();
    long endOffset = r.endOffset();

    if (!offsetInCharacters(startNode->nodeType())) {
        if (startOffset >= 0 && startOffset < static_cast<long>(startNode->childNodeCount())) {
            startNode = startNode->childNode(startOffset);
            startOffset = 0;
        }
    }
    if (!offsetInCharacters(endNode->nodeType())) {
        if (endOffset > 0 && endOffset <= static_cast<long>(endNode->childNodeCount())) {
            endNode = endNode->childNode(endOffset - 1);
            endOffset = LONG_MAX;
        }
    }

    m_node = startNode;
    m_offset = startOffset;
    m_handledNode = false;
    m_handledChildren = false;

    m_endNode = endNode;
    m_endOffset = endOffset;

    m_needAnotherNewline = false;
    m_textBox = 0;

    m_lastTextNode = 0;
    m_lastTextNodeEndedWithCollapsedSpace = false;
    m_lastCharacter = '\n';

#ifndef NDEBUG
    // Need this just because of the assert.
    m_positionNode = startNode;
#endif

    advance();
}

void TextIterator::advance()
{
    assert(m_positionNode);

    m_positionNode = 0;
    m_textLength = 0;

    if (m_needAnotherNewline) {
        // Emit the newline, with position a collapsed range at the end of current node.
        long offset = m_node->nodeIndex();
        emitCharacter('\n', m_node->parentNode(), offset + 1, offset + 1);
        m_needAnotherNewline = false;
        return;
    }

    if (m_textBox) {
        handleTextBox();
        if (m_positionNode) {
            return;
        }
    }

    while (m_node) {
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

        NodeImpl *next = m_handledChildren ? 0 : m_node->firstChild();
        m_offset = 0;
        if (!next && m_node != m_endNode) {
            next = m_node->nextSibling();
            while (!next && m_node->parentNode()) {
                m_node = m_node->parentNode();
                exitNode();
                if (m_positionNode) {
                    m_handledNode = true;
                    m_handledChildren = true;
                    return;
                }
                if (m_node == m_endNode) {
                    break;
                }
                next = m_node->nextSibling();
            }
        }

        m_node = next;
        m_handledNode = false;
        m_handledChildren = false;

        if (m_positionNode) {
            return;
        }
    }
}

bool TextIterator::handleTextNode()
{
    m_lastTextNode = m_node;

    RenderText *renderer = static_cast<RenderText *>(m_node->renderer());
    DOMString str = m_node->nodeValue();

    if (renderer->style()->whiteSpace() == khtml::PRE) {
        long runStart = m_offset;
        if (m_lastTextNodeEndedWithCollapsedSpace) {
            emitCharacter(' ', m_node, runStart, runStart);
            return false;
        }
        long strLength = str.length();
        long end = (m_node == m_endNode) ? m_endOffset : LONG_MAX;
        long runEnd = kMin(strLength, end);

        m_positionNode = m_node;
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

    m_textBox = renderer->firstTextBox();
    handleTextBox();
    return true;
}

void TextIterator::handleTextBox()
{    
    RenderText *renderer = static_cast<RenderText *>(m_node->renderer());
    DOMString str = m_node->nodeValue();
    long start = m_offset;
    long end = (m_node == m_endNode) ? m_endOffset : LONG_MAX;
    for (; m_textBox; m_textBox = m_textBox->nextTextBox()) {
        long textBoxStart = m_textBox->m_start;
        long runStart = kMax(textBoxStart, start);

        // Check for collapsed space at the start of this run.
        bool needSpace = m_lastTextNodeEndedWithCollapsedSpace
            || (m_textBox == renderer->firstTextBox() && textBoxStart == runStart && runStart > 0);
        if (needSpace && !m_lastCharacter.isSpace()) {
            emitCharacter(' ', m_node, runStart, runStart);
            return;
        }

        long textBoxEnd = textBoxStart + m_textBox->m_len;
        long runEnd = kMin(textBoxEnd, end);

        if (runStart < runEnd) {
            // Handle either a single newline character (which becomes a space),
            // or a run of characters that does not include a newline.
            // This effectively translates newlines to spaces without copying the text.
            if (str[runStart] == '\n') {
                emitCharacter(' ', m_node, runStart, runStart + 1);
                m_offset = runStart + 1;
            } else {
                long subrunEnd = str.find('\n', runStart);
                if (subrunEnd == -1 || subrunEnd > runEnd) {
                    subrunEnd = runEnd;
                }

                m_offset = subrunEnd;

                m_positionNode = m_node;
                m_positionStartOffset = runStart;
                m_positionEndOffset = subrunEnd;
                m_textCharacters = str.unicode() + runStart;
                m_textLength = subrunEnd - runStart;

                m_lastCharacter = str[subrunEnd - 1];
            }

            // If we are doing a subrun that doesn't go to the end of the text box,
            // come back again to finish handling this text box; don't advance to the next one.
            if (m_positionEndOffset < runEnd) {
                return;
            }

            // Advance to the next text box.
            InlineTextBox *nextTextBox = m_textBox->nextTextBox();
            long nextRunStart = nextTextBox ? nextTextBox->m_start : str.length();
            if (nextRunStart > runEnd) {
                m_lastTextNodeEndedWithCollapsedSpace = true; // collapsed space between runs or at the end
            }
            m_textBox = nextTextBox;
            return;
        }
    }
}

bool TextIterator::handleReplacedElement()
{
    if (m_lastTextNodeEndedWithCollapsedSpace) {
        long offset = m_lastTextNode->nodeIndex();
        emitCharacter(' ', m_lastTextNode->parentNode(), offset + 1, offset + 1);
        return false;
    }

    long offset = m_node->nodeIndex();

    m_positionNode = m_node->parentNode();
    m_positionStartOffset = offset;
    m_positionEndOffset = offset + 1;

    m_textCharacters = 0;
    m_textLength = 0;

    m_lastCharacter = 0;

    return true;
}

bool TextIterator::handleNonTextNode()
{
    switch (m_node->id()) {
        case ID_BR: {
            long offset = m_node->nodeIndex();
            emitCharacter('\n', m_node->parentNode(), offset, offset + 1);
            break;
        }

        case ID_TD:
        case ID_TH:
            if (m_lastCharacter != '\n' && m_lastTextNode) {
                long offset = m_lastTextNode->nodeIndex();
                emitCharacter('\t', m_lastTextNode->parentNode(), offset, offset + 1);
            }
            break;

        case ID_BLOCKQUOTE:
        case ID_DD:
        case ID_DIV:
        case ID_DL:
        case ID_DT:
        case ID_H1:
        case ID_H2:
        case ID_H3:
        case ID_H4:
        case ID_H5:
        case ID_H6:
        case ID_HR:
        case ID_LI:
        case ID_OL:
        case ID_P:
        case ID_PRE:
        case ID_TR:
        case ID_UL:
            if (m_lastCharacter != '\n' && m_lastTextNode) {
                long offset = m_lastTextNode->nodeIndex();
                emitCharacter('\n', m_lastTextNode->parentNode(), offset, offset + 1);
            }
            break;
    }

    return true;
}

void TextIterator::exitNode()
{
    bool endLine = false;
    bool addNewline = false;

    switch (m_node->id()) {
        case ID_BLOCKQUOTE:
        case ID_DD:
        case ID_DIV:
        case ID_DL:
        case ID_DT:
        case ID_HR:
        case ID_LI:
        case ID_OL:
        case ID_PRE:
        case ID_TR:
        case ID_UL:
            endLine = true;
            break;

        case ID_H1:
        case ID_H2:
        case ID_H3:
        case ID_H4:
        case ID_H5:
        case ID_H6:
        case ID_P:
            endLine = true;
            addNewline = true;
            break;
    }

    if (endLine && m_lastCharacter != '\n' && m_lastTextNode) {
        long offset = m_lastTextNode->nodeIndex();
        emitCharacter('\n', m_lastTextNode->parentNode(), offset, offset + 1);
        m_needAnotherNewline = addNewline;
    } else if (addNewline && m_lastTextNode) {
        long offset = m_node->childNodeCount();
        emitCharacter('\n', m_node, offset, offset);
    }
}

void TextIterator::emitCharacter(QChar c, NodeImpl *textNode, long textStartOffset, long textEndOffset)
{
    m_singleCharacterBuffer = c;
    m_positionNode = textNode;
    m_positionStartOffset = textStartOffset;
    m_positionEndOffset = textEndOffset;
    m_textCharacters = &m_singleCharacterBuffer;
    m_textLength = 1;

    m_lastTextNodeEndedWithCollapsedSpace = false;
    m_lastCharacter = c;
}

Range TextIterator::position() const
{
    assert(m_positionNode);
    return Range(m_positionNode, m_positionStartOffset, m_positionNode, m_positionEndOffset);
}

CharacterIterator::CharacterIterator()
    : m_offset(0), m_runOffset(0), m_atBreak(true)
{
}

CharacterIterator::CharacterIterator(const Range &r)
    : m_offset(0), m_runOffset(0), m_atBreak(true), m_textIterator(r)
{
    while (!atEnd() && m_textIterator.textLength() == 0) {
        m_textIterator.advance();
    }
}

Range CharacterIterator::position() const
{
    Range r = m_textIterator.position();
    if (m_textIterator.textLength() <= 1) {
        assert(m_runOffset == 0);
    } else {
        Node n = r.startContainer();
        assert(n == r.endContainer());
        long offset = r.startOffset() + m_runOffset;
        r.setStart(n, offset);
        r.setEnd(n, offset + 1);
    }
    return r;
}

void CharacterIterator::advance(long count)
{
    assert(!atEnd());

    m_atBreak = false;

    long remaining = m_textIterator.textLength() - m_runOffset;
    if (count < remaining) {
        m_runOffset += count;
        m_offset += count;
        return;
    }

    count -= remaining;
    m_offset += remaining;
    for (m_textIterator.advance(); !atEnd(); m_textIterator.advance()) {
        long runLength = m_textIterator.textLength();
        if (runLength == 0) {
            m_atBreak = true;
        } else {
            if (count < runLength) {
                m_runOffset = count;
                m_offset += count;
                return;
            }
            count -= runLength;
            m_offset += runLength;
        }
    }

    m_atBreak = true;
    m_runOffset = 0;
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

    m_buffer = static_cast<QChar *>(malloc(s.length() * sizeof(QChar)));
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
void CircularSearchBuffer::append(long count, const QChar *characters)
{
    long tailSpace = m_buffer + length() - m_cursor;

    assert(!m_bufferFull);
    assert(count <= tailSpace);

    if (m_isCaseSensitive) {
        for (long i = 0; i != count; ++i) {
            QChar c = characters[i];
            m_cursor[i] = c.unicode() == nonBreakingSpace ? ' ' : c.unicode();
        }
    } else {
        for (long i = 0; i != count; ++i) {
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

long CircularSearchBuffer::neededCharacters() const
{
    return m_bufferFull ? 0 : m_buffer + length() - m_cursor;
}

bool CircularSearchBuffer::isMatch() const
{
    assert(m_bufferFull);

    long headSpace = m_cursor - m_buffer;
    long tailSpace = length() - headSpace;
    return memcmp(m_cursor, m_target.unicode(), tailSpace * sizeof(QChar)) == 0
        && memcmp(m_buffer, m_target.unicode() + tailSpace, headSpace * sizeof(QChar)) == 0;
}

QString plainText(const Range &r)
{
    // Allocate string at the right size, rather than building it up by successive append calls.
    long length = 0;
    for (TextIterator it(r); !it.atEnd(); it.advance()) {
        length += it.textLength();
    }
    QString result("");
    result.reserve(length);
    for (TextIterator it(r); !it.atEnd(); it.advance()) {
        result.append(it.textCharacters(), it.textLength());
    }
    return result;
}

Range findPlainText(const Range &r, const QString &s, bool forward, bool caseSensitive)
{
    // FIXME: Can we do Boyer-Moore or equivalent instead for speed?

    // FIXME: This code does not allow \n at the moment because of issues with <br>.
    // Once we fix those, we can remove this check.
    if (s.isEmpty() || s.find('\n') != -1) {
        Range result = r;
        result.collapse(forward);
        return result;
    }

    CircularSearchBuffer buffer(s, caseSensitive);

    bool found = false;
    CharacterIterator rangeEnd;

    {
        CharacterIterator it(r);
        while (!it.atEnd()) {
            // Fill the buffer.
            while (long needed = buffer.neededCharacters()) {
                long available = it.numCharacters();
                long runLength = kMin(needed, available);
                buffer.append(runLength, it.characters());
                it.advance(runLength);
                if (it.atBreak()) {
                    if (it.atEnd()) {
                        goto done;
                    }
                    buffer.clear();
                }
            }

            // Do the search.
            do {
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
                buffer.append(it.characters()[0]);
                it.advance(1);
            } while (!it.atBreak());
            buffer.clear();
        }
    }

done:
    Range result = r;
    if (!found) {
        result.collapse(!forward);
    } else {
        CharacterIterator it(r);
        it.advance(rangeEnd.characterOffset() - buffer.length());
        result.setStart(it.position().startContainer(), it.position().startOffset());
        it.advance(buffer.length() - 1);
        result.setEnd(it.position().endContainer(), it.position().endOffset());
    }
    return result;
}

}
