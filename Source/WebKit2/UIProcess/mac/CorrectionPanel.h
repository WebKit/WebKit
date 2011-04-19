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

#ifndef CorrectionPanel_h
#define CorrectionPanel_h

#if !defined(BUILDING_ON_SNOW_LEOPARD)
#import <AppKit/NSSpellChecker.h>
#import <WebCore/SpellingCorrectionController.h>
#import <wtf/RetainPtr.h>

@class WKView;

namespace WebKit {

class CorrectionPanel {
public:
    CorrectionPanel();
    ~CorrectionPanel();
    void show(WKView*, WebCore::CorrectionPanelInfo::PanelType, const WebCore::FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings);
    void dismiss(WebCore::ReasonForDismissingCorrectionPanel);
    String dismissSoon(WebCore::ReasonForDismissingCorrectionPanel);
    static void recordAutocorrectionResponse(WKView*, NSCorrectionResponse, const String& replacedString, const String& replacementString);

private:
    bool isShowing() const { return m_view; }
    void dismissInternal(WebCore::ReasonForDismissingCorrectionPanel, bool dismissingExternally);
    void handleAcceptedReplacement(NSString* acceptedReplacement, NSString* replaced, NSString* proposedReplacement, NSCorrectionIndicatorType);

    bool m_wasDismissedExternally;
    WebCore::ReasonForDismissingCorrectionPanel m_reasonForDismissing;
    RetainPtr<WKView> m_view;
    RetainPtr<NSString> m_resultForSynchronousDismissal;
    RetainPtr<NSCondition> m_resultCondition;
};

} // namespace WebKit

#endif // !defined(BUILDING_ON_SNOW_LEOPARD)

#endif // CorrectionPanel_h
