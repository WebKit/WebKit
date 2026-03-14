/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#import "DrawingArea.h"
#import "EditorState.h"
#import "GPUProcessConnection.h"
#import "InsertTextOptions.h"
#import "InteractionInformationAtPosition.h"
#import "LoadParameters.h"
#import "MessageSenderInlines.h"
#import "PDFPlugin.h"
#import "PluginView.h"
#import "PositionInformationForWebPage.h"
#import "PrintInfo.h"
#import "RemoteLayerTreeCommitBundle.h"
#import "RemoteLayerTreeTransaction.h"
#import "RemoteRenderingBackendProxy.h"
#import "RemoteSnapshotRecorderProxy.h"
#import "SharedBufferReference.h"
#import "TextAnimationController.h"
#import "UserMediaCaptureManager.h"
#import "ViewGestureGeometryCollector.h"
#import "WKAccessibilityWebPageObjectBase.h"
#import "WebEventConversion.h"
#import "WebFrame.h"
#import "WebImage.h"
#import "WebPageInternals.h"
#import "WebPageProxyMessages.h"
#import "WebPasteboardOverrides.h"
#import "WebPaymentCoordinator.h"
#import "WebPreferencesKeys.h"
#import "WebProcess.h"
#import "WebRemoteObjectRegistry.h"
#import <WebCore/AXCrossProcessSearch.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/AXRemoteFrame.h>
#import <WebCore/AXSearchManager.h>
#import <WebCore/AccessibilityObject.h>
#import <WebCore/AccessibilityScrollView.h>
#import <WebCore/AnimationTimelinesController.h>
#import <WebCore/Chrome.h>
#import <WebCore/ChromeClient.h>
#if ENABLE(CONTENT_CHANGE_OBSERVER)
#import <WebCore/ContentChangeObserver.h>
#endif
#import <WebCore/DeprecatedGlobalSettings.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/DocumentMarkerController.h>
#import <WebCore/DocumentMarkers.h>
#import <WebCore/DocumentQuirks.h>
#import <WebCore/DocumentView.h>
#import <WebCore/DragImage.h>
#import <WebCore/EditingHTMLConverter.h>
#import <WebCore/EditingInlines.h>
#import <WebCore/Editor.h>
#import <WebCore/ElementAncestorIteratorInlines.h>
#import <WebCore/EventHandler.h>
#import <WebCore/EventNames.h>
#import <WebCore/FixedContainerEdges.h>
#import <WebCore/FocusController.h>
#import <WebCore/FrameDestructionObserverInlines.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/HTMLAnchorElement.h>
#import <WebCore/HTMLAreaElement.h>
#import <WebCore/HTMLBodyElement.h>
#import <WebCore/HTMLFrameOwnerElement.h>
#import <WebCore/HTMLIFrameElement.h>
#import <WebCore/HTMLImageElement.h>
#import <WebCore/HTMLLabelElement.h>
#import <WebCore/HTMLOListElement.h>
#import <WebCore/HTMLPlugInElement.h>
#import <WebCore/HTMLSelectElement.h>
#import <WebCore/HTMLSummaryElement.h>
#import <WebCore/HTMLTextAreaElement.h>
#import <WebCore/HTMLTextFormControlElement.h>
#import <WebCore/HTMLUListElement.h>
#import <WebCore/HandleUserInputEventResult.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/ImageOverlay.h>
#import <WebCore/ImageUtilities.h>
#import <WebCore/LegacyWebArchive.h>
#import <WebCore/LocalFrameInlines.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/MutableStyleProperties.h>
#import <WebCore/NetworkExtensionContentFilter.h>
#import <WebCore/NodeDocument.h>
#import <WebCore/NodeHTMLConverter.h>
#import <WebCore/NodeRenderStyle.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/NowPlayingInfo.h>
#import <WebCore/PaymentCoordinator.h>
#import <WebCore/PlatformMediaSessionManager.h>
#import <WebCore/PlatformMouseEvent.h>
#import <WebCore/PrintContext.h>
#import <WebCore/Range.h>
#import <WebCore/RemoteFrame.h>
#import <WebCore/RemoteFrameGeometryTransformer.h>
#import <WebCore/RemoteFrameView.h>
#import <WebCore/RemoteUserInputEventData.h>
#import <WebCore/RenderBoxInlines.h>
#import <WebCore/RenderElement.h>
#import <WebCore/RenderLayer.h>
#import <WebCore/RenderObjectInlines.h>
#import <WebCore/RenderTheme.h>
#import <WebCore/RenderedDocumentMarker.h>
#import <WebCore/SVGImage.h>
#import <WebCore/Settings.h>
#import <WebCore/StylePropertiesInlines.h>
#import <WebCore/TextIterator.h>
#import <WebCore/TextPlaceholderElement.h>
#import <WebCore/UTIRegistry.h>
#import <WebCore/UTIUtilities.h>
#import <WebCore/UserTypingGestureIndicator.h>
#import <WebCore/VisibleUnits.h>
#import <WebCore/WebAccessibilityObjectWrapperMac.h>
#import <WebCore/markup.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/CoroutineUtilities.h>
#import <wtf/MonotonicTime.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cf/VectorCF.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/text/StringToIntegerConversion.h>

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
#include "LibWebRTCCodecs.h"
#endif

#if PLATFORM(IOS) || PLATFORM(VISION)
#import <WebCore/ParentalControlsContentFilter.h>
#endif

#if USE(EXTENSIONKIT)
#import "WKProcessExtension.h"
#endif

#if ENABLE(THREADED_ANIMATIONS)
#import <WebCore/AcceleratedEffectStackUpdater.h>
#endif

#if HAVE(PDFKIT)
#import "PDFKitSPI.h"
#endif

#if ENABLE(WK_WEB_EXTENSIONS)
#include "WebExtensionControllerProxy.h"
#endif

#import "PDFKitSoftLink.h"

#define WEBPAGE_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [webPageID=%" PRIu64 "] WebPage::" fmt, this, m_identifier.toUInt64(), ##__VA_ARGS__)

#if PLATFORM(COCOA)

namespace WebKit {

using namespace WebCore;

// FIXME: Unclear if callers in this file are correctly choosing which of these two functions to use.

String plainTextForContext(const SimpleRange& range)
{
    return WebCore::plainTextReplacingNoBreakSpace(range);
}

String plainTextForContext(const std::optional<SimpleRange>& range)
{
    return range ? plainTextForContext(*range) : emptyString();
}

String plainTextForDisplay(const SimpleRange& range)
{
    return WebCore::plainTextReplacingNoBreakSpace(range, { }, true);
}

String plainTextForDisplay(const std::optional<SimpleRange>& range)
{
    return range ? plainTextForDisplay(*range) : emptyString();
}

void WebPage::platformInitialize(const WebPageCreationParameters& parameters)
{
#if ENABLE(INITIALIZE_ACCESSIBILITY_ON_DEMAND)
    bool shouldInitializeAccessibility = WebProcess::singleton().shouldInitializeAccessibility() || !parameters.store.getBoolValueForKey(WebPreferencesKey::enableAccessibilityOnDemandKey());
#else
    bool shouldInitializeAccessibility = false;
#endif

    platformInitializeAccessibility(shouldInitializeAccessibility ? ShouldInitializeNSAccessibility::Yes : ShouldInitializeNSAccessibility::No);

#if ENABLE(MEDIA_STREAM)
    protect(WebProcess::singleton().userMediaCaptureManager())->setupCaptureProcesses(parameters.shouldCaptureAudioInUIProcess, parameters.shouldCaptureAudioInGPUProcess, parameters.shouldCaptureVideoInUIProcess, parameters.shouldCaptureVideoInGPUProcess, parameters.shouldCaptureDisplayInUIProcess, parameters.shouldCaptureDisplayInGPUProcess,
#if ENABLE(WEB_RTC)
        m_page->settings().webRTCRemoteVideoFrameEnabled()
#else
        false
#endif // ENABLE(WEB_RTC)
    );
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
    if (!WebProcess::singleton().isLockdownModeEnabled()) {
        WebCore::setAdditionalSupportedImageTypes(parameters.additionalSupportedImageTypes);
        WebCore::setImageSourceAllowableTypes(WebCore::allowableImageTypes());
    }
}

#if HAVE(SANDBOX_STATE_FLAGS)
void WebPage::setHasLaunchedWebContentProcess()
{
    static bool hasSetLaunchVariable = false;
    if (!hasSetLaunchVariable) {
        auto auditToken = WebProcess::singleton().auditTokenForSelf();
#if USE(EXTENSIONKIT)
        if (WKProcessExtension.sharedInstance)
            [WKProcessExtension.sharedInstance lockdownSandbox:@"2.0"];
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
    if (RefPtr manager = mediaSessionManagerIfExists()) {
        if (auto nowPlayingInfo = manager->nowPlayingInfo()) {
            bool registeredAsNowPlayingApplication = manager->registeredAsNowPlayingApplication();
            completionHandler(registeredAsNowPlayingApplication, WTF::move(*nowPlayingInfo));
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
    if (RefPtr pluginView = mainFramePlugIn()) {
        if (pluginView->performDictionaryLookupAtLocation(floatPoint))
            return;
    }
#endif
    
    RefPtr localMainFrame = corePage()->localMainFrame();
    if (!localMainFrame)
        return;
    // Find the frame the point is over.
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowChildFrameContent };
    auto result = localMainFrame->eventHandler().hitTestResultAtPoint(protect(localMainFrame->view())->windowToContents(roundedIntPoint(floatPoint)), hitType);

    RefPtr frame = result.innerNonSharedNode() ? result.innerNonSharedNode()->document().frame() : corePage()->focusController().focusedOrMainFrame();
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

void WebPage::performDictionaryLookupForRange(LocalFrame& frame, const SimpleRange& range, TextIndicatorPresentationTransition presentationTransition)
{
    send(Messages::WebPageProxy::DidPerformDictionaryLookup(dictionaryPopupInfoForRange(frame, range, presentationTransition)));
}

DictionaryPopupInfo WebPage::dictionaryPopupInfoForRange(LocalFrame& frame, const SimpleRange& range, TextIndicatorPresentationTransition presentationTransition)
{
    Ref editor = frame.editor();
    editor->setIsGettingDictionaryPopupInfo(true);

    if (plainText(range).find(deprecatedIsNotSpaceOrNewline) == notFound) {
        editor->setIsGettingDictionaryPopupInfo(false);
        return { };
    }

    auto quads = RenderObject::absoluteTextQuads(range);
    if (quads.isEmpty()) {
        editor->setIsGettingDictionaryPopupInfo(false);
        return { };
    }

    DictionaryPopupInfo dictionaryPopupInfo;

    IntRect rangeRect = protect(frame.view())->contentsToWindow(quads[0].enclosingBoundingBox());

    const CheckedPtr style = range.startContainer().renderStyle();
    float scaledAscent = style ? style->metricsOfPrimaryFont().intAscent() * pageScaleFactor() : 0;
    dictionaryPopupInfo.origin = FloatPoint(rangeRect.x(), rangeRect.y() + scaledAscent);

#if PLATFORM(MAC)
    auto attributedString = editingAttributedString(range, { }).nsAttributedString();
    auto scaledAttributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:[attributedString string]]);
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    [attributedString enumerateAttributesInRange:NSMakeRange(0, [attributedString length]) options:0 usingBlock:^(NSDictionary *attributes, NSRange range, BOOL *stop) {
        RetainPtr<NSMutableDictionary> scaledAttributes = adoptNS([attributes mutableCopy]);
        RetainPtr<NSFont> font = [scaledAttributes objectForKey:NSFontAttributeName];
        if (font)
            font = [fontManager convertFont:font.get() toSize:font.get().pointSize * pageScaleFactor()];
        if (font)
            [scaledAttributes setObject:font.get() forKey:NSFontAttributeName];
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
        editor->setIsGettingDictionaryPopupInfo(false);
        return dictionaryPopupInfo;
    }

    dictionaryPopupInfo.textIndicator = textIndicator;
#if PLATFORM(MAC)
#if ENABLE(LEGACY_PDFKIT_PLUGIN)
    dictionaryPopupInfo.platformData.attributedString = WebCore::AttributedString::fromNSAttributedString(scaledAttributedString);
#else
    dictionaryPopupInfo.text = [scaledAttributedString string];
#endif

#elif PLATFORM(MACCATALYST)
#if ENABLE(LEGACY_PDFKIT_PLUGIN)
    dictionaryPopupInfo.platformData.attributedString = WebCore::AttributedString::fromNSAttributedString(adoptNS([[NSMutableAttributedString alloc] initWithString:plainText(range).createNSString().get()]));
#else
    dictionaryPopupInfo.text = plainText(range);
#endif

#endif

    editor->setIsGettingDictionaryPopupInfo(false);
    return dictionaryPopupInfo;
}

void WebPage::insertDictatedTextAsync(const String& text, const EditingRange& replacementEditingRange, const Vector<WebCore::DictationAlternative>& dictationAlternativeLocations, InsertTextOptions&& options)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    if (replacementEditingRange.location != notFound) {
        auto replacementRange = EditingRange::toRange(*frame, replacementEditingRange);
        if (replacementRange)
            protect(frame->selection())->setSelection(VisibleSelection { *replacementRange });
    }

    if (options.registerUndoGroup)
        send(Messages::WebPageProxy::RegisterInsertionUndoGrouping { });

    RefPtr<Element> focusedElement = frame->document() ? frame->document()->focusedElement() : nullptr;
    if (focusedElement && options.shouldSimulateKeyboardInput)
        focusedElement->dispatchEvent(Event::create(eventNames().keydownEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));

    if (frame->editor().hasComposition())
        return;

    protect(frame->editor())->insertDictatedText(text, dictationAlternativeLocations, nullptr /* triggeringEvent */);

    if (focusedElement && options.shouldSimulateKeyboardInput) {
        focusedElement->dispatchEvent(Event::create(eventNames().keyupEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));
        focusedElement->dispatchEvent(Event::create(eventNames().changeEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));
    }
}

void WebPage::addDictationAlternative(const String& text, DictationContext context, CompletionHandler<void(bool)>&& completion)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
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

    auto firstEditablePosition = firstPositionInNode(*editableRoot);
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

    protect(document->markers())->addMarker(matchRange, DocumentMarkerType::DictationAlternatives, { DocumentMarker::DictationData { context, text } });
    completion(true);
}

void WebPage::dictationAlternativesAtSelection(CompletionHandler<void(Vector<DictationContext>&&)>&& completion)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
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

    auto markers = protect(document->markers())->markersInRange(*expandedSelectionRange, DocumentMarkerType::DictationAlternatives);
    auto contexts = WTF::compactMap(markers, [](auto& marker) -> std::optional<DictationContext> {
        if (std::holds_alternative<DocumentMarker::DictationData>(marker->data()))
            return std::get<DocumentMarker::DictationData>(marker->data()).context;
        return std::nullopt;
    });
    completion(WTF::move(contexts));
}

void WebPage::clearDictationAlternatives(Vector<DictationContext>&& contexts)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
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
    protect(document->markers())->filterMarkers(documentRange, [&] (auto& marker) {
        if (!std::holds_alternative<DocumentMarker::DictationData>(marker.data()))
            return FilterMarkerResult::Keep;
        return setOfContextsToRemove.contains(std::get<WebCore::DocumentMarker::DictationData>(marker.data()).context) ? FilterMarkerResult::Remove : FilterMarkerResult::Keep;
    }, DocumentMarkerType::DictationAlternatives);
}

