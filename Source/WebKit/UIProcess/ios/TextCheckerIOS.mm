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

#if PLATFORM(IOS_FAMILY)

#import "TextCheckerState.h"
#import "UIKitSPI.h"
#import <WebCore/NotImplemented.h>
#import <wtf/HashMap.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/StringView.h>

namespace WebKit {
using namespace WebCore;

static TextCheckerState& mutableState()
{
    static NeverDestroyed<TextCheckerState> state = makeNeverDestroyed([] {
        TextCheckerState initialState;
        initialState.isContinuousSpellCheckingEnabled = TextChecker::isContinuousSpellCheckingAllowed();
        initialState.isGrammarCheckingEnabled = false;
        return initialState;
    }());
    return state;
}

const TextCheckerState& TextChecker::state()
{
    return mutableState();
}

bool TextChecker::isContinuousSpellCheckingAllowed()
{
#if USE(UNIFIED_TEXT_CHECKING)
    return true;
#else
    return false;
#endif
}

void TextChecker::setContinuousSpellCheckingEnabled(bool enabled)
{
    mutableState().isContinuousSpellCheckingEnabled = enabled;
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
    
static bool testingModeEnabled = false;
    
void TextChecker::setTestingMode(bool enabled)
{
    testingModeEnabled = enabled;
}
    
bool TextChecker::isTestingMode()
{
    return testingModeEnabled;
}

bool TextChecker::isSmartInsertDeleteEnabled()
{
    return [[UIKeyboardImpl sharedInstance] smartInsertDeleteIsEnabled];
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

void TextChecker::continuousSpellCheckingEnabledStateChanged(bool enabled)
{
    mutableState().isContinuousSpellCheckingEnabled = enabled;
}

void TextChecker::grammarCheckingEnabledStateChanged(bool)
{
    notImplemented();
}

#if USE(UNIFIED_TEXT_CHECKING)

static HashMap<SpellDocumentTag, RetainPtr<UITextChecker>>& spellDocumentTagMap()
{
    static NeverDestroyed<HashMap<SpellDocumentTag, RetainPtr<UITextChecker>>> tagMap;
    return tagMap;
}

#endif

SpellDocumentTag TextChecker::uniqueSpellDocumentTag(WebPageProxy*)
{
#if USE(UNIFIED_TEXT_CHECKING)
    static SpellDocumentTag nextSpellDocumentTag;
    return ++nextSpellDocumentTag;
#else
    return { };
#endif
}

void TextChecker::closeSpellDocumentWithTag(SpellDocumentTag spellDocumentTag)
{
#if USE(UNIFIED_TEXT_CHECKING)
    spellDocumentTagMap().remove(spellDocumentTag);
#else
    UNUSED_PARAM(spellDocumentTag);
#endif
}

#if USE(UNIFIED_TEXT_CHECKING)

static RetainPtr<UITextChecker> textCheckerFor(SpellDocumentTag spellDocumentTag)
{
    auto addResult = spellDocumentTagMap().add(spellDocumentTag, nullptr);
    if (addResult.isNewEntry)
        addResult.iterator->value = adoptNS([[UITextChecker alloc] _initWithAsynchronousLoading:YES]);
    return addResult.iterator->value;
}

Vector<TextCheckingResult> TextChecker::checkTextOfParagraph(SpellDocumentTag spellDocumentTag, StringView text, int32_t /* insertionPoint */, OptionSet<TextCheckingType> checkingTypes, bool /* initialCapitalizationEnabled */)
{
    Vector<TextCheckingResult> results;
    if (!checkingTypes.contains(TextCheckingType::Spelling))
        return results;

    auto textChecker = textCheckerFor(spellDocumentTag);
    if (![textChecker _doneLoading])
        return results;

    NSArray<NSString *> *keyboardLanguages = @[ ];
    auto *currentInputMode = [UIKeyboardInputModeController sharedInputModeController].currentInputMode;
    if (currentInputMode.multilingualLanguages.count)
        keyboardLanguages = currentInputMode.multilingualLanguages;
    else if (currentInputMode.primaryLanguage)
        keyboardLanguages = @[ currentInputMode.languageWithRegion ];

    auto stringToCheck = text.createNSStringWithoutCopying();
    auto range = NSMakeRange(0, [stringToCheck length]);
    NSUInteger offsetSoFar = 0;
    do {
        auto misspelledRange = [textChecker rangeOfMisspelledWordInString:stringToCheck.get() range:range startingAt:offsetSoFar wrap:NO languages:keyboardLanguages];
        if (misspelledRange.location == NSNotFound)
            break;

        TextCheckingResult result;
        result.type = TextCheckingType::Spelling;
        result.location = misspelledRange.location;
        result.length = misspelledRange.length;
        results.append(WTFMove(result));

        offsetSoFar = misspelledRange.location + misspelledRange.length;
    } while (offsetSoFar < [stringToCheck length]);
    return results;
}

#endif

void TextChecker::checkSpellingOfString(SpellDocumentTag, StringView, int32_t&, int32_t&)
{
    // iOS uses checkTextOfParagraph() instead.
    notImplemented();
}

void TextChecker::checkGrammarOfString(SpellDocumentTag, StringView, Vector<WebCore::GrammarDetail>&, int32_t&, int32_t&)
{
    // iOS uses checkTextOfParagraph() instead.
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

void TextChecker::updateSpellingUIWithMisspelledWord(SpellDocumentTag, const String&)
{
    notImplemented();
}

void TextChecker::updateSpellingUIWithGrammarString(SpellDocumentTag, const String&, const GrammarDetail&)
{
    notImplemented();
}

void TextChecker::getGuessesForWord(SpellDocumentTag, const String&, const String&, int32_t, Vector<String>&, bool)
{
    notImplemented();
}

void TextChecker::learnWord(SpellDocumentTag, const String&)
{
    notImplemented();
}

void TextChecker::ignoreWord(SpellDocumentTag, const String&)
{
    notImplemented();
}

void TextChecker::requestCheckingOfString(Ref<TextCheckerCompletion>&&, int32_t)
{
    notImplemented();
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
