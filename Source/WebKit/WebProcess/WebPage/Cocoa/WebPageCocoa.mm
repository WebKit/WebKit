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
#import "MessageSenderInlines.h"
#import "PluginView.h"
#import "UserMediaCaptureManager.h"
#import "WKAccessibilityWebPageObjectBase.h"
#import "WebFrame.h"
#import "WebPageProxyMessages.h"
#import "WebPasteboardOverrides.h"
#import "WebPaymentCoordinator.h"
#import "WebProcess.h"
#import "WebRemoteObjectRegistry.h"
#import <WebCore/DeprecatedGlobalSettings.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/DocumentMarkerController.h>
#import <WebCore/Editing.h>
#import <WebCore/Editor.h>
#import <WebCore/EventHandler.h>
#import <WebCore/EventNames.h>
#import <WebCore/FocusController.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/HTMLBodyElement.h>
#import <WebCore/HTMLConverter.h>
#import <WebCore/HTMLImageElement.h>
#import <WebCore/HTMLOListElement.h>
#import <WebCore/HTMLTextFormControlElement.h>
#import <WebCore/HTMLUListElement.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/ImageOverlay.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/MutableStyleProperties.h>
#import <WebCore/NetworkExtensionContentFilter.h>
#import <WebCore/NodeRenderStyle.h>
#import <WebCore/PaymentCoordinator.h>
#import <WebCore/PlatformMediaSessionManager.h>
#import <WebCore/Range.h>
#import <WebCore/RenderElement.h>
#import <WebCore/RenderLayer.h>
#import <WebCore/RenderedDocumentMarker.h>
#import <WebCore/TextIterator.h>
#import <WebCore/UTIRegistry.h>
#import <WebCore/UTIUtilities.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
#include "LibWebRTCCodecs.h"
#endif

#if PLATFORM(IOS) || PLATFORM(VISION)
#import <WebCore/ParentalControlsContentFilter.h>
#endif

#define WEBPAGE_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [webPageID=%" PRIu64 "] WebPage::" fmt, this, m_identifier.toUInt64(), ##__VA_ARGS__)

#if PLATFORM(COCOA)

namespace WebKit {

using namespace WebCore;

void WebPage::platformInitialize(const WebPageCreationParameters& parameters)
{
    platformInitializeAccessibility();

#if ENABLE(MEDIA_STREAM)
    if (auto* captureManager = WebProcess::singleton().supplement<UserMediaCaptureManager>())
        captureManager->setupCaptureProcesses(parameters.shouldCaptureAudioInUIProcess, parameters.shouldCaptureAudioInGPUProcess, parameters.shouldCaptureVideoInUIProcess, parameters.shouldCaptureVideoInGPUProcess, parameters.shouldCaptureDisplayInUIProcess, parameters.shouldCaptureDisplayInGPUProcess, m_page->settings().webRTCRemoteVideoFrameEnabled());
#endif
#if USE(LIBWEBRTC)
    LibWebRTCCodecs::setCallbacks(m_page->settings().webRTCPlatformCodecsInGPUProcessEnabled(), m_page->settings().webRTCRemoteVideoFrameEnabled());
    LibWebRTCCodecs::setWebRTCMediaPipelineAdditionalLoggingEnabled(m_page->settings().webRTCMediaPipelineAdditionalLoggingEnabled());
#endif    
#if PLATFORM(MAC)
    // In order to be able to block launchd on macOS, we need to eagerly open up a connection to CARenderServer here.
    // This is because PDF rendering on macOS requires access to CARenderServer, unless we're in Lockdown mode.
    if (!WebProcess::singleton().isLockdownModeEnabled())
        CARenderServerGetServerPort(nullptr);
#endif
#if PLATFORM(IOS_FAMILY)
    setInsertionPointColor(parameters.insertionPointColor);
#endif
    WebCore::setAdditionalSupportedImageTypes(parameters.additionalSupportedImageTypes);
    WebCore::setImageSourceAllowableTypes(WebCore::allowableImageTypes());
}

void WebPage::platformDidReceiveLoadParameters(const LoadParameters& parameters)
{
    m_dataDetectionReferenceDate = parameters.dataDetectionReferenceDate;
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
#if ENABLE(LEGACY_PDFKIT_PLUGIN)
    if (auto* pluginView = mainFramePlugIn()) {
        if (pluginView->performDictionaryLookupAtLocation(floatPoint))
            return;
    }
#endif
    
    auto* localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    if (!localMainFrame)
        return;
    // Find the frame the point is over.
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowChildFrameContent };
    auto result = localMainFrame->eventHandler().hitTestResultAtPoint(localMainFrame->view()->windowToContents(roundedIntPoint(floatPoint)), hitType);

