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

#import "EditorState.h"
#import "InsertTextOptions.h"
#import "LoadParameters.h"
#import "PluginView.h"
#import "UserMediaCaptureManager.h"
#import "WKAccessibilityWebPageObjectBase.h"
#import "WebPageProxyMessages.h"
#import "WebPasteboardOverrides.h"
#import "WebPaymentCoordinator.h"
#import "WebRemoteObjectRegistry.h"
#import <WebCore/DeprecatedGlobalSettings.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/DocumentMarkerController.h>
#import <WebCore/Editing.h>
#import <WebCore/Editor.h>
#import <WebCore/EventHandler.h>
#import <WebCore/EventNames.h>
#import <WebCore/FocusController.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/HTMLBodyElement.h>
#import <WebCore/HTMLConverter.h>
#import <WebCore/HTMLImageElement.h>
#import <WebCore/HTMLOListElement.h>
#import <WebCore/HTMLUListElement.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/ImageOverlay.h>
#import <WebCore/NetworkExtensionContentFilter.h>
#import <WebCore/NodeRenderStyle.h>
#import <WebCore/PaymentCoordinator.h>
#import <WebCore/PlatformMediaSessionManager.h>
#import <WebCore/Range.h>
#import <WebCore/RenderElement.h>
#import <WebCore/RenderedDocumentMarker.h>
#import <WebCore/TextIterator.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>

#if PLATFORM(IOS)
#import <WebCore/ParentalControlsContentFilter.h>
#endif

#define WEBPAGE_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [webPageID=%" PRIu64 "] WebPage::" fmt, this, m_identifier.toUInt64(), ##__VA_ARGS__)

#if PLATFORM(COCOA)

namespace WebKit {

void WebPage::platformInitialize(const WebPageCreationParameters& parameters)
{
    platformInitializeAccessibility();

#if ENABLE(MEDIA_STREAM)
    if (auto* captureManager = WebProcess::singleton().supplement<UserMediaCaptureManager>())
        captureManager->setupCaptureProcesses(parameters.shouldCaptureAudioInUIProcess, parameters.shouldCaptureAudioInGPUProcess, parameters.shouldCaptureVideoInUIProcess, parameters.shouldCaptureVideoInGPUProcess, parameters.shouldCaptureDisplayInUIProcess, parameters.shouldCaptureDisplayInGPUProcess, m_page->settings().webRTCRemoteVideoFrameEnabled());
#endif
}

void WebPage::platformDidReceiveLoadParameters(const LoadParameters& parameters)
{
    m_dataDetectionContext = parameters.dataDetectionContext;

#if !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    consumeNetworkExtensionSandboxExtensions(parameters.networkExtensionSandboxExtensionHandles);
#if PLATFORM(IOS)
    if (parameters.contentFilterExtensionHandle)
        SandboxExtension::consumePermanently(*parameters.contentFilterExtensionHandle);
    ParentalControlsContentFilter::setHasConsumedSandboxExtension(parameters.contentFilterExtensionHandle.has_value());

    if (parameters.frontboardServiceExtensionHandle)
        SandboxExtension::consumePermanently(*parameters.frontboardServiceExtensionHandle);
#endif // PLATFORM(IOS)
#endif // !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
}

void WebPage::requestActiveNowPlayingSessionInfo(CompletionHandler<void(bool, bool, const String&, double, double, uint64_t)>&& completionHandler)
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

    completionHandler(hasActiveSession, registeredAsNowPlayingApplication, title, duration, elapsedTime, uniqueIdentifier);
}
    