std::optional<SimpleRange> WebPage::findDictatedTextRangeBeforeCursor(LocalFrame& frame, const String& text)
{
    VisiblePosition position = frame.selection().selection().start();
    for (auto i = numGraphemeClusters(text); i; --i)
        position = position.previous();
    if (position.isNull())
        position = startOfDocument(protect(frame.document()));

    auto range = makeSimpleRange(position, frame.selection().selection().start());
    if (!range || plainTextForContext(*range) != text)
        return std::nullopt;
    return range;
}

void WebPage::setDictationStreamingOpacity(const String& hypothesisText, WebCore::CharacterRange streamingRangeInHypothesis, float opacity)
{
    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame) {
        WEBPAGE_RELEASE_LOG(ViewState, "setDictationStreamingOpacity - no focused frame");
        return;
    }

    if (frame->selection().isNone() || !frame->selection().selection().isContentEditable()) {
        WEBPAGE_RELEASE_LOG(ViewState, "setDictationStreamingOpacity - no editable selection");
        return;
    }

    auto hypothesisRange = findDictatedTextRangeBeforeCursor(*frame, hypothesisText);
    if (!hypothesisRange) {
        WEBPAGE_RELEASE_LOG(ViewState, "setDictationStreamingOpacity - hypothesis text not found, expected length %u", hypothesisText.length());
        return;
    }

    auto streamingRange = resolveCharacterRange(*hypothesisRange, streamingRangeInHypothesis);
    if (streamingRange.collapsed()) {
        WEBPAGE_RELEASE_LOG(ViewState, "setDictationStreamingOpacity - resolved streaming range is collapsed");
        return;
    }

    protect(protect(frame->document())->markers())->addDictationStreamingOpacityMarker(streamingRange, opacity);
}

void WebPage::clearDictationStreamingOpacity()
{
    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame || !frame->document())
        return;

    protect(protect(frame->document())->markers())->removeAllDictationStreamingOpacityMarkers();
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

void WebPage::bindRemoteAccessibilityFrames(int processIdentifier, WebCore::FrameIdentifier frameID, WebCore::AccessibilityRemoteToken dataToken, CompletionHandler<void(WebCore::AccessibilityRemoteToken, int)>&& completionHandler)
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

    if (!coreLocalFrame->contentRenderer()) {
        ASSERT_NOT_REACHED();
        return completionHandler({ }, 0);
    }

    registerRemoteFrameAccessibilityTokens(processIdentifier, dataToken, frameID);

    // Get our remote token data and send back to the RemoteFrame.
#if PLATFORM(MAC)
    completionHandler({ makeVector(accessibilityRemoteTokenData().get()) }, getpid());
#else
    completionHandler({ dataToken }, getpid());
#endif
}

void WebPage::resolveAccessibilityHitTestForTesting(WebCore::FrameIdentifier frameID, const WebCore::IntPoint& point, CompletionHandler<void(String)>&& completionHandler)
{
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame)
        return completionHandler("NULL"_s);
#if PLATFORM(MAC)
    if (RetainPtr coreObject = [m_mockAccessibilityElement accessibilityRootObjectWrapper:protect(webFrame->coreLocalFrame()).get()]) {
        if (RetainPtr hitTestResult = [coreObject accessibilityHitTest:point]) {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            completionHandler([hitTestResult accessibilityAttributeValue:@"AXInfoStringForTesting"]);
            ALLOW_DEPRECATED_DECLARATIONS_END
            return;
        }
    }
#endif
    UNUSED_PARAM(point);
    completionHandler("NULL"_s);
}

#if PLATFORM(MAC)
// Creates an AccessibilityRemoteToken from an accessibility wrapper.
// Returns std::nullopt if the wrapper is nil or token creation fails.
static std::optional<WebCore::AccessibilityRemoteToken> tokenFromWrapper(id wrapper)
{
    if (!wrapper)
        return std::nullopt;
    if (RetainPtr tokenData = [NSAccessibilityRemoteUIElement remoteTokenForLocalUIElement:wrapper])
        return WebCore::AccessibilityRemoteToken { makeVector(tokenData.get()) };
    return std::nullopt;
}

static Vector<WebCore::AccessibilityRemoteToken> convertSearchResultsToRemoteTokens(WebCore::AccessibilitySearchResults&& searchResults)
{
    Vector<WebCore::AccessibilityRemoteToken> results;
    for (auto& result : searchResults) {
        if (RefPtr object = result.objectIfLocalResult()) {
            if (auto token = tokenFromWrapper(protect(object->wrapper())))
                results.append(WTF::move(*token));
        } else if (result.isRemote())
            results.append(*result.remoteToken());
    }
    return results;
}

void WebPage::performAccessibilitySearchInRemoteFrame(WebCore::FrameIdentifier frameID, WebCore::AccessibilitySearchCriteriaIPC criteria, CompletionHandler<void(Vector<WebCore::AccessibilityRemoteToken>&&)>&& completionHandler)
{
    AX_ASSERT(isMainRunLoop());

    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    RefPtr coreFrame = webFrame ? webFrame->coreLocalFrame() : nullptr;
    RefPtr document = coreFrame ? coreFrame->document() : nullptr;
    CheckedPtr cache = document ? document->axObjectCache() : nullptr;
    // Get the web area for this frame as the anchor object.
    RefPtr webArea = cache ? cache->rootObjectForFrame(*coreFrame) : nullptr;
    if (!webArea) {
        completionHandler({ });
        return;
    }

    // Convert IPC criteria to local criteria with this frame's web area as anchor.
    auto localCriteria = criteria.toSearchCriteria(webArea.get());

    // Since this frame is being searched by a parent frame (via performAccessibilitySearchInRemoteFrame),
    // only search this frame and its nested child frames - do NOT coordinate with the parent.
    // The parent already handles its own elements and will merge our results appropriately.
    auto searchResults = WebCore::performSearchWithCrossProcessCoordination(*webArea, WTF::move(localCriteria));

    completionHandler(convertSearchResultsToRemoteTokens(WTF::move(searchResults)));
}

void WebPage::continueAccessibilitySearchInParentFrame(WebCore::FrameIdentifier childFrameID, WebCore::AccessibilitySearchCriteriaIPC criteria, CompletionHandler<void(Vector<WebCore::AccessibilityRemoteToken>&&)>&& completionHandler)
{
    // Get the main frame's document and AXObjectCache for this (parent) process.
    RefPtr coreFrame = m_mainFrame->coreLocalFrame();
    RefPtr document = coreFrame ? coreFrame->document() : nullptr;
    CheckedPtr cache = document ? document->axObjectCache() : nullptr;
    if (!cache) {
        completionHandler({ });
        return;
    }

    // Find the AXRemoteFrame via the frame tree and AccessibilityScrollView.
    RefPtr remoteFrame = dynamicDowncast<WebCore::RemoteFrame>(coreFrame->tree().descendantByFrameID(childFrameID));
    RefPtr remoteFrameView = remoteFrame ? remoteFrame->view() : nullptr;
    RefPtr scrollView = dynamicDowncast<WebCore::AccessibilityScrollView>(cache->get(remoteFrameView.get()));
    RefPtr childRemoteFrame = scrollView ? scrollView->remoteFrame() : nullptr;

    if (!childRemoteFrame) {
        completionHandler({ });
        return;
    }

    unsigned originalLimit = criteria.resultsLimit;
    // Get the web area as the anchor and use the child remote frame as the start object.
    RefPtr webArea = cache->rootObjectForFrame(*coreFrame);
    if (!webArea) {
        completionHandler({ });
        return;
    }

    // Create local criteria with webArea as anchor and AXRemoteFrame as start object.
    auto localCriteria = criteria.toSearchCriteria(webArea.get());
    localCriteria.startObject = childRemoteFrame.get();

    auto stream = WebCore::AXSearchManager().findMatchingObjectsAsStream(WTF::move(localCriteria));
    // Perform cross-process search coordination if there are nested remote frames in this process.
    // Pass childFrameID as the excluded frame to prevent re-searching the frame that requested continuation.
    auto searchResults = WebCore::performCrossProcessSearch(WTF::move(stream), criteria, webArea->treeID(), originalLimit, childFrameID);

    completionHandler(convertSearchResultsToRemoteTokens(WTF::move(searchResults)));
}
#endif // PLATFORM(MAC)

#if PLATFORM(MAC)
void WebPage::getAccessibilityWebProcessDebugInfo(CompletionHandler<void(WebCore::AXDebugInfo)>&& completionHandler)
{
    if (!AXObjectCache::isAppleInternalInstall()) {
        completionHandler({ });
        return;
    }

    bool isAXThreadInitialized = false;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    isAXThreadInitialized = WebCore::AXObjectCache::isAXThreadInitialized();
#endif
    Vector<String> warnings;

    RefPtr focusedFrame = [m_mockAccessibilityElement focusedLocalFrame];
    RefPtr document = focusedFrame ? focusedFrame->document() : nullptr;

    if (document) {
        if (CheckedPtr cache = document->axObjectCache()) {
            auto treeData = cache->treeData();
            warnings = WTF::move(treeData.warnings);
            completionHandler({ WebCore::AXObjectCache::accessibilityEnabled(), isAXThreadInitialized, WTF::move(treeData.liveTree), WTF::move(treeData.isolatedTree), WTF::move(warnings), [m_mockAccessibilityElement remoteTokenHash], [accessibilityRemoteTokenData() hash] });
            return;
        }
        warnings.append("No AXObjectCache"_s);
    } else if (!focusedFrame)
        warnings.append("No focused LocalFrame found"_s);
    else
        warnings.append("Focused LocalFrame has no document"_s);

    completionHandler({ WebCore::AXObjectCache::accessibilityEnabled(), isAXThreadInitialized, emptyString(), emptyString(), WTF::move(warnings), [m_mockAccessibilityElement remoteTokenHash], [accessibilityRemoteTokenData() hash] });
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void WebPage::clearAccessibilityIsolatedTree()
{
    if (RefPtr page = m_page)
        page->clearAccessibilityIsolatedTree();
}
#endif

#endif // PLATFORM(MAC)

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
    RefPtr localFrame = corePage()->localMainFrame();
    completionHandler(localFrame ? attributedString(makeRangeSelectingNodeContents(*protect(localFrame->document())), IgnoreUserSelectNone::No) : AttributedString { });
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
    [m_mockAccessibilityElement setHasMainFramePlugin:document && document->isPluginDocument()];
}

void WebPage::getProcessDisplayName(CompletionHandler<void(String&&)>&& completionHandler)
{
#if PLATFORM(MAC)
#if ENABLE(SET_WEBCONTENT_PROCESS_INFORMATION_IN_NETWORK_PROCESS)
    WebProcess::singleton().getProcessDisplayName(WTF::move(completionHandler));
#else
    completionHandler(adoptCF((CFStringRef)_LSCopyApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), _kLSDisplayNameKey)).get());
#endif
#else
    completionHandler({ });
#endif
}

static bool rendererIsTransparentOrFullyClipped(const RenderObject& renderer)
{
    CheckedPtr enclosingLayer = renderer.enclosingLayer();
    if (enclosingLayer && enclosingLayer->isTransparentRespectingParentFrames())
        return true;

    return renderer.hasEmptyVisibleRectRespectingParentFrames();
}

bool WebPage::isTransparentOrFullyClipped(const Node& node) const
{
    CheckedPtr renderer = node.renderer();
    if (!renderer)
        return false;
    return rendererIsTransparentOrFullyClipped(*renderer);
}

static bool selectionIsTransparentOrFullyClipped(const VisibleSelection& selection)
{
    RefPtr startContainer = selection.start().containerNode();
    if (!startContainer)
        return false;

    RefPtr endContainer = selection.end().containerNode();
    if (!endContainer)
        return false;

    CheckedPtr startRenderer = startContainer->renderer();
    if (!startRenderer)
        return false;

    CheckedPtr endRenderer = endContainer->renderer();
    if (!endRenderer)
        return false;

    if (!rendererIsTransparentOrFullyClipped(*startRenderer))
        return false;

    return startRenderer == endRenderer || rendererIsTransparentOrFullyClipped(*endRenderer);
}

static void convertContentToRootView(const LocalFrameView& view, Vector<SelectionGeometry>& geometries)
{
    for (auto& geometry : geometries)
        geometry.setQuad(view.contentsToRootView(geometry.quad()));
}

