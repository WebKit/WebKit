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
#import "GPUProcessConnection.h"
#import "InsertTextOptions.h"
#import "LoadParameters.h"
#import "MessageSenderInlines.h"
#import "PDFPlugin.h"
#import "PluginView.h"
#import "TextAnimationController.h"
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
#import <WebCore/DocumentInlines.h>
#import <WebCore/DocumentMarkerController.h>
#import <WebCore/DragImage.h>
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
#import <WebCore/ImageUtilities.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/MutableStyleProperties.h>
#import <WebCore/NetworkExtensionContentFilter.h>
#import <WebCore/NodeRenderStyle.h>
#import <WebCore/NowPlayingInfo.h>
#import <WebCore/PaymentCoordinator.h>
#import <WebCore/PlatformMediaSessionManager.h>
#import <WebCore/Range.h>
#import <WebCore/RenderElement.h>
#import <WebCore/RenderLayer.h>
#import <WebCore/RenderedDocumentMarker.h>
#import <WebCore/TextIterator.h>
#import <WebCore/UTIRegistry.h>
#import <WebCore/UTIUtilities.h>
#import <WebCore/markup.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cf/VectorCF.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/spi/darwin/SandboxSPI.h>

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
#include "LibWebRTCCodecs.h"
#endif

#if PLATFORM(IOS) || PLATFORM(VISION)
#import <WebCore/ParentalControlsContentFilter.h>
#endif

#if USE(EXTENSIONKIT)
#import "WKProcessExtension.h"
#endif

#define WEBPAGE_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [webPageID=%" PRIu64 "] WebPage::" fmt, this, m_identifier.toUInt64(), ##__VA_ARGS__)

#if PLATFORM(COCOA)