    RefPtr frame = result.innerNonSharedNode() ? result.innerNonSharedNode()->document().frame() : &CheckedRef(m_page->focusController())->focusedOrMainFrame();
    if (!frame)
        return;

    auto rangeResult = DictionaryLookup::rangeAtHitTestResult(result);
    if (!rangeResult)
        return;

    auto [range, options] = WTFMove(*rangeResult);
    performDictionaryLookupForRange(*frame, range, options, TextIndicatorPresentationTransition::Bounce);
}

void WebPage::performDictionaryLookupForSelection(LocalFrame& frame, const VisibleSelection& selection, TextIndicatorPresentationTransition presentationTransition)
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
    
void WebPage::performDictionaryLookupForRange(LocalFrame& frame, const SimpleRange& range, NSDictionary *options, TextIndicatorPresentationTransition presentationTransition)
{
    send(Messages::WebPageProxy::DidPerformDictionaryLookup(dictionaryPopupInfoForRange(frame, range, options, presentationTransition)));
}

DictionaryPopupInfo WebPage::dictionaryPopupInfoForRange(LocalFrame& frame, const SimpleRange& range, NSDictionary *options, TextIndicatorPresentationTransition presentationTransition)
{
    Editor& editor = frame.editor();
    editor.setIsGettingDictionaryPopupInfo(true);

    if (plainText(range).find(deprecatedIsNotSpaceOrNewline) == notFound) {
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
    auto attributedString = editingAttributedString(range, IncludeImages::No).nsAttributedString();
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
    dictionaryPopupInfo.platformData.attributedString = WebCore::AttributedString::fromNSAttributedString(scaledAttributedString);
#elif PLATFORM(MACCATALYST)
    dictionaryPopupInfo.platformData.attributedString = WebCore::AttributedString::fromNSAttributedString(adoptNS([[NSMutableAttributedString alloc] initWithString:plainText(range)]));
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

    if (frame->editor().hasComposition())
        return;

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
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    RefPtr document = frame->document();
    if (!document) {
        completion({ });
        return;
    }

    auto selection = frame->selection().selection();
    auto expandedSelectionRange = VisibleSelection { selection.visibleStart().previous(CannotCrossEditingBoundary), selection.visibleEnd().next(CannotCrossEditingBoundary) }.range();
    if (!expandedSelectionRange) {
        completion({ });
        return;
    }

    auto markers = document->markers().markersInRange(*expandedSelectionRange, DocumentMarker::DictationAlternatives);
    auto contexts = WTF::compactMap(markers, [](auto& marker) -> std::optional<DictationContext> {
        if (std::holds_alternative<DocumentMarker::DictationData>(marker->data()))
            return std::get<DocumentMarker::DictationData>(marker->data()).context;
        return std::nullopt;
    });
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
    completionHandler(is<LocalFrame>(m_page->mainFrame()) ? attributedString(makeRangeSelectingNodeContents(Ref { *downcast<LocalFrame>(m_page->mainFrame()).document() })) : AttributedString());
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
    RefPtr mainFrame = dynamicDowncast<WebCore::LocalFrame>(this->mainFrame());
    RefPtr document = mainFrame ? mainFrame->document() : nullptr;
    [m_mockAccessibilityElement setHasMainFramePlugin:document ? document->isPluginDocument() : false];
}

RetainPtr<CFDataRef> WebPage::pdfSnapshotAtSize(IntRect rect, IntSize bitmapSize, SnapshotOptions options)
{
    RefPtr coreFrame = m_mainFrame->coreLocalFrame();
    if (!coreFrame)
        return nullptr;

    RefPtr frameView = coreFrame->view();
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

bool WebPage::isTransparentOrFullyClipped(const Element& element) const
{
    auto* renderer = element.renderer();
    if (!renderer)
        return false;

    auto* enclosingLayer = renderer->enclosingLayer();
    if (enclosingLayer && enclosingLayer->isTransparentRespectingParentFrames())
        return true;

    return renderer->hasNonEmptyVisibleRectRespectingParentFrames();
}

void WebPage::getPlatformEditorStateCommon(const LocalFrame& frame, EditorState& result) const
{
    if (!result.hasPostLayoutAndVisualData())
        return;

    const auto& selection = frame.selection().selection();

    if (selection.isNone())
        return;

    auto& postLayoutData = *result.postLayoutData;

    if (result.isContentEditable) {
        if (auto editingStyle = EditingStyle::styleAtSelectionStart(selection)) {
            if (editingStyle->hasStyle(CSSPropertyFontWeight, "bold"_s))
                postLayoutData.typingAttributes.add(TypingAttribute::Bold);

            if (editingStyle->hasStyle(CSSPropertyFontStyle, "italic"_s) || editingStyle->hasStyle(CSSPropertyFontStyle, "oblique"_s))
                postLayoutData.typingAttributes.add(TypingAttribute::Italics);

            if (editingStyle->hasStyle(CSSPropertyWebkitTextDecorationsInEffect, "underline"_s))
                postLayoutData.typingAttributes.add(TypingAttribute::Underline);

            if (auto* styleProperties = editingStyle->style()) {
                bool isLeftToRight = styleProperties->propertyAsValueID(CSSPropertyDirection) == CSSValueLtr;
                switch (styleProperties->propertyAsValueID(CSSPropertyTextAlign).value_or(CSSValueInvalid)) {
                case CSSValueRight:
                case CSSValueWebkitRight:
                    postLayoutData.textAlignment = TextAlignment::Right;
                    break;
                case CSSValueLeft:
                case CSSValueWebkitLeft:
                    postLayoutData.textAlignment = TextAlignment::Left;
                    break;
                case CSSValueCenter:
                case CSSValueWebkitCenter:
                    postLayoutData.textAlignment = TextAlignment::Center;
                    break;
                case CSSValueJustify:
                    postLayoutData.textAlignment = TextAlignment::Justified;
                    break;
                case CSSValueStart:
                    postLayoutData.textAlignment = isLeftToRight ? TextAlignment::Left : TextAlignment::Right;
                    break;
                case CSSValueEnd:
                    postLayoutData.textAlignment = isLeftToRight ? TextAlignment::Right : TextAlignment::Left;
                    break;
                default:
                    break;
                }
                if (auto textColor = styleProperties->propertyAsColor(CSSPropertyColor))
                    postLayoutData.textColor = *textColor;
            }
        }

        if (RefPtr enclosingListElement = enclosingList(RefPtr { selection.start().containerNode() }.get())) {
            if (is<HTMLUListElement>(*enclosingListElement))
                postLayoutData.enclosingListType = ListType::UnorderedList;
            else if (is<HTMLOListElement>(*enclosingListElement))
                postLayoutData.enclosingListType = ListType::OrderedList;
            else
                ASSERT_NOT_REACHED();
        }

        postLayoutData.baseWritingDirection = frame.editor().baseWritingDirectionForSelectionStart();
    }

    if (RefPtr editableRootOrFormControl = enclosingTextFormControl(selection.start()) ?: selection.rootEditableElement()) {
#if PLATFORM(IOS_FAMILY)
        auto& visualData = *result.visualData;
        visualData.selectionClipRect = rootViewInteractionBounds(*editableRootOrFormControl);
#endif
        postLayoutData.editableRootIsTransparentOrFullyClipped = result.isContentEditable && isTransparentOrFullyClipped(*editableRootOrFormControl);
    }
}

void WebPage::getPDFFirstPageSize(WebCore::FrameIdentifier frameID, CompletionHandler<void(WebCore::FloatSize)>&& completionHandler)
{
#if !ENABLE(LEGACY_PDFKIT_PLUGIN)
    return completionHandler({ });
#else
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame)
        return completionHandler({ });

    auto* pluginView = pluginViewForFrame(webFrame->coreLocalFrame());
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

    constexpr auto restoreSelectionOptions = FrameSelection::defaultSetSelectionOptions(UserTriggered::Yes);
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
    Ref frame = m_page->focusController().focusedOrMainFrame();
    if (frame->selection().isNone())
        return completionHandler(false);
    frame->editor().readSelectionFromPasteboard(pasteboardName);
    completionHandler(true);
}

URL WebPage::applyLinkDecorationFiltering(const URL& url, LinkDecorationFilteringTrigger trigger)
{
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    if (m_linkDecorationFilteringData.isEmpty() && m_domainScopedLinkDecorationFilteringData.isEmpty()) {
        RELEASE_LOG_ERROR(ResourceLoadStatistics, "Unable to filter tracking query parameters (missing data)");
        return url;
    }

    RefPtr mainFrame = m_mainFrame->coreLocalFrame();
    if (!mainFrame)
        return url;

    auto isLinkDecorationFilteringEnabled = [&](const DocumentLoader* loader) {
        if (!loader)
            return false;
        auto effectivePolicies = trigger == LinkDecorationFilteringTrigger::Navigation ? loader->originatorAdvancedPrivacyProtections() : loader->advancedPrivacyProtections();
        return effectivePolicies.contains(AdvancedPrivacyProtections::LinkDecorationFiltering);
    };

    bool shouldApplyLinkDecorationFiltering = [&] {
        if (isLinkDecorationFilteringEnabled(RefPtr { mainFrame->loader().documentLoader() }.get()))
            return true;

        if (isLinkDecorationFilteringEnabled(RefPtr { mainFrame->loader().provisionalDocumentLoader() }.get()))
            return true;

        return isLinkDecorationFilteringEnabled(RefPtr { mainFrame->loader().policyDocumentLoader() }.get());
    }();

    if (!shouldApplyLinkDecorationFiltering)
        return url;

    if (!url.hasQuery())
        return url;

    auto sanitizedURL = url;

    auto domainScopedQueryParameters = m_domainScopedLinkDecorationFilteringData.get(RegistrableDomain { sanitizedURL });
    auto removedParameters = WTF::removeQueryParameters(sanitizedURL, [&](auto& parameter) {
        return m_linkDecorationFilteringData.contains(parameter) || domainScopedQueryParameters.contains(parameter);
    });

    if (!removedParameters.isEmpty() && trigger != LinkDecorationFilteringTrigger::Unspecified) {
        if (trigger == LinkDecorationFilteringTrigger::Navigation)
            send(Messages::WebPageProxy::DidApplyLinkDecorationFiltering(url, sanitizedURL));
        auto removedParametersString = makeStringByJoining(removedParameters, ", "_s);
        WEBPAGE_RELEASE_LOG(ResourceLoadStatistics, "Blocked known tracking query parameters: %s", removedParametersString.utf8().data());
    }

    return sanitizedURL;
#else
    return url;
#endif
}

URL WebPage::allowedQueryParametersForAdvancedPrivacyProtections(const URL& url)
{
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    if (m_allowedQueryParametersForAdvancedPrivacyProtections.isEmpty()) {
        RELEASE_LOG_ERROR(ResourceLoadStatistics, "Unable to allow query parameters (missing data)");
        return url;
    }

    if (!url.hasQuery() && !url.hasFragmentIdentifier())
        return url;

    auto sanitizedURL = url;

    auto allowedParameters = m_allowedQueryParametersForAdvancedPrivacyProtections.get(RegistrableDomain { sanitizedURL });

    if (!allowedParameters.contains("#"_s))
        sanitizedURL.removeFragmentIdentifier();

    WTF::removeQueryParameters(sanitizedURL, [&](auto& parameter) {
        return !allowedParameters.contains(parameter);
    });

    return sanitizedURL;
#else
    return url;
#endif
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
