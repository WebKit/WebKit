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

#ifndef TextIterator_h
#define TextIterator_h

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
PassRefPtr<Range> findPlainText(const Range*, const String&, FindOptions);

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

// Iterates through the DOM range, returning all the text, and 0-length boundaries
// at points where replaced elements break up the text flow.  The text comes back in
// chunks so as to optimize for performance of the iteration.

class TextIterator {
public:
    explicit TextIterator(const Range*, TextIteratorBehavior = TextIteratorDefaultBehavior);
    ~TextIterator();

    bool atEnd() const { return !m_positionNode || m_shouldStop; }
    void advance();
    
    StringView text() const { return m_textCharacters ? StringView(m_textCharacters, m_textLength) : StringView(m_text).substring(m_positionStartOffset, m_textLength); }
    int length() const { return m_textLength; }

    const UChar* deprecatedTextIteratorCharacters() const { return m_textCharacters ? m_textCharacters : m_text.deprecatedCharacters() + m_positionStartOffset; }

    UChar characterAt(unsigned index) const;
    void appendTextToStringBuilder(StringBuilder&) const;
    
    PassRefPtr<Range> range() const;
    Node* node() const;
     
    static int rangeLength(const Range*, bool spacesForReplacedElements = false);
    static PassRefPtr<Range> rangeFromLocationAndLength(ContainerNode* scope, int rangeLocation, int rangeLength, bool spacesForReplacedElements = false);
    static bool getLocationAndLengthFromRange(Node* scope, const Range*, size_t& location, size_t& length);
    static PassRefPtr<Range> subrange(Range* entireRange, int characterOffset, int characterCount);
    
private:
    void exitNode();
    bool shouldRepresentNodeOffsetZero();
    bool shouldEmitSpaceBeforeAndAfterNode(Node*);
    void representNodeOffsetZero();
    bool handleTextNode();
    bool handleReplacedElement();
    bool handleNonTextNode();
    void handleTextBox();
    void handleTextNodeFirstLetter(RenderTextFragment*);
    bool hasVisibleTextNode(RenderText*);
    void emitCharacter(UChar, Node* textNode, Node* offsetBaseNode, int textStartOffset, int textEndOffset);
    void emitText(Node* textNode, RenderObject* renderObject, int textStartOffset, int textEndOffset);
    void emitText(Node* textNode, int textStartOffset, int textEndOffset);
    
    // Current position, not necessarily of the text being returned, but position
    // as we walk through the DOM tree.
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
    const UChar* m_textCharacters; // If null, then use m_text for character data.
    int m_textLength;
    // Hold string m_textCharacters points to so we ensure it won't be deleted.
    String m_text;

    // Used when there is still some pending text from the current node; when these
    // are false and 0, we go back to normal iterating.
    bool m_needsAnotherNewline;
    InlineTextBox* m_textBox;
    // Used when iteration over :first-letter text to save pointer to
    // remaining text box.
    InlineTextBox* m_remainingTextBox;
    // Used to point to RenderText object for :first-letter.
    RenderText *m_firstLetterText;
    
    // Used to do the whitespace collapsing logic.
    Node* m_lastTextNode;    
    bool m_lastTextNodeEndedWithCollapsedSpace;
    UChar m_lastCharacter;
    
    // Used for whitespace characters that aren't in the DOM, so we can point at them.
    UChar m_singleCharacterBuffer;
    
    // Used when text boxes are out of order (Hebrew/Arabic w/ embeded LTR text)
    Vector<InlineTextBox*> m_sortedTextBoxes;
    size_t m_sortedTextBoxesPosition;
    
    // Used when deciding whether to emit a "positioning" (e.g. newline) before any other content
    bool m_hasEmitted;
    
    bool m_emitsCharactersBetweenAllVisiblePositions;
    bool m_entersTextControls;
    bool m_emitsTextWithoutTranscoding;
    bool m_emitsOriginalText;

    // Used when deciding text fragment created by :first-letter should be looked into.
    bool m_handledFirstLetter;

    bool m_ignoresStyleVisibility;
    bool m_emitsObjectReplacementCharacters;
    bool m_stopsOnFormControls;

    // Used when m_stopsOnFormControls is set to determine if the iterator should keep advancing.
    bool m_shouldStop;

    bool m_emitsImageAltText;
    bool m_hasNodesFollowing;
};

