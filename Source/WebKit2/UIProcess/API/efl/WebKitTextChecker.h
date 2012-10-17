/*
 * Copyright (C) 2012 Samsung Electronics
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebKitTextChecker_h
#define WebKitTextChecker_h

#if ENABLE(SPELLCHECK)

#include "TextCheckerEnchant.h"
#include "WKArray.h"
#include "WKPage.h"
#include "WKString.h"

namespace WebKit {

// The functions mainly choose between client's implementation of spelling and the WebKit one.

// Callbacks required by WKTextChecker.
bool isContinuousSpellCheckingEnabled(const void* clientInfo);
void setContinuousSpellCheckingEnabled(bool enabled, const void* clientInfo);

uint64_t uniqueSpellDocumentTag(WKPageRef page, const void* clientInfo);
void closeSpellDocumentWithTag(uint64_t tag, const void* clientInfo);

void checkSpellingOfString(uint64_t tag, WKStringRef text, int32_t* misspellingLocation, int32_t* misspellingLength, const void* clientInfo);
WKArrayRef guessesForWord(uint64_t tag, WKStringRef word, const void* clientInfo);
void learnWord(uint64_t tag, WKStringRef word, const void* clientInfo);
void ignoreWord(uint64_t tag, WKStringRef word, const void* clientInfo);

// Enchant's helper.
Vector<String> availableSpellCheckingLanguages();
void updateSpellCheckingLanguages(const Vector<String>& languages);
Vector<String> loadedSpellCheckingLanguages();

} // namespace WebKit

#endif // ENABLE(SPELLCHECK)
#endif // WebKitTextChecker_h