namespace WebKit {

using namespace WebCore;

void WebPage::platformInitialize(const WebPageCreationParameters& parameters)
{
    platformInitializeAccessibility();

#if ENABLE(MEDIA_STREAM)
    if (auto* captureManager = WebProcess::singleton().supplement<UserMediaCaptureManager>()) {
        captureManager->setupCaptureProcesses(parameters.shouldCaptureAudioInUIProcess, parameters.shouldCaptureAudioInGPUProcess, parameters.shouldCaptureVideoInUIProcess, parameters.shouldCaptureVideoInGPUProcess, parameters.shouldCaptureDisplayInUIProcess, parameters.shouldCaptureDisplayInGPUProcess,
#if ENABLE(WEB_RTC)
            m_page->settings().webRTCRemoteVideoFrameEnabled()
#else
            false
#endif // ENABLE(WEB_RTC)
        );
    }
#endif // ENABLE(MEDIA_STREAM)
#if USE(LIBWEBRTC)
    LibWebRTCCodecs::setCallbacks(m_page->settings().webRTCPlatformCodecsInGPUProcessEnabled(), m_page->settings().webRTCRemoteVideoFrameEnabled());
    LibWebRTCCodecs::setWebRTCMediaPipelineAdditionalLoggingEnabled(m_page->settings().webRTCMediaPipelineAdditionalLoggingEnabled());
#endif

#if PLATFORM(MAC)
    // In order to be able to block launchd on macOS, we need to eagerly open up a connection to CARenderServer here.
    // This is because PDF rendering on macOS requires access to CARenderServer, unless unified PDF is enabled.
    // In Lockdown mode we always block access to CARenderServer.
    bool pdfRenderingRequiresRenderServerAccess = true;
#if ENABLE(UNIFIED_PDF)
    pdfRenderingRequiresRenderServerAccess = !m_page->settings().unifiedPDFEnabled();
#endif
    if (pdfRenderingRequiresRenderServerAccess && !WebProcess::singleton().isLockdownModeEnabled())
        CARenderServerGetServerPort(nullptr);
#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)
    setInsertionPointColor(parameters.insertionPointColor);
    setHardwareKeyboardState(parameters.hardwareKeyboardState);
#endif
    WebCore::setAdditionalSupportedImageTypes(parameters.additionalSupportedImageTypes);
    WebCore::setImageSourceAllowableTypes(WebCore::allowableImageTypes());
}

#if HAVE(SANDBOX_STATE_FLAGS)
void WebPage::setHasLaunchedWebContentProcess()
{
    static bool hasSetLaunchVariable = false;
    if (!hasSetLaunchVariable) {
        auto auditToken = WebProcess::singleton().auditTokenForSelf();
#if USE(EXTENSIONKIT)
        if (WKProcessExtension.sharedInstance)
            [WKProcessExtension.sharedInstance lockdownSandbox:@"1.0"];
#endif
        sandbox_enable_state_flag("local:WebContentProcessLaunched", *auditToken);
        hasSetLaunchVariable = true;
    }
}
#endif

void WebPage::platformDidReceiveLoadParameters(const LoadParameters& parameters)
{
    WebCore::PublicSuffixStore::singleton().addPublicSuffix(parameters.publicSuffix);
    m_dataDetectionReferenceDate = parameters.dataDetectionReferenceDate;
}

void WebPage::requestActiveNowPlayingSessionInfo(CompletionHandler<void(bool, WebCore::NowPlayingInfo&&)>&& completionHandler)
{
    if (auto* sharedManager = WebCore::PlatformMediaSessionManager::singletonIfExists()) {
        if (auto nowPlayingInfo = sharedManager->nowPlayingInfo()) {
            bool registeredAsNowPlayingApplication = sharedManager->registeredAsNowPlayingApplication();
            completionHandler(registeredAsNowPlayingApplication, WTFMove(*nowPlayingInfo));
            return;
        }
    }

    completionHandler(false, { });
}

#if ENABLE(PDF_PLUGIN)
bool WebPage::shouldUsePDFPlugin(const String& contentType, StringView path) const
{
#if ENABLE(PDFJS)
    if (corePage()->settings().pdfJSViewerEnabled())
        return false;
#endif

    bool pluginEnabled = false;
#if ENABLE(LEGACY_PDFKIT_PLUGIN)
    pluginEnabled |= pdfPluginEnabled() && PDFPlugin::pdfKitLayerControllerIsAvailable();
#endif
#if ENABLE(UNIFIED_PDF)
    pluginEnabled |= corePage()->settings().unifiedPDFEnabled();
#endif
    if (!pluginEnabled)
        return false;

    return MIMETypeRegistry::isPDFMIMEType(contentType) || (contentType.isEmpty() && path.endsWithIgnoringASCIICase(".pdf"_s));
}
#endif

void WebPage::performDictionaryLookupAtLocation(const FloatPoint& floatPoint)
{
#if ENABLE(PDF_PLUGIN)
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

    RefPtr frame = result.innerNonSharedNode() ? result.innerNonSharedNode()->document().frame() : m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    auto rangeResult = DictionaryLookup::rangeAtHitTestResult(result);
    if (!rangeResult)
        return;

    performDictionaryLookupForRange(*frame, *rangeResult, TextIndicatorPresentationTransition::Bounce);
}

void WebPage::performDictionaryLookupForSelection(LocalFrame& frame, const VisibleSelection& selection, TextIndicatorPresentationTransition presentationTransition)
{
    auto range = DictionaryLookup::rangeForSelection(selection);
    if (!range)
        return;

    performDictionaryLookupForRange(frame, *range, presentationTransition);
}

void WebPage::performDictionaryLookupOfCurrentSelection()
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    performDictionaryLookupForSelection(*frame, frame->selection().selection(), TextIndicatorPresentationTransition::BounceAndCrossfade);
}

void WebPage::performDictionaryLookupForRange(LocalFrame& frame, const SimpleRange& range, TextIndicatorPresentationTransition presentationTransition)
{
    send(Messages::WebPageProxy::DidPerformDictionaryLookup(dictionaryPopupInfoForRange(frame, range, presentationTransition)));
}

