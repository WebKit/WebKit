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

#ifndef __khtml_text_operations_h__
#define __khtml_text_operations_h__

#include <qstring.h>
#include "dom/dom2_range.h"

namespace DOM {
class NodeImpl;
}

// FIXME: This class should probably use the render tree and not the DOM tree, since elements could
// be hidden using CSS, or additional generated content could be added.  For now, we just make sure
// text objects walk their renderers' InlineTextBox objects, so that we at least get the whitespace 
// stripped out properly and obey CSS visibility for text runs.

namespace khtml {

class InlineTextBox;

// General utillity functions

QString plainText(const DOM::Range &);
DOM::Range findPlainText(const DOM::Range &, const QString &, bool forward, bool caseSensitive);


// Iterates through the DOM range, returning all the text, and 0-length boundaries
// at points where replaced elements break up the text flow.  The text comes back in
// chunks so as to optimize for performance of the iteration.
class TextIterator
{
public:
    TextIterator();
    explicit TextIterator(const DOM::Range &);
    
    bool atEnd() const { return !m_positionNode; }
    void advance();
    
    long length() const { return m_textLength; }
    const QChar *characters() const { return m_textCharacters; }
    
    DOM::Range range() const;
        
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

// Iterates through the DOM range, returning all the text, and 0-length boundaries
// at points where replaced elements break up the text flow.  The text comes back in
// chunks so as to optimize for performance of the iteration.
class SimplifiedBackwardsTextIterator
{
public:
    SimplifiedBackwardsTextIterator();
    explicit SimplifiedBackwardsTextIterator(const DOM::Range &);
    
    bool atEnd() const { return !m_positionNode; }
    void advance();
    
    long length() const { return m_textLength; }
    const QChar *characters() const { return m_textCharacters; }
    
    DOM::Range range() const;
        
private:
    void exitNode();
    bool handleTextNode();
    bool handleReplacedElement();
    bool handleNonTextNode();
    void emitCharacter(QChar, DOM::NodeImpl *Node, long startOffset, long endOffset);
    
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
    
    long length() const { return m_textIterator.length() - m_runOffset; }
    const QChar *characters() const { return m_textIterator.characters() + m_runOffset; }
    QString string(long numChars);
    
    long characterOffset() const { return m_offset; }
    DOM::Range range() const;
        
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
    explicit WordAwareIterator(const DOM::Range &r);

    bool atEnd() const { return !m_didLookAhead && m_textIterator.atEnd(); }
    void advance();
    
    long length() const;
    const QChar *characters() const;
    
    // Range of the text we're currently returning
    DOM::Range range() const { return m_range; }

private:
    // text from the previous chunk from the textIterator
    const QChar *m_previousText;
    long m_previousLength;

    // many chunks from textIterator concatenated
    QString m_buffer;
    
    // Did we have to look ahead in the textIterator to confirm the current chunk?
    bool m_didLookAhead;

    DOM::Range m_range;

    TextIterator m_textIterator;
};

}

#endif
