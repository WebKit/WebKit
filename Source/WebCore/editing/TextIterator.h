/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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

#include "CharacterRange.h"
#include "FindOptions.h"
#include "LayoutIntegrationRunIterator.h"
#include "SimpleRange.h"
#include "TextIteratorBehavior.h"
#include <wtf/Vector.h>

namespace WebCore {

class RenderTextFragment;

// Character ranges based on characters from the text iterator.
WEBCORE_EXPORT uint64_t characterCount(const SimpleRange&, TextIteratorBehavior = TextIteratorDefaultBehavior);
CharacterRange characterRange(const BoundaryPoint& start, const SimpleRange&, TextIteratorBehavior = TextIteratorDefaultBehavior);
CharacterRange characterRange(const SimpleRange& scope, const SimpleRange&, TextIteratorBehavior = TextIteratorDefaultBehavior);
BoundaryPoint resolveCharacterLocation(const SimpleRange& scope, uint64_t, TextIteratorBehavior = TextIteratorDefaultBehavior);
WEBCORE_EXPORT SimpleRange resolveCharacterRange(const SimpleRange& scope, CharacterRange, TextIteratorBehavior = TextIteratorDefaultBehavior);

// Text from the text iterator.
WEBCORE_EXPORT String plainText(const SimpleRange&, TextIteratorBehavior = TextIteratorDefaultBehavior, bool isDisplayString = false);
WEBCORE_EXPORT bool hasAnyPlainText(const SimpleRange&, TextIteratorBehavior = TextIteratorDefaultBehavior);
WEBCORE_EXPORT String plainTextReplacingNoBreakSpace(const SimpleRange&, TextIteratorBehavior = TextIteratorDefaultBehavior, bool isDisplayString = false);

// Find within the document, based on the text from the text iterator.
SimpleRange findPlainText(const SimpleRange&, const String&, FindOptions);
WEBCORE_EXPORT SimpleRange findClosestPlainText(const SimpleRange&, const String&, FindOptions, uint64_t targetCharacterOffset);
bool containsPlainText(const String& document, const String&, FindOptions); // Lets us use the search algorithm on a string.
WEBCORE_EXPORT String foldQuoteMarks(const String&);

// FIXME: Move this somewhere else in the editing directory. It doesn't belong in the header with TextIterator.
bool isRendererReplacedElement(RenderObject*);

// FIXME: Move each iterator class into a separate header file.

class BitStack {
public:
    void push(bool);
    void pop();
    bool top() const;

private:
    unsigned m_size { 0 };
    Vector<unsigned, 1> m_words;
};

class TextIteratorCopyableText {
public:
    StringView text() const { return m_singleCharacter ? StringView(&m_singleCharacter, 1) : StringView(m_string).substring(m_offset, m_length); }
    void appendToStringBuilder(StringBuilder&) const;

    void reset();
    void set(String&&);
    void set(String&&, unsigned offset, unsigned length);
    void set(UChar);

private:
    UChar m_singleCharacter { 0 };
    String m_string;
    unsigned m_offset { 0 };
    unsigned m_length { 0 };
};

// Iterates through the DOM range, returning all the text, and 0-length boundaries
// at points where replaced elements break up the text flow. The text is delivered in
// the chunks it's already stored in, to avoid copying any text.

class TextIterator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT explicit TextIterator(const SimpleRange&, TextIteratorBehavior = TextIteratorDefaultBehavior);
    WEBCORE_EXPORT ~TextIterator();

    bool atEnd() const { return !m_positionNode; }
    WEBCORE_EXPORT void advance();

    StringView text() const { ASSERT(!atEnd()); return m_text; }
    WEBCORE_EXPORT SimpleRange range() const;
    WEBCORE_EXPORT Node* node() const;

    const TextIteratorCopyableText& copyableText() const { ASSERT(!atEnd()); return m_copyableText; }
    void appendTextToStringBuilder(StringBuilder& builder) const { copyableText().appendToStringBuilder(builder); }

private:
    void init();
    void exitNode(Node*);
    bool shouldRepresentNodeOffsetZero();
    bool shouldEmitSpaceBeforeAndAfterNode(Node&);
    void representNodeOffsetZero();
    bool handleTextNode();
    bool handleReplacedElement();
    bool handleNonTextNode();
    void handleTextRun();
    void handleTextNodeFirstLetter(RenderTextFragment&);
    void emitCharacter(UChar, Node& characterNode, Node* offsetBaseNode, int textStartOffset, int textEndOffset);
    void emitText(Text& textNode, RenderText&, int textStartOffset, int textEndOffset);

    Node* baseNodeForEmittingNewLine() const;

    const TextIteratorBehavior m_behavior { TextIteratorDefaultBehavior };

    // Current position, not necessarily of the text being returned, but position as we walk through the DOM tree.
    Node* m_node { nullptr };
    int m_offset { 0 };
    bool m_handledNode { false };
    bool m_handledChildren { false };
    BitStack m_fullyClippedStack;

    // The range.
    Node* m_startContainer { nullptr };
    int m_startOffset { 0 };
    Node* m_endContainer { nullptr };
    int m_endOffset { 0 };
    Node* m_pastEndNode { nullptr };

    // The current text and its position, in the form to be returned from the iterator.
    Node* m_positionNode { nullptr };
    mutable Node* m_positionOffsetBaseNode { nullptr };
    mutable int m_positionStartOffset { 0 };
    mutable int m_positionEndOffset { 0 };
    TextIteratorCopyableText m_copyableText;
    StringView m_text;