DictionaryPopupInfo WebPage::dictionaryPopupInfoForRange(LocalFrame& frame, const SimpleRange& range, TextIndicatorPresentationTransition presentationTransition)
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
    float scaledAscent = style ? style->metricsOfPrimaryFont().intAscent() * pageScaleFactor() : 0;
    dictionaryPopupInfo.origin = FloatPoint(rangeRect.x(), rangeRect.y() + scaledAscent);

#if PLATFORM(MAC)
    auto attributedString = editingAttributedString(range, { }).nsAttributedString();
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
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

    if (replacementEditingRange.location != notFound) {
        auto replacementRange = EditingRange::toRange(*frame, replacementEditingRange);
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
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

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
    auto matchRange = findClosestPlainText(*searchRange, text, { FindOption::Backwards, FindOption::DoNotRevealSelection }, targetOffset);
    if (matchRange.collapsed()) {
        completion(false);
        return;
    }

    document->markers().addMarker(matchRange, DocumentMarker::Type::DictationAlternatives, { DocumentMarker::DictationData { context, text } });
    completion(true);
}

void WebPage::dictationAlternativesAtSelection(CompletionHandler<void(Vector<DictationContext>&&)>&& completion)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

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

    auto markers = document->markers().markersInRange(*expandedSelectionRange, DocumentMarker::Type::DictationAlternatives);
    auto contexts = WTF::compactMap(markers, [](auto& marker) -> std::optional<DictationContext> {
        if (std::holds_alternative<DocumentMarker::DictationData>(marker->data()))
            return std::get<DocumentMarker::DictationData>(marker->data()).context;
        return std::nullopt;
    });
    completion(WTFMove(contexts));
}

void WebPage::clearDictationAlternatives(Vector<DictationContext>&& contexts)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

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
    }, DocumentMarker::Type::DictationAlternatives);
}

void WebPage::accessibilityTransferRemoteToken(RetainPtr<NSData> remoteToken)
{
    send(Messages::WebPageProxy::RegisterWebProcessAccessibilityToken(span(remoteToken.get())));
}

void WebPage::accessibilityManageRemoteElementStatus(bool registerStatus, int processIdentifier)
{
#if PLATFORM(MAC)
    if (registerStatus)
        [NSAccessibilityRemoteUIElement registerRemoteUIProcessIdentifier:processIdentifier];
    else
        [NSAccessibilityRemoteUIElement unregisterRemoteUIProcessIdentifier:processIdentifier];
#else
    UNUSED_PARAM(registerStatus);
    UNUSED_PARAM(processIdentifier);
#endif
}

void WebPage::bindRemoteAccessibilityFrames(int processIdentifier, WebCore::FrameIdentifier frameID, Vector<uint8_t> dataToken, CompletionHandler<void(Vector<uint8_t>, int)>&& completionHandler)
{
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame) {
        ASSERT_NOT_REACHED();
        return completionHandler({ }, 0);
    }

    RefPtr coreLocalFrame = webFrame->coreLocalFrame();
    if (!coreLocalFrame) {
        ASSERT_NOT_REACHED();
        return completionHandler({ }, 0);
    }

    auto* renderer = coreLocalFrame->contentRenderer();
    if (!renderer) {
        ASSERT_NOT_REACHED();
        return completionHandler({ }, 0);
    }

    registerRemoteFrameAccessibilityTokens(processIdentifier, dataToken.span());

    // Get our remote token data and send back to the RemoteFrame.
    completionHandler({ span(accessibilityRemoteTokenData().get()) }, getpid());
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
    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    completionHandler(localFrame ? attributedString(makeRangeSelectingNodeContents(Ref { *localFrame->document() }), IgnoreUserSelectNone::No) : AttributedString { });
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

