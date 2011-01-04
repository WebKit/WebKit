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

#include "TextChecker.h"

#include "TextCheckerState.h"

#define WebAutomaticSpellingCorrectionEnabled @"WebAutomaticSpellingCorrectionEnabled"
#define WebContinuousSpellCheckingEnabled @"WebContinuousSpellCheckingEnabled"
#define WebGrammarCheckingEnabled @"WebGrammarCheckingEnabled"

namespace WebKit {

TextCheckerState textCheckerState;

static void initializeState()
{
    static bool didInitializeState;
    if (didInitializeState)
        return;

    textCheckerState.isContinuousSpellCheckingEnabled = [[NSUserDefaults standardUserDefaults] boolForKey:WebContinuousSpellCheckingEnabled] && TextChecker::isContinuousSpellCheckingAllowed();
    textCheckerState.isGrammarCheckingEnabled = [[NSUserDefaults standardUserDefaults] boolForKey:WebGrammarCheckingEnabled];
    textCheckerState.isAutomaticSpellingCorrectionEnabled = [[NSUserDefaults standardUserDefaults] boolForKey:WebAutomaticSpellingCorrectionEnabled];
    
#if !defined(BUILDING_ON_SNOW_LEOPARD)
    if (![[NSUserDefaults standardUserDefaults] objectForKey:WebAutomaticSpellingCorrectionEnabled])
        textCheckerState.isAutomaticSpellingCorrectionEnabled = [NSSpellChecker isAutomaticSpellingCorrectionEnabled];
#endif

    didInitializeState = true;
}

const TextCheckerState& TextChecker::state()
{
    initializeState();
    return textCheckerState;
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

void TextChecker::setContinuousSpellCheckingEnabled(bool isContinuousSpellCheckingEnabled)
{
    if (state().isContinuousSpellCheckingEnabled == isContinuousSpellCheckingEnabled)
        return;
                                                                                      
    textCheckerState.isContinuousSpellCheckingEnabled = isContinuousSpellCheckingEnabled;
    [[NSUserDefaults standardUserDefaults] setBool:isContinuousSpellCheckingEnabled forKey:WebContinuousSpellCheckingEnabled];

    // FIXME: preflight the spell checker.
}

void TextChecker::setGrammarCheckingEnabled(bool isGrammarCheckingEnabled)
{
    if (state().isGrammarCheckingEnabled == isGrammarCheckingEnabled)
        return;

    textCheckerState.isGrammarCheckingEnabled = isGrammarCheckingEnabled;
    [[NSUserDefaults standardUserDefaults] setBool:isGrammarCheckingEnabled forKey:WebGrammarCheckingEnabled];    
    [[NSSpellChecker sharedSpellChecker] updatePanels];
    
    // We call preflightSpellChecker() when turning continuous spell checking on, but we don't need to do that here
    // because grammar checking only occurs on code paths that already preflight spell checking appropriately.
}

void TextChecker::setAutomaticSpellingCorrectionEnabled(bool isAutomaticSpellingCorrectionEnabled)
{
    if (state().isAutomaticSpellingCorrectionEnabled == isAutomaticSpellingCorrectionEnabled)
        return;

    textCheckerState.isAutomaticSpellingCorrectionEnabled = isAutomaticSpellingCorrectionEnabled;
    [[NSUserDefaults standardUserDefaults] setBool:isAutomaticSpellingCorrectionEnabled forKey:WebAutomaticSpellingCorrectionEnabled];    

    [[NSSpellChecker sharedSpellChecker] updatePanels];
}

} // namespace WebKit
