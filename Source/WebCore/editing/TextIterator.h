/*
 * Copyright (C) 2004, 2006, 2009, 2014 Apple Inc. All rights reserved.
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

#ifndef TextIterator_h
#define TextIterator_h

// FIXME: Move each iterator class into a separate header file.

#include "FindOptions.h"
#include "Range.h"
#include "TextIteratorBehavior.h"
#include <wtf/Vector.h>
#include <wtf/text/StringView.h>

namespace WebCore {

class InlineTextBox;
class RenderText;
class RenderTextFragment;

String plainText(const Range*, TextIteratorBehavior = TextIteratorDefaultBehavior, bool isDisplayString = false);
String plainTextReplacingNoBreakSpace(const Range*, TextIteratorBehavior = TextIteratorDefaultBehavior, bool isDisplayString = false);
PassRefPtr<Range> findPlainText(const Range&, const String&, FindOptions);

// FIXME: Move this somewhere else in the editing directory. It doesn't belong here.
bool isRendererReplacedElement(RenderObject*);

class BitStack {
public:
    BitStack();
    ~BitStack();

    void push(bool);
    void pop();

    bool top() const;
    unsigned size() const;

private:
    unsigned m_size;
    Vector<unsigned, 1> m_words;
};

class TextIteratorCopyableText {
public:
    TextIteratorCopyableText()
        : m_singleCharacter(0)
        , m_offset(0)
        , m_length(0)
    {
    }

    StringView text() const { return m_singleCharacter ? StringView(&m_singleCharacter, 1) : StringView(m_string).substring(m_offset, m_length); }
    void appendToStringBuilder(StringBuilder&) const;

    void reset();
    void set(String&&);
    void set(String&&, unsigned offset, unsigned length);
    void set(UChar);

private:
    UChar m_singleCharacter;
    String m_string;
    unsigned m_offset;
    unsigned m_length;
};

// Iterates through the DOM range, returning all the text, and 0-length boundaries
// at points where replaced elements break up the text flow. The text is delivered in
// the chunks it's already stored in, to avoid copying any text.

class TextIterator {
public:
    explicit TextIterator(const Range*, TextIteratorBehavior = TextIteratorDefaultBehavior);
    ~TextIterator();

    bool atEnd() const { return !m_positionNode; }
    void advance();

    StringView text() const { ASSERT(!atEnd()); return m_text; }
    PassRefPtr<Range> range() const;
    Node* node() const;

    const TextIteratorCopyableText& copyableText() const { ASSERT(!atEnd()); return m_copyableText; }
    void appendTextToStringBuilder(StringBuilder& builder) const { copyableText().appendToStringBuilder(builder); }

    static int rangeLength(const Range*, bool spacesForReplacedElements = false);
    static PassRefPtr<Range> rangeFromLocationAndLength(ContainerNode* scope, int rangeLocation, int rangeLength, bool spacesForReplacedElements = false);
    static bool getLocationAndLengthFromRange(Node* scope, const Range*, size_t& location, size_t& length);
    static PassRefPtr<Range> subrange(Range* entireRange, int characterOffset, int characterCount);

private:
    void exitNode();
    bool shouldRepresentNodeOffsetZero();
    bool shouldEmitSpaceBeforeAndAfterNode(Node&);
    void representNodeOffsetZero();
    bool handleTextNode();
    bool handleReplacedElement();
    bool handleNonTextNode();
    void handleTextBox();
    void handleTextNodeFirstLetter(RenderTextFragment&);
    void emitCharacter(UChar, Node& characterNode, Node* offsetBaseNode, int textStartOffset, int textEndOffset);
    void emitText(Text& textNode, RenderText&, int textStartOffset, int textEndOffset);

    const TextIteratorBehavior m_behavior;

    // Current position, not necessarily of the text being returned, but position as we walk through the DOM tree.
    Node* m_node;
    int m_offset;
    bool m_handledNode;
    bool m_handledChildren;
    BitStack m_fullyClippedStack;

    // The range.
    Node* m_startContainer;
    int m_startOffset;
    Node* m_endContainer;
    int m_endOffset;
    Node* m_pastEndNode;

    // The current text and its position, in the form to be returned from the iterator.
    Node* m_positionNode;
    mutable Node* m_positionOffsetBaseNode;
    mutable int m_positionStartOffset;
    mutable int m_positionEndOffset;
    TextIteratorCopyableText m_copyableText;
    StringView m_text;

    // Used when there is still some pending text from the current node; when these are false and null, we go back to normal iterating.
    bool m_needsAnotherNewline;
    InlineTextBox* m_textBox;

    // Used when iterating over :first-letter text to save pointer to remaining text box.
    InlineTextBox* m_remainingTextBox;

    // Used to point to RenderText object for :first-letter.
    RenderText* m_firstLetterText;

    // Used to do the whitespace collapsing logic.
    Text* m_lastTextNode;
    bool m_lastTextNodeEndedWithCollapsedSpace;
    UChar m_lastCharacter;

    // Used when text boxes are out of order (Hebrew/Arabic with embedded LTR text)
    Vector<InlineTextBox*> m_sortedTextBoxes;
    size_t m_sortedTextBoxesPosition;

    // Used when deciding whether to emit a "positioning" (e.g. newline) before any other content
    bool m_hasEmitted;

    // Used when deciding text fragment created by :first-letter should be looked into.
    bool m_handledFirstLetter;
};

// Iterates through the DOM range, returning all the text, and 0-length boundaries
// at points where replaced elements break up the text flow. The text comes back in
// chunks so as to optimize for performance of the iteration.
class SimplifiedBackwardsTextIterator {
public:
    explicit SimplifiedBackwardsTextIterator(const Range&, TextIteratorBehavior = TextIteratorDefaultBehavior);

    bool atEnd() const { return !m_positionNode; }
    void advance();

    StringView text() const { ASSERT(!atEnd()); return m_text; }
    PassRefPtr<Range> range() const;
    Node* node() const { ASSERT(!atEnd()); return m_node; }

private:
    void exitNode();
    bool handleTextNode();
    RenderText* handleFirstLetter(int& startOffset, int& offsetInNode);
    bool handleReplacedElement();
    bool handleNonTextNode();
    void emitCharacter(UChar, Node&, int startOffset, int endOffset);
    bool advanceRespectingRange(Node*);

    const TextIteratorBehavior m_behavior;

    // Current position, not necessarily of the text being returned, but position as we walk through the DOM tree.
    Node* m_node;
    int m_offset;
    bool m_handledNode;
    bool m_handledChildren;
    BitStack m_fullyClippedStack;

    // The range.
    Node* m_startContainer;
    int m_startOffset;
    Node* m_endContainer;
    int m_endOffset;
    
    // The current text and its position, in the form to be returned from the iterator.
    Node* m_positionNode;
    int m_positionStartOffset;
    int m_positionEndOffset;
    TextIteratorCopyableText m_copyableText;
    StringView m_text;

    // Used to do the whitespace logic.
    Text* m_lastTextNode;
    UChar m_lastCharacter;

    // Whether m_node has advanced beyond the iteration range (i.e. m_startContainer).
    bool m_havePassedStartContainer;

    // Should handle first-letter renderer in the next call to handleTextNode.
    bool m_shouldHandleFirstLetter;
};

// Builds on the text iterator, adding a character position so we can walk one
// character at a time, or faster, as needed. Useful for searching.
class CharacterIterator {
public:
    explicit CharacterIterator(const Range&, TextIteratorBehavior = TextIteratorDefaultBehavior);
    
    bool atEnd() const { return m_underlyingIterator.atEnd(); }
    void advance(int numCharacters);
    
    StringView text() const { return m_underlyingIterator.text().substring(m_runOffset); }
    PassRefPtr<Range> range() const;

    bool atBreak() const { return m_atBreak; }
    int characterOffset() const { return m_offset; }

private:
    TextIterator m_underlyingIterator;

    int m_offset;
    int m_runOffset;
    bool m_atBreak;
};
    
class BackwardsCharacterIterator {
public:
    explicit BackwardsCharacterIterator(const Range&);

    bool atEnd() const { return m_underlyingIterator.atEnd(); }
    void advance(int numCharacters);

    PassRefPtr<Range> range() const;

private:
    SimplifiedBackwardsTextIterator m_underlyingIterator;

    int m_offset;
    int m_runOffset;
    bool m_atBreak;
};

// Similar to the TextIterator, except that the chunks of text returned are "well behaved", meaning
// they never split up a word. This is useful for spell checking and perhaps one day for searching as well.
class WordAwareIterator {
public:
    explicit WordAwareIterator(const Range&);

    bool atEnd() const { return !m_didLookAhead && m_underlyingIterator.atEnd(); }
    void advance();

    StringView text() const;

private:
    TextIterator m_underlyingIterator;

    // Text from the previous chunk from the text iterator.
    TextIteratorCopyableText m_previousText;

    // Many chunks from text iterator concatenated.
    Vector<UChar> m_buffer;
    
    // Did we have to look ahead in the text iterator to confirm the current chunk?
    bool m_didLookAhead;
};

}

#endif
