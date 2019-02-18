/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#import "WebPage.h"


#import "LoadParameters.h"
#import "PluginView.h"
#import "WebPageProxyMessages.h"
#import <WebCore/DictionaryLookup.h>
#import <WebCore/Editor.h>
#import <WebCore/EventHandler.h>
#import <WebCore/FocusController.h>
#import <WebCore/HTMLConverter.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/NodeRenderStyle.h>
#import <WebCore/PlatformMediaSessionManager.h>
#import <WebCore/RenderElement.h>
#import <WebCore/RenderObject.h>

#if PLATFORM(COCOA)

namespace WebKit {
using namespace WebCore;

void WebPage::platformDidReceiveLoadParameters(const LoadParameters& loadParameters)
{
    m_dataDetectionContext = loadParameters.dataDetectionContext;
}

void WebPage::requestActiveNowPlayingSessionInfo(CallbackID callbackID)
{
    bool hasActiveSession = false;
    String title = emptyString();
    double duration = NAN;
    double elapsedTime = NAN;
    uint64_t uniqueIdentifier = 0;
    bool registeredAsNowPlayingApplication = false;
    if (auto* sharedManager = WebCore::PlatformMediaSessionManager::sharedManagerIfExists()) {
        hasActiveSession = sharedManager->hasActiveNowPlayingSession();
        title = sharedManager->lastUpdatedNowPlayingTitle();
        duration = sharedManager->lastUpdatedNowPlayingDuration();
        elapsedTime = sharedManager->lastUpdatedNowPlayingElapsedTime();
        uniqueIdentifier = sharedManager->lastUpdatedNowPlayingInfoUniqueIdentifier();
        registeredAsNowPlayingApplication = sharedManager->registeredAsNowPlayingApplication();
    }

    send(Messages::WebPageProxy::NowPlayingInfoCallback(hasActiveSession, registeredAsNowPlayingApplication, title, duration, elapsedTime, uniqueIdentifier, callbackID));
}
    
void WebPage::performDictionaryLookupAtLocation(const FloatPoint& floatPoint)
{
    if (auto* pluginView = pluginViewForFrame(&m_page->mainFrame())) {
        if (pluginView->performDictionaryLookupAtLocation(floatPoint))
            return;
    }
    
    // Find the frame the point is over.
    HitTestResult result = m_page->mainFrame().eventHandler().hitTestResultAtPoint(m_page->mainFrame().view()->windowToContents(roundedIntPoint(floatPoint)));
    RefPtr<Range> range;
    NSDictionary *options;
    std::tie(range, options) = DictionaryLookup::rangeAtHitTestResult(result);
    if (!range)
        return;
    
    auto* frame = result.innerNonSharedNode() ? result.innerNonSharedNode()->document().frame() : &m_page->focusController().focusedOrMainFrame();
    if (!frame)
        return;
    
    performDictionaryLookupForRange(*frame, *range, options, TextIndicatorPresentationTransition::Bounce);
}

void WebPage::performDictionaryLookupForSelection(Frame& frame, const VisibleSelection& selection, TextIndicatorPresentationTransition presentationTransition)
{
    RefPtr<Range> selectedRange;
    NSDictionary *options;
    std::tie(selectedRange, options) = DictionaryLookup::rangeForSelection(selection);
    if (selectedRange)
        performDictionaryLookupForRange(frame, *selectedRange, options, presentationTransition);
}

void WebPage::performDictionaryLookupOfCurrentSelection()
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    performDictionaryLookupForSelection(frame, frame.selection().selection(), TextIndicatorPresentationTransition::BounceAndCrossfade);
}
    
void WebPage::performDictionaryLookupForRange(Frame& frame, Range& range, NSDictionary *options, TextIndicatorPresentationTransition presentationTransition)
{
    send(Messages::WebPageProxy::DidPerformDictionaryLookup(dictionaryPopupInfoForRange(frame, range, options, presentationTransition)));
}

DictionaryPopupInfo WebPage::dictionaryPopupInfoForRange(Frame& frame, Range& range, NSDictionary *options, TextIndicatorPresentationTransition presentationTransition)
{
    Editor& editor = frame.editor();
    editor.setIsGettingDictionaryPopupInfo(true);
    
    DictionaryPopupInfo dictionaryPopupInfo;
    if (range.text().stripWhiteSpace().isEmpty()) {
        editor.setIsGettingDictionaryPopupInfo(false);
        return dictionaryPopupInfo;
    }
    
    Vector<FloatQuad> quads;
    range.absoluteTextQuads(quads);
    if (quads.isEmpty()) {
        editor.setIsGettingDictionaryPopupInfo(false);
        return dictionaryPopupInfo;
    }
    
    IntRect rangeRect = frame.view()->contentsToWindow(quads[0].enclosingBoundingBox());
    
    const RenderStyle* style = range.startContainer().renderStyle();
    float scaledAscent = style ? style->fontMetrics().ascent() * pageScaleFactor() : 0;
    dictionaryPopupInfo.origin = FloatPoint(rangeRect.x(), rangeRect.y() + scaledAscent);
    dictionaryPopupInfo.options = options;

#if PLATFORM(MAC)

    NSAttributedString *nsAttributedString = editingAttributedStringFromRange(range, IncludeImagesInAttributedString::No);
    
    RetainPtr<NSMutableAttributedString> scaledNSAttributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:[nsAttributedString string]]);
    
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    
    [nsAttributedString enumerateAttributesInRange:NSMakeRange(0, [nsAttributedString length]) options:0 usingBlock:^(NSDictionary *attributes, NSRange range, BOOL *stop) {
        RetainPtr<NSMutableDictionary> scaledAttributes = adoptNS([attributes mutableCopy]);
        
        NSFont *font = [scaledAttributes objectForKey:NSFontAttributeName];
        if (font)
            font = [fontManager convertFont:font toSize:font.pointSize * pageScaleFactor()];
        if (font)
            [scaledAttributes setObject:font forKey:NSFontAttributeName];
        
        [scaledNSAttributedString addAttributes:scaledAttributes.get() range:range];
    }];

#endif // PLATFORM(MAC)
    
    TextIndicatorOptions indicatorOptions = TextIndicatorOptionUseBoundingRectAndPaintAllContentForComplexRanges;
    if (presentationTransition == TextIndicatorPresentationTransition::BounceAndCrossfade)
        indicatorOptions |= TextIndicatorOptionIncludeSnapshotWithSelectionHighlight;
    
    auto textIndicator = TextIndicator::createWithRange(range, indicatorOptions, presentationTransition);
    if (!textIndicator) {
        editor.setIsGettingDictionaryPopupInfo(false);
        return dictionaryPopupInfo;
    }
    
    dictionaryPopupInfo.textIndicator = textIndicator->data();
#if PLATFORM(MAC)
    dictionaryPopupInfo.attributedString = scaledNSAttributedString;
#endif // PLATFORM(MAC)
    
#if PLATFORM(IOSMAC)
    dictionaryPopupInfo.attributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:range.text()]);
#endif // PLATFORM(IOSMAC)
    
    editor.setIsGettingDictionaryPopupInfo(false);
    return dictionaryPopupInfo;
}

void WebPage::accessibilityTransferRemoteToken(RetainPtr<NSData> remoteToken)
{
    IPC::DataReference dataToken = IPC::DataReference(reinterpret_cast<const uint8_t*>([remoteToken bytes]), [remoteToken length]);
    send(Messages::WebPageProxy::RegisterWebProcessAccessibilityToken(dataToken));
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