bool WebPage::isTransparentOrFullyClipped(const Node& node) const
{
    CheckedPtr renderer = node.renderer();
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
        postLayoutData.canEnableWritingSuggestions = [&] {
            if (!selection.canEnableWritingSuggestions())
                return false;

            if (!m_lastNodeBeforeWritingSuggestions)
                return true;

            RefPtr currentNode = frame.editor().nodeBeforeWritingSuggestions();
            return !currentNode || m_lastNodeBeforeWritingSuggestions == currentNode.get();
        }();
    }

    RefPtr enclosingFormControl = enclosingTextFormControl(selection.start());
    if (RefPtr editableRootOrFormControl = enclosingFormControl.get() ?: selection.rootEditableElement()) {
        postLayoutData.selectionIsTransparentOrFullyClipped = result.isContentEditable && isTransparentOrFullyClipped(*editableRootOrFormControl);
#if PLATFORM(IOS_FAMILY)
        result.visualData->editableRootBounds = rootViewInteractionBounds(Ref { *editableRootOrFormControl });
#endif
    } else if (result.selectionIsRange) {
        if (RefPtr ancestorContainer = commonInclusiveAncestor(selection.start(), selection.end()))
            postLayoutData.selectionIsTransparentOrFullyClipped = isTransparentOrFullyClipped(*ancestorContainer);
    }

#if PLATFORM(IOS_FAMILY)
    bool honorOverflowScrolling = m_page->settings().selectionHonorsOverflowScrolling();
    if (enclosingFormControl || !honorOverflowScrolling)
        result.visualData->selectionClipRect = result.visualData->editableRootBounds;

    if (honorOverflowScrolling)
        computeEnclosingLayerID(result, selection);
#endif // PLATFORM(IOS_FAMILY)
}

void WebPage::getPDFFirstPageSize(WebCore::FrameIdentifier frameID, CompletionHandler<void(WebCore::FloatSize)>&& completionHandler)
{
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame)
        return completionHandler({ });

#if ENABLE(PDF_PLUGIN)
    if (auto* pluginView = pluginViewForFrame(webFrame->coreLocalFrame()))
        return completionHandler(pluginView->pdfDocumentSizeForPrinting());
#endif

    completionHandler({ });
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
    WTF_MAKE_TZONE_ALLOCATED_INLINE(OverridePasteboardForSelectionReplacement);
public:
    OverridePasteboardForSelectionReplacement(const Vector<String>& types, std::span<const uint8_t> data)
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

void WebPage::replaceImageForRemoveBackground(const ElementContext& elementContext, const Vector<String>& types, std::span<const uint8_t> data)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return;

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
        IgnoreSelectionChangeForScope ignoreSelectionChanges { *frame };
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

void WebPage::replaceSelectionWithPasteboardData(const Vector<String>& types, std::span<const uint8_t> data)
{
    OverridePasteboardForSelectionReplacement overridePasteboard { types, data };
    readSelectionFromPasteboard(replaceSelectionPasteboardName(), [](bool) { });
}

void WebPage::readSelectionFromPasteboard(const String& pasteboardName, CompletionHandler<void(bool&&)>&& completionHandler)
{
    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame)
        return completionHandler(false);
    if (frame->selection().isNone())
        return completionHandler(false);
    frame->editor().readSelectionFromPasteboard(pasteboardName);
    completionHandler(true);
}

#if ENABLE(MULTI_REPRESENTATION_HEIC)
void WebPage::insertMultiRepresentationHEIC(std::span<const uint8_t> data, const String& altText)
{
    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame)
        return;
    if (frame->selection().isNone())
        return;
    frame->editor().insertMultiRepresentationHEIC(data, altText);
}
#endif