void WebPage::getPlatformEditorStateCommon(LocalFrame& frame, EditorState& result) const
{
    if (!result.hasPostLayoutAndVisualData())
        return;

    const auto& selection = frame.selection().selection();

    if (selection.isNone())
        return;

    ASSERT(frame.view());
    Ref view = *frame.view();

    auto& postLayoutData = *result.postLayoutData;
    auto& visualData = *result.visualData;

    if (result.isContentEditable) {
        if (auto editingStyle = EditingStyle::styleAtSelectionStart(selection, false, EditingStyle::PropertiesToInclude::PostLayoutProperties)) {
            if (editingStyle->fontWeightIsBold())
                postLayoutData.typingAttributes.add(TypingAttribute::Bold);

            if (editingStyle->fontStyleIsItalic())
                postLayoutData.typingAttributes.add(TypingAttribute::Italics);

            if (editingStyle->webkitTextDecorationsInEffectIsUnderline())
                postLayoutData.typingAttributes.add(TypingAttribute::Underline);

            if (RefPtr styleProperties = editingStyle->style()) {
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

        postLayoutData.baseWritingDirection = protect(frame.editor())->baseWritingDirectionForSelectionStart();
        postLayoutData.canEnableWritingSuggestions = [&] {
            if (!selection.canEnableWritingSuggestions())
                return false;

            if (!m_lastNodeBeforeWritingSuggestions)
                return true;

            RefPtr currentNode = protect(frame.editor())->nodeBeforeWritingSuggestions();
            return !currentNode || m_lastNodeBeforeWritingSuggestions == currentNode.get();
        }();
    }

    RefPtr enclosingFormControl = enclosingTextFormControl(selection.start());
    if (RefPtr editableRootOrFormControl = enclosingFormControl.get() ?: selection.rootEditableElement()) {
        postLayoutData.selectionIsTransparentOrFullyClipped = result.isContentEditable && isTransparentOrFullyClipped(*editableRootOrFormControl);
#if PLATFORM(IOS_FAMILY)
        result.visualData->editableRootBounds = rootViewInteractionBounds(Ref { *editableRootOrFormControl });
#endif
    } else if (result.selectionType == WebCore::SelectionType::Range)
        postLayoutData.selectionIsTransparentOrFullyClipped = selectionIsTransparentOrFullyClipped(selection);

#if PLATFORM(IOS_FAMILY)
    if (enclosingFormControl || !m_page->settings().selectionHonorsOverflowScrolling())
        result.visualData->selectionClipRect = result.visualData->editableRootBounds;
#endif

    bool startNodeIsInsideFixedPosition = false;
    bool endNodeIsInsideFixedPosition = false;

    if (selection.isCaret()) {
        visualData.caretRectAtStart = view->contentsToRootView(WTF::protect(frame.selection())->absoluteCaretBounds(&startNodeIsInsideFixedPosition));
        endNodeIsInsideFixedPosition = startNodeIsInsideFixedPosition;
        visualData.caretRectAtEnd = visualData.caretRectAtStart;
    } else if (selection.isRange()) {
        visualData.caretRectAtStart = view->contentsToRootView(VisiblePosition(selection.start()).absoluteCaretBounds(&startNodeIsInsideFixedPosition));
        visualData.caretRectAtEnd = view->contentsToRootView(VisiblePosition(selection.end()).absoluteCaretBounds(&endNodeIsInsideFixedPosition));

        auto selectedRange = selection.toNormalizedRange();
        if (selectedRange) {
            auto [selectionGeometries, intersectingLayerIDs] = RenderObject::collectSelectionGeometries(*selectedRange);
            convertContentToRootView(view, selectionGeometries);

            visualData.selectionGeometries = WTF::move(selectionGeometries);
            visualData.intersectingLayerIDs = WTF::move(intersectingLayerIDs);
        }
    }

    postLayoutData.insideFixedPosition = startNodeIsInsideFixedPosition || endNodeIsInsideFixedPosition;
}

void WebPage::getPDFFirstPageSize(WebCore::FrameIdentifier frameID, CompletionHandler<void(WebCore::FloatSize)>&& completionHandler)
{
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame)
        return completionHandler({ });

#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = pluginViewForFrame(protect(webFrame->coreLocalFrame()).get()))
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
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
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
        protect(frame->editor())->replaceNodeFromPasteboard(*element, replaceSelectionPasteboardName(), EditAction::RemoveBackground);

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
        protect(frame->selection())->setSelection(originalSelection, restoreSelectionOptions);
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
    protect(frame->selection())->setSelection(newSelectionRange, restoreSelectionOptions);
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
    protect(frame->editor())->readSelectionFromPasteboard(pasteboardName);
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
    if (m_internals->linkDecorationFilteringData.isEmpty()) {
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
        return effectivePolicies.contains(AdvancedPrivacyProtections::LinkDecorationFiltering);
    };

    bool hasOptedInToLinkDecorationFiltering = [&] {
        if (isLinkDecorationFilteringEnabled(mainFrame->loader().activeDocumentLoader()))
            return true;

        return isLinkDecorationFilteringEnabled(mainFrame->loader().policyDocumentLoader());
    }();

    RefPtr document = mainFrame ? mainFrame->document() : nullptr;
    bool isConsistentQueryParameterFilteringQuirkEnabled = document && (document->quirks().needsConsistentQueryParameterFilteringQuirk(document->url()) || document->quirks().needsConsistentQueryParameterFilteringQuirk(url));
    if (!hasOptedInToLinkDecorationFiltering && !m_page->settings().filterLinkDecorationByDefaultEnabled() && !isConsistentQueryParameterFilteringQuirkEnabled)
        return { url, DidFilterLinkDecoration::No };

    if (!url.hasQuery())
        return { url, DidFilterLinkDecoration::No };

    auto sanitizedURL = url;
    bool allowLowEntropyException = !(hasOptedInToLinkDecorationFiltering || isConsistentQueryParameterFilteringQuirkEnabled);
    auto removedParameters = WTF::removeQueryParameters(sanitizedURL, [&](auto& key, auto& value) {
        auto it = m_internals->linkDecorationFilteringData.find(key);
        if (it == m_internals->linkDecorationFilteringData.end())
            return false;

        constexpr auto base = 10;
        if (value.length() == 3 && allowLowEntropyException && WTF::parseInteger<uint8_t>(value, base, WTF::ParseIntegerWhitespacePolicy::Disallow))
            return false;

        const auto& conditionals = it->value;
        bool isEmptyOrFoundDomain = conditionals.domains.isEmpty() || conditionals.domains.contains(RegistrableDomain { url });
        bool isEmptyOrFoundPath = conditionals.paths.isEmpty() || std::ranges::any_of(conditionals.paths,
            [&url](auto& path) {
                return url.path().contains(path);
            });

        return isEmptyOrFoundDomain && isEmptyOrFoundPath;
    });

    if (!removedParameters.isEmpty() && trigger != LinkDecorationFilteringTrigger::Unspecified) {
        if (trigger == LinkDecorationFilteringTrigger::Navigation)
            send(Messages::WebPageProxy::DidApplyLinkDecorationFiltering(url, sanitizedURL));
        auto removedParametersString = makeStringByJoining(removedParameters, ", "_s);
        WEBPAGE_RELEASE_LOG(ResourceLoadStatistics, "applyLinkDecorationFilteringWithResult: Blocked known tracking query parameters: %s", removedParametersString.utf8().data());
    }

    return { sanitizedURL, DidFilterLinkDecoration::Yes };
#else
    return { url, DidFilterLinkDecoration::No };
#endif
}

URL WebPage::allowedQueryParametersForAdvancedPrivacyProtections(const URL& url)
{
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    if (m_internals->allowedQueryParametersForAdvancedPrivacyProtections.isEmpty()) {
        RELEASE_LOG_ERROR(ResourceLoadStatistics, "Unable to hide query parameters from script (missing data)");
        return url;
    }

    if (!url.hasQuery() && !url.hasFragmentIdentifier())
        return url;

    auto sanitizedURL = url;

    auto allowedParameters = m_internals->allowedQueryParametersForAdvancedPrivacyProtections.get(RegistrableDomain { sanitizedURL });

    if (!allowedParameters.contains("#"_s))
        sanitizedURL.removeFragmentIdentifier();

    WTF::removeQueryParameters(sanitizedURL, [&](auto& key, auto&) {
        return !allowedParameters.contains(key);
    });

    return sanitizedURL;
#else
    return url;
#endif
}

#if ENABLE(EXTENSION_CAPABILITIES)
void WebPage::setMediaPlaybackEnvironment(const String& environment)
{
    m_mediaPlaybackEnvironment = environment;
    if (RefPtr gpuProcessConnection = WebProcess::singleton().existingGPUProcessConnection())
        gpuProcessConnection->setMediaPlaybackEnvironment(identifier(), environment);
}

void WebPage::setDisplayCaptureEnvironment(const String& environment)
{
    m_displayCaptureEnvironment = environment;
    if (RefPtr gpuProcessConnection = WebProcess::singleton().existingGPUProcessConnection())
        gpuProcessConnection->setDisplayCaptureEnvironment(identifier(), environment);
}
#endif

#if ENABLE(WRITING_TOOLS)
void WebPage::willBeginWritingToolsSession(const std::optional<WebCore::WritingTools::Session>& session, CompletionHandler<void(const Vector<WebCore::WritingTools::Context>&)>&& completionHandler)
{
    protect(corePage())->willBeginWritingToolsSession(session, WTF::move(completionHandler));
}

void WebPage::didBeginWritingToolsSession(const WebCore::WritingTools::Session& session, const Vector<WebCore::WritingTools::Context>& contexts)
{
    protect(corePage())->didBeginWritingToolsSession(session, contexts);
}

void WebPage::proofreadingSessionDidReceiveSuggestions(const WebCore::WritingTools::Session& session, const Vector<WebCore::WritingTools::TextSuggestion>& suggestions, const WebCore::CharacterRange& processedRange, const WebCore::WritingTools::Context& context, bool finished, CompletionHandler<void()>&& completionHandler)
{
    protect(corePage())->proofreadingSessionDidReceiveSuggestions(session, suggestions, processedRange, context, finished);
    completionHandler();
}

void WebPage::proofreadingSessionDidUpdateStateForSuggestion(const WebCore::WritingTools::Session& session, WebCore::WritingTools::TextSuggestion::State state, const WebCore::WritingTools::TextSuggestion& suggestion, const WebCore::WritingTools::Context& context)
{
    protect(corePage())->proofreadingSessionDidUpdateStateForSuggestion(session, state, suggestion, context);
}

void WebPage::willEndWritingToolsSession(const WebCore::WritingTools::Session& session, bool accepted, CompletionHandler<void()>&& completionHandler)
{
    protect(corePage())->willEndWritingToolsSession(session, accepted);
    completionHandler();
}

void WebPage::didEndWritingToolsSession(const WebCore::WritingTools::Session& session, bool accepted)
{
    protect(corePage())->didEndWritingToolsSession(session, accepted);
}

void WebPage::compositionSessionDidReceiveTextWithReplacementRange(const WebCore::WritingTools::Session& session, const WebCore::AttributedString& attributedText, const WebCore::CharacterRange& range, const WebCore::WritingTools::Context& context, bool finished, CompletionHandler<void()>&& completionHandler)
{
    protect(corePage())->compositionSessionDidReceiveTextWithReplacementRange(session, attributedText, range, context, finished);
    completionHandler();
}

void WebPage::writingToolsSessionDidReceiveAction(const WritingTools::Session& session, WebCore::WritingTools::Action action)
{
    protect(corePage())->writingToolsSessionDidReceiveAction(session, action);
}

void WebPage::proofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(const WebCore::WritingTools::TextSuggestion::ID& replacementID, WebCore::IntRect rect)
{
    send(Messages::WebPageProxy::ProofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(replacementID, rect));
}

void WebPage::proofreadingSessionUpdateStateForSuggestionWithID(WebCore::WritingTools::TextSuggestion::State state, const WebCore::WritingTools::TextSuggestion::ID& replacementID)
{
    send(Messages::WebPageProxy::ProofreadingSessionUpdateStateForSuggestionWithID(state, replacementID));
}

void WebPage::addTextAnimationForAnimationID(const WTF::UUID& uuid, const WebCore::TextAnimationData& styleData, const RefPtr<WebCore::TextIndicator> textIndicator, CompletionHandler<void(WebCore::TextAnimationRunMode)>&& completionHandler)
{
    if (completionHandler)
        sendWithAsyncReply(Messages::WebPageProxy::AddTextAnimationForAnimationIDWithCompletionHandler(uuid, styleData, textIndicator), WTF::move(completionHandler));
    else
        send(Messages::WebPageProxy::AddTextAnimationForAnimationID(uuid, styleData, textIndicator));
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
    m_textAnimationController->addSourceTextAnimationForActiveWritingToolsSession(sourceAnimationUUID, destinationAnimationUUID, finished, range, string, WTF::move(completionHandler));
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

void WebPage::createTextIndicatorForTextAnimationID(const WTF::UUID& uuid, CompletionHandler<void(RefPtr<WebCore::TextIndicator>&&)>&& completionHandler)
{
    m_textAnimationController->createTextIndicatorForTextAnimationID(uuid, WTF::move(completionHandler));
}

void WebPage::updateUnderlyingTextVisibilityForTextAnimationID(const WTF::UUID& uuid, bool visible, CompletionHandler<void()>&& completionHandler)
{
    m_textAnimationController->updateUnderlyingTextVisibilityForTextAnimationID(uuid, visible, WTF::move(completionHandler));
}

void WebPage::proofreadingSessionSuggestionTextRectsInRootViewCoordinates(const WebCore::CharacterRange& enclosingRangeRelativeToSessionRange, CompletionHandler<void(Vector<FloatRect>&&)>&& completionHandler) const
{
    auto rects = protect(corePage())->proofreadingSessionSuggestionTextRectsInRootViewCoordinates(enclosingRangeRelativeToSessionRange);
    completionHandler(WTF::move(rects));
}

void WebPage::updateTextVisibilityForActiveWritingToolsSession(const WebCore::CharacterRange& rangeRelativeToSessionRange, bool visible, const WTF::UUID& identifier, CompletionHandler<void()>&& completionHandler)
{
    protect(corePage())->updateTextVisibilityForActiveWritingToolsSession(rangeRelativeToSessionRange, visible, identifier);
    completionHandler();
}

void WebPage::textPreviewDataForActiveWritingToolsSession(const WebCore::CharacterRange& rangeRelativeToSessionRange, CompletionHandler<void(RefPtr<WebCore::TextIndicator>&&)>&& completionHandler)
{
    RefPtr textIndicator = protect(corePage())->textPreviewDataForActiveWritingToolsSession(rangeRelativeToSessionRange);
    completionHandler(WTF::move(textIndicator));
}

void WebPage::decorateTextReplacementsForActiveWritingToolsSession(const WebCore::CharacterRange& rangeRelativeToSessionRange, CompletionHandler<void(void)>&& completionHandler)
{
    protect(corePage())->decorateTextReplacementsForActiveWritingToolsSession(rangeRelativeToSessionRange);
    completionHandler();
}

void WebPage::setSelectionForActiveWritingToolsSession(const WebCore::CharacterRange& rangeRelativeToSessionRange, CompletionHandler<void(void)>&& completionHandler)
{
    protect(corePage())->setSelectionForActiveWritingToolsSession(rangeRelativeToSessionRange);
    completionHandler();
}

void WebPage::intelligenceTextAnimationsDidComplete()
{
    protect(corePage())->intelligenceTextAnimationsDidComplete();
}

void WebPage::didEndPartialIntelligenceTextAnimation()
{
    send(Messages::WebPageProxy::DidEndPartialIntelligenceTextAnimation());
}

#endif

static std::optional<bool> elementHasHiddenVisibility(StyledElement* styledElement)
{
    RefPtr inlineStyle = styledElement->inlineStyle();
    if (!inlineStyle)
        return std::nullopt;

    RefPtr value = inlineStyle->getPropertyCSSValue(CSSPropertyVisibility);
    if (!value)
        return false;

    return value->valueID() == CSSValueHidden;
}

void WebPage::createTextIndicatorForElementWithID(const String& elementID, CompletionHandler<void(RefPtr<WebCore::TextIndicator>&&)>&& completionHandler)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame) {
        ASSERT_NOT_REACHED();
        completionHandler(nil);
        return;
    }

    RefPtr document = frame->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        completionHandler(nil);
        return;
    }

    RefPtr element = document->getElementById(elementID);
    if (!element) {
        ASSERT_NOT_REACHED();
        completionHandler(nil);
        return;
    }

    RefPtr styledElement = dynamicDowncast<StyledElement>(element.get());
    if (!styledElement) {
        ASSERT_NOT_REACHED();
        completionHandler(nil);
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
        completionHandler(nil);
        return;
    }

    // If `initialVisibility` is an empty optional, this means there was no initial inline style.
    // Ensure the state is idempotent after by removing the inline style if this is the case.

    if (isHiddenInitially.has_value())
        styledElement->setInlineStyleProperty(CSSPropertyVisibility, *isHiddenInitially ? CSSValueHidden : CSSValueVisible, IsImportant::Yes);
    else
        styledElement->removeInlineStyleProperty(CSSPropertyVisibility);

    completionHandler(WTF::move(textIndicator));
}

void WebPage::createBitmapsFromImageData(Ref<WebCore::SharedBuffer>&& buffer, const Vector<unsigned>& lengths, CompletionHandler<void(Vector<Ref<WebCore::ShareableBitmap>>&&)>&& completionHandler)
{
    WebCore::createBitmapsFromImageData(buffer->span(), lengths.span(), WTF::move(completionHandler));
}

void WebPage::decodeImageData(Ref<WebCore::SharedBuffer>&& buffer, std::optional<WebCore::FloatSize> preferredSize, CompletionHandler<void(RefPtr<WebCore::ShareableBitmap>&&)>&& completionHandler)
{
    decodeImageWithSize(buffer->span(), preferredSize, WTF::move(completionHandler));
}

#if HAVE(PDFKIT)

void WebPage::computePagesForPrintingPDFDocument(WebCore::FrameIdentifier frameID, const PrintInfo& printInfo, Vector<IntRect>& resultPageRects)
{
    ASSERT(resultPageRects.isEmpty());
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    RefPtr coreFrame = frame ? frame->coreLocalFrame() : nullptr;
    RetainPtr<PDFDocument> pdfDocument = coreFrame ? pdfDocumentForPrintingFrame(coreFrame.get()) : 0;
    if ([pdfDocument allowsPrinting]) {
        NSUInteger pageCount = [pdfDocument pageCount];
        IntRect pageRect(0, 0, ceilf(printInfo.availablePaperWidth), ceilf(printInfo.availablePaperHeight));
        for (NSUInteger i = 1; i <= pageCount; ++i) {
            resultPageRects.append(pageRect);
            pageRect.move(0, pageRect.height());
        }
    }
}

