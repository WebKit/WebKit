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

#import "config.h"
#import "CorrectionPanel.h"

#if USE(AUTOCORRECTION_PANEL)

#import "WebPageProxy.h"
#import "WebViewImpl.h"
#import <wtf/cocoa/VectorCocoa.h>

static inline NSCorrectionIndicatorType correctionIndicatorType(WebCore::AlternativeTextType alternativeTextType)
{
    switch (alternativeTextType) {
    case WebCore::AlternativeTextType::Correction:
        return NSCorrectionIndicatorTypeDefault;
    case WebCore::AlternativeTextType::Reversion:
        return NSCorrectionIndicatorTypeReversion;
    case WebCore::AlternativeTextType::SpellingSuggestions:
        return NSCorrectionIndicatorTypeGuesses;
    case WebCore::AlternativeTextType::DictationAlternatives:
        ASSERT_NOT_REACHED();
        return NSCorrectionIndicatorTypeDefault;
    }
}

namespace WebKit {
using namespace WebCore;

CorrectionPanel::CorrectionPanel()
    : m_wasDismissedExternally(false)
    , m_reasonForDismissing(ReasonForDismissingAlternativeTextIgnored)
{
}

CorrectionPanel::~CorrectionPanel()
{
    dismissInternal(ReasonForDismissingAlternativeTextIgnored, false);
}

void CorrectionPanel::show(NSView *view, WebViewImpl& webViewImpl, AlternativeTextType type, const FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings)
{
    dismissInternal(ReasonForDismissingAlternativeTextIgnored, false);

    if (!view)
        return;

    NSInteger spellCheckerDocumentTag = webViewImpl.spellCheckerDocumentTag();

    NSString *replacedStringAsNSString = replacedString;
    NSString *replacementStringAsNSString = replacementString;

    m_view = view;
    m_spellCheckerDocumentTag = spellCheckerDocumentTag;
    NSCorrectionIndicatorType indicatorType = correctionIndicatorType(type);

    RetainPtr<NSArray> alternativeStrings;
    if (!alternativeReplacementStrings.isEmpty())
        alternativeStrings = createNSArray(alternativeReplacementStrings);

    NSSpellChecker* spellChecker = [NSSpellChecker sharedSpellChecker];
    [spellChecker showCorrectionIndicatorOfType:indicatorType primaryString:replacementStringAsNSString alternativeStrings:alternativeStrings.get() forStringInRect:boundingBoxOfReplacedString view:m_view.get() completionHandler:^(NSString* acceptedString) {
        handleAcceptedReplacement(webViewImpl, acceptedString, replacedStringAsNSString, replacementStringAsNSString, indicatorType);
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

void CorrectionPanel::recordAutocorrectionResponse(WebViewImpl& webViewImpl, NSInteger spellCheckerDocumentTag, NSCorrectionResponse response, const String& replacedString, const String& replacementString)
{
    if (webViewImpl.page().sessionID().isEphemeral())
        return;

    [[NSSpellChecker sharedSpellChecker] recordResponse:response toCorrection:replacementString forWord:replacedString language:nil inSpellDocumentWithTag:spellCheckerDocumentTag];
}

void CorrectionPanel::handleAcceptedReplacement(WebViewImpl& webViewImpl, NSString* acceptedReplacement, NSString* replaced, NSString* proposedReplacement,  NSCorrectionIndicatorType correctionIndicatorType)
{
    if (!m_view)
        return;

    switch (correctionIndicatorType) {
    case NSCorrectionIndicatorTypeDefault:
        if (acceptedReplacement)
            recordAutocorrectionResponse(webViewImpl, m_spellCheckerDocumentTag, NSCorrectionResponseAccepted, replaced, acceptedReplacement);
        else {
            if (!m_wasDismissedExternally || m_reasonForDismissing == ReasonForDismissingAlternativeTextCancelled)
                recordAutocorrectionResponse(webViewImpl, m_spellCheckerDocumentTag, NSCorrectionResponseRejected, replaced, proposedReplacement);
            else
                recordAutocorrectionResponse(webViewImpl, m_spellCheckerDocumentTag, NSCorrectionResponseIgnored, replaced, proposedReplacement);
        }
        break;
    case NSCorrectionIndicatorTypeReversion:
        if (acceptedReplacement)
            recordAutocorrectionResponse(webViewImpl, m_spellCheckerDocumentTag, NSCorrectionResponseReverted, replaced, acceptedReplacement);
        break;
    case NSCorrectionIndicatorTypeGuesses:
        if (acceptedReplacement)
            recordAutocorrectionResponse(webViewImpl, m_spellCheckerDocumentTag, NSCorrectionResponseAccepted, replaced, acceptedReplacement);
        break;
    }

    webViewImpl.handleAcceptedAlternativeText(acceptedReplacement);
    m_spellCheckerDocumentTag = 0;
    m_view = nullptr;
    if (acceptedReplacement)
        m_resultForDismissal = adoptNS([acceptedReplacement copy]);
}

} // namespace WebKit

#endif // USE(AUTOCORRECTION_PANEL)