std::pair<URL, DidFilterLinkDecoration> WebPage::applyLinkDecorationFilteringWithResult(const URL& url, LinkDecorationFilteringTrigger trigger)
{
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    if (m_linkDecorationFilteringData.isEmpty()) {
        RELEASE_LOG_ERROR(ResourceLoadStatistics, "Unable to filter tracking query parameters (missing data)");
        return { url, DidFilterLinkDecoration::No };
    }

    RefPtr mainFrame = m_mainFrame->coreLocalFrame();
    if (!mainFrame || !WebCore::DeprecatedGlobalSettings::trackingPreventionEnabled())
        return { url, DidFilterLinkDecoration::No };

    auto isLinkDecorationFilteringEnabled = [&](const DocumentLoader* loader) {
        if (!loader)
            return false;
        auto effectivePolicies = trigger == LinkDecorationFilteringTrigger::Navigation ? loader->navigationalAdvancedPrivacyProtections() : loader->advancedPrivacyProtections();
        return effectivePolicies.contains(AdvancedPrivacyProtections::LinkDecorationFiltering) || m_page->settings().filterLinkDecorationByDefaultEnabled();
    };

    bool shouldApplyLinkDecorationFiltering = [&] {
        if (isLinkDecorationFilteringEnabled(RefPtr { mainFrame->loader().activeDocumentLoader() }.get()))
            return true;

        return isLinkDecorationFilteringEnabled(RefPtr { mainFrame->loader().policyDocumentLoader() }.get());
    }();

    if (!shouldApplyLinkDecorationFiltering)
        return { url, DidFilterLinkDecoration::No };

    if (!url.hasQuery())
        return { url, DidFilterLinkDecoration::No };

    auto sanitizedURL = url;
    auto removedParameters = WTF::removeQueryParameters(sanitizedURL, [&](auto& parameter) {
        auto it = m_linkDecorationFilteringData.find(parameter);
        if (it == m_linkDecorationFilteringData.end())
            return false;

        const auto& conditionals = it->value;
        bool isEmptyOrFoundDomain = conditionals.domains.isEmpty() || conditionals.domains.contains(RegistrableDomain { url });
        bool isEmptyOrFoundPath = conditionals.paths.isEmpty() || std::any_of(conditionals.paths.begin(), conditionals.paths.end(),
            [&url](auto& path) {
                return url.path().contains(path);
            });

        return isEmptyOrFoundDomain && isEmptyOrFoundPath;
    });

    if (!removedParameters.isEmpty() && trigger != LinkDecorationFilteringTrigger::Unspecified) {
        if (trigger == LinkDecorationFilteringTrigger::Navigation)
            send(Messages::WebPageProxy::DidApplyLinkDecorationFiltering(url, sanitizedURL));
        auto removedParametersString = makeStringByJoining(removedParameters, ", "_s);
        WEBPAGE_RELEASE_LOG(ResourceLoadStatistics, "Blocked known tracking query parameters: %s", removedParametersString.utf8().data());
    }

    return { sanitizedURL, DidFilterLinkDecoration::Yes };
#else
    return { url, DidFilterLinkDecoration::No };
#endif
}