static inline CGFloat roundCGFloat(CGFloat f)
{
    if (sizeof(CGFloat) == sizeof(float))
        return roundf(static_cast<float>(f));
    return static_cast<CGFloat>(round(f));
}

static void drawPDFPage(PDFDocument *pdfDocument, CFIndex pageIndex, CGContextRef context, CGFloat pageSetupScaleFactor, CGSize paperSize)
{
    CGContextSaveGState(context);

    CGContextScaleCTM(context, pageSetupScaleFactor, pageSetupScaleFactor);

    RetainPtr<PDFPage> pdfPage = [pdfDocument pageAtIndex:pageIndex];
    NSRect cropBox = [pdfPage boundsForBox:kPDFDisplayBoxCropBox];
    if (NSIsEmptyRect(cropBox))
        cropBox = [pdfPage boundsForBox:kPDFDisplayBoxMediaBox];
    else
        cropBox = NSIntersectionRect(cropBox, [pdfPage boundsForBox:kPDFDisplayBoxMediaBox]);

    // Always auto-rotate PDF content regardless of the paper orientation.
    NSInteger rotation = [pdfPage rotation];
    if (rotation == 90 || rotation == 270)
        std::swap(cropBox.size.width, cropBox.size.height);

    bool shouldRotate = (paperSize.width < paperSize.height) != (cropBox.size.width < cropBox.size.height);
    if (shouldRotate)
        std::swap(cropBox.size.width, cropBox.size.height);

    // Center.
    CGFloat widthDifference = paperSize.width / pageSetupScaleFactor - cropBox.size.width;
    CGFloat heightDifference = paperSize.height / pageSetupScaleFactor - cropBox.size.height;
    if (widthDifference || heightDifference)
        CGContextTranslateCTM(context, roundCGFloat(widthDifference / 2), roundCGFloat(heightDifference / 2));

    if (shouldRotate) {
        CGContextRotateCTM(context, static_cast<CGFloat>(piOverTwoDouble));
        CGContextTranslateCTM(context, 0, -cropBox.size.width);
    }

    [pdfPage drawWithBox:kPDFDisplayBoxCropBox toContext:context];

    CGAffineTransform transform = CGContextGetCTM(context);

    for (PDFAnnotation *annotation in [pdfPage annotations]) {
        if (![[annotation valueForAnnotationKey:get_PDFKit_PDFAnnotationKeySubtypeSingleton()] isEqualToString:get_PDFKit_PDFAnnotationSubtypeLinkSingleton()])
            continue;

        RetainPtr<NSURL> url = annotation.URL;
        if (!url)
            continue;

        CGRect transformedRect = CGRectApplyAffineTransform(annotation.bounds, transform);
        CGPDFContextSetURLForRect(context, (CFURLRef)url.get(), transformedRect);
    }

    CGContextRestoreGState(context);
}

void WebPage::drawPDFDocument(CGContextRef context, PDFDocument *pdfDocument, const PrintInfo& printInfo, const WebCore::IntRect& rect)
{
    NSUInteger pageCount = [pdfDocument pageCount];
    IntSize paperSize(ceilf(printInfo.availablePaperWidth), ceilf(printInfo.availablePaperHeight));
    IntRect pageRect(IntPoint(), paperSize);
    for (NSUInteger i = 0; i < pageCount; ++i) {
        if (pageRect.intersects(rect)) {
            CGContextSaveGState(context);

            CGContextTranslateCTM(context, pageRect.x() - rect.x(), pageRect.y() - rect.y());
            drawPDFPage(pdfDocument, i, context, printInfo.pageSetupScaleFactor, paperSize);

            CGContextRestoreGState(context);
        }
        pageRect.move(0, pageRect.height());
    }
}

void WebPage::drawPagesToPDFFromPDFDocument(GraphicsContext& context, PDFDocument *pdfDocument, const PrintInfo& printInfo, const WebCore::FloatRect& mediaBox, uint32_t first, uint32_t count)
{
    NSUInteger pageCount = [pdfDocument pageCount];
    for (uint32_t page = first; page < first + count; ++page) {
        if (page >= pageCount)
            break;

        context.beginPage(mediaBox);
        drawPDFPage(pdfDocument, page, protect(context.platformContext()).get(), printInfo.pageSetupScaleFactor, CGSizeMake(printInfo.availablePaperWidth, printInfo.availablePaperHeight));
        context.endPage();
    }
}

#else

void WebPage::drawPDFDocument(CGContextRef, PDFDocument *, const PrintInfo&, const WebCore::IntRect&)
{
    notImplemented();
}

void WebPage::computePagesForPrintingPDFDocument(WebCore::FrameIdentifier, const PrintInfo&, Vector<IntRect>&)
{
    notImplemented();
}

void WebPage::drawPagesToPDFFromPDFDocument(GraphicsContext&, PDFDocument *, const PrintInfo&, const WebCore::FloatRect&, uint32_t, uint32_t)
{
    notImplemented();
}

#endif

BoxSideSet WebPage::sidesRequiringFixedContainerEdges() const
{
    if (!m_page->settings().contentInsetBackgroundFillEnabled())
        return { };

#if PLATFORM(IOS_FAMILY)
    auto obscuredInsets = m_page->obscuredInsets();
#else
    auto obscuredInsets = m_page->obscuredContentInsets();
#endif

#if PLATFORM(MAC)
    auto additionalHeight = m_overflowHeightForTopScrollEdgeEffect;
#else
    auto additionalHeight = 0;
#endif

    auto sides = m_page->fixedContainerEdges().fixedEdges();

    if ((additionalHeight + obscuredInsets.top()) > 0)
        sides.add(BoxSide::Top);

    if (obscuredInsets.left() > 0)
        sides.add(BoxSide::Left);

    if (obscuredInsets.right() > 0)
        sides.add(BoxSide::Right);

    if (obscuredInsets.bottom() > 0)
        sides.add(BoxSide::Bottom);

    return sides;
}

void WebPage::getWebArchivesForFrames(const Vector<WebCore::FrameIdentifier>& frameIdentifiers, CompletionHandler<void(HashMap<WebCore::FrameIdentifier, Ref<WebCore::LegacyWebArchive>>&&)>&& completionHandler)
{
    if (!m_page)
        return completionHandler({ });

    HashMap<WebCore::FrameIdentifier, Ref<LegacyWebArchive>> result;
    for (auto& frameIdentifier : frameIdentifiers) {
        RefPtr frame = WebFrame::webFrame(frameIdentifier);
        if (!frame)
            continue;

        RefPtr localFrame = frame->coreLocalFrame();
        if (!localFrame)
            continue;

        RefPtr document = localFrame->document();
        if (!document)
            continue;

        WebCore::LegacyWebArchive::ArchiveOptions options {
            LegacyWebArchive::ShouldSaveScriptsFromMemoryCache::Yes,
            LegacyWebArchive::ShouldArchiveSubframes::No
        };
        if (RefPtr archive = WebCore::LegacyWebArchive::create(*document, WTF::move(options)))
            result.add(localFrame->frameID(), archive.releaseNonNull());
    }
    completionHandler(WTF::move(result));
}

void WebPage::getWebArchiveData(CompletionHandler<void(const std::optional<IPC::SharedBufferReference>&)>&& completionHandler)
{
    RetainPtr<CFDataRef> data = m_mainFrame->webArchiveData(nullptr, nullptr);
    completionHandler(IPC::SharedBufferReference(SharedBuffer::create(data.get())));
}

void WebPage::processSystemWillSleep() const
{
    if (RefPtr manager = mediaSessionManagerIfExists())
        manager->processSystemWillSleep();
}

void WebPage::processSystemDidWake() const
{
    if (RefPtr manager = mediaSessionManagerIfExists())
        manager->processSystemDidWake();
}

NSObject *WebPage::accessibilityObjectForMainFramePlugin()
{
#if ENABLE(PDF_PLUGIN)
    if (!m_page)
        return nil;

    if (RefPtr pluginView = mainFramePlugIn())
        return pluginView->accessibilityObject();
#endif

    return nil;
}

bool WebPage::shouldFallbackToWebContentAXObjectForMainFramePlugin() const
{
#if ENABLE(PDF_PLUGIN)
    RefPtr pluginView = mainFramePlugIn();
    return pluginView && pluginView->isPresentingLockedContent();
#else
    return false;
#endif
}

WKAccessibilityWebPageObject* WebPage::accessibilityRemoteObject()
{
    return m_mockAccessibilityElement.get();
}

RetainPtr<PDFDocument> WebPage::pdfDocumentForPrintingFrame(LocalFrame* coreFrame)
{
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = pluginViewForFrame(coreFrame))
        return pluginView->pdfDocumentForPrinting();
#endif
    return nullptr;
}

void WebPage::drawToPDF(const std::optional<FloatRect>& rect, bool allowTransparentBackground, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&& completionHandler)
{
    RefPtr localMainFrame = this->localMainFrame();
    if (!localMainFrame)
        return;

    Ref frameView = *localMainFrame->view();
    auto snapshotRect = IntRect { rect.value_or(FloatRect { { }, frameView->contentsSize() }) };

    RefPtr buffer = ImageBuffer::create(snapshotRect.size(), RenderingMode::PDFDocument, RenderingPurpose::Snapshot, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!buffer)
        return;

    drawMainFrameToPDF(*localMainFrame, buffer->context(), snapshotRect, allowTransparentBackground);
    completionHandler(buffer->sinkIntoPDFDocument());
}

void WebPage::drawRectToImage(FrameIdentifier frameID, const PrintInfo& printInfo, const IntRect& rect, const WebCore::IntSize& imageSize, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&& completionHandler)
{
    PrintContextAccessScope scope { *this };
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    RefPtr coreFrame = frame ? frame->coreLocalFrame() : 0;

    RefPtr<WebImage> image;

#if USE(CG)
    if (coreFrame) {
        ASSERT(coreFrame->document()->printing() || pdfDocumentForPrintingFrame(coreFrame.get()));
        image = WebImage::create(imageSize, ImageOption::Local, DestinationColorSpace::SRGB(), &m_page->chrome().client());
        if (!image || !image->context()) {
            ASSERT_NOT_REACHED();
            return completionHandler({ });
        }

        auto& graphicsContext = *image->context();
        float printingScale = static_cast<float>(imageSize.width()) / rect.width();
        graphicsContext.scale(printingScale);

        if (RetainPtr<PDFDocument> pdfDocument = pdfDocumentForPrintingFrame(coreFrame.get())) {
            ASSERT(!m_printContext);
            graphicsContext.scale(FloatSize(1, -1));
            graphicsContext.translate(0, -rect.height());
            drawPDFDocument(protect(graphicsContext.platformContext()).get(), pdfDocument.get(), printInfo, rect);
        } else
            Ref { *m_printContext }->spoolRect(graphicsContext, rect);
    }
#endif

    std::optional<ShareableBitmap::Handle> handle;
    if (image)
        handle = image->createHandle(SharedMemory::Protection::ReadOnly);

    completionHandler(WTF::move(handle));
}

void WebPage::drawPagesToPDF(FrameIdentifier frameID, const PrintInfo& printInfo, uint32_t first, uint32_t count, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&& callback)
{
    PrintContextAccessScope scope { *this };
    RefPtr<SharedBuffer> pdfPageData;
    drawPagesToPDFImpl(frameID, printInfo, first, count, pdfPageData);
    callback(WTF::move(pdfPageData));
}

void WebPage::drawPagesToPDFImpl(FrameIdentifier frameID, const PrintInfo& printInfo, uint32_t first, uint32_t count, RefPtr<SharedBuffer>& pdfPageData)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    RefPtr coreFrame = frame ? frame->coreLocalFrame() : 0;

#if USE(CG)
    if (coreFrame) {
        ASSERT(coreFrame->document()->printing() || pdfDocumentForPrintingFrame(coreFrame.get()));

        FloatRect mediaBox = (m_printContext && m_printContext->pageCount()) ? m_printContext->pageRect(0) : FloatRect { 0, 0, printInfo.availablePaperWidth, printInfo.availablePaperHeight };

        RefPtr buffer = ImageBuffer::create(mediaBox.size(), RenderingMode::PDFDocument, RenderingPurpose::Snapshot, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
        if (!buffer)
            return;
        GraphicsContext& context = buffer->context();

        if (RetainPtr<PDFDocument> pdfDocument = pdfDocumentForPrintingFrame(coreFrame.get())) {
            ASSERT(!m_printContext);
            drawPagesToPDFFromPDFDocument(context, pdfDocument.get(), printInfo, mediaBox, first, count);
        } else {
            if (!m_printContext)
                return;

            drawPrintContextPagesToGraphicsContext(context, mediaBox, first, count);
        }
        pdfPageData = ImageBuffer::sinkIntoPDFDocument(WTF::move(buffer));
    }
#endif
}

void WebPage::drawPrintContextPagesToGraphicsContext(GraphicsContext& context, const FloatRect& pageRect, uint32_t first, uint32_t count)
{
    RefPtr printContext = m_printContext;
    for (uint32_t page = first; page < first + count; ++page) {
        if (page >= printContext->pageCount())
            break;

        context.beginPage(pageRect);

        context.scale(FloatSize(1, -1));
        context.translate(0, -printContext->pageRect(page).height());
        printContext->spoolPage(context, page, printContext->pageRect(page).width());

        context.endPage();
    }
}

void WebPage::drawPrintingRectToSnapshot(RemoteSnapshotIdentifier snapshotIdentifier, WebCore::FrameIdentifier frameID, const PrintInfo& printInfo, const WebCore::IntRect& rect, const WebCore::IntSize& imageSize, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame) {
        completionHandler(false);
        return;
    }

    RefPtr coreFrame = frame->coreLocalFrame();
    if (!coreFrame) {
        completionHandler(false);
        return;
    }

    if (pdfDocumentForPrintingFrame(coreFrame.get())) {
        // Can't do this remotely.
        completionHandler(false);
        return;
    }
    ASSERT(coreFrame->document()->printing());
    PrintContextAccessScope scope { *this };

    Ref remoteRenderingBackend = ensureRemoteRenderingBackendProxy();
    m_remoteSnapshotState = {
        snapshotIdentifier,
        remoteRenderingBackend->createSnapshotRecorder(snapshotIdentifier),
        MainRunLoopSuccessCallbackAggregator::create([completionHandler = WTF::move(completionHandler)] (bool success) mutable {
            completionHandler(success);
        })
    };
    GraphicsContext& context = m_remoteSnapshotState->recorder.get();

    float printingScale = static_cast<float>(imageSize.width()) / rect.width();
    context.scale(printingScale);

    Ref { *m_printContext }->spoolRect(context, rect);

    remoteRenderingBackend->sinkSnapshotRecorderIntoSnapshotFrame(WTF::move(m_remoteSnapshotState->recorder), frameID, Ref { m_remoteSnapshotState->callback }->chain());
    m_remoteSnapshotState = std::nullopt;
}

