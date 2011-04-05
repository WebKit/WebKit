/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2010 Martin Robinson <mrobinson@webkit.org>
 *
 * All rights reserved.
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

#ifndef TextCheckerClientEnchant_h
#define TextCheckerClientEnchant_h

#include "TextCheckerClient.h"

typedef struct _GSList GSList;
typedef struct _WebKitWebView WebKitWebView;
typedef struct str_enchant_broker EnchantBroker;

namespace WebKit {

class TextCheckerClientEnchant : public WebCore::TextCheckerClient {
    public:
        TextCheckerClientEnchant(WebKitWebView*);
        ~TextCheckerClientEnchant();
        virtual void ignoreWordInSpellDocument(const WTF::String&);
        virtual void learnWord(const WTF::String&);
        virtual void checkSpellingOfString(const UChar*, int length, int* misspellingLocation, int* misspellingLength);
        virtual WTF::String getAutoCorrectSuggestionForMisspelledWord(const WTF::String&);
        virtual void checkGrammarOfString(const UChar*, int length, WTF::Vector<WebCore::GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength);
        virtual void getGuessesForWord(const WTF::String& word, const WTF::String& context, WTF::Vector<WTF::String>& guesses);
        virtual void requestCheckingOfString(WebCore::SpellChecker*, int, WebCore::TextCheckingTypeMask, const WTF::String&) {}

        void updateSpellCheckingLanguage(const char*);
        static void freeSpellCheckingLanguage(gpointer, gpointer);
    private:
        WebKitWebView* m_webView;
        GSList* m_enchantDicts;
        static EnchantBroker* broker;
    };
}
#endif