    // Used when there is still some pending text from the current node; when these are false and null, we go back to normal iterating.
    Node* m_nodeForAdditionalNewline { nullptr };
    LayoutIntegration::TextRunIterator m_textRun;

    // Used when iterating over :first-letter text to save pointer to remaining text box.
    LayoutIntegration::TextRunIterator m_remainingTextRun;

    // Used to point to RenderText object for :first-letter.
    RenderText* m_firstLetterText { nullptr };

    // Used to do the whitespace collapsing logic.
    Text* m_lastTextNode { nullptr };
    bool m_lastTextNodeEndedWithCollapsedSpace { false };
    UChar m_lastCharacter { 0 };

    // Used when deciding whether to emit a "positioning" (e.g. newline) before any other content
    bool m_hasEmitted { false };

    // Used when deciding text fragment created by :first-letter should be looked into.
    bool m_handledFirstLetter { false };
};

// Iterates through the DOM range, returning all the text, and 0-length boundaries
// at points where replaced elements break up the text flow. The text comes back in
// chunks so as to optimize for performance of the iteration.
class SimplifiedBackwardsTextIterator {
public:
    WEBCORE_EXPORT explicit SimplifiedBackwardsTextIterator(const SimpleRange&);

    bool atEnd() const { return !m_positionNode; }
    WEBCORE_EXPORT void advance();

    StringView text() const { ASSERT(!atEnd()); return m_text; }
    WEBCORE_EXPORT SimpleRange range() const;
    Node* node() const { ASSERT(!atEnd()); return m_node; }

private:
    void exitNode();
    bool handleTextNode();
    RenderText* handleFirstLetter(int& startOffset, int& offsetInNode);
    bool handleReplacedElement();
    bool handleNonTextNode();
    void emitCharacter(UChar, Node&, int startOffset, int endOffset);
    bool advanceRespectingRange(Node*);

    const TextIteratorBehavior m_behavior { TextIteratorDefaultBehavior };

    // Current position, not necessarily of the text being returned, but position as we walk through the DOM tree.
    Node* m_node { nullptr };
    int m_offset { 0 };
    bool m_handledNode { false };
    bool m_handledChildren { false };
    BitStack m_fullyClippedStack;

    // The range.
    Node* m_startContainer { nullptr };
    int m_startOffset { 0 };
    Node* m_endContainer { nullptr };
    int m_endOffset { 0 };
    
    // The current text and its position, in the form to be returned from the iterator.
    Node* m_positionNode { nullptr };
    int m_positionStartOffset { 0 };
    int m_positionEndOffset { 0 };
    TextIteratorCopyableText m_copyableText;
    StringView m_text;

    // Used to do the whitespace logic.
    Text* m_lastTextNode { nullptr };
    UChar m_lastCharacter { 0 };

    // Whether m_node has advanced beyond the iteration range (i.e. m_startContainer).
    bool m_havePassedStartContainer { false };

    // Should handle first-letter renderer in the next call to handleTextNode.
    bool m_shouldHandleFirstLetter { false };
};

// Builds on the text iterator, adding a character position so we can walk one
// character at a time, or faster, as needed. Useful for searching.
class CharacterIterator {
public:
    WEBCORE_EXPORT explicit CharacterIterator(const SimpleRange&, TextIteratorBehavior = TextIteratorDefaultBehavior);
    
    bool atEnd() const { return m_underlyingIterator.atEnd(); }
    WEBCORE_EXPORT void advance(int numCharacters);
    
    StringView text() const { return m_underlyingIterator.text().substring(m_runOffset); }
    WEBCORE_EXPORT SimpleRange range() const;

    bool atBreak() const { return m_atBreak; }
    unsigned characterOffset() const { return m_offset; }

private:
    TextIterator m_underlyingIterator;

    unsigned m_offset { 0 };
    unsigned m_runOffset { 0 };
    bool m_atBreak { true };
};
    
class BackwardsCharacterIterator {
public:
    explicit BackwardsCharacterIterator(const SimpleRange&);

    bool atEnd() const { return m_underlyingIterator.atEnd(); }
    void advance(int numCharacters);

    SimpleRange range() const;

private:
    SimplifiedBackwardsTextIterator m_underlyingIterator;

    unsigned m_offset { 0 };
    unsigned m_runOffset { 0 };
    bool m_atBreak { true };
};

// Similar to the TextIterator, except that the chunks of text returned are "well behaved", meaning
// they never split up a word. This is useful for spell checking and perhaps one day for searching as well.
class WordAwareIterator {
public:
    explicit WordAwareIterator(const SimpleRange&);

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
    bool m_didLookAhead { true };
};

inline CharacterRange characterRange(const BoundaryPoint& start, const SimpleRange& range, TextIteratorBehavior behavior)
{
    return { characterCount({ start, range.start }, behavior), characterCount(range, behavior) };
}

inline CharacterRange characterRange(const SimpleRange& scope, const SimpleRange& range, TextIteratorBehavior behavior)
{
    return characterRange(scope.start, range, behavior);
}

inline BoundaryPoint resolveCharacterLocation(const SimpleRange& scope, uint64_t location, TextIteratorBehavior behavior)
{
    return resolveCharacterRange(scope, { location, 0 }, behavior).start;
}

} // namespace WebCore