void WebPage::drawPrintingPagesToSnapshot(RemoteSnapshotIdentifier snapshotIdentifier, FrameIdentifier frameID, const PrintInfo& printInfo, uint32_t first, uint32_t count, CompletionHandler<void(std::optional<WebCore::FloatSize>)>&& completionHandler)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    if (!frame) {
        completionHandler({ });
        return;
    }

    RefPtr coreFrame = frame->coreLocalFrame();
    if (!coreFrame) {
        completionHandler({ });
        return;
    }

    if (pdfDocumentForPrintingFrame(coreFrame.get())) {
        // Can't do this remotely.
        completionHandler({ });
        return;
    }

    if (!m_printContext) {
        completionHandler({ });
        return;
    }

    PrintContextAccessScope scope { *this };
    ASSERT(coreFrame->document()->printing());

    FloatRect mediaBox = (m_printContext && m_printContext->pageCount()) ? m_printContext->pageRect(0) : FloatRect { 0, 0, printInfo.availablePaperWidth, printInfo.availablePaperHeight };

    Ref remoteRenderingBackend = ensureRemoteRenderingBackendProxy();
    m_remoteSnapshotState = {
        snapshotIdentifier,
        remoteRenderingBackend->createSnapshotRecorder(snapshotIdentifier),
        MainRunLoopSuccessCallbackAggregator::create([completionHandler = WTF::move(completionHandler), snapshotSize = mediaBox.size()] (bool success) mutable {
            completionHandler(success ? std::optional<FloatSize>(snapshotSize) : std::nullopt);
        })
    };

    GraphicsContext& context = m_remoteSnapshotState->recorder.get();

    drawPrintContextPagesToGraphicsContext(context, mediaBox, first, count);

    remoteRenderingBackend->sinkSnapshotRecorderIntoSnapshotFrame(WTF::move(m_remoteSnapshotState->recorder), frameID, Ref { m_remoteSnapshotState->callback }->chain());
    m_remoteSnapshotState = std::nullopt;
}

void WebPage::handleAlternativeTextUIResult(const String& result)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    protect(frame->editor())->handleAlternativeTextUIResult(result);
}

void WebPage::setTextAsync(const String& text)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    if (frame->selection().selection().isContentEditable()) {
        UserTypingGestureIndicator indicator(*frame);
        protect(frame->selection())->selectAll();
        if (text.isEmpty())
            protect(frame->editor())->deleteSelectionWithSmartDelete(false);
        else
            protect(frame->editor())->insertText(text, nullptr, TextEventInputKeyboard);
        return;
    }

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(m_focusedElement)) {
        input->setValueForUser(text);
        return;
    }

    ASSERT_NOT_REACHED();
}

void WebPage::insertTextAsync(const String& text, const EditingRange& replacementEditingRange, InsertTextOptions&& options)
{
    platformWillPerformEditingCommand();

    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    UserGestureIndicator gestureIndicator { options.processingUserGesture ? IsProcessingUserGesture::Yes : IsProcessingUserGesture::No, frame->document() };
    std::optional<UserTypingGestureIndicator> userTypingGestureIndicator;
    if (options.processingUserGesture)
        userTypingGestureIndicator.emplace(*frame);

    bool replacesText = false;
    if (replacementEditingRange.location != notFound) {
        if (auto replacementRange = EditingRange::toRange(*frame, replacementEditingRange, options.editingRangeIsRelativeTo)) {
            SetForScope isSelectingTextWhileInsertingAsynchronously(m_isSelectingTextWhileInsertingAsynchronously, options.suppressSelectionUpdate);
            protect(frame->selection())->setSelection(VisibleSelection(*replacementRange));
            replacesText = replacementEditingRange.length;
        }
    }

    if (options.registerUndoGroup)
        send(Messages::WebPageProxy::RegisterInsertionUndoGrouping());

    RefPtr focusedElement = frame->document() ? frame->document()->focusedElement() : nullptr;
    if (focusedElement && options.shouldSimulateKeyboardInput)
        focusedElement->dispatchEvent(Event::create(eventNames().keydownEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));

    Ref editor = frame->editor();
    if (!editor->hasComposition()) {
        if (text.isEmpty() && frame->selection().isRange())
            editor->deleteWithDirection(SelectionDirection::Backward, TextGranularity::CharacterGranularity, false, true);
        else {
            // An insertText: might be handled by other responders in the chain if we don't handle it.
            // One example is space bar that results in scrolling down the page.
            editor->insertText(text, nullptr, replacesText ? TextEventInputAutocompletion : TextEventInputKeyboard);
        }
    } else
        editor->confirmComposition(text);

    auto baseWritingDirectionFromInputMode = [&] -> std::optional<WritingDirection> {
        auto direction = options.directionFromCurrentInputMode;
        if (!direction)
            return { };

        if (text != "\n"_s)
            return { };

        auto selection = frame->selection().selection();
        if (!selection.isCaret() || !selection.isContentEditable())
            return { };

        auto start = selection.visibleStart();
        if (!isStartOfLine(start) || !isEndOfLine(start))
            return { };

        if (direction == directionOfEnclosingBlock(start.deepEquivalent()))
            return { };

        return { direction == TextDirection::LTR ? WritingDirection::LeftToRight : WritingDirection::RightToLeft };
    }();

    if (baseWritingDirectionFromInputMode) {
        editor->setBaseWritingDirection(*baseWritingDirectionFromInputMode);
        editor->setTextAlignmentForChangedBaseWritingDirection(*baseWritingDirectionFromInputMode);
    }

    if (focusedElement && options.shouldSimulateKeyboardInput) {
        focusedElement->dispatchEvent(Event::create(eventNames().keyupEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));
        focusedElement->dispatchEvent(Event::create(eventNames().changeEvent, Event::CanBubble::Yes, Event::IsCancelable::Yes));
    }
}

void WebPage::hasMarkedText(CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr focusedOrMainFrame = corePage()->focusController().focusedOrMainFrame();
    if (!focusedOrMainFrame)
        return completionHandler(false);
    completionHandler(focusedOrMainFrame->editor().hasComposition());
}

void WebPage::getMarkedRangeAsync(CompletionHandler<void(const EditingRange&)>&& completionHandler)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return completionHandler({ });

    completionHandler(EditingRange::fromRange(*frame, protect(frame->editor())->compositionRange()));
}

void WebPage::getSelectedRangeAsync(CompletionHandler<void(const EditingRange& selectedRange, const EditingRange& compositionRange)>&& completionHandler)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return completionHandler({ }, { });

    completionHandler(EditingRange::fromRange(*frame, frame->selection().selection().toNormalizedRange()),
        EditingRange::fromRange(*frame, protect(frame->editor())->compositionRange()));
}

void WebPage::characterIndexForPointAsync(const WebCore::IntPoint& point, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    RefPtr localMainFrame = this->localMainFrame();
    if (!localMainFrame)
        return;
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent,  HitTestRequest::Type::AllowChildFrameContent };
    auto result = localMainFrame->eventHandler().hitTestResultAtPoint(point, hitType);
    RefPtr frame = result.innerNonSharedNode() ? result.innerNodeFrame() : corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return completionHandler({ });
    auto range = frame->rangeForPoint(result.roundedPointInInnerNodeFrame());
    auto editingRange = EditingRange::fromRange(*frame, range);
    completionHandler(editingRange.location);
}

void WebPage::firstRectForCharacterRangeAsync(const EditingRange& editingRange, CompletionHandler<void(const WebCore::IntRect&, const EditingRange&)>&& completionHandler)
{
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return completionHandler({ }, { });

    auto range = EditingRange::toRange(*frame, editingRange);
    if (!range)
        return completionHandler({ }, editingRange);

    auto rect = RefPtr(frame->view())->contentsToWindow(protect(frame->editor())->firstRectForRange(*range));
    auto startPosition = makeContainerOffsetPosition(range->start);

    auto endPosition = endOfLine(startPosition);
    if (endPosition.isNull())
        endPosition = startPosition;
    else if (endPosition.affinity() == Affinity::Downstream && inSameLine(startPosition, endPosition)) {
        auto nextLineStartPosition = positionOfNextBoundaryOfGranularity(endPosition, TextGranularity::LineGranularity, SelectionDirection::Forward);
        if (nextLineStartPosition.isNotNull() && endPosition < nextLineStartPosition)
            endPosition = nextLineStartPosition;
    }

    auto endBoundary = makeBoundaryPoint(endPosition);
    if (!endBoundary)
        return completionHandler({ }, editingRange);

    auto rangeForFirstLine = EditingRange::fromRange(*frame, makeSimpleRange(range->start, WTF::move(endBoundary)));

    rangeForFirstLine.location = std::min(std::max(rangeForFirstLine.location, editingRange.location), editingRange.location + editingRange.length);
    rangeForFirstLine.length = std::min(rangeForFirstLine.location + rangeForFirstLine.length, editingRange.location + editingRange.length) - rangeForFirstLine.location;

    completionHandler(rect, rangeForFirstLine);
}

void WebPage::setCompositionAsync(const String& text, const Vector<CompositionUnderline>& underlines, const Vector<CompositionHighlight>& highlights, const HashMap<String, Vector<CharacterRange>>& annotations, const EditingRange& selection, const EditingRange& replacementEditingRange)
{
    platformWillPerformEditingCommand();

    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    if (frame->selection().selection().isContentEditable()) {
        if (replacementEditingRange.location != notFound) {
            if (auto replacementRange = EditingRange::toRange(*frame, replacementEditingRange))
                protect(frame->selection())->setSelection(VisibleSelection(*replacementRange));
        }
        protect(frame->editor())->setComposition(text, underlines, highlights, annotations, selection.location, selection.location + selection.length);
    }
}

void WebPage::setWritingSuggestion(const String& fullTextWithPrediction, const EditingRange& selection)
{
    platformWillPerformEditingCommand();

    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    protect(frame->editor())->setWritingSuggestion(fullTextWithPrediction, { selection.location, selection.length });
}

void WebPage::confirmCompositionAsync()
{
    platformWillPerformEditingCommand();

    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    protect(frame->editor())->confirmComposition();
}

void WebPage::getInformationFromImageData(const Vector<uint8_t>& data, CompletionHandler<void(Expected<std::pair<String, Vector<IntSize>>, WebCore::ImageDecodingError>&&)>&& completionHandler)
{
    if (m_isClosed)
        return completionHandler(makeUnexpected(ImageDecodingError::Internal));

    if (SVGImage::isDataDecodable(m_page->settings(), data.span()))
        return completionHandler(std::make_pair(String { "public.svg-image"_s }, Vector<IntSize> { }));

    completionHandler(utiAndAvailableSizesFromImageData(data.span()));
}

void WebPage::insertTextPlaceholder(const IntSize& size, CompletionHandler<void(const std::optional<WebCore::ElementContext>&)>&& completionHandler)
{
    // Inserting the placeholder may run JavaScript, which can do anything, including frame destruction.
    RefPtr frame = corePage()->focusController().focusedOrMainFrame();
    if (!frame)
        return completionHandler({ });

    auto placeholder = protect(frame->editor())->insertTextPlaceholder(size);
    completionHandler(placeholder ? contextForElement(*placeholder) : std::nullopt);
}

void WebPage::removeTextPlaceholder(const ElementContext& placeholder, CompletionHandler<void()>&& completionHandler)
{
    if (auto element = elementForContext(placeholder)) {
        if (RefPtr frame = element->document().frame())
            protect(frame->editor())->removeTextPlaceholder(downcast<TextPlaceholderElement>(*element));
    }
    completionHandler();
}

void WebPage::setObscuredContentInsetsFenced(const FloatBoxExtent& obscuredContentInsets, const WTF::MachSendRight& machSendRight)
{
    protect(drawingArea())->addFence(machSendRight);
    setObscuredContentInsets(obscuredContentInsets);
}

void WebPage::willCommitLayerTree(RemoteLayerTreeTransaction& layerTransaction, WebCore::FrameIdentifier rootFrameID)
{
    RefPtr rootFrame = WebProcess::singleton().webFrame(rootFrameID);
    if (!rootFrame)
        return;

    RefPtr localRootFrame = rootFrame->coreLocalFrame();
    if (!localRootFrame)
        return;

    RefPtr frameView = localRootFrame->view();
    if (!frameView)
        return;

    Ref page = *corePage();

#if ENABLE(THREADED_ANIMATIONS)
    if (auto* acceleratedTimelinesUpdater = page->acceleratedTimelinesUpdater())
        layerTransaction.setTimelinesUpdate(acceleratedTimelinesUpdater->takeTimelinesUpdate());
#endif

    layerTransaction.setContentsSize(frameView->contentsSize());
    layerTransaction.setScrollGeometryContentSize(frameView->scrollGeometryContentSize());
    layerTransaction.setScrollOrigin(frameView->scrollOrigin());
    layerTransaction.setScrollPosition(frameView->scrollPosition());

    m_pendingThemeColorChange = false;
    m_pendingPageExtendedBackgroundColorChange = false;
    m_pendingSampledPageTopColorChange = false;
}

void WebPage::willCommitMainFrameData(MainFrameData& data, const TransactionID& transactionID)
{
    RefPtr mainFrameView = localMainFrameView();
    if (!mainFrameView)
        return;

    Ref page = *corePage();
    data.pageScaleFactor = page->pageScaleFactor();
    data.themeColor = page->themeColor();
    data.pageExtendedBackgroundColor = page->pageExtendedBackgroundColor();
    data.sampledPageTopColor = page->sampledPageTopColor();

    if (std::exchange(m_needsFixedContainerEdgesUpdate, false)) {
        page->updateFixedContainerEdges(sidesRequiringFixedContainerEdges());
        data.fixedContainerEdges = page->fixedContainerEdges();
    }

    data.baseLayoutViewportSize = mainFrameView->baseLayoutViewportSize();
    data.minStableLayoutViewportOrigin = mainFrameView->minStableLayoutViewportOrigin();
    data.maxStableLayoutViewportOrigin = mainFrameView->maxStableLayoutViewportOrigin();

#if PLATFORM(IOS_FAMILY)
    data.scaleWasSetByUIProcess = scaleWasSetByUIProcess();
    data.minimumScaleFactor = m_viewportConfiguration.minimumScale();
    data.maximumScaleFactor = m_viewportConfiguration.maximumScale();
    data.initialScaleFactor = m_viewportConfiguration.initialScale();
    data.viewportMetaTagInteractiveWidget = m_viewportConfiguration.viewportArguments().interactiveWidget;
    data.viewportMetaTagWidth = m_viewportConfiguration.viewportArguments().width;
    data.viewportMetaTagWidthWasExplicit = m_viewportConfiguration.viewportArguments().widthWasExplicit;
    data.viewportMetaTagCameFromImageDocument = m_viewportConfiguration.viewportArguments().type == ViewportArguments::Type::ImageDocument;
    data.avoidsUnsafeArea = m_viewportConfiguration.avoidsUnsafeArea();
    data.isInStableState = m_isInStableState;
    data.allowsUserScaling = allowsUserScaling();
    if (m_pendingDynamicViewportSizeUpdateID) {
        data.dynamicViewportSizeUpdateID = *m_pendingDynamicViewportSizeUpdateID;
        m_pendingDynamicViewportSizeUpdateID = std::nullopt;
    }
    if (m_lastTransactionPageScaleFactor != data.pageScaleFactor) {
        m_lastTransactionPageScaleFactor = data.pageScaleFactor;
        m_internals->lastTransactionIDWithScaleChange = transactionID;
    }
#endif

    if (hasPendingEditorStateUpdate() || m_needsEditorStateVisualDataUpdate) {
        data.editorState = editorState();
        m_pendingEditorStateUpdateStatus = PendingEditorStateUpdateStatus::NotScheduled;
        m_needsEditorStateVisualDataUpdate = false;
    }

#if ENABLE(SCROLL_STRETCH_NOTIFICATIONS)
    auto scrollOrigin = mainFrameView->scrollOrigin();
    auto scrollPosition = mainFrameView->scrollPosition();
    data.topScrollStretch = static_cast<uint64_t>(std::max(0, -scrollOrigin.y() - scrollPosition.y()));
#endif
}