void WebPage::performDictionaryLookupAtLocation(const FloatPoint& floatPoint)
{
#if ENABLE(PDFKIT_PLUGIN)
    if (auto* pluginView = mainFramePlugIn()) {
        if (pluginView->performDictionaryLookupAtLocation(floatPoint))
            return;
    }
#endif
    
    // Find the frame the point is over.
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowChildFrameContent };
    auto result = m_page->mainFrame().eventHandler().hitTestResultAtPoint(m_page->mainFrame().view()->windowToContents(roundedIntPoint(floatPoint)), hitType);

    RefPtr frame = result.innerNonSharedNode() ? result.innerNonSharedNode()->document().frame() : &CheckedRef(m_page->focusController())->focusedOrMainFrame();
    if (!frame)
        return;

    auto rangeResult = DictionaryLookup::rangeAtHitTestResult(result);
    if (!rangeResult)
        return;

    auto [range, options] = WTFMove(*rangeResult);
    performDictionaryLookupForRange(*frame, range, options, TextIndicatorPresentationTransition::Bounce);
}

void WebPage::performDictionaryLookupForSelection(Frame& frame, const VisibleSelection& selection, TextIndicatorPresentationTransition presentationTransition)
{
    auto result = DictionaryLookup::rangeForSelection(selection);
    if (!result)
        return;

    auto [range, options] = WTFMove(*result);
    performDictionaryLookupForRange(frame, range, options, presentationTransition);
}

void WebPage::performDictionaryLookupOfCurrentSelection()
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    performDictionaryLookupForSelection(frame, frame->selection().selection(), TextIndicatorPresentationTransition::BounceAndCrossfade);
}
    
void WebPage::performDictionaryLookupForRange(Frame& frame, const SimpleRange& range, NSDictionary *options, TextIndicatorPresentationTransition presentationTransition)
{
    send(Messages::WebPageProxy::DidPerformDictionaryLookup(dictionaryPopupInfoForRange(frame, range, options, presentationTransition)));
}

DictionaryPopupInfo WebPage::dictionaryPopupInfoForRange(Frame& frame, const SimpleRange& range, NSDictionary *options, TextIndicatorPresentationTransition presentationTransition)
{
    Editor& editor = frame.editor();
    editor.setIsGettingDictionaryPopupInfo(true);

    if (plainText(range).find(isNotSpaceOrNewline) == notFound) {
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
    float scaledAscent = style ? style->metricsOfPrimaryFont().ascent() * pageScaleFactor() : 0;
    dictionaryPopupInfo.origin = FloatPoint(rangeRect.x(), rangeRect.y() + scaledAscent);
    dictionaryPopupInfo.platformData.options = options;

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
    if (ImageOverlay::isInsideOverlay(range))
        indicatorOptions.add({ TextIndicatorOption::PaintAllContent, TextIndicatorOption::PaintBackgrounds });

    if (presentationTransition == TextIndicatorPresentationTransition::BounceAndCrossfade)
        indicatorOptions.add(TextIndicatorOption::IncludeSnapshotWithSelectionHighlight);
    
    auto textIndicator = TextIndicator::createWithRange(range, indicatorOptions, presentationTransition);
    if (!textIndicator) {
        editor.setIsGettingDictionaryPopupInfo(false);
        return dictionaryPopupInfo;
    }

    dictionaryPopupInfo.textIndicator = textIndicator->data();
#if PLATFORM(MAC)
    dictionaryPopupInfo.platformData.attributedString = scaledAttributedString;
#elif PLATFORM(MACCATALYST)
    dictionaryPopupInfo.platformData.attributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:plainText(range)]);
#endif

    editor.setIsGettingDictionaryPopupInfo(false);
    return dictionaryPopupInfo;
}

void WebPage::insertDictatedTextAsync(const String& text, const EditingRange& replacementEditingRange, const Vector<WebCore::DictationAlternative>& dictationAlternativeLocations, InsertTextOptions&& options)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();

    if (replacementEditingRange.location != notFound) {
        auto replacementRange = EditingRange::toRange(frame, replacementEditingRange);
        if (replacementRange)
            frame->selection().setSelection(VisibleSelection { *replacementRange });
    }

    if (options.registerUndoGroup)
        send(Messages::WebPageProxy::RegisterInsertionUndoGrouping { });

    RefPtr<Element> focusedElement = frame->document() ? frame->document()->focusedElement() : nullptr;
    if (focusedElement && options.shouldSimulateKeyboardInput)
        focusedElement->dispatchEvent(Event::create(eventNames().keydownEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));

    ASSERT(!frame->editor().hasComposition());
    frame->editor().insertDictatedText(text, dictationAlternativeLocations, nullptr /* triggeringEvent */);

    if (focusedElement && options.shouldSimulateKeyboardInput) {
        focusedElement->dispatchEvent(Event::create(eventNames().keyupEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));
        focusedElement->dispatchEvent(Event::create(eventNames().changeEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));
    }
}

