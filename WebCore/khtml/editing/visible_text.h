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

#ifndef KHTML_EDITING_VISIBLE_TEXT_H
#define KHTML_EDITING_VISIBLE_TEXT_H

#include "xml/dom2_rangeimpl.h"

#include <qstring.h>

namespace khtml {

class InlineTextBox;

// FIXME: Can't really answer this question without knowing the white-space mode.
// FIXME: Move this along with the white-space position functions above
// somewhere in the editing directory. It doesn't belong here.
inline bool isCollapsibleWhitespace(const QChar &c)
{
    switch (c.unicode()) {
        case ' ':
        case '\n':
            return true;
        default:
            return false;
    }
}

QString plainText(const DOM::RangeImpl *);
SharedPtr<DOM::RangeImpl> findPlainText(const DOM::RangeImpl *, const QString &, bool forward, bool caseSensitive);

// Iterates through the DOM range, returning all the text, and 0-length boundaries
// at points where replaced elements break up the text flow.  The text comes back in
// chunks so as to optimize for performance of the iteration.

enum IteratorKind { CONTENT = 0, RUNFINDER = 1 };

class TextIterator
{
public:
    TextIterator();
    explicit TextIterator(const DOM::RangeImpl *, IteratorKind kind = CONTENT );
    
    bool atEnd() const { return !m_positionNode; }
    void advance();
    
    long length() const { return m_textLength; }
    const QChar *characters() const { return m_textCharacters; }
    
    SharedPtr<DOM::RangeImpl> range() const;
     
    static long TextIterator::rangeLength(const DOM::RangeImpl *r);
    static DOM::RangeImpl *TextIterator::rangeFromLocationAndLength(DOM::DocumentImpl *doc, long rangeLocation, long rangeLength);
    
private:
    void exitNode();
    bool handleTextNode();
    bool handleReplacedElement();
    bool handleNonTextNode();
    void handleTextBox();
    void emitCharacter(QChar, DOM::NodeImpl *textNode, DOM::NodeImpl *offsetBaseNode, long textStartOffset, long textEndOffset);
    
    // Current position, not necessarily of the text being returned, but position
    // as we walk through the DOM tree.
    DOM::NodeImpl *m_node;
    long m_offset;
    bool m_handledNode;
    bool m_handledChildren;
    
    // End of the range.
    DOM::NodeImpl *m_endContainer;
    long m_endOffset;
    DOM::NodeImpl *m_pastEndNode;
    
    // The current text and its position, in the form to be returned from the iterator.
    DOM::NodeImpl *m_positionNode;
    mutable DOM::NodeImpl *m_positionOffsetBaseNode;
    mutable long m_positionStartOffset;
    mutable long m_positionEndOffset;
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

// Iterates through the DOM range, returning all the text, and 0-length boundaries
// at points where replaced elements break up the text flow.  The text comes back in
// chunks so as to optimize for performance of the iteration.
class SimplifiedBackwardsTextIterator
{
public:
    SimplifiedBackwardsTextIterator();
    explicit SimplifiedBackwardsTextIterator(const DOM::RangeImpl *);
    
    bool atEnd() const { return !m_positionNode; }
    void advance();
    
    long length() const { return m_textLength; }
    const QChar *characters() const { return m_textCharacters; }
    
    SharedPtr<DOM::RangeImpl> range() const;
        
private:
    void exitNode();
    bool handleTextNode();
    bool handleReplacedElement();
    bool handleNonTextNode();
    void emitCharacter(QChar, DOM::NodeImpl *Node, long startOffset, long endOffset);
    void emitNewlineForBROrText();
    
    // Current position, not necessarily of the text being returned, but position
    // as we walk through the DOM tree.
    DOM::NodeImpl *m_node;
    long m_offset;
    bool m_handledNode;
    bool m_handledChildren;
    
    // End of the range.
    DOM::NodeImpl *m_startNode;
    long m_startOffset;
    
    // The current text and its position, in the form to be returned from the iterator.
    DOM::NodeImpl *m_positionNode;
    long m_positionStartOffset;
    long m_positionEndOffset;
    const QChar *m_textCharacters;
    long m_textLength;

    // Used to do the whitespace logic.
    DOM::NodeImpl *m_lastTextNode;    
    QChar m_lastCharacter;
    
    // Used for whitespace characters that aren't in the DOM, so we can point at them.
    QChar m_singleCharacterBuffer;
};

// Builds on the text iterator, adding a character position so we can walk one
// character at a time, or faster, as needed. Useful for searching.
class CharacterIterator {
public:
    CharacterIterator();
    explicit CharacterIterator(const DOM::RangeImpl *r);
    
    void advance(long numCharacters);
    
    bool atBreak() const { return m_atBreak; }
    bool atEnd() const { return m_textIterator.atEnd(); }
    
    long length() const { return m_textIterator.length() - m_runOffset; }
    const QChar *characters() const { return m_textIterator.characters() + m_runOffset; }
    QString string(long numChars);
    
    long characterOffset() const { return m_offset; }
    SharedPtr<DOM::RangeImpl> range() const;
        
private:
    long m_offset;
    long m_runOffset;
    bool m_atBreak;
    
    TextIterator m_textIterator;
};
    
// Very similar to the TextIterator, except that the chunks of text returned are "well behaved",
// meaning they never end split up a word.  This is useful for spellcheck or (perhaps one day) searching.
class WordAwareIterator {
public:
    WordAwareIterator();
    explicit WordAwareIterator(const DOM::RangeImpl *r);

    bool atEnd() const { return !m_didLookAhead && m_textIterator.atEnd(); }
    void advance();
    
    long length() const;
    const QChar *characters() const;
    
    // Range of the text we're currently returning
    SharedPtr<DOM::RangeImpl> range() const { return m_range; }

private:
    // text from the previous chunk from the textIterator
    const QChar *m_previousText;
    long m_previousLength;

    // many chunks from textIterator concatenated
    QString m_buffer;
    
    // Did we have to look ahead in the textIterator to confirm the current chunk?
    bool m_didLookAhead;

    SharedPtr<DOM::RangeImpl> m_range;

    TextIterator m_textIterator;
};

}

#endif
