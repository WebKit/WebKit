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

#include "TestCommon.h"
#include <public/WebCString.h>

using namespace WebKit;
using namespace std;

namespace WebTestRunner {

namespace {

void append(WebVector<WebString>* data, const WebString& item)
{
    WebVector<WebString> result(data->size() + 1);
    for (size_t i = 0; i < data->size(); ++i)
        result[i] = (*data)[i];
    result[data->size()] = item;
    data->swap(result);
}

}

MockSpellCheck::MockSpellCheck()
    : m_initialized(false) { }

MockSpellCheck::~MockSpellCheck() { }

bool MockSpellCheck::spellCheckWord(const WebString& text, int* misspelledOffset, int* misspelledLength)
{
    WEBKIT_ASSERT(misspelledOffset);
    WEBKIT_ASSERT(misspelledLength);

    // Initialize this spellchecker.
    initializeIfNeeded();

    // Reset the result values as our spellchecker does.
    *misspelledOffset = 0;
    *misspelledLength = 0;

    // Convert to a string16 because we store string16 instances in
    // m_misspelledWords and WebString has no find().
    string16 stringText = text;
    int skippedLength = 0;

    while (!stringText.empty()) {
        // Extract the first possible English word from the given string.
        // The given string may include non-ASCII characters or numbers. So, we
        // should filter out such characters before start looking up our
        // misspelled-word table.
        // (This is a simple version of our SpellCheckWordIterator class.)
        // If the given string doesn't include any ASCII characters, we can treat the
        // string as valid one.
        string16::iterator firstChar = find_if(stringText.begin(), stringText.end(), isASCIIAlpha);
        if (firstChar == stringText.end())
            return true;
        int wordOffset = distance(stringText.begin(), firstChar);
        int maxWordLength = static_cast<int>(stringText.length()) - wordOffset;
        int wordLength;
        string16 word;

        // Look up our misspelled-word table to check if the extracted word is a
        // known misspelled word, and return the offset and the length of the
        // extracted word if this word is a known misspelled word.
        // (See the comment in MockSpellCheck::initializeIfNeeded() why we use a
        // misspelled-word table.)
        for (size_t i = 0; i < m_misspelledWords.size(); ++i) {
            wordLength = static_cast<int>(m_misspelledWords.at(i).length()) > maxWordLength ? maxWordLength : static_cast<int>(m_misspelledWords.at(i).length());
            word = stringText.substr(wordOffset, wordLength);
            if (word == m_misspelledWords.at(i) && (static_cast<int>(stringText.length()) == wordOffset + wordLength || isNotASCIIAlpha(stringText[wordOffset + wordLength]))) {
                *misspelledOffset = wordOffset + skippedLength;
                *misspelledLength = wordLength;
                break;
            }
        }

        if (*misspelledLength > 0)
            break;

        string16::iterator lastChar = find_if(stringText.begin() + wordOffset, stringText.end(), isNotASCIIAlpha);
        if (lastChar == stringText.end())
            wordLength = static_cast<int>(stringText.length()) - wordOffset;
        else
            wordLength = distance(firstChar, lastChar);

        WEBKIT_ASSERT(0 < wordOffset + wordLength);
        stringText = stringText.substr(wordOffset + wordLength);
        skippedLength += wordOffset + wordLength;
    }

    return false;
}

bool MockSpellCheck::hasInCache(const WebString& word)
{
    return word == WebString::fromUTF8("Spell cheher. Is it broken?") || word == WebString::fromUTF8("Spell cheher.\x007F");
}

void MockSpellCheck::fillSuggestionList(const WebString& word, WebVector<WebString>* suggestions)
{
    if (word == WebString::fromUTF8("wellcome"))
        append(suggestions, WebString::fromUTF8("welcome"));
    else if (word == WebString::fromUTF8("upper case"))
        append(suggestions, WebString::fromUTF8("uppercase"));
    else if (word == WebString::fromUTF8("cheher"))
        append(suggestions, WebString::fromUTF8("checker"));
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
        "upper case",
        "wellcome",
        "cheher"
    };

    m_misspelledWords.clear();
    for (size_t i = 0; i < arraysize(misspelledWords); ++i)
        m_misspelledWords.push_back(string16(misspelledWords[i], misspelledWords[i] + strlen(misspelledWords[i])));

    // Mark as initialized to prevent this object from being initialized twice
    // or more.
    m_initialized = true;

    // Since this MockSpellCheck class doesn't download dictionaries, this
    // function always returns false.
    return false;
}

}
