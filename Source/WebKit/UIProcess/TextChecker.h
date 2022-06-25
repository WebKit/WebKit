/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "TextCheckerCompletion.h"
#include <WebCore/EditorClient.h>
#include <WebCore/TextCheckerClient.h>

namespace WebKit {

class WebPageProxy;
struct TextCheckerState;

using SpellDocumentTag = int64_t;
    
class TextChecker {
public:
    static const TextCheckerState& state();
    static bool isContinuousSpellCheckingAllowed();

    static bool setContinuousSpellCheckingEnabled(bool);
    static void setGrammarCheckingEnabled(bool);
    
    static void setTestingMode(bool);
    static bool isTestingMode();

#if PLATFORM(COCOA)
    static void setAutomaticSpellingCorrectionEnabled(bool);
    static void setAutomaticQuoteSubstitutionEnabled(bool);
    static void setAutomaticDashSubstitutionEnabled(bool);
    static void setAutomaticLinkDetectionEnabled(bool);
    static void setAutomaticTextReplacementEnabled(bool);

    static void didChangeAutomaticTextReplacementEnabled();
    static void didChangeAutomaticSpellingCorrectionEnabled();
    static void didChangeAutomaticQuoteSubstitutionEnabled();
    static void didChangeAutomaticDashSubstitutionEnabled();

    static bool isSmartInsertDeleteEnabled();
    static void setSmartInsertDeleteEnabled(bool);

    static bool substitutionsPanelIsShowing();
    static void toggleSubstitutionsPanelIsShowing();
#endif

#if PLATFORM(GTK)
    static void setSpellCheckingLanguages(const Vector<String>&);
    static Vector<String> loadedSpellCheckingLanguages();
#endif

    static void continuousSpellCheckingEnabledStateChanged(bool);
    static void grammarCheckingEnabledStateChanged(bool);
    
    static SpellDocumentTag uniqueSpellDocumentTag(WebPageProxy*);
    static void closeSpellDocumentWithTag(SpellDocumentTag);
#if USE(UNIFIED_TEXT_CHECKING)
    static Vector<WebCore::TextCheckingResult> checkTextOfParagraph(SpellDocumentTag, StringView, int32_t insertionPoint, OptionSet<WebCore::TextCheckingType>, bool initialCapitalizationEnabled);
#endif
    static void checkSpellingOfString(SpellDocumentTag, StringView text, int32_t& misspellingLocation, int32_t& misspellingLength);
    static void checkGrammarOfString(SpellDocumentTag, StringView text, Vector<WebCore::GrammarDetail>&, int32_t& badGrammarLocation, int32_t& badGrammarLength);
    static bool spellingUIIsShowing();
    static void toggleSpellingUIIsShowing();
    static void updateSpellingUIWithMisspelledWord(SpellDocumentTag, const String& misspelledWord);
    static void updateSpellingUIWithGrammarString(SpellDocumentTag, const String& badGrammarPhrase, const WebCore::GrammarDetail&);
    static void getGuessesForWord(SpellDocumentTag, const String& word, const String& context, int32_t insertionPoint, Vector<String>& guesses, bool initialCapitalizationEnabled);
    static void learnWord(SpellDocumentTag, const String& word);
    static void ignoreWord(SpellDocumentTag, const String& word);
    static void requestCheckingOfString(Ref<TextCheckerCompletion>&&, int32_t insertionPoint);
};

} // namespace WebKit
