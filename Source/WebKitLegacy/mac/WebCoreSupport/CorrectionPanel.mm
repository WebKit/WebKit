/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#import "CorrectionPanel.h"

#import "WebViewInternal.h"
#import <wtf/cocoa/VectorCocoa.h>

#if USE(AUTOCORRECTION_PANEL)

using namespace WebCore;

static inline NSCorrectionIndicatorType correctionIndicatorType(AlternativeTextType alternativeTextType)
{
    switch (alternativeTextType) {
    case AlternativeTextTypeCorrection:
        return NSCorrectionIndicatorTypeDefault;
    case AlternativeTextTypeReversion:
        return NSCorrectionIndicatorTypeReversion;
    case AlternativeTextTypeSpellingSuggestions:
        return NSCorrectionIndicatorTypeGuesses;
    case AlternativeTextTypeDictationAlternatives:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
    return NSCorrectionIndicatorTypeDefault;
}

CorrectionPanel::CorrectionPanel()
    : m_wasDismissedExternally(false)
    , m_reasonForDismissing(ReasonForDismissingAlternativeTextIgnored)
{
}

CorrectionPanel::~CorrectionPanel()
{
    dismissInternal(ReasonForDismissingAlternativeTextIgnored, false);
}

void CorrectionPanel::show(WebView* view, AlternativeTextType type, const FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings)
{
    dismissInternal(ReasonForDismissingAlternativeTextIgnored, false);
    
    if (!view)
        return;

    NSString* replacedStringAsNSString = replacedString;
    NSString* replacementStringAsNSString = replacementString;
    m_view = view;
    NSCorrectionIndicatorType indicatorType = correctionIndicatorType(type);
    
    RetainPtr<NSArray> alternativeStrings;
    if (!alternativeReplacementStrings.isEmpty())
        alternativeStrings = createNSArray(alternativeReplacementStrings);

    [[NSSpellChecker sharedSpellChecker] showCorrectionIndicatorOfType:indicatorType primaryString:replacementStringAsNSString alternativeStrings:alternativeStrings.get() forStringInRect:[view _convertRectFromRootView:boundingBoxOfReplacedString] view:m_view.get() completionHandler:^(NSString* acceptedString) {
        handleAcceptedReplacement(acceptedString, replacedStringAsNSString, replacementStringAsNSString, indicatorType);
    }];
}

String CorrectionPanel::dismiss(ReasonForDismissingAlternativeText reason)
{
    return dismissInternal(reason, true);
}

String CorrectionPanel::dismissInternal(ReasonForDismissingAlternativeText reason, bool dismissingExternally)
{
    if (!isShowing())
        return String();
    
    m_wasDismissedExternally = dismissingExternally;
    m_reasonForDismissing = reason;
    m_resultForDismissal.clear();
    [[NSSpellChecker sharedSpellChecker] dismissCorrectionIndicatorForView:m_view.get()];
    return m_resultForDismissal.get();
}

void CorrectionPanel::recordAutocorrectionResponse(NSInteger spellCheckerDocumentTag, NSCorrectionResponse response, const String& replacedString, const String& replacementString)
{
    [[NSSpellChecker sharedSpellChecker] recordResponse:response toCorrection:replacementString forWord:replacedString language:nil inSpellDocumentWithTag:spellCheckerDocumentTag];
}

void CorrectionPanel::handleAcceptedReplacement(NSString* acceptedReplacement, NSString* replaced, NSString* proposedReplacement,  NSCorrectionIndicatorType correctionIndicatorType)
{    
    if (!m_view)
        return;
    
    NSInteger documentTag = [m_view.get() spellCheckerDocumentTag];

    switch (correctionIndicatorType) {
    case NSCorrectionIndicatorTypeDefault:
        if (acceptedReplacement)
            recordAutocorrectionResponse(documentTag, NSCorrectionResponseAccepted, replaced, acceptedReplacement);
        else {
            if (!m_wasDismissedExternally || m_reasonForDismissing == ReasonForDismissingAlternativeTextCancelled)
                recordAutocorrectionResponse(documentTag, NSCorrectionResponseRejected, replaced, proposedReplacement);
            else
                recordAutocorrectionResponse(documentTag, NSCorrectionResponseIgnored, replaced, proposedReplacement);
        }
        break;
    case NSCorrectionIndicatorTypeReversion:
        if (acceptedReplacement)
            recordAutocorrectionResponse(documentTag, NSCorrectionResponseReverted, replaced, acceptedReplacement);
        break;
    case NSCorrectionIndicatorTypeGuesses:
        if (acceptedReplacement)
            recordAutocorrectionResponse(documentTag, NSCorrectionResponseAccepted, replaced, acceptedReplacement);
        break;
    }
    
    [m_view.get() handleAcceptedAlternativeText:acceptedReplacement];
    m_view.clear();
    if (acceptedReplacement)
        m_resultForDismissal = adoptNS([acceptedReplacement copy]);
}

#endif //USE(AUTOCORRECTION_PANEL)