void WebPage::addDictationAlternative(const String& text, DictationContext context, CompletionHandler<void(bool)>&& completion)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    RefPtr document = frame->document();
    if (!document) {
        completion(false);
        return;
    }

    auto selection = frame->selection().selection();
    RefPtr editableRoot = selection.rootEditableElement();
    if (!editableRoot) {
        completion(false);
        return;
    }

    auto firstEditablePosition = firstPositionInNode(editableRoot.get());
    auto selectionEnd = selection.end();
    auto searchRange = makeSimpleRange(firstEditablePosition, selectionEnd);
    if (!searchRange) {
        completion(false);
        return;
    }

    auto targetOffset = characterCount(*searchRange);
    targetOffset -= std::min<uint64_t>(targetOffset, text.length());
    auto matchRange = findClosestPlainText(*searchRange, text, { Backwards, DoNotRevealSelection }, targetOffset);
    if (matchRange.collapsed()) {
        completion(false);
        return;
    }

    document->markers().addMarker(matchRange, DocumentMarker::DictationAlternatives, { DocumentMarker::DictationData { context, text } });
    completion(true);
}

void WebPage::dictationAlternativesAtSelection(CompletionHandler<void(Vector<DictationContext>&&)>&& completion)
{
    Vector<DictationContext> contexts;
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    RefPtr document = frame->document();
    if (!document) {
        completion(WTFMove(contexts));
        return;
    }

    auto selection = frame->selection().selection();
    auto expandedSelectionRange = VisibleSelection { selection.visibleStart().previous(CannotCrossEditingBoundary), selection.visibleEnd().next(CannotCrossEditingBoundary) }.range();
    if (!expandedSelectionRange) {
        completion(WTFMove(contexts));
        return;
    }

    auto markers = document->markers().markersInRange(*expandedSelectionRange, DocumentMarker::DictationAlternatives);
    contexts.reserveInitialCapacity(markers.size());
    for (auto* marker : markers) {
        if (std::holds_alternative<DocumentMarker::DictationData>(marker->data()))
            contexts.uncheckedAppend(std::get<DocumentMarker::DictationData>(marker->data()).context);
    }
    completion(WTFMove(contexts));
}

void WebPage::clearDictationAlternatives(Vector<DictationContext>&& contexts)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    RefPtr document = frame->document();
    if (!document)
        return;

    HashSet<DictationContext> setOfContextsToRemove;
    setOfContextsToRemove.reserveInitialCapacity(contexts.size());
    for (auto context : contexts)
        setOfContextsToRemove.add(context);

    auto documentRange = makeRangeSelectingNodeContents(*document);
    document->markers().filterMarkers(documentRange, [&] (auto& marker) {
        if (!std::holds_alternative<DocumentMarker::DictationData>(marker.data()))
            return FilterMarkerResult::Keep;
        return setOfContextsToRemove.contains(std::get<WebCore::DocumentMarker::DictationData>(marker.data()).context) ? FilterMarkerResult::Remove : FilterMarkerResult::Keep;
    }, DocumentMarker::DictationAlternatives);
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
    return dynamicDowncast<WebPaymentCoordinator>(m_page->paymentCoordinator().client());
}
#endif

void WebPage::getContentsAsAttributedString(CompletionHandler<void(const WebCore::AttributedString&)>&& completionHandler)
{
    completionHandler(attributedString(makeRangeSelectingNodeContents(*m_page->mainFrame().document())));
}

