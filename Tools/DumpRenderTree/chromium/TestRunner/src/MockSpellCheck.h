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

#ifndef MockSpellCheck_h
#define MockSpellCheck_h

#include "Platform/chromium/public/WebString.h"
#include "Platform/chromium/public/WebVector.h"
#include <vector>

namespace WebTestRunner {

// A mock implementation of a spell-checker used for WebKit tests.
// This class only implements the minimal functionarities required by WebKit
// tests, i.e. this class just compares the given string with known misspelled
// words in webkit tests and mark them as missspelled.
// Even though this is sufficent for webkit tests, this class is not suitable
// for any other usages.
class MockSpellCheck {
public:
    static void fillSuggestionList(const WebKit::WebString& word, WebKit::WebVector<WebKit::WebString>* suggestions);

    MockSpellCheck();
    ~MockSpellCheck();

    // Checks the spellings of the specified text.
    // This function returns true if the text consists of valid words, and
    // returns false if it includes invalid words.
    // When the given text includes invalid words, this function sets the
    // position of the first invalid word to misspelledOffset, and the length of
    // the first invalid word to misspelledLength, respectively.
    // For example, when the given text is "   zz zz", this function sets 3 to
    // misspelledOffset and 2 to misspelledLength, respectively.
    bool spellCheckWord(const WebKit::WebString& text, int* misspelledOffset, int* misspelledLength);

    // Checks whether the specified text can be spell checked immediately using
    // the spell checker cache.
    bool hasInCache(const WebKit::WebString& text);

private:
    // Initialize the internal resources if we need to initialize it.
    // Initializing this object may take long time. To prevent from hurting
    // the performance of test_shell, we initialize this object when
    // SpellCheckWord() is called for the first time.
    // To be compliant with SpellCheck:InitializeIfNeeded(), this function
    // returns true if this object is downloading a dictionary, otherwise
    // it returns false.
    bool initializeIfNeeded();

    // A table that consists of misspelled words.
    std::vector<string16> m_misspelledWords;

    // A flag representing whether or not this object is initialized.
    bool m_initialized;
};

}

#endif // MockSpellCheck_h
