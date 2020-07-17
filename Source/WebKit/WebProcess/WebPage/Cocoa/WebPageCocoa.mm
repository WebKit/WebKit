/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#import "InsertTextOptions.h"
#import "LaunchServicesDatabaseManager.h"
#import "LoadParameters.h"
#import "PluginView.h"
#import "WKAccessibilityWebPageObjectBase.h"
#import "WebPageProxyMessages.h"
#import "WebPaymentCoordinator.h"
#import "WebRemoteObjectRegistry.h"
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/Editing.h>
#import <WebCore/Editor.h>
#import <WebCore/EventHandler.h>
#import <WebCore/EventNames.h>
#import <WebCore/FocusController.h>
#import <WebCore/FrameView.h>
#import <WebCore/HTMLConverter.h>
#import <WebCore/HTMLOListElement.h>
#import <WebCore/HTMLUListElement.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/NetworkExtensionContentFilter.h>
#import <WebCore/NodeRenderStyle.h>
#import <WebCore/PaymentCoordinator.h>
#import <WebCore/PlatformMediaSessionManager.h>
#import <WebCore/Range.h>
#import <WebCore/RenderElement.h>
#import <WebCore/TextIterator.h>

#if PLATFORM(IOS)
#import <WebCore/ParentalControlsContentFilter.h>
#endif

#if PLATFORM(COCOA)

namespace WebKit {

void WebPage::platformDidReceiveLoadParameters(const LoadParameters& parameters)
{
    bool databaseUpdated = LaunchServicesDatabaseManager::singleton().waitForDatabaseUpdate(5_s);
    ASSERT_UNUSED(databaseUpdated, databaseUpdated);
    if (!databaseUpdated)
        WTFLogAlways("Timed out waiting for Launch Services database update.");

    m_dataDetectionContext = parameters.dataDetectionContext;

    if (parameters.neHelperExtensionHandle)
        SandboxExtension::consumePermanently(*parameters.neHelperExtensionHandle);
    if (parameters.neSessionManagerExtensionHandle)
        SandboxExtension::consumePermanently(*parameters.neSessionManagerExtensionHandle);
    NetworkExtensionContentFilter::setHasConsumedSandboxExtensions(parameters.neHelperExtensionHandle.hasValue() && parameters.neSessionManagerExtensionHandle.hasValue());

#if PLATFORM(IOS)
    if (parameters.contentFilterExtensionHandle)
        SandboxExtension::consumePermanently(*parameters.contentFilterExtensionHandle);
    ParentalControlsContentFilter::setHasConsumedSandboxExtension(parameters.contentFilterExtensionHandle.hasValue());

    if (parameters.frontboardServiceExtensionHandle)
        SandboxExtension::consumePermanently(*parameters.frontboardServiceExtensionHandle);
#endif
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
        uniqueIdentifier = sharedManager->lastUpdatedNowPlayingInfoUniqueIdentifier().toUInt64();
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
    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::ReadOnly, HitTestRequest::Active, HitTestRequest::DisallowUserAgentShadowContent, HitTestRequest::AllowChildFrameContent };
    auto result = m_page->mainFrame().eventHandler().hitTestResultAtPoint(m_page->mainFrame().view()->windowToContents(roundedIntPoint(floatPoint)), hitType);

    auto* frame = result.innerNonSharedNode() ? result.innerNonSharedNode()->document().frame() : &m_page->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    auto rangeResult = DictionaryLookup::rangeAtHitTestResult(result);
    if (!rangeResult)
        return;

    auto [range, options] = WTFMove(*rangeResult);
    performDictionaryLookupForRange(*frame, createLiveRange(range), options, TextIndicatorPresentationTransition::Bounce);
}

void WebPage::performDictionaryLookupForSelection(Frame& frame, const VisibleSelection& selection, TextIndicatorPresentationTransition presentationTransition)
{
    auto result = DictionaryLookup::rangeForSelection(selection);
    if (!result)
        return;

    auto [range, options] = WTFMove(*result);
    performDictionaryLookupForRange(frame, createLiveRange(range), options, presentationTransition);
}

