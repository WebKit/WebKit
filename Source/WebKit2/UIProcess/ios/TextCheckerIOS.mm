/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import "config.h"
#import "TextChecker.h"

#import "TextCheckerState.h"
#import <WebCore/NotImplemented.h>

using namespace WebCore;

namespace WebKit {
    
TextCheckerState textCheckerState;

const TextCheckerState& TextChecker::state()
{
    notImplemented();
    return textCheckerState;
}

bool TextChecker::isContinuousSpellCheckingAllowed()
{
    notImplemented();
    return false;
}

void TextChecker::setContinuousSpellCheckingEnabled(bool)
{
    notImplemented();
}

void TextChecker::setGrammarCheckingEnabled(bool)
{
    notImplemented();
}

void TextChecker::setAutomaticSpellingCorrectionEnabled(bool)
{
    notImplemented();
}

void TextChecker::setAutomaticQuoteSubstitutionEnabled(bool)
{
    notImplemented();
}

void TextChecker::setAutomaticDashSubstitutionEnabled(bool)
{
    notImplemented();
}

void TextChecker::setAutomaticLinkDetectionEnabled(bool)
{
    notImplemented();
}

void TextChecker::setAutomaticTextReplacementEnabled(bool)
{
    notImplemented();
}

static bool smartInsertDeleteEnabled;

bool TextChecker::isSmartInsertDeleteEnabled()
{
    notImplemented();
    return smartInsertDeleteEnabled;
}

void TextChecker::setSmartInsertDeleteEnabled(bool)
{
    notImplemented();
}

bool TextChecker::substitutionsPanelIsShowing()
{
    notImplemented();
    return false;
}

void TextChecker::toggleSubstitutionsPanelIsShowing()
{
    notImplemented();
}

void TextChecker::continuousSpellCheckingEnabledStateChanged(bool)
{
    notImplemented();
}

void TextChecker::grammarCheckingEnabledStateChanged(bool)
{
    notImplemented();
}

int64_t TextChecker::uniqueSpellDocumentTag(WebPageProxy*)
{
    notImplemented();
    return 0;
}

void TextChecker::closeSpellDocumentWithTag(int64_t)
{
    notImplemented();
}

#if USE(UNIFIED_TEXT_CHECKING)

Vector<TextCheckingResult> TextChecker::checkTextOfParagraph(int64_t, const UChar*, int, uint64_t)
{
    notImplemented();
    return Vector<TextCheckingResult>();
}

#endif

void TextChecker::checkSpellingOfString(int64_t, const UChar*, uint32_t, int32_t&, int32_t&)
{
    // Mac uses checkTextOfParagraph instead.
    notImplemented();
}

void TextChecker::checkGrammarOfString(int64_t, const UChar*, uint32_t, Vector<WebCore::GrammarDetail>&, int32_t&, int32_t&)
{
    // Mac uses checkTextOfParagraph instead.
    notImplemented();
}

bool TextChecker::spellingUIIsShowing()
{
    notImplemented();
    return false;
}

void TextChecker::toggleSpellingUIIsShowing()
{
    notImplemented();
}

void TextChecker::updateSpellingUIWithMisspelledWord(int64_t, const String&)
{
    notImplemented();
}

void TextChecker::updateSpellingUIWithGrammarString(int64_t, const String&, const GrammarDetail&)
{
    notImplemented();
}

void TextChecker::getGuessesForWord(int64_t, const String&, const String&, Vector<String>&)
{
    notImplemented();
}

void TextChecker::learnWord(int64_t, const String&)
{
    notImplemented();
}

void TextChecker::ignoreWord(int64_t, const String&)
{
    notImplemented();
}

void TextChecker::requestCheckingOfString(PassRefPtr<TextCheckerCompletion>)
{
    notImplemented();
}

} // namespace WebKit
