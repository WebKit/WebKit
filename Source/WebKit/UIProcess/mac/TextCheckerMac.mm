/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "TextCheckerState.h"
#import <WebCore/NotImplemented.h>
#import <pal/spi/cocoa/FoundationSPI.h>
#import <pal/spi/mac/NSSpellCheckerSPI.h>
#import <wtf/CheckedArithmetic.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/StringView.h>

static NSString* const WebAutomaticSpellingCorrectionEnabled = @"WebAutomaticSpellingCorrectionEnabled";
static NSString* const WebContinuousSpellCheckingEnabled = @"WebContinuousSpellCheckingEnabled";
static NSString* const WebGrammarCheckingEnabled = @"WebGrammarCheckingEnabled";
static NSString* const WebSmartInsertDeleteEnabled = @"WebSmartInsertDeleteEnabled";
static NSString* const WebSmartListsEnabled = @"WebSmartListsEnabled";
static NSString* const WebAutomaticQuoteSubstitutionEnabled = @"WebAutomaticQuoteSubstitutionEnabled";
static NSString* const WebAutomaticDashSubstitutionEnabled = @"WebAutomaticDashSubstitutionEnabled";
static NSString* const WebAutomaticLinkDetectionEnabled = @"WebAutomaticLinkDetectionEnabled";
static NSString* const WebAutomaticTextReplacementEnabled = @"WebAutomaticTextReplacementEnabled";