void WebPage::performDictionaryLookupOfCurrentSelection()
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    performDictionaryLookupForSelection(frame, frame.selection().selection(), TextIndicatorPresentationTransition::BounceAndCrossfade);
}
    
void WebPage::performDictionaryLookupForRange(Frame& frame, const SimpleRange& range, NSDictionary *options, TextIndicatorPresentationTransition presentationTransition)
{
    send(Messages::WebPageProxy::DidPerformDictionaryLookup(dictionaryPopupInfoForRange(frame, range, options, presentationTransition)));
}

DictionaryPopupInfo WebPage::dictionaryPopupInfoForRange(Frame& frame, const SimpleRange& range, NSDictionary *options, TextIndicatorPresentationTransition presentationTransition)
{
    Editor& editor = frame.editor();
    editor.setIsGettingDictionaryPopupInfo(true);

    // FIXME: Inefficient to call stripWhiteSpace to detect whether a string has a non-whitespace character in it.
    if (plainText(range).stripWhiteSpace().isEmpty()) {
        editor.setIsGettingDictionaryPopupInfo(false);
        return { };
    }

    auto quads = RenderObject::absoluteTextQuads(range);
    if (quads.isEmpty()) {
        editor.setIsGettingDictionaryPopupInfo(false);
        return { };
    }

    DictionaryPopupInfo dictionaryPopupInfo;

    IntRect rangeRect = frame.view()->contentsToWindow(quads[0].enclosingBoundingBox());

    const RenderStyle* style = range.startContainer().renderStyle();
    float scaledAscent = style ? style->fontMetrics().ascent() * pageScaleFactor() : 0;
    dictionaryPopupInfo.origin = FloatPoint(rangeRect.x(), rangeRect.y() + scaledAscent);
    dictionaryPopupInfo.options = options;

#if PLATFORM(MAC)
    auto attributedString = editingAttributedString(range, IncludeImages::No).string;
    auto scaledAttributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:[attributedString string]]);
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    [attributedString enumerateAttributesInRange:NSMakeRange(0, [attributedString length]) options:0 usingBlock:^(NSDictionary *attributes, NSRange range, BOOL *stop) {
        RetainPtr<NSMutableDictionary> scaledAttributes = adoptNS([attributes mutableCopy]);
        NSFont *font = [scaledAttributes objectForKey:NSFontAttributeName];
        if (font)
            font = [fontManager convertFont:font toSize:font.pointSize * pageScaleFactor()];
        if (font)
            [scaledAttributes setObject:font forKey:NSFontAttributeName];
        [scaledAttributedString addAttributes:scaledAttributes.get() range:range];
    }];
#endif // PLATFORM(MAC)

    OptionSet<TextIndicatorOption> indicatorOptions { TextIndicatorOption::UseBoundingRectAndPaintAllContentForComplexRanges };
    if (presentationTransition == TextIndicatorPresentationTransition::BounceAndCrossfade)
        indicatorOptions.add(TextIndicatorOption::IncludeSnapshotWithSelectionHighlight);
    
    auto textIndicator = TextIndicator::createWithRange(range, indicatorOptions, presentationTransition);
    if (!textIndicator) {
        editor.setIsGettingDictionaryPopupInfo(false);
        return dictionaryPopupInfo;
    }

    dictionaryPopupInfo.textIndicator = textIndicator->data();
#if PLATFORM(MAC)
    dictionaryPopupInfo.attributedString = scaledAttributedString;
#elif PLATFORM(MACCATALYST)
    dictionaryPopupInfo.attributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:plainText(range)]);
#endif

    editor.setIsGettingDictionaryPopupInfo(false);
    return dictionaryPopupInfo;
}