URL WebPage::allowedQueryParametersForAdvancedPrivacyProtections(const URL& url)
{
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    if (m_allowedQueryParametersForAdvancedPrivacyProtections.isEmpty()) {
        RELEASE_LOG_ERROR(ResourceLoadStatistics, "Unable to hide query parameters from script (missing data)");
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

#if ENABLE(EXTENSION_CAPABILITIES)
void WebPage::setMediaEnvironment(const String& mediaEnvironment)
{
    m_mediaEnvironment = mediaEnvironment;
    if (auto gpuProcessConnection = WebProcess::singleton().existingGPUProcessConnection())
        gpuProcessConnection->setMediaEnvironment(identifier(), mediaEnvironment);
}
#endif

#if ENABLE(WRITING_TOOLS)
void WebPage::willBeginWritingToolsSession(const std::optional<WebCore::WritingTools::Session>& session, CompletionHandler<void(const Vector<WebCore::WritingTools::Context>&)>&& completionHandler)
{
    corePage()->willBeginWritingToolsSession(session, WTFMove(completionHandler));
}

void WebPage::didBeginWritingToolsSession(const WebCore::WritingTools::Session& session, const Vector<WebCore::WritingTools::Context>& contexts)
{
    corePage()->didBeginWritingToolsSession(session, contexts);
}

void WebPage::proofreadingSessionDidReceiveSuggestions(const WebCore::WritingTools::Session& session, const Vector<WebCore::WritingTools::TextSuggestion>& suggestions, const WebCore::CharacterRange& processedRange, const WebCore::WritingTools::Context& context, bool finished, CompletionHandler<void()>&& completionHandler)
{
    corePage()->proofreadingSessionDidReceiveSuggestions(session, suggestions, processedRange, context, finished);
    completionHandler();
}

void WebPage::proofreadingSessionDidUpdateStateForSuggestion(const WebCore::WritingTools::Session& session, WebCore::WritingTools::TextSuggestion::State state, const WebCore::WritingTools::TextSuggestion& suggestion, const WebCore::WritingTools::Context& context)
{
    corePage()->proofreadingSessionDidUpdateStateForSuggestion(session, state, suggestion, context);
}

void WebPage::willEndWritingToolsSession(const WebCore::WritingTools::Session& session, bool accepted, CompletionHandler<void()>&& completionHandler)
{
    corePage()->willEndWritingToolsSession(session, accepted);
    completionHandler();
}

void WebPage::didEndWritingToolsSession(const WebCore::WritingTools::Session& session, bool accepted)
{
    corePage()->didEndWritingToolsSession(session, accepted);
}

void WebPage::compositionSessionDidReceiveTextWithReplacementRange(const WebCore::WritingTools::Session& session, const WebCore::AttributedString& attributedText, const WebCore::CharacterRange& range, const WebCore::WritingTools::Context& context, bool finished)
{
    corePage()->compositionSessionDidReceiveTextWithReplacementRange(session, attributedText, range, context, finished);
}

void WebPage::writingToolsSessionDidReceiveAction(const WritingTools::Session& session, WebCore::WritingTools::Action action)
{
    corePage()->writingToolsSessionDidReceiveAction(session, action);
}

void WebPage::proofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(const WebCore::WritingTools::TextSuggestion::ID& replacementID, WebCore::IntRect rect)
{
    send(Messages::WebPageProxy::ProofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(replacementID, rect));
}

void WebPage::proofreadingSessionUpdateStateForSuggestionWithID(WebCore::WritingTools::TextSuggestion::State state, const WebCore::WritingTools::TextSuggestion::ID& replacementID)
{
    send(Messages::WebPageProxy::ProofreadingSessionUpdateStateForSuggestionWithID(state, replacementID));
}

void WebPage::addTextAnimationForAnimationID(const WTF::UUID& uuid, const WebCore::TextAnimationData& styleData, const WebCore::TextIndicatorData& indicatorData, CompletionHandler<void(WebCore::TextAnimationRunMode)>&& completionHandler)
{
    if (completionHandler)
        sendWithAsyncReply(Messages::WebPageProxy::AddTextAnimationForAnimationIDWithCompletionHandler(uuid, styleData, indicatorData), WTFMove(completionHandler));
    else
        send(Messages::WebPageProxy::AddTextAnimationForAnimationID(uuid, styleData, indicatorData));
}

void WebPage::removeTextAnimationForAnimationID(const WTF::UUID& uuid)
{
    send(Messages::WebPageProxy::RemoveTextAnimationForAnimationID(uuid));
}

void WebPage::removeInitialTextAnimationForActiveWritingToolsSession()
{
    m_textAnimationController->removeInitialTextAnimationForActiveWritingToolsSession();
}

void WebPage::addInitialTextAnimationForActiveWritingToolsSession()
{
    m_textAnimationController->addInitialTextAnimationForActiveWritingToolsSession();
}

void WebPage::addSourceTextAnimationForActiveWritingToolsSession(const WTF::UUID& sourceAnimationUUID, const WTF::UUID& destinationAnimationUUID, bool finished, const CharacterRange& range, const String& string, CompletionHandler<void(WebCore::TextAnimationRunMode)>&& completionHandler)
{
    m_textAnimationController->addSourceTextAnimationForActiveWritingToolsSession(sourceAnimationUUID, destinationAnimationUUID, finished, range, string, WTFMove(completionHandler));
}

void WebPage::addDestinationTextAnimationForActiveWritingToolsSession(const WTF::UUID& sourceAnimationUUID, const WTF::UUID& destinationAnimationUUID, const std::optional<CharacterRange>& range, const String& string)
{
    m_textAnimationController->addDestinationTextAnimationForActiveWritingToolsSession(sourceAnimationUUID, destinationAnimationUUID, range, string);
}

void WebPage::saveSnapshotOfTextPlaceholderForAnimation(const WebCore::SimpleRange& placeholderRange)
{
    m_textAnimationController->saveSnapshotOfTextPlaceholderForAnimation(placeholderRange);
}

void WebPage::clearAnimationsForActiveWritingToolsSession()
{
    m_textAnimationController->clearAnimationsForActiveWritingToolsSession();
}

void WebPage::createTextIndicatorForTextAnimationID(const WTF::UUID& uuid, CompletionHandler<void(std::optional<WebCore::TextIndicatorData>&&)>&& completionHandler)
{
    m_textAnimationController->createTextIndicatorForTextAnimationID(uuid, WTFMove(completionHandler));
}

void WebPage::updateUnderlyingTextVisibilityForTextAnimationID(const WTF::UUID& uuid, bool visible, CompletionHandler<void()>&& completionHandler)
{
    m_textAnimationController->updateUnderlyingTextVisibilityForTextAnimationID(uuid, visible, WTFMove(completionHandler));
}

void WebPage::enableSourceTextAnimationAfterElementWithID(const String& elementID)
{
    m_textAnimationController->enableSourceTextAnimationAfterElementWithID(elementID);
}

void WebPage::enableTextAnimationTypeForElementWithID(const String& elementID)
{
    m_textAnimationController->enableTextAnimationTypeForElementWithID(elementID);
}

void WebPage::proofreadingSessionSuggestionTextRectsInRootViewCoordinates(const WebCore::CharacterRange& enclosingRangeRelativeToSessionRange, CompletionHandler<void(Vector<FloatRect>&&)>&& completionHandler) const
{
    auto rects = corePage()->proofreadingSessionSuggestionTextRectsInRootViewCoordinates(enclosingRangeRelativeToSessionRange);
    completionHandler(WTFMove(rects));
}

void WebPage::updateTextVisibilityForActiveWritingToolsSession(const WebCore::CharacterRange& rangeRelativeToSessionRange, bool visible, const WTF::UUID& identifier, CompletionHandler<void()>&& completionHandler)
{
    corePage()->updateTextVisibilityForActiveWritingToolsSession(rangeRelativeToSessionRange, visible, identifier);
    completionHandler();
}

void WebPage::textPreviewDataForActiveWritingToolsSession(const WebCore::CharacterRange& rangeRelativeToSessionRange, CompletionHandler<void(std::optional<WebCore::TextIndicatorData>&&)>&& completionHandler)
{
    auto data = corePage()->textPreviewDataForActiveWritingToolsSession(rangeRelativeToSessionRange);
    completionHandler(WTFMove(data));
}

void WebPage::decorateTextReplacementsForActiveWritingToolsSession(const WebCore::CharacterRange& rangeRelativeToSessionRange, CompletionHandler<void(void)>&& completionHandler)
{
    corePage()->decorateTextReplacementsForActiveWritingToolsSession(rangeRelativeToSessionRange);
    completionHandler();
}

void WebPage::setSelectionForActiveWritingToolsSession(const WebCore::CharacterRange& rangeRelativeToSessionRange, CompletionHandler<void(void)>&& completionHandler)
{
    corePage()->setSelectionForActiveWritingToolsSession(rangeRelativeToSessionRange);
    completionHandler();
}

void WebPage::intelligenceTextAnimationsDidComplete()
{
    corePage()->intelligenceTextAnimationsDidComplete();
}

void WebPage::didEndPartialIntelligenceTextAnimation()
{
    send(Messages::WebPageProxy::DidEndPartialIntelligenceTextAnimation());
}

#endif

static std::optional<bool> elementHasHiddenVisibility(StyledElement* styledElement)
{
    auto* inlineStyle = styledElement->inlineStyle();
    if (!inlineStyle)
        return std::nullopt;

    RefPtr value = inlineStyle->getPropertyCSSValue(CSSPropertyVisibility);
    if (!value)
        return false;

    return value->valueID() == CSSValueHidden;
}

void WebPage::createTextIndicatorForElementWithID(const String& elementID, CompletionHandler<void(std::optional<WebCore::TextIndicatorData>&&)>&& completionHandler)
{
    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame) {
        ASSERT_NOT_REACHED();
        completionHandler(std::nullopt);
        return;
    }

    RefPtr document = frame->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        completionHandler(std::nullopt);
        return;
    }

    RefPtr element = document->getElementById(elementID);
    if (!element) {
        ASSERT_NOT_REACHED();
        completionHandler(std::nullopt);
        return;
    }

    RefPtr styledElement = dynamicDowncast<StyledElement>(element.get());
    if (!styledElement) {
        ASSERT_NOT_REACHED();
        completionHandler(std::nullopt);
        return;
    }

    // Temporarily force the content to be visible so that it can be snapshotted.

    auto isHiddenInitially = elementHasHiddenVisibility(styledElement.get());

    styledElement->setInlineStyleProperty(CSSPropertyVisibility, CSSValueVisible, IsImportant::Yes);

    auto elementRange = WebCore::makeRangeSelectingNodeContents(*styledElement);

    std::optional<WebCore::TextIndicatorData> textIndicatorData;
    constexpr OptionSet textIndicatorOptions {
        WebCore::TextIndicatorOption::IncludeSnapshotOfAllVisibleContentWithoutSelection,
        WebCore::TextIndicatorOption::ExpandClipBeyondVisibleRect,
        WebCore::TextIndicatorOption::SkipReplacedContent,
        WebCore::TextIndicatorOption::RespectTextColor,
#if PLATFORM(VISION)
        WebCore::TextIndicatorOption::SnapshotContentAt3xBaseScale,
#endif
    };

    RefPtr textIndicator = WebCore::TextIndicator::createWithRange(elementRange, textIndicatorOptions, WebCore::TextIndicatorPresentationTransition::None, { });
    if (!textIndicator) {
        completionHandler(std::nullopt);
        return;
    }

    // If `initialVisibility` is an empty optional, this means there was no initial inline style.
    // Ensure the state is idempotent after by removing the inline style if this is the case.

    if (isHiddenInitially.has_value())
        styledElement->setInlineStyleProperty(CSSPropertyVisibility, *isHiddenInitially ? CSSValueHidden : CSSValueVisible, IsImportant::Yes);
    else
        styledElement->removeInlineStyleProperty(CSSPropertyVisibility);

    completionHandler(textIndicator->data());
}

void WebPage::createIconDataFromImageData(Ref<WebCore::SharedBuffer>&& buffer, const Vector<unsigned>& lengths, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&& completionHandler)
{
    return completionHandler(WebCore::createIconDataFromImageData(buffer->span(), lengths.span()));
}

void WebPage::decodeImageData(Ref<WebCore::SharedBuffer>&& buffer, std::optional<WebCore::FloatSize> preferredSize, CompletionHandler<void(RefPtr<WebCore::ShareableBitmap>&&)>&& completionHandler)
{
    completionHandler(decodeImageWithSize(buffer->span(), preferredSize));
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