void WebPage::didFlushLayerTreeAtTime(MonotonicTime timestamp, bool flushSucceeded)
{
#if PLATFORM(IOS_FAMILY)
    if (m_oldestNonStableUpdateVisibleContentRectsTimestamp != MonotonicTime()) {
        Seconds elapsed = timestamp - m_oldestNonStableUpdateVisibleContentRectsTimestamp;
        m_oldestNonStableUpdateVisibleContentRectsTimestamp = MonotonicTime();

        m_estimatedLatency = m_estimatedLatency * 0.80 + elapsed * 0.20;
    }
#else
    UNUSED_PARAM(timestamp);
#endif
#if ENABLE(GPU_PROCESS)
    if (!flushSucceeded) {
        if (RefPtr proxy = m_remoteRenderingBackendProxy)
            proxy->didBecomeUnresponsive();
    }
#endif
}

bool WebPage::isSpeaking() const
{
    auto sendResult = const_cast<WebPage*>(this)->sendSync(Messages::WebPageProxy::GetIsSpeaking());
    auto [result] = sendResult.takeReplyOr(false);
    return result;
}

bool WebPage::shouldAllowSingleClickToChangeSelection(WebCore::Node& targetNode, const WebCore::VisibleSelection& newSelection, WebCore::MouseEventInputSource inputSource)
{
#if !PLATFORM(MAC) || HAVE(APPKIT_GESTURES_SUPPORT)
#if HAVE(APPKIT_GESTURES_SUPPORT)
    if (!m_page->settings().useAppKitGestures())
        return true;
#endif

    if (RefPtr editableRoot = newSelection.rootEditableElement(); editableRoot && editableRoot == targetNode.rootEditableElement()) {
        // FIXME: This logic should be made consistent for both macOS and iOS.
#if PLATFORM(MAC)
        return inputSource != WebCore::MouseEventInputSource::Automation;
#else
        // Text interaction gestures will handle selection in the case where we are already editing the node. In the case where we're
        // just starting to focus an editable element by tapping on it, only change the selection if we weren't already showing an
        // input view prior to handling the tap.
        return !(m_completingSyntheticClick ? m_wasShowingInputViewForFocusedElementDuringLastPotentialTap : m_isShowingInputViewForFocusedElement);
#endif
    }
#endif // !PLATFORM(MAC) || HAVE(APPKIT_GESTURES_SUPPORT)

    return true;
}

void WebPage::selectWithGesture(const IntPoint& point, GestureType gestureType, GestureRecognizerState gestureState, bool isInteractingWithFocusedElement, CompletionHandler<void(const WebCore::IntPoint&, GestureType, GestureRecognizerState, OptionSet<SelectionFlags>)>&& completionHandler)
{
    if (gestureState == GestureRecognizerState::Began)
        updateFocusBeforeSelectingTextAtLocation(point);

    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame) {
        completionHandler({ }, gestureType, gestureState, { });
        return;
    }

    VisiblePosition position = visiblePositionInFocusedNodeForPoint(*frame, point, isInteractingWithFocusedElement);

    if (position.isNull()) {
        completionHandler(point, gestureType, gestureState, { });
        return;
    }
    std::optional<SimpleRange> range;
    OptionSet<SelectionFlags> flags;
    GestureRecognizerState wkGestureState = gestureState;
    switch (gestureType) {
    case GestureType::PhraseBoundary: {
        if (!frame->editor().hasComposition())
            break;
        auto markedRange = WTF::protect(frame->editor())->compositionRange();
        auto startPosition = VisiblePosition { makeDeprecatedLegacyPosition(markedRange->start) };
        position = std::clamp(position, startPosition, VisiblePosition { makeDeprecatedLegacyPosition(markedRange->end) });
        if (wkGestureState != GestureRecognizerState::Began)
            flags = distanceBetweenPositions(startPosition, frame->selection().selection().start()) != distanceBetweenPositions(startPosition, position) ? SelectionFlags::PhraseBoundaryChanged : OptionSet<SelectionFlags> { };
        else
            flags = SelectionFlags::PhraseBoundaryChanged;
        range = makeSimpleRange(position);
        break;
    }

    case GestureType::OneFingerTap: {
        auto [adjustedPosition, withinWordBoundary] = wordBoundaryForPositionWithoutCrossingLine(position);
        if (withinWordBoundary == WithinWordBoundary::Yes)
            flags = SelectionFlags::WordIsNearTap;
        range = makeSimpleRange(adjustedPosition);
        break;
    }

    case GestureType::Loupe:
        if (position.rootEditableElement())
            range = makeSimpleRange(position);
        else {
#if !PLATFORM(MACCATALYST)
            range = wordRangeFromPosition(position);
#else
            switch (wkGestureState) {
            case GestureRecognizerState::Began:
                m_startingGestureRange = makeSimpleRange(position);
                break;
            case GestureRecognizerState::Changed:
                if (m_startingGestureRange) {
                    auto& start = m_startingGestureRange->start;
                    if (makeDeprecatedLegacyPosition(start) < position)
                        range = makeSimpleRange(start, position);
                    else
                        range = makeSimpleRange(position, start);
                }
                break;
            case GestureRecognizerState::Ended:
            case GestureRecognizerState::Cancelled:
                m_startingGestureRange = std::nullopt;
                break;
            case GestureRecognizerState::Failed:
            case GestureRecognizerState::Possible:
                ASSERT_NOT_REACHED();
                break;
            }
#endif
        }
        break;

    case GestureType::TapAndAHalf:
        switch (wkGestureState) {
        case GestureRecognizerState::Began:
            range = wordRangeFromPosition(position);
            if (range)
                m_currentWordRange = { { *range } };
            else
                m_currentWordRange = std::nullopt;
            break;
        case GestureRecognizerState::Changed:
            if (!m_currentWordRange)
                break;
            range = m_currentWordRange;
            if (position < makeDeprecatedLegacyPosition(range->start))
                range->start = *makeBoundaryPoint(position);
            if (position > makeDeprecatedLegacyPosition(range->end))
                range->end = *makeBoundaryPoint(position);
            break;
        case GestureRecognizerState::Ended:
        case GestureRecognizerState::Cancelled:
            m_currentWordRange = std::nullopt;
            break;
        case GestureRecognizerState::Failed:
        case GestureRecognizerState::Possible:
            ASSERT_NOT_REACHED();
            break;
        }
        break;

    case GestureType::OneFingerDoubleTap:
        if (atBoundaryOfGranularity(position, TextGranularity::LineGranularity, SelectionDirection::Forward)) {
            // Double-tap at end of line only places insertion point there.
            // This helps to get the callout for pasting at ends of lines,
            // paragraphs, and documents.
            range = makeSimpleRange(position);
        } else
            range = wordRangeFromPosition(position);
        break;

    case GestureType::TwoFingerSingleTap:
        // Single tap with two fingers selects the entire paragraph.
        range = enclosingTextUnitOfGranularity(position, TextGranularity::ParagraphGranularity, SelectionDirection::Forward);
        break;

    case GestureType::OneFingerTripleTap:
        if (atBoundaryOfGranularity(position, TextGranularity::LineGranularity, SelectionDirection::Forward)) {
            // Triple-tap at end of line only places insertion point there.
            // This helps to get the callout for pasting at ends of lines, paragraphs, and documents.
            range = makeSimpleRange(position);
        } else
            range = enclosingTextUnitOfGranularity(position, TextGranularity::ParagraphGranularity, SelectionDirection::Forward);
        break;

    default:
        break;
    }
    if (range)
        WTF::protect(frame->selection())->setSelectedRange(range, position.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered::Yes);

    completionHandler(point, gestureType, gestureState, flags);
}

void WebPage::updateFocusBeforeSelectingTextAtLocation(const IntPoint& point)
{
    static constexpr OptionSet hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowVisibleChildFrameContentOnly };
    RefPtr localMainFrame = m_page->localMainFrame();
    if (!localMainFrame)
        return;

    auto result = localMainFrame->eventHandler().hitTestResultAtPoint(point, hitType);
    RefPtr hitNode = result.innerNode();
    if (!hitNode || !hitNode->renderer())
        return;

    RefPtr frame = result.innerNodeFrame();
    m_page->focusController().setFocusedFrame(frame.get());

    if (!result.isOverWidget())
        return;

#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = pluginViewForFrame(frame.get()))
        pluginView->focusPluginElement();
#endif
}

IntRect WebPage::rootViewInteractionBounds(const Node& node)
{
    RefPtr frame = node.document().frame();
    if (!frame)
        return { };

    RefPtr view = frame->view();
    if (!view)
        return { };

    return view->contentsToRootView(absoluteInteractionBounds(node));
}

IntRect WebPage::absoluteInteractionBounds(const Node& node)
{
    RefPtr frame = node.document().frame();
    if (!frame)
        return { };

    RefPtr view = frame->view();
    if (!view)
        return { };

    CheckedPtr renderer = node.renderer();
    if (!renderer)
        return { };

    if (CheckedPtr box = dynamicDowncast<RenderBox>(*renderer)) {
        FloatRect rect;
        // FIXME: want borders or not?
        if (box->style().isOverflowVisible())
            rect = box->layoutOverflowRect();
        else
            rect = box->clientBoxRect();
        return box->localToAbsoluteQuad(rect).enclosingBoundingBox();
    }

    CheckedRef style = renderer->style();
    FloatRect boundingBox = renderer->absoluteBoundingBoxRect(true /* use transforms*/);
    // This is wrong. It's subtracting borders after converting to absolute coords on something that probably doesn't represent a rectangular element.
    boundingBox.move(WebCore::Style::evaluate<float>(style->usedBorderLeftWidth(), WebCore::Style::ZoomNeeded { }), WebCore::Style::evaluate<float>(style->usedBorderTopWidth(), WebCore::Style::ZoomNeeded { }));
    boundingBox.setWidth(boundingBox.width() - WebCore::Style::evaluate<float>(style->usedBorderLeftWidth(), WebCore::Style::ZoomNeeded { }) - WebCore::Style::evaluate<float>(style->usedBorderRightWidth(), WebCore::Style::ZoomNeeded { }));
    boundingBox.setHeight(boundingBox.height() - WebCore::Style::evaluate<float>(style->usedBorderBottomWidth(), WebCore::Style::ZoomNeeded { }) - WebCore::Style::evaluate<float>(style->usedBorderTopWidth(), WebCore::Style::ZoomNeeded { }));
    return enclosingIntRect(boundingBox);
}

static IntRect elementBoundsInFrame(const LocalFrame& frame, const Element& focusedElement)
{
    WTF::protect(frame.document())->updateLayout(LayoutOptions::IgnorePendingStylesheets);

    if (focusedElement.hasTagName(HTMLNames::textareaTag) || focusedElement.hasTagName(HTMLNames::inputTag) || focusedElement.hasTagName(HTMLNames::selectTag))
        return WebPage::absoluteInteractionBounds(focusedElement);

    if (RefPtr rootEditableElement = focusedElement.rootEditableElement())
        return WebPage::absoluteInteractionBounds(*rootEditableElement);

    return { };
}

IntPoint WebPage::constrainPoint(const IntPoint& point, const LocalFrame& frame, const Element& focusedElement)
{
    ASSERT(&focusedElement.document() == frame.document());
    const int DEFAULT_CONSTRAIN_INSET = 2;
    IntRect innerFrame = elementBoundsInFrame(frame, focusedElement);
    IntPoint constrainedPoint = point;

    int minX = innerFrame.x() + DEFAULT_CONSTRAIN_INSET;
    int maxX = innerFrame.maxX() - DEFAULT_CONSTRAIN_INSET;
    int minY = innerFrame.y() + DEFAULT_CONSTRAIN_INSET;
    int maxY = innerFrame.maxY() - DEFAULT_CONSTRAIN_INSET;

    if (point.x() < minX)
        constrainedPoint.setX(minX);
    else if (point.x() > maxX)
        constrainedPoint.setX(maxX);

    if (point.y() < minY)
        constrainedPoint.setY(minY);
    else if (point.y() >= maxY)
        constrainedPoint.setY(maxY);

    return constrainedPoint;
}

VisiblePosition WebPage::visiblePositionInFocusedNodeForPoint(const LocalFrame& frame, const IntPoint& point, bool isInteractingWithFocusedElement)
{
    IntPoint adjustedPoint(WTF::protect(frame.view())->rootViewToContents(point));
    IntPoint constrainedPoint = m_focusedElement && isInteractingWithFocusedElement ? WebPage::constrainPoint(adjustedPoint, frame, WTF::protect(*m_focusedElement)) : adjustedPoint;
    return frame.visiblePositionForPoint(constrainedPoint);
}

InteractionInformationAtPosition WebPage::positionInformation(const InteractionInformationRequest& request)
{
    return WebKit::positionInformationForWebPage(*this, request);
}

void WebPage::requestPositionInformation(const InteractionInformationRequest& request)
{
    sendEditorStateUpdate();
    send(Messages::WebPageProxy::DidReceivePositionInformation(positionInformation(request)));
}

bool WebPage::isAssistableElement(Element& element)
{
    if (is<HTMLSelectElement>(element))
        return true;
    if (is<HTMLTextAreaElement>(element))
        return true;
    if (RefPtr inputElement = dynamicDowncast<HTMLInputElement>(element)) {
        // FIXME: This laundry list of types is not a good way to factor this. Need a suitable function on HTMLInputElement itself.
#if ENABLE(INPUT_TYPE_WEEK_PICKER)
        if (inputElement->isWeekField())
            return true;
#endif
        return inputElement->isTextField() || inputElement->isDateField() || inputElement->isDateTimeLocalField() || inputElement->isMonthField() || inputElement->isTimeField() || inputElement->isColorControl();
    }
    if (is<HTMLIFrameElement>(element))
        return false;
    return element.isContentEditable();
}

RefPtr<HTMLAnchorElement> WebPage::containingLinkAnchorElement(Element& element)
{
    // FIXME: There is code in the drag controller that supports any link, even if it's not an HTMLAnchorElement. Why is this different?
    for (Ref currentElement : lineageOfType<HTMLAnchorElement>(element)) {
        if (currentElement->isLink())
            return currentElement.ptr();
    }
    return nullptr;
}

void WebPage::selectPositionAtPoint(WebCore::IntPoint point, bool isInteractingWithFocusedElement, CompletionHandler<void()>&& completionHandler)
{
    SetForScope userIsInteractingChange { m_userIsInteracting, true };

    updateFocusBeforeSelectingTextAtLocation(point);

    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame)
        return completionHandler();

    VisiblePosition position = visiblePositionInFocusedNodeForPoint(*frame, point, isInteractingWithFocusedElement);

    if (position.isNotNull())
        WTF::protect(frame->selection())->setSelectedRange(makeSimpleRange(position), position.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered::Yes);
    completionHandler();
}

std::optional<SimpleRange> WebPage::rangeForGranularityAtPoint(LocalFrame& frame, const WebCore::IntPoint& point, WebCore::TextGranularity granularity, bool isInteractingWithFocusedElement)
{
    auto position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithFocusedElement);
    switch (granularity) {
    case TextGranularity::CharacterGranularity:
        return makeSimpleRange(position);
    case TextGranularity::WordGranularity:
        return wordRangeFromPosition(position);
    case TextGranularity::SentenceGranularity:
    case TextGranularity::ParagraphGranularity:
        return enclosingTextUnitOfGranularity(position, granularity, SelectionDirection::Forward);
    case TextGranularity::DocumentGranularity:
        // FIXME: Makes no sense that this mutates the current selection and returns null.
        protect(frame.selection())->selectAll();
        return std::nullopt;
    case TextGranularity::LineGranularity:
    case TextGranularity::LineBoundary:
    case TextGranularity::SentenceBoundary:
    case TextGranularity::ParagraphBoundary:
    case TextGranularity::DocumentBoundary:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

void WebPage::setSelectionRange(WebCore::IntPoint point, WebCore::TextGranularity granularity, bool isInteractingWithFocusedElement)
{
    updateFocusBeforeSelectingTextAtLocation(point);

    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame)
        return;

#if ENABLE(PDF_PLUGIN) && ENABLE(TWO_PHASE_CLICKS)
    if (RefPtr pluginView = focusedPluginViewForFrame(*frame)) {
        pluginView->setSelectionRange(point, granularity);
        return;
    }
#endif // ENABLE(PDF_PLUGIN) && ENABLE(TWO_PHASE_CLICKS)

    auto range = rangeForGranularityAtPoint(*frame, point, granularity, isInteractingWithFocusedElement);
    if (range)
        protect(frame->selection())->setSelectedRange(*range, Affinity::Upstream, WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered::Yes);

    m_initialSelection = range;
}

