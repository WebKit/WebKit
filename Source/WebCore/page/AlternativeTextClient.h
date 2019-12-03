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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FloatRect.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum ReasonForDismissingAlternativeText {
    ReasonForDismissingAlternativeTextCancelled = 0,
    ReasonForDismissingAlternativeTextIgnored,
    ReasonForDismissingAlternativeTextAccepted
};

enum AlternativeTextType {
    AlternativeTextTypeCorrection = 0,
    AlternativeTextTypeReversion,
    AlternativeTextTypeSpellingSuggestions,
    AlternativeTextTypeDictationAlternatives
};

enum class AutocorrectionResponse {
    Edited,
    Reverted,
    Accepted
};

class AlternativeTextClient {
public:
    virtual ~AlternativeTextClient() = default;
#if USE(AUTOCORRECTION_PANEL)
    virtual void showCorrectionAlternative(AlternativeTextType, const FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacmentString, const Vector<String>& alternativeReplacementStrings) = 0;
    virtual void dismissAlternative(ReasonForDismissingAlternativeText) = 0;
    virtual String dismissAlternativeSoon(ReasonForDismissingAlternativeText) = 0;
    virtual void recordAutocorrectionResponse(AutocorrectionResponse, const String& replacedString, const String& replacementString) = 0;
#endif
#if USE(DICTATION_ALTERNATIVES)
    virtual void showDictationAlternativeUI(const WebCore::FloatRect& boundingBoxOfDictatedText, uint64_t dictationContext) = 0;
    virtual void removeDictationAlternatives(uint64_t dictationContext) = 0;
    virtual Vector<String> dictationAlternatives(uint64_t dictationContext) = 0;
#endif
};
    
} // namespace WebCore