void WebPage::insertDictatedTextAsync(const String& text, const EditingRange& replacementEditingRange, const Vector<WebCore::DictationAlternative>& dictationAlternativeLocations, InsertTextOptions&& options)
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    Ref<Frame> protector { frame };

    if (replacementEditingRange.location != notFound) {
        auto replacementRange = EditingRange::toRange(frame, replacementEditingRange);
        if (replacementRange)
            frame.selection().setSelection(VisibleSelection { *replacementRange, SEL_DEFAULT_AFFINITY });
    }

    if (options.registerUndoGroup)
        send(Messages::WebPageProxy::RegisterInsertionUndoGrouping { });

    RefPtr<Element> focusedElement = frame.document() ? frame.document()->focusedElement() : nullptr;
    if (focusedElement && options.shouldSimulateKeyboardInput)
        focusedElement->dispatchEvent(Event::create(eventNames().keydownEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));

    ASSERT(!frame.editor().hasComposition());
    frame.editor().insertDictatedText(text, dictationAlternativeLocations, nullptr /* triggeringEvent */);

    if (focusedElement && options.shouldSimulateKeyboardInput) {
        focusedElement->dispatchEvent(Event::create(eventNames().keyupEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));
        focusedElement->dispatchEvent(Event::create(eventNames().changeEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));
    }
}

void WebPage::accessibilityTransferRemoteToken(RetainPtr<NSData> remoteToken)
{
    IPC::DataReference dataToken = IPC::DataReference(reinterpret_cast<const uint8_t*>([remoteToken bytes]), [remoteToken length]);
    send(Messages::WebPageProxy::RegisterWebProcessAccessibilityToken(dataToken));
}

#if ENABLE(APPLE_PAY)
WebPaymentCoordinator* WebPage::paymentCoordinator()
{
    if (!m_page)
        return nullptr;
    auto& client = m_page->paymentCoordinator().client();
    return is<WebPaymentCoordinator>(client) ? downcast<WebPaymentCoordinator>(&client) : nullptr;
}
#endif

void WebPage::getContentsAsAttributedString(CompletionHandler<void(const WebCore::AttributedString&)>&& completionHandler)
{
    completionHandler(attributedString(makeRangeSelectingNodeContents(*m_page->mainFrame().document())));
}

void WebPage::setRemoteObjectRegistry(WebRemoteObjectRegistry* registry)
{
    m_remoteObjectRegistry = makeWeakPtr(registry);
}

WebRemoteObjectRegistry* WebPage::remoteObjectRegistry()
{
    return m_remoteObjectRegistry.get();
}

void WebPage::updateMockAccessibilityElementAfterCommittingLoad()
{
    auto* document = mainFrame()->document();
    [m_mockAccessibilityElement setHasMainFramePlugin:document ? document->isPluginDocument() : false];
}

RetainPtr<CFDataRef> WebPage::pdfSnapshotAtSize(IntRect rect, IntSize bitmapSize, SnapshotOptions options)
{
    Frame* coreFrame = m_mainFrame->coreFrame();
    if (!coreFrame)
        return nullptr;

    FrameView* frameView = coreFrame->view();
    if (!frameView)
        return nullptr;

    auto data = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, 0));

    auto dataConsumer = adoptCF(CGDataConsumerCreateWithCFData(data.get()));
    auto mediaBox = CGRectMake(0, 0, bitmapSize.width(), bitmapSize.height());
    auto pdfContext = adoptCF(CGPDFContextCreate(dataConsumer.get(), &mediaBox, nullptr));

    int64_t remainingHeight = bitmapSize.height();
    int64_t nextRectY = rect.y();
    while (remainingHeight > 0) {
        // PDFs have a per-page height limit of 200 inches at 72dpi.
        // We'll export one PDF page at a time, up to that maximum height.
        static const int64_t maxPageHeight = 72 * 200;
        bitmapSize.setHeight(std::min(remainingHeight, maxPageHeight));
        rect.setHeight(bitmapSize.height());
        rect.setY(nextRectY);

        CGRect mediaBox = CGRectMake(0, 0, bitmapSize.width(), bitmapSize.height());
        auto mediaBoxData = adoptCF(CFDataCreate(NULL, (const UInt8 *)&mediaBox, sizeof(CGRect)));
        auto dictionary = (CFDictionaryRef)@{
            (NSString *)kCGPDFContextMediaBox : (NSData *)mediaBoxData.get()
        };

        CGPDFContextBeginPage(pdfContext.get(), dictionary);

        GraphicsContext graphicsContext { pdfContext.get() };
        graphicsContext.scale({ 1, -1 });
        graphicsContext.translate(0, -bitmapSize.height());

        paintSnapshotAtSize(rect, bitmapSize, options, *coreFrame, *frameView, graphicsContext);

        CGPDFContextEndPage(pdfContext.get());

        nextRectY += bitmapSize.height();
        remainingHeight -= maxPageHeight;
    }

    CGPDFContextClose(pdfContext.get());

    return data;
}