void WebPage::updateSelectionWithExtentPointAndBoundary(WebCore::IntPoint point, WebCore::TextGranularity granularity, bool isInteractingWithFocusedElement, TextInteractionSource source, CompletionHandler<void(bool)>&& callback)
{
    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame)
        return callback(false);

#if ENABLE(PDF_PLUGIN) && ENABLE(TWO_PHASE_CLICKS)
    if (RefPtr pluginView = focusedPluginViewForFrame(*frame)) {
        auto movedEndpoint = pluginView->extendInitialSelection(point, granularity);
        return callback(movedEndpoint == SelectionEndpoint::End);
    }
#endif // ENABLE(PDF_PLUGIN) && ENABLE(TWO_PHASE_CLICKS)

    auto position = visiblePositionInFocusedNodeForPoint(*frame, point, isInteractingWithFocusedElement);
    auto newRange = rangeForGranularityAtPoint(*frame, point, granularity, isInteractingWithFocusedElement);

    if (position.isNull() || !m_initialSelection || !newRange)
        return callback(false);

#if PLATFORM(IOS_FAMILY)
    addTextInteractionSources(source);
#endif

    auto initialSelectionStartPosition = makeDeprecatedLegacyPosition(m_initialSelection->start);
    auto initialSelectionEndPosition = makeDeprecatedLegacyPosition(m_initialSelection->end);

    VisiblePosition selectionStart = initialSelectionStartPosition;
    VisiblePosition selectionEnd = initialSelectionEndPosition;
    if (position > initialSelectionEndPosition)
        selectionEnd = makeDeprecatedLegacyPosition(newRange->end);
    else if (position < initialSelectionStartPosition)
        selectionStart = makeDeprecatedLegacyPosition(newRange->start);

    if (auto range = makeSimpleRange(selectionStart, selectionEnd))
        protect(frame->selection())->setSelectedRange(range, Affinity::Upstream, WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered::Yes);

#if PLATFORM(IOS_FAMILY)
    if (!m_hasAnyActiveTouchPoints) {
        // Ensure that `Touch` doesn't linger around in `m_activeTextInteractionSources` after
        // the user has ended all active touches.
        removeTextInteractionSources(TextInteractionSource::Touch);
    }
#endif // PLATFORM(IOS_FAMILY)

    callback(selectionStart == initialSelectionStartPosition);
}

void WebPage::updateSelectionWithExtentPoint(WebCore::IntPoint point, bool isInteractingWithFocusedElement, RespectSelectionAnchor respectSelectionAnchor, CompletionHandler<void(bool)>&& callback)
{
    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame)
        return callback(false);

    auto position = visiblePositionInFocusedNodeForPoint(*frame, point, isInteractingWithFocusedElement);

    if (position.isNull())
        return callback(false);

    VisiblePosition selectionStart;
    VisiblePosition selectionEnd;

    if (respectSelectionAnchor == RespectSelectionAnchor::Yes) {
#if PLATFORM(IOS_FAMILY)
        if (m_selectionAnchor == SelectionAnchor::Start) {
            selectionStart = frame->selection().selection().visibleStart();
            selectionEnd = position;
            if (position <= selectionStart) {
                selectionStart = selectionStart.previous();
                selectionEnd = frame->selection().selection().visibleEnd();
                m_selectionAnchor = SelectionAnchor::End;
            }
        } else {
            selectionStart = position;
            selectionEnd = frame->selection().selection().visibleEnd();
            if (position >= selectionEnd) {
                selectionStart = frame->selection().selection().visibleStart();
                selectionEnd = selectionEnd.next();
                m_selectionAnchor = SelectionAnchor::Start;
            }
        }
#endif // PLATFORM(IOS_FAMILY)
    } else {
        auto currentStart = frame->selection().selection().visibleStart();
        auto currentEnd = frame->selection().selection().visibleEnd();
        if (position <= currentStart) {
            selectionStart = position;
            selectionEnd = currentEnd;
        } else if (position >= currentEnd) {
            selectionStart = currentStart;
            selectionEnd = position;
        }
    }

    if (auto range = makeSimpleRange(selectionStart, selectionEnd))
        protect(frame->selection())->setSelectedRange(range, Affinity::Upstream, WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered::Yes);

#if PLATFORM(IOS_FAMILY)
    callback(m_selectionAnchor == SelectionAnchor::Start);
#else
    callback(true);
#endif
}

void WebPage::selectTextWithGranularityAtPoint(WebCore::IntPoint point, WebCore::TextGranularity granularity, bool isInteractingWithFocusedElement, CompletionHandler<void()>&& completionHandler)
{
#if PLATFORM(IOS_FAMILY)
    if (!m_potentialTapNode) {
        setSelectionRange(point, granularity, isInteractingWithFocusedElement);
        completionHandler();
        return;
    }

    if (auto selectionChangedHandler = std::exchange(m_selectionChangedHandler, { }))
        selectionChangedHandler();

    m_selectionChangedHandler = [point, granularity, isInteractingWithFocusedElement, completionHandler = WTF::move(completionHandler), weakThis = WeakPtr { *this }]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            completionHandler();
            return;
        }
        protectedThis->setSelectionRange(point, granularity, isInteractingWithFocusedElement);
        completionHandler();
    };
#else
    setSelectionRange(point, granularity, isInteractingWithFocusedElement);
    completionHandler();
#endif
}

#if ENABLE(TWO_PHASE_CLICKS)

static void dispatchSyntheticMouseMove(LocalFrame& localFrame, const WebCore::FloatPoint& location, OptionSet<WebEventModifier> modifiers, WebCore::PointerID pointerId, WebCore::MouseEventInputSource inputSource)
{
    auto roundedAdjustedPoint = roundedIntPoint(location);
    auto mouseEvent = PlatformMouseEvent(
        roundedAdjustedPoint, roundedAdjustedPoint,
        MouseButton::None, PlatformEvent::Type::MouseMoved, 0,
        platform(modifiers), MonotonicTime::now(),
        WebCore::ForceAtClick, WebCore::SyntheticClickType::OneFingerTap,
        inputSource,
        pointerId
    );
    localFrame.eventHandler().dispatchSyntheticMouseMove(mouseEvent);
}

void WebPage::handleSyntheticClick(std::optional<WebCore::FrameIdentifier> frameID, Node& nodeRespondingToClick, const WebCore::FloatPoint& location, OptionSet<WebEventModifier> modifiers, WebCore::PointerID pointerId)
{
    Ref respondingDocument = nodeRespondingToClick.document();
    m_hasHandledSyntheticClick = true;

    if (!respondingDocument->settings().contentChangeObserverEnabled() || respondingDocument->quirks().shouldIgnoreContentObservationForClick(nodeRespondingToClick)) {
        completeSyntheticClick(frameID, nodeRespondingToClick, location, modifiers, WebCore::SyntheticClickType::OneFingerTap, pointerId);
        return;
    }

    Ref contentChangeObserver = respondingDocument->contentChangeObserver();
    contentChangeObserver->setClickTarget(nodeRespondingToClick);
    auto targetNodeWentFromHiddenToVisible = contentChangeObserver->hiddenTouchTarget() == &nodeRespondingToClick && ContentChangeObserver::isConsideredVisible(nodeRespondingToClick);
    {
        LOG_WITH_STREAM(ContentObservation, stream << "handleSyntheticClick: node(" << &nodeRespondingToClick << ") " << location);
        ContentChangeObserver::MouseMovedScope observingScope(respondingDocument);
        RefPtr localRootFrame = this->localRootFrame(frameID);
        if (!localRootFrame)
            return;
        dispatchSyntheticMouseMove(*localRootFrame, location, modifiers, pointerId, m_potentialTapInputSource);
        protect(localRootFrame->document())->updateStyleIfNeeded();
        if (m_isClosed)
            return;
    }

    if (targetNodeWentFromHiddenToVisible) {
        LOG(ContentObservation, "handleSyntheticClick: target node was hidden and now is visible -> hover.");
        didHandleTapAsHover();
        return;
    }

    auto nodeTriggersFastPath = [&](auto& targetNode) {
        RefPtr element = dynamicDowncast<Element>(targetNode);
        if (!element)
            return false;
        if (is<HTMLFormControlElement>(*element))
            return true;
        if (element->document().quirks().shouldIgnoreAriaForFastPathContentObservationCheck())
            return false;
        auto ariaRole = AccessibilityObject::ariaRoleToWebCoreRole(element->getAttribute(HTMLNames::roleAttr));
        return AccessibilityObject::isARIAControl(ariaRole);
    };
    auto targetNodeTriggersFastPath = nodeTriggersFastPath(nodeRespondingToClick);

    auto observedContentChange = contentChangeObserver->observedContentChange();
    auto continueContentObservation = !(observedContentChange == WebCore::ContentChange::Visibility || targetNodeTriggersFastPath);
    if (continueContentObservation) {
        // Wait for callback to didFinishContentChangeObserving() to decide whether to send the click event.
        const Seconds observationDuration = 32_ms;
        contentChangeObserver->startContentObservationForDuration(observationDuration);
        LOG(ContentObservation, "handleSyntheticClick: Can't decide it yet -> wait.");
        m_pendingSyntheticClickNode = nodeRespondingToClick;
        m_pendingSyntheticClickLocation = location;
        m_pendingSyntheticClickModifiers = modifiers;
        m_pendingSyntheticClickPointerId = pointerId;
        return;
    }
    contentChangeObserver->stopContentObservation();
    callOnMainRunLoop([protectedThis = Ref { *this }, targetNode = Ref<Node>(nodeRespondingToClick), location, modifiers, observedContentChange, pointerId, frameID] {
        if (protectedThis->m_isClosed || !protectedThis->corePage())
            return;

        auto shouldStayAtHoverState = observedContentChange == WebCore::ContentChange::Visibility;
        if (shouldStayAtHoverState) {
            // The move event caused new contents to appear. Don't send synthetic click event, but just ensure that the mouse is on the most recent content.
            if (RefPtr localRootFrame = protectedThis->localRootFrame(frameID))
                dispatchSyntheticMouseMove(*localRootFrame, location, modifiers, pointerId, protectedThis->m_potentialTapInputSource);
            LOG(ContentObservation, "handleSyntheticClick: Observed meaningful visible change -> hover.");
            protectedThis->didHandleTapAsHover();
            return;
        }
        LOG(ContentObservation, "handleSyntheticClick: calling completeSyntheticClick -> click.");
        protectedThis->completeSyntheticClick(frameID, targetNode, location, modifiers, WebCore::SyntheticClickType::OneFingerTap, pointerId);
    });
}

Awaitable<std::optional<WebCore::RemoteUserInputEventData>> WebPage::potentialTapAtPosition(std::optional<WebCore::FrameIdentifier> frameID, WebKit::TapIdentifier requestID, WebCore::FloatPoint position, bool shouldRequestMagnificationInformation, WebKit::WebMouseEventInputSource inputSource)
{
    m_potentialTapInputSource = platform(inputSource);

    RefPtr localMainFrame = protect(*m_page)->localMainFrame();

    if (RefPtr localRootFrame = this->localRootFrame(frameID))
        m_potentialTapNode = localRootFrame->nodeRespondingToClickEvents(position, m_potentialTapLocation, m_potentialTapSecurityOrigin.get());

    RefPtr frameOwner = dynamicDowncast<HTMLFrameOwnerElement>(m_potentialTapNode.get());
    if (RefPtr remoteFrame = frameOwner ? dynamicDowncast<RemoteFrame>(frameOwner->contentFrame()) : nullptr) {
        RefPtr localFrame = frameOwner->document().frame();
        if (RefPtr frameView = localFrame ? localFrame->view() : nullptr) {
            if (RefPtr remoteFrameView = remoteFrame->view()) {
                RemoteFrameGeometryTransformer transformer(remoteFrameView.releaseNonNull(), frameView.releaseNonNull(), remoteFrame->frameID());
                co_return WebCore::RemoteUserInputEventData {
                    remoteFrame->frameID(),
                    transformer.transformToRemoteFrameCoordinates(position)
                };
            }
        }
    }

#if PLATFORM(IOS_FAMILY)
    auto lastTouchLocation = std::exchange(m_lastTouchLocationBeforeTap, { });
    bool ignorePotentialTap = [&] {
        if (!m_potentialTapNode)
            return false;

        if (!localMainFrame)
            return false;

        if (!lastTouchLocation)
            return false;

        static constexpr auto maxAllowedMovementSquared = 200 * 200;
        if ((position - *lastTouchLocation).diagonalLengthSquared() <= maxAllowedMovementSquared)
            return false;

        FloatPoint adjustedLocation;
        RefPtr lastTouchedNode = localMainFrame->nodeRespondingToClickEvents(*lastTouchLocation, adjustedLocation, m_potentialTapSecurityOrigin.get());
        return lastTouchedNode != m_potentialTapNode;
    }();

    if (ignorePotentialTap) {
        RELEASE_LOG(ViewGestures, "Ignoring potential tap (distance from last touch: %.0f)", (position - *lastTouchLocation).diagonalLength());
        m_potentialTapNode = nullptr;
        co_return std::nullopt;
    }

    m_wasShowingInputViewForFocusedElementDuringLastPotentialTap = m_isShowingInputViewForFocusedElement;

    RefPtr viewGestureGeometryCollector = m_viewGestureGeometryCollector;

    if (shouldRequestMagnificationInformation && m_potentialTapNode && viewGestureGeometryCollector) {
        FloatPoint origin = position;
        FloatRect absoluteBoundingRect;
        bool fitEntireRect;
        double viewportMinimumScale;
        double viewportMaximumScale;

        viewGestureGeometryCollector->computeZoomInformationForNode(*protect(m_potentialTapNode), origin, absoluteBoundingRect, fitEntireRect, viewportMinimumScale, viewportMaximumScale);

        bool nodeIsRootLevel = is<WebCore::Document>(*m_potentialTapNode) || is<WebCore::HTMLBodyElement>(*m_potentialTapNode);
        bool nodeIsPluginElement = is<WebCore::HTMLPlugInElement>(*m_potentialTapNode);
        send(Messages::WebPageProxy::HandleSmartMagnificationInformationForPotentialTap(requestID, absoluteBoundingRect, fitEntireRect, viewportMinimumScale, viewportMaximumScale, nodeIsRootLevel, nodeIsPluginElement));
    }

    sendTapHighlightForNodeIfNecessary(requestID, m_potentialTapNode.get(), position);
#if ENABLE(TWO_PHASE_CLICKS)
    if (RefPtr potentialTapNode = m_potentialTapNode; potentialTapNode && !potentialTapNode->allowsDoubleTapGesture())
        send(Messages::WebPageProxy::DisableDoubleTapGesturesDuringTapIfNecessary(requestID));
#endif
#endif // PLATFORM(IOS_FAMILY)

    co_return std::nullopt;
}

