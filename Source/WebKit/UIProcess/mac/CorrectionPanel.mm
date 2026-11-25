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
#import <WebCore/CorrectionIndicator.h>
#import <pal/SessionID.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {
using namespace WebCore;

CorrectionPanel::CorrectionPanel()
    : m_wasDismissedExternally(false)
    , m_reasonForDismissing(ReasonForDismissingAlternativeText::Ignored)
{
}

CorrectionPanel::~CorrectionPanel()
{
    dismissInternal(ReasonForDismissingAlternativeText::Ignored, false);
}

void CorrectionPanel::show(NSView *view, WebViewImpl& webViewImpl, AlternativeTextType type, const FloatRect& boundingBoxOfReplacedString, StringImpl* replacedString, StringImpl* replacementString, const Vector<RefPtr<StringImpl>>& alternativeReplacementStrings)
{
    dismissInternal(ReasonForDismissingAlternativeText::Ignored, false);

    if (!view)
        return;

    NSInteger spellCheckerDocumentTag = webViewImpl.spellCheckerDocumentTag();

    RetainPtr replacedStringAsNSString = createNSString(replacedString);
    RetainPtr replacementStringAsNSString = createNSString(replacementString);

    m_view = view;
    m_spellCheckerDocumentTag = spellCheckerDocumentTag;
    NSCorrectionIndicatorType indicatorType = correctionIndicatorType(type);

    RetainPtr<NSArray> alternativeStrings;
    if (!alternativeReplacementStrings.isEmpty())
        alternativeStrings = createNSArray(alternativeReplacementStrings);

    WeakPtr weakWebViewImpl { webViewImpl };
    RetainPtr spellChecker = [NSSpellChecker sharedSpellChecker];
    [spellChecker showCorrectionIndicatorOfType:indicatorType primaryString:replacementStringAsNSString.get() alternativeStrings:alternativeStrings.get() forStringInRect:boundingBoxOfReplacedString view:m_view.get() completionHandler:^(NSString *acceptedString) {
        CheckedPtr webViewImpl = weakWebViewImpl.get();
        if (!webViewImpl)
            return;
        handleAcceptedReplacement(*webViewImpl, acceptedString, replacedStringAsNSString.get(), replacementStringAsNSString.get(), indicatorType);
    }];
}

RefPtr<StringImpl> CorrectionPanel::dismiss(ReasonForDismissingAlternativeText reason)
{
    return dismissInternal(reason, true);
}

RefPtr<StringImpl> CorrectionPanel::dismissInternal(ReasonForDismissingAlternativeText reason, bool dismissingExternally)
{
    if (!isShowing())
        return nullptr;

    m_wasDismissedExternally = dismissingExternally;
    m_reasonForDismissing = reason;
    m_resultForDismissal.clear();
    [[NSSpellChecker sharedSpellChecker] dismissCorrectionIndicatorForView:m_view.get()];
    return StringImpl::create(m_resultForDismissal.get());
}

void CorrectionPanel::recordAutocorrectionResponse(WebViewImpl& webViewImpl, NSInteger spellCheckerDocumentTag, NSCorrectionResponse response, StringImpl* replacedString, StringImpl* replacementString)
{
    if (webViewImpl.page().sessionID().isEphemeral())
        return;

    [[NSSpellChecker sharedSpellChecker] recordResponse:response toCorrection:createNSString(replacementString).get() forWord:createNSString(replacedString).get() language:nil inSpellDocumentWithTag:spellCheckerDocumentTag];
}

void CorrectionPanel::handleAcceptedReplacement(WebViewImpl& webViewImpl, NSString* acceptedReplacement, NSString* replaced, NSString* proposedReplacement,  NSCorrectionIndicatorType correctionIndicatorType)
{
    if (!m_view)
        return;

    switch (correctionIndicatorType) {
    case NSCorrectionIndicatorTypeDefault:
        if (acceptedReplacement)
            recordAutocorrectionResponse(webViewImpl, m_spellCheckerDocumentTag, NSCorrectionResponseAccepted, StringImpl::create(replaced).get(), StringImpl::create(acceptedReplacement).get());
        else {
            if (!m_wasDismissedExternally || m_reasonForDismissing == ReasonForDismissingAlternativeText::Cancelled)
                recordAutocorrectionResponse(webViewImpl, m_spellCheckerDocumentTag, NSCorrectionResponseRejected, StringImpl::create(replaced).get(), StringImpl::create(proposedReplacement).get());
            else
                recordAutocorrectionResponse(webViewImpl, m_spellCheckerDocumentTag, NSCorrectionResponseIgnored, StringImpl::create(replaced).get(), StringImpl::create(proposedReplacement).get());
        }
        break;
    case NSCorrectionIndicatorTypeReversion:
        if (acceptedReplacement)
            recordAutocorrectionResponse(webViewImpl, m_spellCheckerDocumentTag, NSCorrectionResponseReverted, StringImpl::create(replaced).get(), StringImpl::create(acceptedReplacement).get());
        break;
    case NSCorrectionIndicatorTypeGuesses:
        if (acceptedReplacement)
            recordAutocorrectionResponse(webViewImpl, m_spellCheckerDocumentTag, NSCorrectionResponseAccepted, StringImpl::create(replaced).get(), StringImpl::create(acceptedReplacement).get());
        break;
    }

    webViewImpl.handleAcceptedAlternativeText(StringImpl::create(acceptedReplacement).get());
    m_spellCheckerDocumentTag = 0;
    m_view = nullptr;
    if (acceptedReplacement)
        m_resultForDismissal = adoptNS([acceptedReplacement copy]);
}

} // namespace WebKit

#endif // USE(AUTOCORRECTION_PANEL)