// Iterates through the DOM range, returning all the text, and 0-length boundaries
// at points where replaced elements break up the text flow. The text comes back in
// chunks so as to optimize for performance of the iteration.
class SimplifiedBackwardsTextIterator {
public:
    explicit SimplifiedBackwardsTextIterator(const Range*, TextIteratorBehavior = TextIteratorDefaultBehavior);
    
    bool atEnd() const { return !m_positionNode || m_shouldStop; }
    void advance();

    Node* node() const { return m_node; }

    StringView text() const { return StringView(m_textCharacters, m_textLength); }
    int length() const { return m_textLength; }

    PassRefPtr<Range> range() const;
        
private:
    void exitNode();
    bool handleTextNode();
    RenderText* handleFirstLetter(int& startOffset, int& offsetInNode);
    bool handleReplacedElement();
    bool handleNonTextNode();
    void emitCharacter(UChar, Node*, int startOffset, int endOffset);
    bool advanceRespectingRange(Node*);

    // Current position, not necessarily of the text being returned, but position
    // as we walk through the DOM tree.
    Node* m_node;
    int m_offset;
    bool m_handledNode;
    bool m_handledChildren;
    BitStack m_fullyClippedStack;

    // End of the range.
    Node* m_startNode;
    int m_startOffset;
    // Start of the range.
    Node* m_endNode;
    int m_endOffset;
    
    // The current text and its position, in the form to be returned from the iterator.
    Node* m_positionNode;
    int m_positionStartOffset;
    int m_positionEndOffset;
    const UChar* m_textCharacters;
    int m_textLength;

    // Used to do the whitespace logic.
    Node* m_lastTextNode;    
    UChar m_lastCharacter;
    
    // Used for whitespace characters that aren't in the DOM, so we can point at them.
    UChar m_singleCharacterBuffer;

    // Whether m_node has advanced beyond the iteration range (i.e. m_startNode).
    bool m_havePassedStartNode;

    // Should handle first-letter renderer in the next call to handleTextNode.
    bool m_shouldHandleFirstLetter;

    // Used when the iteration should stop if form controls are reached.
    bool m_stopsOnFormControls;

    // Used when m_stopsOnFormControls is set to determine if the iterator should keep advancing.
    bool m_shouldStop;

    // Used in pasting inside password field.
    bool m_emitsOriginalText;
};

// Builds on the text iterator, adding a character position so we can walk one
// character at a time, or faster, as needed. Useful for searching.
class CharacterIterator {
public:
    explicit CharacterIterator(const Range*, TextIteratorBehavior = TextIteratorDefaultBehavior);
    
    void advance(int numCharacters);
    
    bool atBreak() const { return m_atBreak; }
    bool atEnd() const { return m_textIterator.atEnd(); }
    
    StringView text() const { return m_textIterator.text().substring(m_runOffset); }
    int length() const { return m_textIterator.length() - m_runOffset; }

    String string(int numCharacters);
    
    int characterOffset() const { return m_offset; }
    PassRefPtr<Range> range() const;

private:
    int m_offset;
    int m_runOffset;
    bool m_atBreak;
    
    TextIterator m_textIterator;
};
    
class BackwardsCharacterIterator {
public:
    explicit BackwardsCharacterIterator(const Range*, TextIteratorBehavior = TextIteratorDefaultBehavior);

    void advance(int);

    bool atEnd() const { return m_textIterator.atEnd(); }

    PassRefPtr<Range> range() const;

private:
    int m_offset;
    int m_runOffset;
    bool m_atBreak;

    SimplifiedBackwardsTextIterator m_textIterator;
};

// Very similar to the TextIterator, except that the chunks of text returned are "well behaved", meaning
// they never split up a word. This is useful for spell checking and perhaps one day for searching as well.
class WordAwareIterator {
public:
    explicit WordAwareIterator(const Range*);
    ~WordAwareIterator();

    bool atEnd() const { return !m_didLookAhead && m_textIterator.atEnd(); }
    void advance();
    
    int length() const;

    StringView text() const;
    PassRefPtr<Range> range() const { return m_range; }

private:
    // Text from the previous chunk from the text iterator.
    StringView m_previousText;

    // Many chunks from text iterator concatenated.
    Vector<UChar> m_buffer;
    
    // Did we have to look ahead in the text iterator to confirm the current chunk?
    bool m_didLookAhead;

    RefPtr<Range> m_range;

    TextIterator m_textIterator;
};

}

#endif
