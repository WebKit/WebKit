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
#if !defined(BUILDING_ON_SNOW_LEOPARD)
#import "CorrectionPanel.h"

#import "WebPageProxy.h"
#import "WKView.h"
#import "WKViewPrivate.h"

using namespace WebCore;

static inline NSCorrectionIndicatorType correctionIndicatorType(CorrectionPanelInfo::PanelType panelType)
{
    switch (panelType) {
    case CorrectionPanelInfo::PanelTypeCorrection:
        return NSCorrectionIndicatorTypeDefault;
    case CorrectionPanelInfo::PanelTypeReversion:
        return NSCorrectionIndicatorTypeReversion;
    case CorrectionPanelInfo::PanelTypeSpellingSuggestions:
        return NSCorrectionIndicatorTypeGuesses;
    }
    ASSERT_NOT_REACHED();
    return NSCorrectionIndicatorTypeDefault;
}

namespace WebKit {

CorrectionPanel::CorrectionPanel()
    : m_wasDismissedExternally(false)
    , m_reasonForDismissing(ReasonForDismissingCorrectionPanelIgnored)
    , m_resultCondition(AdoptNS, [[NSCondition alloc] init])
{
}

CorrectionPanel::~CorrectionPanel()
{
    dismissInternal(ReasonForDismissingCorrectionPanelIgnored, false);
}

void CorrectionPanel::show(WKView* view, CorrectionPanelInfo::PanelType type, const FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings)
{
    dismissInternal(ReasonForDismissingCorrectionPanelIgnored, false);
    
    if (!view)
        return;

    NSString* replacedStringAsNSString = replacedString;
    NSString* replacementStringAsNSString = replacementString;
    m_view = view;
    NSCorrectionIndicatorType indicatorType = correctionIndicatorType(type);
    
    NSMutableArray* alternativeStrings = 0;
    if (!alternativeReplacementStrings.isEmpty()) {
        size_t size = alternativeReplacementStrings.size();
        alternativeStrings = [NSMutableArray arrayWithCapacity:size];
        for (size_t i = 0; i < size; ++i)
            [alternativeStrings addObject:(NSString*)alternativeReplacementStrings[i]];
    }

    NSSpellChecker* spellChecker = [NSSpellChecker sharedSpellChecker];
    [spellChecker showCorrectionIndicatorOfType:indicatorType primaryString:replacementStringAsNSString alternativeStrings:alternativeStrings forStringInRect:boundingBoxOfReplacedString view:m_view.get() completionHandler:^(NSString* acceptedString) {
        handleAcceptedReplacement(acceptedString, replacedStringAsNSString, replacementStringAsNSString, indicatorType);
    }];
}

void CorrectionPanel::dismiss(ReasonForDismissingCorrectionPanel reason)
{
    dismissInternal(reason, true);
}

String CorrectionPanel::dismissSoon(ReasonForDismissingCorrectionPanel reason)
{
    if (!isShowing())
        return String();

    dismissInternal(reason, true);
    [m_resultCondition.get() lock];
    while (!m_resultForSynchronousDismissal)
        [m_resultCondition.get() wait];
    [m_resultCondition.get() unlock];
    return m_resultForSynchronousDismissal.get();
}

void CorrectionPanel::dismissInternal(ReasonForDismissingCorrectionPanel reason, bool dismissingExternally)
{
    m_wasDismissedExternally = dismissingExternally;
    if (!isShowing())
        return;
    
    m_reasonForDismissing = reason;
    m_resultForSynchronousDismissal.clear();
    [[NSSpellChecker sharedSpellChecker] dismissCorrectionIndicatorForView:m_view.get()];
    m_view.clear();
}

void CorrectionPanel::recordAutocorrectionResponse(WKView* view, NSCorrectionResponse response, const String& replacedString, const String& replacementString)
{
    [[NSSpellChecker sharedSpellChecker] recordResponse:response toCorrection:replacementString forWord:replacedString language:nil inSpellDocumentWithTag:[view spellCheckerDocumentTag]];
}

void CorrectionPanel::handleAcceptedReplacement(NSString* acceptedReplacement, NSString* replaced, NSString* proposedReplacement,  NSCorrectionIndicatorType correctionIndicatorType)
{
    NSSpellChecker* spellChecker = [NSSpellChecker sharedSpellChecker];
    NSInteger documentTag = [m_view.get() spellCheckerDocumentTag];
    
    switch (correctionIndicatorType) {
    case NSCorrectionIndicatorTypeDefault:
        if (acceptedReplacement)
            [spellChecker recordResponse:NSCorrectionResponseAccepted toCorrection:acceptedReplacement forWord:replaced language:nil inSpellDocumentWithTag:documentTag];
        else {
            if (!m_wasDismissedExternally || m_reasonForDismissing == ReasonForDismissingCorrectionPanelCancelled)
                [spellChecker recordResponse:NSCorrectionResponseRejected toCorrection:proposedReplacement forWord:replaced language:nil inSpellDocumentWithTag:documentTag];
            else
                [spellChecker recordResponse:NSCorrectionResponseIgnored toCorrection:proposedReplacement forWord:replaced language:nil inSpellDocumentWithTag:documentTag];
        }
        break;
    case NSCorrectionIndicatorTypeReversion:
        if (acceptedReplacement)
            [spellChecker recordResponse:NSCorrectionResponseReverted toCorrection:replaced forWord:acceptedReplacement language:nil inSpellDocumentWithTag:documentTag];
        break;
    case NSCorrectionIndicatorTypeGuesses:
        if (acceptedReplacement)
            [spellChecker recordResponse:NSCorrectionResponseAccepted toCorrection:acceptedReplacement forWord:replaced language:nil inSpellDocumentWithTag:documentTag];
        break;
    }
    
    if (!m_wasDismissedExternally) {
        [m_view.get() handleCorrectionPanelResult:acceptedReplacement];
        return;
    }
    
    [m_resultCondition.get() lock];
    if (acceptedReplacement)
        m_resultForSynchronousDismissal.adoptNS([acceptedReplacement copy]);
    [m_resultCondition.get() signal];
    [m_resultCondition.get() unlock];
}

} // namespace WebKit

#endif //!defined(BUILDING_ON_SNOW_LEOPARD)