void WebPage::getProcessDisplayName(CompletionHandler<void(String&&)>&& completionHandler)
{
#if PLATFORM(MAC)
    completionHandler(adoptCF((CFStringRef)_LSCopyApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), _kLSDisplayNameKey)).get());
#else
    completionHandler({ });
#endif
}

void WebPage::getPlatformEditorStateCommon(const Frame& frame, EditorState& result) const
{
    if (result.isMissingPostLayoutData)
        return;

    const auto& selection = frame.selection().selection();

    if (!result.isContentEditable || selection.isNone())
        return;

    auto& postLayoutData = result.postLayoutData();
    if (auto editingStyle = EditingStyle::styleAtSelectionStart(selection)) {
        if (editingStyle->hasStyle(CSSPropertyFontWeight, "bold"_s))
            postLayoutData.typingAttributes |= AttributeBold;

        if (editingStyle->hasStyle(CSSPropertyFontStyle, "italic"_s) || editingStyle->hasStyle(CSSPropertyFontStyle, "oblique"_s))
            postLayoutData.typingAttributes |= AttributeItalics;

        if (editingStyle->hasStyle(CSSPropertyWebkitTextDecorationsInEffect, "underline"_s))
            postLayoutData.typingAttributes |= AttributeUnderline;

        if (auto* styleProperties = editingStyle->style()) {
            bool isLeftToRight = styleProperties->propertyAsValueID(CSSPropertyDirection) == CSSValueLtr;
            switch (styleProperties->propertyAsValueID(CSSPropertyTextAlign)) {
            case CSSValueRight:
            case CSSValueWebkitRight:
                postLayoutData.textAlignment = RightAlignment;
                break;
            case CSSValueLeft:
            case CSSValueWebkitLeft:
                postLayoutData.textAlignment = LeftAlignment;
                break;
            case CSSValueCenter:
            case CSSValueWebkitCenter:
                postLayoutData.textAlignment = CenterAlignment;
                break;
            case CSSValueJustify:
                postLayoutData.textAlignment = JustifiedAlignment;
                break;
            case CSSValueStart:
                postLayoutData.textAlignment = isLeftToRight ? LeftAlignment : RightAlignment;
                break;
            case CSSValueEnd:
                postLayoutData.textAlignment = isLeftToRight ? RightAlignment : LeftAlignment;
                break;
            default:
                break;
            }
            if (auto textColor = styleProperties->propertyAsColor(CSSPropertyColor))
                postLayoutData.textColor = *textColor;
        }
    }

    if (auto* enclosingListElement = enclosingList(selection.start().containerNode())) {
        if (is<HTMLUListElement>(*enclosingListElement))
            postLayoutData.enclosingListType = UnorderedList;
        else if (is<HTMLOListElement>(*enclosingListElement))
            postLayoutData.enclosingListType = OrderedList;
        else
            ASSERT_NOT_REACHED();
    }

    postLayoutData.baseWritingDirection = frame.editor().baseWritingDirectionForSelectionStart();
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