namespace WebKit {
using namespace WebCore;

static bool shouldAutomaticTextReplacementBeEnabled()
{
    RetainPtr defaults = [NSUserDefaults standardUserDefaults];
    if (![defaults objectForKey:WebAutomaticTextReplacementEnabled])
        return [NSSpellChecker isAutomaticTextReplacementEnabled];
    return [defaults boolForKey:WebAutomaticTextReplacementEnabled];
}

static bool shouldAutomaticSpellingCorrectionBeEnabled()
{
    RetainPtr defaults = [NSUserDefaults standardUserDefaults];
    if (![defaults objectForKey:WebAutomaticSpellingCorrectionEnabled])
        return [NSSpellChecker isAutomaticTextReplacementEnabled];
    return [defaults boolForKey:WebAutomaticSpellingCorrectionEnabled];
}

static bool shouldAutomaticQuoteSubstitutionBeEnabled()
{
    RetainPtr defaults = [NSUserDefaults standardUserDefaults];
    if (![defaults objectForKey:WebAutomaticQuoteSubstitutionEnabled])
        return [NSSpellChecker isAutomaticQuoteSubstitutionEnabled];

    return [defaults boolForKey:WebAutomaticQuoteSubstitutionEnabled];
}

static bool shouldAutomaticDashSubstitutionBeEnabled()
{
    RetainPtr defaults = [NSUserDefaults standardUserDefaults];
    if (![defaults objectForKey:WebAutomaticDashSubstitutionEnabled])
        return [NSSpellChecker isAutomaticDashSubstitutionEnabled];

    return [defaults boolForKey:WebAutomaticDashSubstitutionEnabled];
}

static bool shouldGrammarCheckingBeEnabled()
{
    RetainPtr defaults = [NSUserDefaults standardUserDefaults];
#if USE(NSSPELLCHECKER_GRAMMAR_CHECKING_POLICY)
    if (![defaults objectForKey:WebGrammarCheckingEnabled])
        return [NSSpellChecker grammarCheckingEnabled];
#endif

    return [defaults boolForKey:WebGrammarCheckingEnabled];
}

static bool shouldSmartListsBeEnabled()
{
    RetainPtr defaults = [NSUserDefaults standardUserDefaults];
    if (![defaults objectForKey:WebSmartListsEnabled])
        return true; // The Smart Lists preference should be true by default.

    return [defaults boolForKey:WebSmartListsEnabled];
}

static OptionSet<TextCheckerState>& mutableState()
{
    static OptionSet<TextCheckerState> state = [&] {
        OptionSet<TextCheckerState> state;
        if ([[NSUserDefaults standardUserDefaults] boolForKey:WebContinuousSpellCheckingEnabled] && TextChecker::isContinuousSpellCheckingAllowed())
            state.add(TextCheckerState::ContinuousSpellCheckingEnabled);

        if (shouldGrammarCheckingBeEnabled())
            state.add(TextCheckerState::GrammarCheckingEnabled);

        if (shouldAutomaticTextReplacementBeEnabled())
            state.add(TextCheckerState::AutomaticTextReplacementEnabled);

        if (shouldAutomaticSpellingCorrectionBeEnabled())
            state.add(TextCheckerState::AutomaticSpellingCorrectionEnabled);

        if (shouldAutomaticQuoteSubstitutionBeEnabled())
            state.add(TextCheckerState::AutomaticQuoteSubstitutionEnabled);

        if (shouldAutomaticDashSubstitutionBeEnabled())
            state.add(TextCheckerState::AutomaticDashSubstitutionEnabled);

        if ([[NSUserDefaults standardUserDefaults] boolForKey:WebAutomaticLinkDetectionEnabled])
            state.add(TextCheckerState::AutomaticLinkDetectionEnabled);

        if (shouldSmartListsBeEnabled())
            state.add(TextCheckerState::SmartListsEnabled);
        return state;
    }();
    return state;
}

OptionSet<TextCheckerState> TextChecker::state()
{
    return mutableState();
}
    
static bool testingModeEnabled = false;

void TextChecker::setTestingMode(bool enabled)
{
    if (enabled && !testingModeEnabled) {
        [[NSUserDefaults standardUserDefaults] setBool:state().contains(TextCheckerState::ContinuousSpellCheckingEnabled) forKey:WebContinuousSpellCheckingEnabled];
        [[NSUserDefaults standardUserDefaults] setBool:state().contains(TextCheckerState::GrammarCheckingEnabled) forKey:WebGrammarCheckingEnabled];
        [[NSUserDefaults standardUserDefaults] setBool:state().contains(TextCheckerState::AutomaticSpellingCorrectionEnabled) forKey:WebAutomaticSpellingCorrectionEnabled];
        [[NSUserDefaults standardUserDefaults] setBool:state().contains(TextCheckerState::AutomaticQuoteSubstitutionEnabled) forKey:WebAutomaticQuoteSubstitutionEnabled];
        [[NSUserDefaults standardUserDefaults] setBool:state().contains(TextCheckerState::AutomaticDashSubstitutionEnabled) forKey:WebAutomaticDashSubstitutionEnabled];
        [[NSUserDefaults standardUserDefaults] setBool:state().contains(TextCheckerState::AutomaticLinkDetectionEnabled) forKey:WebAutomaticLinkDetectionEnabled];
        [[NSUserDefaults standardUserDefaults] setBool:state().contains(TextCheckerState::AutomaticTextReplacementEnabled) forKey:WebAutomaticTextReplacementEnabled];
        [[NSUserDefaults standardUserDefaults] setBool:state().contains(TextCheckerState::SmartListsEnabled) forKey:WebSmartListsEnabled];
        [[NSUserDefaults standardUserDefaults] setBool:isSmartInsertDeleteEnabled() forKey:WebSmartInsertDeleteEnabled];
    }
    testingModeEnabled = enabled;
}

bool TextChecker::isTestingMode()
{
    return testingModeEnabled;
}

bool TextChecker::isContinuousSpellCheckingAllowed()
{
    static bool allowContinuousSpellChecking = true;
    static bool readAllowContinuousSpellCheckingDefault = false;

    if (!readAllowContinuousSpellCheckingDefault) {
        if ([[NSUserDefaults standardUserDefaults] objectForKey:@"NSAllowContinuousSpellChecking"])
            allowContinuousSpellChecking = [[NSUserDefaults standardUserDefaults] boolForKey:@"NSAllowContinuousSpellChecking"];

        readAllowContinuousSpellCheckingDefault = true;
    }

    return allowContinuousSpellChecking;
}

bool TextChecker::setContinuousSpellCheckingEnabled(bool isContinuousSpellCheckingEnabled)
{
    if (state().contains(TextCheckerState::ContinuousSpellCheckingEnabled) == isContinuousSpellCheckingEnabled)
        return false;

    mutableState().set(TextCheckerState::ContinuousSpellCheckingEnabled, isContinuousSpellCheckingEnabled);
    if (!testingModeEnabled)
        [[NSUserDefaults standardUserDefaults] setBool:isContinuousSpellCheckingEnabled forKey:WebContinuousSpellCheckingEnabled];

    // FIXME: preflight the spell checker.
    return true;
}

void TextChecker::setGrammarCheckingEnabled(bool isGrammarCheckingEnabled)
{
    if (state().contains(TextCheckerState::GrammarCheckingEnabled) == isGrammarCheckingEnabled)
        return;

    mutableState().set(TextCheckerState::GrammarCheckingEnabled, isGrammarCheckingEnabled);
    if (!testingModeEnabled)
        [[NSUserDefaults standardUserDefaults] setBool:isGrammarCheckingEnabled forKey:WebGrammarCheckingEnabled];
    [[NSSpellChecker sharedSpellChecker] updatePanels];
    
    // We call preflightSpellChecker() when turning continuous spell checking on, but we don't need to do that here
    // because grammar checking only occurs on code paths that already preflight spell checking appropriately.
}

void TextChecker::setAutomaticSpellingCorrectionEnabled(bool isAutomaticSpellingCorrectionEnabled)
{
    if (state().contains(TextCheckerState::AutomaticSpellingCorrectionEnabled) == isAutomaticSpellingCorrectionEnabled)
        return;

    mutableState().set(TextCheckerState::AutomaticSpellingCorrectionEnabled, isAutomaticSpellingCorrectionEnabled);
    if (!testingModeEnabled)
        [[NSUserDefaults standardUserDefaults] setBool:isAutomaticSpellingCorrectionEnabled forKey:WebAutomaticSpellingCorrectionEnabled];

    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

void TextChecker::setAutomaticQuoteSubstitutionEnabled(bool isAutomaticQuoteSubstitutionEnabled)
{
    if (state().contains(TextCheckerState::AutomaticQuoteSubstitutionEnabled) == isAutomaticQuoteSubstitutionEnabled)
        return;

    mutableState().set(TextCheckerState::AutomaticQuoteSubstitutionEnabled, isAutomaticQuoteSubstitutionEnabled);
    if (!testingModeEnabled)
        [[NSUserDefaults standardUserDefaults] setBool:isAutomaticQuoteSubstitutionEnabled forKey:WebAutomaticQuoteSubstitutionEnabled];
    
    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

void TextChecker::setAutomaticDashSubstitutionEnabled(bool isAutomaticDashSubstitutionEnabled)
{
    if (state().contains(TextCheckerState::AutomaticDashSubstitutionEnabled) == isAutomaticDashSubstitutionEnabled)
        return;

    mutableState().set(TextCheckerState::AutomaticDashSubstitutionEnabled, isAutomaticDashSubstitutionEnabled);
    if (!testingModeEnabled)
        [[NSUserDefaults standardUserDefaults] setBool:isAutomaticDashSubstitutionEnabled forKey:WebAutomaticDashSubstitutionEnabled];

    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

void TextChecker::setAutomaticLinkDetectionEnabled(bool isAutomaticLinkDetectionEnabled)
{
    if (state().contains(TextCheckerState::AutomaticLinkDetectionEnabled) == isAutomaticLinkDetectionEnabled)
        return;

    mutableState().set(TextCheckerState::AutomaticLinkDetectionEnabled, isAutomaticLinkDetectionEnabled);
    if (!testingModeEnabled)
        [[NSUserDefaults standardUserDefaults] setBool:isAutomaticLinkDetectionEnabled forKey:WebAutomaticLinkDetectionEnabled];
    
    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

void TextChecker::setAutomaticTextReplacementEnabled(bool isAutomaticTextReplacementEnabled)
{
    if (state().contains(TextCheckerState::AutomaticTextReplacementEnabled) == isAutomaticTextReplacementEnabled)
        return;

    mutableState().set(TextCheckerState::AutomaticTextReplacementEnabled, isAutomaticTextReplacementEnabled);
    if (!testingModeEnabled)
        [[NSUserDefaults standardUserDefaults] setBool:isAutomaticTextReplacementEnabled forKey:WebAutomaticTextReplacementEnabled];

    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

static bool smartInsertDeleteEnabled;
    
bool TextChecker::isSmartInsertDeleteEnabled()
{
    static bool readSmartInsertDeleteEnabledDefault;

    if (!readSmartInsertDeleteEnabledDefault) {
        smartInsertDeleteEnabled = ![[NSUserDefaults standardUserDefaults] objectForKey:WebSmartInsertDeleteEnabled] || [[NSUserDefaults standardUserDefaults] boolForKey:WebSmartInsertDeleteEnabled];

        readSmartInsertDeleteEnabledDefault = true;
    }

    return smartInsertDeleteEnabled;
}

void TextChecker::setSmartInsertDeleteEnabled(bool flag)
{
    if (flag == isSmartInsertDeleteEnabled())
        return;

    smartInsertDeleteEnabled = flag;

    if (!testingModeEnabled)
        [[NSUserDefaults standardUserDefaults] setBool:flag forKey:WebSmartInsertDeleteEnabled];
}

void TextChecker::setSmartListsEnabled(bool smartListsEnabled)
{
    if (state().contains(TextCheckerState::SmartListsEnabled) == smartListsEnabled)
        return;

    mutableState().set(TextCheckerState::SmartListsEnabled, smartListsEnabled);
    if (!testingModeEnabled)
        [[NSUserDefaults standardUserDefaults] setBool:smartListsEnabled forKey:WebSmartListsEnabled];
}

void TextChecker::didChangeAutomaticTextReplacementEnabled()
{
    mutableState().set(TextCheckerState::AutomaticTextReplacementEnabled, shouldAutomaticTextReplacementBeEnabled());
    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

void TextChecker::didChangeAutomaticSpellingCorrectionEnabled()
{
    mutableState().set(TextCheckerState::AutomaticSpellingCorrectionEnabled, shouldAutomaticSpellingCorrectionBeEnabled());
    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

void TextChecker::didChangeAutomaticQuoteSubstitutionEnabled()
{
    mutableState().set(TextCheckerState::AutomaticQuoteSubstitutionEnabled, shouldAutomaticQuoteSubstitutionBeEnabled());
    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

void TextChecker::didChangeAutomaticDashSubstitutionEnabled()
{
    mutableState().set(TextCheckerState::AutomaticDashSubstitutionEnabled, shouldAutomaticDashSubstitutionBeEnabled());
    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

void TextChecker::didChangeSmartListsEnabled()
{
    mutableState().set(TextCheckerState::SmartListsEnabled, shouldSmartListsBeEnabled());
}

bool TextChecker::substitutionsPanelIsShowing()
{
    return [[[NSSpellChecker sharedSpellChecker] substitutionsPanel] isVisible];
}

void TextChecker::toggleSubstitutionsPanelIsShowing()
{
    RetainPtr substitutionsPanel = [[NSSpellChecker sharedSpellChecker] substitutionsPanel];
    if ([substitutionsPanel isVisible]) {
        [substitutionsPanel orderOut:nil];
        return;
    }
    [substitutionsPanel orderFront:nil];
}

void TextChecker::continuousSpellCheckingEnabledStateChanged(bool enabled)
{
    mutableState().set(TextCheckerState::ContinuousSpellCheckingEnabled, enabled);
}

void TextChecker::grammarCheckingEnabledStateChanged(bool enabled)
{
    mutableState().set(TextCheckerState::GrammarCheckingEnabled, enabled);
}

SpellDocumentTag TextChecker::uniqueSpellDocumentTag(WebPageProxy*)
{
    return [NSSpellChecker uniqueSpellDocumentTag];
}

void TextChecker::closeSpellDocumentWithTag(SpellDocumentTag tag)
{
    [[NSSpellChecker sharedSpellChecker] closeSpellDocumentWithTag:tag];
}

#if USE(UNIFIED_TEXT_CHECKING)

Vector<TextCheckingResult> TextChecker::checkTextOfParagraph(SpellDocumentTag spellDocumentTag, StringView text, int32_t insertionPoint, OptionSet<TextCheckingType> checkingTypes, bool initialCapitalizationEnabled)
{
    Vector<TextCheckingResult> results;

    RetainPtr<NSString> textString = text.createNSStringWithoutCopying();
    NSDictionary *options = @{
        NSTextCheckingInsertionPointKey : @(insertionPoint),
        NSTextCheckingSuppressInitialCapitalizationKey : @(!initialCapitalizationEnabled)
    };
    RetainPtr incomingResults = [[NSSpellChecker sharedSpellChecker] checkString:textString.get()
                                                                          range:NSMakeRange(0, text.length())
                                                                          types:nsTextCheckingTypes(checkingTypes) | NSTextCheckingTypeOrthography
                                                                        options:options
                                                         inSpellDocumentWithTag:spellDocumentTag 
                                                                    orthography:NULL
                                                                      wordCount:NULL];
    for (NSTextCheckingResult *incomingResult in incomingResults.get()) {
        NSTextCheckingType resultType = [incomingResult resultType];
        ASSERT(incomingResult.range.location != NSNotFound);
        ASSERT(incomingResult.range.length > 0);
        auto resultRange = incomingResult.range;
        if (resultType == NSTextCheckingTypeSpelling && checkingTypes.contains(TextCheckingType::Spelling)) {
            TextCheckingResult result;
            result.type = TextCheckingType::Spelling;
            result.range = resultRange;
            results.append(WTFMove(result));
        } else if (resultType == NSTextCheckingTypeGrammar && checkingTypes.contains(TextCheckingType::Grammar)) {
            TextCheckingResult result;
            RetainPtr details = [incomingResult grammarDetails];
            result.type = TextCheckingType::Grammar;
            result.range = resultRange;
            result.details.reserveInitialCapacity(details.get().count);
            for (NSDictionary *incomingDetail in details.get()) {
                ASSERT(incomingDetail);
                GrammarDetail detail;
                RetainPtr detailRangeAsNSValue = [incomingDetail objectForKey:NSGrammarRange];
                ASSERT(detailRangeAsNSValue);
                NSRange detailNSRange = [detailRangeAsNSValue rangeValue];
                ASSERT(detailNSRange.location != NSNotFound);
                ASSERT(detailNSRange.length > 0);
                detail.range = detailNSRange;
                detail.userDescription = [incomingDetail objectForKey:NSGrammarUserDescription];
                RetainPtr<NSArray> guesses = [incomingDetail objectForKey:NSGrammarCorrections];
                detail.guesses = makeVector<String>(guesses.get());
                result.details.append(WTFMove(detail));
            }
            results.append(result);
        } else if (resultType == NSTextCheckingTypeLink && checkingTypes.contains(TextCheckingType::Link)) {
            TextCheckingResult result;
            result.type = TextCheckingType::Link;
            result.range = resultRange;
            result.replacement = [[incomingResult URL] absoluteString];
            results.append(WTFMove(result));
        } else if (resultType == NSTextCheckingTypeQuote && checkingTypes.contains(TextCheckingType::Quote)) {
            TextCheckingResult result;
            result.type = TextCheckingType::Quote;
            result.range = resultRange;
            result.replacement = [incomingResult replacementString];
            results.append(WTFMove(result));
        } else if (resultType == NSTextCheckingTypeDash && checkingTypes.contains(TextCheckingType::Dash)) {
            TextCheckingResult result;
            result.type = TextCheckingType::Dash;
            result.range = resultRange;
            result.replacement = [incomingResult replacementString];
            results.append(WTFMove(result));
        } else if (resultType == NSTextCheckingTypeReplacement && checkingTypes.contains(TextCheckingType::Replacement)) {
            TextCheckingResult result;
            result.type = TextCheckingType::Replacement;
            result.range = resultRange;
            result.replacement = [incomingResult replacementString];
            results.append(WTFMove(result));
        } else if (resultType == NSTextCheckingTypeCorrection && checkingTypes.contains(TextCheckingType::Correction)) {
            TextCheckingResult result;
            result.type = TextCheckingType::Correction;
            result.range = resultRange;
            result.replacement = [incomingResult replacementString];
            if ([incomingResult respondsToSelector:@selector(detail)]) {
                if (RetainPtr incomingDetail = [incomingResult detail]) {
                    result.details.reserveInitialCapacity(1);
                    GrammarDetail detail;

                    RetainPtr detailRangeAsNSValue = [incomingDetail objectForKey:NSGrammarRange];
                    ASSERT(detailRangeAsNSValue);

                    NSRange detailNSRange = [detailRangeAsNSValue rangeValue];
                    ASSERT(detailNSRange.location != NSNotFound);
                    ASSERT(detailNSRange.length > 0);

                    detail.range = detailNSRange;
                    detail.userDescription = [incomingDetail objectForKey:NSGrammarUserDescription];
                    RetainPtr<NSArray> guesses = [incomingDetail objectForKey:NSGrammarCorrections];
                    detail.guesses = makeVector<String>(guesses.get());
                    result.details.append(WTFMove(detail));
                }
            }
            results.append(WTFMove(result));
        }
    }

    return results;
}

#endif // USE(UNIFIED_TEXT_CHECKING)

void TextChecker::checkSpellingOfString(SpellDocumentTag, StringView, int32_t&, int32_t&)
{
    // Mac uses checkTextOfParagraph instead.
    notImplemented();
}

void TextChecker::checkGrammarOfString(SpellDocumentTag, StringView, Vector<WebCore::GrammarDetail>&, int32_t&, int32_t&)
{
    // Mac uses checkTextOfParagraph instead.
    notImplemented();
}

bool TextChecker::spellingUIIsShowing()
{
    return [[[NSSpellChecker sharedSpellChecker] spellingPanel] isVisible];
}

void TextChecker::toggleSpellingUIIsShowing()
{
    RetainPtr spellingPanel = [[NSSpellChecker sharedSpellChecker] spellingPanel];
    if ([spellingPanel isVisible])
        [spellingPanel orderOut:nil];
    else
        [spellingPanel orderFront:nil];
}

void TextChecker::updateSpellingUIWithMisspelledWord(SpellDocumentTag, const String& misspelledWord)
{
    [[NSSpellChecker sharedSpellChecker] updateSpellingPanelWithMisspelledWord:misspelledWord.createNSString().get()];
}

void TextChecker::updateSpellingUIWithGrammarString(SpellDocumentTag, const String& badGrammarPhrase, const GrammarDetail& grammarDetail)
{
    CheckedUint64 endOfRangeChecked = grammarDetail.range.location;
    endOfRangeChecked += grammarDetail.range.length;

    if (endOfRangeChecked.hasOverflowed() || endOfRangeChecked >= badGrammarPhrase.length())
        return;

    RetainPtr detail = @{
        NSGrammarRange : [NSValue valueWithRange:grammarDetail.range],
        NSGrammarUserDescription : grammarDetail.userDescription.createNSString().get(),
        NSGrammarCorrections : createNSArray(grammarDetail.guesses).get(),
    };
    [[NSSpellChecker sharedSpellChecker] updateSpellingPanelWithGrammarString:badGrammarPhrase.createNSString().get() detail:detail.get()];
}

void TextChecker::getGuessesForWord(SpellDocumentTag spellDocumentTag, const String& word, const String& context, int32_t insertionPoint, Vector<String>& guesses, bool initialCapitalizationEnabled)
{
    RetainPtr<NSString> language;
    NSOrthography* orthography = nil;
    RetainPtr checker = [NSSpellChecker sharedSpellChecker];
    NSDictionary *options = @{
        NSTextCheckingInsertionPointKey : @(insertionPoint),
        NSTextCheckingSuppressInitialCapitalizationKey : @(!initialCapitalizationEnabled)
    };
    if (context.length()) {
        [checker checkString:context.createNSString().get() range:NSMakeRange(0, context.length()) types:NSTextCheckingTypeOrthography options:options inSpellDocumentWithTag:spellDocumentTag orthography:&orthography wordCount:0];
        language = [checker languageForWordRange:NSMakeRange(0, context.length()) inString:context.createNSString().get() orthography:orthography];
    }
    guesses = makeVector<String>(retainPtr([checker guessesForWordRange:NSMakeRange(0, word.length()) inString:word.createNSString().get() language:language.get() inSpellDocumentWithTag:spellDocumentTag]).get());
}

void TextChecker::learnWord(SpellDocumentTag, const String& word)
{
    [[NSSpellChecker sharedSpellChecker] learnWord:word.createNSString().get()];
}

void TextChecker::ignoreWord(SpellDocumentTag spellDocumentTag, const String& word)
{
    [[NSSpellChecker sharedSpellChecker] ignoreWord:word.createNSString().get() inSpellDocumentWithTag:spellDocumentTag];
}

void TextChecker::requestCheckingOfString(Ref<TextCheckerCompletion>&&, int32_t)
{
    notImplemented();
}

} // namespace WebKit

#endif // PLATFORM(MAC)