Awaitable<std::optional<WebCore::FrameIdentifier>> WebPage::commitPotentialTap(std::optional<WebCore::FrameIdentifier> frameID, OptionSet<WebEventModifier> modifiers, TransactionID lastLayerTreeTransactionId, WebCore::PointerID pointerId)
{
    RefPtr frameOwner = dynamicDowncast<HTMLFrameOwnerElement>(m_potentialTapNode.get());
    RefPtr remoteFrame = frameOwner ? dynamicDowncast<RemoteFrame>(frameOwner->contentFrame()) : nullptr;
    if (remoteFrame)
        co_return remoteFrame->frameID();

#if ENABLE(TWO_PHASE_CLICKS)
    auto invalidTargetForSingleClick = !m_potentialTapNode;
    if (!invalidTargetForSingleClick) {
        bool targetRenders = m_potentialTapNode->renderer();
        if (RefPtr element = dynamicDowncast<Element>(m_potentialTapNode); element && !targetRenders)
            targetRenders = element->renderOrDisplayContentsStyle();
        if (RefPtr shadowRoot = dynamicDowncast<ShadowRoot>(m_potentialTapNode); shadowRoot && !targetRenders)
            targetRenders = protect(shadowRoot->host())->renderOrDisplayContentsStyle();
        invalidTargetForSingleClick = !targetRenders && !is<HTMLAreaElement>(m_potentialTapNode);
    }

    RefPtr localRootFrame = this->localRootFrame(frameID);

    if (invalidTargetForSingleClick) {
#if PLATFORM(IOS_FAMILY)
        if (localRootFrame) {
            constexpr OptionSet hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowVisibleChildFrameContentOnly };
            auto roundedPoint = IntPoint { m_potentialTapLocation };
            auto result = localRootFrame->eventHandler().hitTestResultAtPoint(roundedPoint, hitType);
            localRootFrame->eventHandler().setLastTouchedNode(protect(result.innerNode()));
        }
#endif

        commitPotentialTapFailed();
        co_return std::nullopt;
    }

#if PLATFORM(IOS_FAMILY)
    if (localRootFrame)
        localRootFrame->eventHandler().setLastTouchedNode(nullptr);
#endif

    FloatPoint adjustedPoint;
    RefPtr nodeRespondingToClick = localRootFrame ? localRootFrame->nodeRespondingToClickEvents(m_potentialTapLocation, adjustedPoint, m_potentialTapSecurityOrigin.get()) : nullptr;
    RefPtr frameRespondingToClick = nodeRespondingToClick ? nodeRespondingToClick->document().frame() : nullptr;

    if (!frameRespondingToClick) {
        commitPotentialTapFailed();
        co_return std::nullopt;
    }

    auto firstTransactionID = WebFrame::fromCoreFrame(*frameRespondingToClick)->firstLayerTreeTransactionIDAfterDidCommitLoad();
    if (firstTransactionID
        && lastLayerTreeTransactionId.processIdentifier() == firstTransactionID->processIdentifier()
        && lastLayerTreeTransactionId.lessThanSameProcess(*firstTransactionID)) {
        commitPotentialTapFailed();
        co_return std::nullopt;
    }

    if (m_potentialTapNode == nodeRespondingToClick)
        handleSyntheticClick(frameID, *nodeRespondingToClick, adjustedPoint, modifiers, pointerId);
    else
        commitPotentialTapFailed();
#endif // ENABLE(TWO_PHASE_CLICKS)

    m_potentialTapNode = nullptr;
    m_potentialTapLocation = FloatPoint();
    m_potentialTapSecurityOrigin = nullptr;

    co_return std::nullopt;
}

void WebPage::cancelPotentialTapInFrame(WebFrame& frame)
{
    if (auto selectionChangedHandler = std::exchange(m_selectionChangedHandler, { }))
        selectionChangedHandler();

    if (m_potentialTapNode) {
        RefPtr potentialTapFrame = m_potentialTapNode->document().frame();
        if (potentialTapFrame && !potentialTapFrame->tree().isDescendantOf(protect(frame.coreLocalFrame())))
            return;
    }

    m_potentialTapNode = nullptr;
    m_potentialTapLocation = FloatPoint();
    m_potentialTapSecurityOrigin = nullptr;
}

void WebPage::cancelPotentialTap()
{
#if ENABLE(CONTENT_CHANGE_OBSERVER)
    if (RefPtr localMainFrame = protect(*m_page)->localMainFrame())
        ContentChangeObserver::didCancelPotentialTap(*localMainFrame);
#endif
    cancelPotentialTapInFrame(m_mainFrame);
}

void WebPage::didHandleTapAsHover()
{
    invokePendingSyntheticClickCallback(SyntheticClickResult::Hover);
    send(Messages::WebPageProxy::DidHandleTapAsHover());
}

void WebPage::sendTapHighlightForNodeIfNecessary(WebKit::TapIdentifier requestID, Node* node, FloatPoint point)
{
#if ENABLE(TWO_PHASE_CLICKS)
    if (!node)
        return;

    RefPtr localMainFrame = m_page->localMainFrame();
    if (!localMainFrame)
        return;

    if (m_page->isEditable() && node == protect(localMainFrame->document())->body())
        return;

    if (RefPtr element = dynamicDowncast<Element>(*node)) {
        ASSERT(m_page);
        localMainFrame->loader().prefetchDNSIfNeeded(element->absoluteLinkURL());
    }

    RefPtr updatedNode = node;
    if (RefPtr area = dynamicDowncast<HTMLAreaElement>(*node)) {
        updatedNode = area->imageElement();
        if (!updatedNode)
            return;
    }

#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = pluginViewForFrame(protect(updatedNode->document().frame()))) {
        if (auto rect = pluginView->highlightRectForTapAtPoint(point)) {
#if ENABLE(CSS_TAP_HIGHLIGHT_COLOR)
            auto highlightColor = RenderTheme::singleton().platformTapHighlightColor();
#else
            auto highlightColor = Color::transparentBlack;
#endif
            auto highlightQuads = Vector { FloatQuad { WTF::move(*rect) } };
            send(Messages::WebPageProxy::DidGetTapHighlightGeometries(requestID, WTF::move(highlightColor), WTF::move(highlightQuads), { }, { }, { }, { }, true));
            return;
        }
    }
#endif // ENABLE(PDF_PLUGIN)

    Vector<FloatQuad> quads;
    if (CheckedPtr renderer = updatedNode->renderer()) {
        renderer->absoluteQuads(quads);
#if ENABLE(CSS_TAP_HIGHLIGHT_COLOR)
        auto highlightColor = renderer->style().tapHighlightColorResolvingCurrentColor();
#else
        auto highlightColor = Color::transparentBlack;
#endif
        if (!updatedNode->document().frame()->isMainFrame()) {
            RefPtr view = updatedNode->document().frame()->view();
            for (auto& quad : quads)
                quad = view->contentsToRootView(quad);
        }

        LayoutRoundedRect::Radii borderRadii;
        if (CheckedPtr renderBox = dynamicDowncast<RenderBox>(*renderer))
            borderRadii = renderBox->borderRadii();

        RefPtr element = dynamicDowncast<Element>(*updatedNode);
        bool nodeHasBuiltInClickHandling = element && (is<HTMLFormControlElement>(*element) || is<HTMLAnchorElement>(*element) || is<HTMLLabelElement>(*element) || is<HTMLSummaryElement>(*element) || element->isLink());
        send(Messages::WebPageProxy::DidGetTapHighlightGeometries(requestID, highlightColor, quads, roundedIntSize(borderRadii.topLeft()), roundedIntSize(borderRadii.topRight()), roundedIntSize(borderRadii.bottomLeft()), roundedIntSize(borderRadii.bottomRight()), nodeHasBuiltInClickHandling));
    }
#else
    UNUSED_PARAM(requestID);
    UNUSED_PARAM(node);
    UNUSED_PARAM(point);
#endif
}

#if ENABLE(CONTENT_CHANGE_OBSERVER)
void WebPage::didFinishContentChangeObserving(WebCore::FrameIdentifier frameID, WebCore::ContentChange observedContentChange)
{
    LOG_WITH_STREAM(ContentObservation, stream << "didFinishContentChangeObserving: pending target node(" << m_pendingSyntheticClickNode << ")");
    if (!m_pendingSyntheticClickNode)
        return;
    callOnMainRunLoop([
        protectedThis = Ref { *this },
        targetNode = Ref<Node>(*m_pendingSyntheticClickNode),
        originalDocument = WeakPtr<Document, WeakPtrImplWithEventTargetData> { m_pendingSyntheticClickNode->document() },
        observedContentChange,
        location = m_pendingSyntheticClickLocation,
        modifiers = m_pendingSyntheticClickModifiers,
        pointerId = m_pendingSyntheticClickPointerId,
        inputSource = m_potentialTapInputSource,
        frameID
    ] {
        if (protectedThis->m_isClosed || !protectedThis->corePage())
            return;
        if (!originalDocument || &targetNode->document() != originalDocument)
            return;

        // Only dispatch the click if the document didn't get changed by any timers started by the move event.
        if (observedContentChange == WebCore::ContentChange::None) {
            LOG(ContentObservation, "No change was observed -> click.");
            protectedThis->completeSyntheticClick(frameID, targetNode, location, modifiers, WebCore::SyntheticClickType::OneFingerTap, pointerId);
            return;
        }
        // Ensure that the mouse is on the most recent content.
        LOG(ContentObservation, "Observed meaningful visible change -> hover.");
        if (RefPtr localRootFrame = protectedThis->localRootFrame(frameID))
            dispatchSyntheticMouseMove(*localRootFrame, location, modifiers, pointerId, inputSource);

        protectedThis->didHandleTapAsHover();
    });
    m_pendingSyntheticClickNode = nullptr;
    m_pendingSyntheticClickLocation = { };
    m_pendingSyntheticClickModifiers = { };
    m_pendingSyntheticClickPointerId = 0;
}
#endif // ENABLE(CONTENT_CHANGE_OBSERVER)

void WebPage::invokePendingSyntheticClickCallback(SyntheticClickResult result)
{
    if (auto callback = std::exchange(m_pendingSyntheticClickCallback, { }))
        callback(result);
}

void WebPage::commitPotentialTapFailed()
{
    if (auto selectionChangedHandler = std::exchange(m_selectionChangedHandler, { }))
        selectionChangedHandler();

#if ENABLE(CONTENT_CHANGE_OBSERVER)
    if (RefPtr localMainFrame = protect(*m_page)->localMainFrame())
        ContentChangeObserver::didCancelPotentialTap(*localMainFrame);
#endif
#if PLATFORM(IOS_FAMILY)
    clearSelectionAfterTapIfNeeded();
#endif
    invokePendingSyntheticClickCallback(SyntheticClickResult::Failed);

    send(Messages::WebPageProxy::CommitPotentialTapFailed());
    send(Messages::WebPageProxy::DidNotHandleTapAsClick(roundedIntPoint(m_potentialTapLocation)));
}

void WebPage::completeSyntheticClick(std::optional<WebCore::FrameIdentifier> frameID, Node& nodeRespondingToClick, const WebCore::FloatPoint& location, OptionSet<WebEventModifier> modifiers, SyntheticClickType syntheticClickType, WebCore::PointerID pointerId)
{
    SetForScope completeSyntheticClickScope { m_completingSyntheticClick, true };
    IntPoint roundedAdjustedPoint = roundedIntPoint(location);

    // FIXME: Make this function take a root frame's ID instead of taking a frame ID of a non-root frame and replacing it with the root frame.
    auto rootFrameID = frameID;
    if (RefPtr webFrame = WebProcess::singleton().webFrame(frameID)) {
        if (RefPtr frame = webFrame->coreLocalFrame(); frame && !frame->isRootFrame())
            rootFrameID = WebFrame::fromCoreFrame(protect(frame->rootFrame()))->frameID();
    }

    RefPtr localRootFrame = this->localRootFrame(rootFrameID);
    if (!localRootFrame) {
        invokePendingSyntheticClickCallback(SyntheticClickResult::PageInvalid);
        return;
    }

    RefPtr oldFocusedFrame = m_page->focusController().focusedLocalFrame();
    RefPtr<Element> oldFocusedElement = oldFocusedFrame ? oldFocusedFrame->document()->focusedElement() : nullptr;

    SetForScope userIsInteractingChange { m_userIsInteracting, true };

#if PLATFORM(IOS_FAMILY)
    m_lastInteractionLocation = roundedAdjustedPoint;
#endif

    // FIXME: Pass caps lock state.
    auto platformModifiers = platform(modifiers);

    bool handledPress = localRootFrame->eventHandler().handleMousePressEvent(PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, MouseButton::Left, PlatformEvent::Type::MousePressed, 1, platformModifiers, MonotonicTime::now(), WebCore::ForceAtClick, syntheticClickType, m_potentialTapInputSource, pointerId)).wasHandled();
    if (m_isClosed)
        return;

    if (auto selectionChangedHandler = std::exchange(m_selectionChangedHandler, { }))
        selectionChangedHandler();
#if PLATFORM(IOS_FAMILY)
    else if (!handledPress)
        clearSelectionAfterTapIfNeeded();
#endif

    auto releaseEvent = PlatformMouseEvent { roundedAdjustedPoint, roundedAdjustedPoint, MouseButton::Left, PlatformEvent::Type::MouseReleased, 1, platformModifiers, MonotonicTime::now(), ForceAtClick, syntheticClickType, m_potentialTapInputSource, pointerId };
    bool handledRelease = localRootFrame->eventHandler().handleMouseReleaseEvent(releaseEvent).wasHandled();
    if (m_isClosed)
        return;

    RefPtr newFocusedFrame = m_page->focusController().focusedLocalFrame();
    RefPtr<Element> newFocusedElement = newFocusedFrame ? newFocusedFrame->document()->focusedElement() : nullptr;

    if (nodeRespondingToClick.document().settings().contentChangeObserverEnabled()) {
        Ref document = nodeRespondingToClick.document();
        // Dispatch mouseOut to dismiss tooltip content when tapping on the control bar buttons (cc, settings).
        if (document->quirks().needsYouTubeMouseOutQuirk()) {
            if (RefPtr frame = document->frame()) {
                PlatformMouseEvent event { roundedAdjustedPoint, roundedAdjustedPoint, MouseButton::Left, PlatformEvent::Type::NoType, 0, platformModifiers, MonotonicTime::now(), 0, WebCore::SyntheticClickType::NoTap, m_potentialTapInputSource, pointerId };
                if (!nodeRespondingToClick.isConnected())
                    frame->eventHandler().dispatchSyntheticMouseMove(event);
                frame->eventHandler().dispatchSyntheticMouseOut(event);
            }
        }
    }

    if (m_isClosed)
        return;

#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginElement = dynamicDowncast<HTMLPlugInElement>(nodeRespondingToClick)) {
        if (RefPtr pluginWidget = downcast<PluginView>(pluginElement->pluginWidget()))
            pluginWidget->handleSyntheticClick(WTF::move(releaseEvent));
    }
#endif

    invokePendingSyntheticClickCallback(SyntheticClickResult::Click);

    if ((!handledPress && !handledRelease) || !nodeRespondingToClick.isElementNode())
        send(Messages::WebPageProxy::DidNotHandleTapAsClick(roundedIntPoint(location)));

    send(Messages::WebPageProxy::DidCompleteSyntheticClick());

#if PLATFORM(IOS_FAMILY)
    scheduleLayoutViewportHeightExpansionUpdate();
#endif
}

#endif // ENABLE(TWO_PHASE_CLICKS)

} // namespace WebKit

#endif // PLATFORM(COCOA)
