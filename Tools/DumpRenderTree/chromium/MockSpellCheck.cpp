/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MockSpellCheck.h"

#include "WebString.h"
#include <wtf/ASCIICType.h>
#include <wtf/Assertions.h>
#include <wtf/text/WTFString.h>

using namespace WebKit;

MockSpellCheck::MockSpellCheck()
    : m_initialized(false) {}

MockSpellCheck::~MockSpellCheck() {}

static bool isNotASCIIAlpha(UChar ch) { return !isASCIIAlpha(ch); }

bool MockSpellCheck::spellCheckWord(const WebString& text, int* misspelledOffset, int* misspelledLength)
{
    ASSERT(misspelledOffset);
    ASSERT(misspelledLength);

    // Initialize this spellchecker.
    initializeIfNeeded();

    // Reset the result values as our spellchecker does.
    *misspelledOffset = 0;
    *misspelledLength = 0;

    // Convert to a String because we store String instances in
    // m_misspelledWords and WebString has no find().
    const WTF::String stringText(text.data(), text.length());

    // Extract the first possible English word from the given string.
    // The given string may include non-ASCII characters or numbers. So, we
    // should filter out such characters before start looking up our
    // misspelled-word table.
    // (This is a simple version of our SpellCheckWordIterator class.)
    // If the given string doesn't include any ASCII characters, we can treat the
    // string as valid one.
    // Unfortunately, This implementation splits a contraction, i.e. "isn't" is
    // split into two pieces "isn" and "t". This is OK because webkit tests
    // don't have misspelled contractions.
    int wordOffset = stringText.find(isASCIIAlpha);
    if (wordOffset == -1)
        return true;
    int wordEnd = stringText.find(isNotASCIIAlpha, wordOffset);
    int wordLength = wordEnd == -1 ? stringText.length() - wordOffset : wordEnd - wordOffset;

    // Look up our misspelled-word table to check if the extracted word is a
    // known misspelled word, and return the offset and the length of the
    // extracted word if this word is a known misspelled word.
    // (See the comment in MockSpellCheck::initializeIfNeeded() why we use a
    // misspelled-word table.)
    WTF::String word = stringText.substring(wordOffset, wordLength);
    if (!m_misspelledWords.contains(word))
        return true;

    *misspelledOffset = wordOffset;
    *misspelledLength = wordLength;
    return false;
}

void MockSpellCheck::fillSuggestionList(const WebString& word, Vector<WebString>* suggestions)
{
    if (word == WebString::fromUTF8("wellcome"))
        suggestions->append(WebString::fromUTF8("welcome"));
}

bool MockSpellCheck::initializeIfNeeded()
{
    // Exit if we have already initialized this object.
    if (m_initialized)
        return false;

    // Create a table that consists of misspelled words used in WebKit layout
    // tests.
    // Since WebKit layout tests don't have so many misspelled words as
    // well-spelled words, it is easier to compare the given word with misspelled
    // ones than to compare with well-spelled ones.
    static const char* misspelledWords[] = {
        // These words are known misspelled words in webkit tests.
        // If there are other misspelled words in webkit tests, please add them in
        // this array.
        "foo",
        "Foo",
        "baz",
        "fo",
        "LibertyF",
        "chello",
        "xxxtestxxx",
        "XXxxx",
        "Textx",
        "blockquoted",
        "asd",
        "Lorem",
        "Nunc",
        "Curabitur",
        "eu",
        "adlj",
        "adaasj",
        "sdklj",
        "jlkds",
        "jsaada",
        "jlda",
        "zz",
        "contentEditable",
        // The following words are used by unit tests.
        "ifmmp",
        "qwertyuiopasd",
        "qwertyuiopasdf",
        "wellcome"
    };

    m_misspelledWords.clear();
    for (size_t i = 0; i < arraysize(misspelledWords); ++i)
        m_misspelledWords.add(WTF::String::fromUTF8(misspelledWords[i]), false);

    // Mark as initialized to prevent this object from being initialized twice
    // or more.
    m_initialized = true;

    // Since this MockSpellCheck class doesn't download dictionaries, this
    // function always returns false.
    return false;
}