void WebPage::setRemoteObjectRegistry(WebRemoteObjectRegistry* registry)
{
    m_remoteObjectRegistry = registry;
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

        GraphicsContextCG graphicsContext { pdfContext.get() };
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
#if ENABLE(SET_WEBCONTENT_PROCESS_INFORMATION_IN_NETWORK_PROCESS)
    WebProcess::singleton().getProcessDisplayName(WTFMove(completionHandler));
#else
    completionHandler(adoptCF((CFStringRef)_LSCopyApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), _kLSDisplayNameKey)).get());
#endif
#else
    completionHandler({ });
#endif
}

void WebPage::getPlatformEditorStateCommon(const Frame& frame, EditorState& result) const
{
    if (!result.hasPostLayoutAndVisualData())
        return;

    const auto& selection = frame.selection().selection();

    if (!result.isContentEditable || selection.isNone())
        return;

    auto& postLayoutData = *result.postLayoutData;
    if (auto editingStyle = EditingStyle::styleAtSelectionStart(selection)) {
        if (editingStyle->hasStyle(CSSPropertyFontWeight, "bold"_s))
            postLayoutData.typingAttributes |= AttributeBold;

        if (editingStyle->hasStyle(CSSPropertyFontStyle, "italic"_s) || editingStyle->hasStyle(CSSPropertyFontStyle, "oblique"_s))
            postLayoutData.typingAttributes |= AttributeItalics;

        if (editingStyle->hasStyle(CSSPropertyWebkitTextDecorationsInEffect, "underline"_s))
            postLayoutData.typingAttributes |= AttributeUnderline;

        if (auto* styleProperties = editingStyle->style()) {
            bool isLeftToRight = styleProperties->propertyAsValueID(CSSPropertyDirection) == CSSValueLtr;
            switch (styleProperties->propertyAsValueID(CSSPropertyTextAlign).value_or(CSSValueInvalid)) {
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

#if !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
void WebPage::consumeNetworkExtensionSandboxExtensions(const Vector<SandboxExtension::Handle>& networkExtensionsHandles)
{
#if ENABLE(CONTENT_FILTERING)
    SandboxExtension::consumePermanently(networkExtensionsHandles);
    NetworkExtensionContentFilter::setHasConsumedSandboxExtensions(networkExtensionsHandles.size());
#else
    UNUSED_PARAM(networkExtensionsHandles);
#endif
}
#endif

void WebPage::getPDFFirstPageSize(WebCore::FrameIdentifier frameID, CompletionHandler<void(WebCore::FloatSize)>&& completionHandler)
{
#if !ENABLE(PDFKIT_PLUGIN)
    return completionHandler({ });
#else
    auto* webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame)
        return completionHandler({ });

    auto* pluginView = pluginViewForFrame(webFrame->coreFrame());
    if (!pluginView)
        return completionHandler({ });
    
    completionHandler(FloatSize(pluginView->pdfDocumentSizeForPrinting()));
#endif
}

#if ENABLE(DATA_DETECTION)

void WebPage::handleClickForDataDetectionResult(const DataDetectorElementInfo& info, const IntPoint& clickLocation)
{
    send(Messages::WebPageProxy::HandleClickForDataDetectionResult(info, clickLocation));
}

#endif

static String& replaceSelectionPasteboardName()
{
    static NeverDestroyed<String> string("ReplaceSelectionPasteboard"_s);
    return string;
}

class OverridePasteboardForSelectionReplacement {
    WTF_MAKE_NONCOPYABLE(OverridePasteboardForSelectionReplacement);
    WTF_MAKE_FAST_ALLOCATED;
public:
    OverridePasteboardForSelectionReplacement(const Vector<String>& types, const IPC::DataReference& data)
        : m_types(types)
    {
        for (auto& type : types)
            WebPasteboardOverrides::sharedPasteboardOverrides().addOverride(replaceSelectionPasteboardName(), type, { data });
    }

    ~OverridePasteboardForSelectionReplacement()
    {
        for (auto& type : m_types)
            WebPasteboardOverrides::sharedPasteboardOverrides().removeOverride(replaceSelectionPasteboardName(), type);
    }

private:
    Vector<String> m_types;
};

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

void WebPage::replaceImageForRemoveBackground(const ElementContext& elementContext, const Vector<String>& types, const IPC::DataReference& data)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    auto element = elementForContext(elementContext);
    if (!element || !element->isContentEditable())
        return;

    Ref document = element->document();
    if (frame->document() != document.ptr())
        return;

    auto originalSelection = frame->selection().selection();
    RefPtr selectionHost = originalSelection.rootEditableElement() ?: document->body();
    if (!selectionHost)
        return;

    constexpr OptionSet iteratorOptions = TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions;
    std::optional<CharacterRange> rangeToRestore;
    uint64_t numberOfCharactersInSelectionHost = 0;
    if (auto range = originalSelection.range()) {
        auto selectionHostRangeBeforeReplacement = makeRangeSelectingNodeContents(*selectionHost);
        rangeToRestore = characterRange(selectionHostRangeBeforeReplacement, *range, iteratorOptions);
        numberOfCharactersInSelectionHost = characterCount(selectionHostRangeBeforeReplacement, iteratorOptions);
    }

    {
        OverridePasteboardForSelectionReplacement overridePasteboard { types, data };
        IgnoreSelectionChangeForScope ignoreSelectionChanges { frame.get() };
        frame->editor().replaceNodeFromPasteboard(*element, replaceSelectionPasteboardName(), EditAction::RemoveBackground);

        auto position = frame->selection().selection().visibleStart();
        if (auto imageRange = makeSimpleRange(WebCore::VisiblePositionRange { position.previous(), position })) {
            for (WebCore::TextIterator iterator { *imageRange, { } }; !iterator.atEnd(); iterator.advance()) {
                if (RefPtr image = dynamicDowncast<HTMLImageElement>(iterator.node())) {
                    m_elementsToExcludeFromRemoveBackground.add(*image);
                    break;
                }
            }
        }
    }

    constexpr auto restoreSelectionOptions = FrameSelection::defaultSetSelectionOptions(UserTriggered);
    if (!originalSelection.isNoneOrOrphaned()) {
        frame->selection().setSelection(originalSelection, restoreSelectionOptions);
        return;
    }

    if (!rangeToRestore || !selectionHost->isConnected())
        return;

    auto selectionHostRange = makeRangeSelectingNodeContents(*selectionHost);
    if (numberOfCharactersInSelectionHost != characterCount(selectionHostRange, iteratorOptions)) {
        // FIXME: We don't attempt to restore the selection if the replaced element contains a different
        // character count than the content that replaces it, since this codepath is currently only used
        // to replace a single non-text element with another. If this is used to replace text content in
        // the future, we should adjust the `rangeToRestore` to fit the newly inserted content.
        return;
    }

    // The node replacement may have orphaned the original selection range; in this case, try to restore
    // the original selected character range.
    auto newSelectionRange = resolveCharacterRange(selectionHostRange, *rangeToRestore, iteratorOptions);
    frame->selection().setSelection(newSelectionRange, restoreSelectionOptions);
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

void WebPage::replaceSelectionWithPasteboardData(const Vector<String>& types, const IPC::DataReference& data)
{
    OverridePasteboardForSelectionReplacement overridePasteboard { types, data };
    readSelectionFromPasteboard(replaceSelectionPasteboardName(), [](bool) { });
}

void WebPage::readSelectionFromPasteboard(const String& pasteboardName, CompletionHandler<void(bool&&)>&& completionHandler)
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    if (frame.selection().isNone())
        return completionHandler(false);
    frame.editor().readSelectionFromPasteboard(pasteboardName);
    completionHandler(true);
}

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/WebPageCocoaAdditions.mm>)
#include <WebKitAdditions/WebPageCocoaAdditions.mm>
#else
URL WebPage::sanitizeLookalikeCharacters(const URL& url) const
{
    return url;
}
#endif

} // namespace WebKit

#endif // PLATFORM(COCOA)
