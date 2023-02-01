/*
 * Copyright (C) 2012-2022 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "AccessibilityIOS.h"
#import "DataReference.h"
#import "DocumentEditingContext.h"
#import "DrawingArea.h"
#import "EditingRange.h"
#import "EditorState.h"
#import "InteractionInformationAtPosition.h"
#import "Logging.h"
#import "NativeWebKeyboardEvent.h"
#import "PluginView.h"
#import "PrintInfo.h"
#import "RemoteLayerTreeDrawingArea.h"
#import "RemoteScrollingCoordinator.h"
#import "RevealItem.h"
#import "SandboxUtilities.h"
#import "ShareableBitmapUtilities.h"
#import "SharedBufferReference.h"
#import "SharedMemory.h"
#import "SyntheticEditingCommandType.h"
#import "TapHandlingResult.h"
#import "TextCheckingControllerProxy.h"
#import "UIKitSPI.h"
#import "UserData.h"
#import "ViewGestureGeometryCollector.h"
#import "VisibleContentRectUpdateInfo.h"
#import "WKAccessibilityWebPageObjectIOS.h"
#import "WebAutocorrectionContext.h"
#import "WebAutocorrectionData.h"
#import "WebChromeClient.h"
#import "WebCoreArgumentCoders.h"
#import "WebEventConversion.h"
#import "WebFrame.h"
#import "WebImage.h"
#import "WebPageMessages.h"
#import "WebPageProxyMessages.h"
#import "WebPreviewLoaderClient.h"
#import "WebProcess.h"
#import "WebTouchEvent.h"
#import <CoreText/CTFont.h>
#import <WebCore/Autofill.h>
#import <WebCore/AutofillElements.h>
#import <WebCore/Chrome.h>
#import <WebCore/ContentChangeObserver.h>
#import <WebCore/DOMTimerHoldingTank.h>
#import <WebCore/DataDetection.h>
#import <WebCore/DataDetectionResultsStorage.h>
#import <WebCore/DiagnosticLoggingClient.h>
#import <WebCore/DiagnosticLoggingKeys.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/DocumentMarkerController.h>
#import <WebCore/DragController.h>
#import <WebCore/Editing.h>
#import <WebCore/Editor.h>
#import <WebCore/EditorClient.h>
#import <WebCore/Element.h>
#import <WebCore/ElementAncestorIterator.h>
#import <WebCore/EventHandler.h>
#import <WebCore/File.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/FocusController.h>
#import <WebCore/FontCache.h>
#import <WebCore/FontCacheCoreText.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoaderClient.h>
#import <WebCore/FrameView.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/HTMLAreaElement.h>
#import <WebCore/HTMLAttachmentElement.h>
#import <WebCore/HTMLBodyElement.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLElementTypeHelpers.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLIFrameElement.h>
#import <WebCore/HTMLImageElement.h>
#import <WebCore/HTMLInputElement.h>
#import <WebCore/HTMLLabelElement.h>
#import <WebCore/HTMLOptGroupElement.h>
#import <WebCore/HTMLOptionElement.h>
#import <WebCore/HTMLParserIdioms.h>
#import <WebCore/HTMLSelectElement.h>
#import <WebCore/HTMLSummaryElement.h>
#import <WebCore/HTMLTextAreaElement.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/ImageOverlay.h>
#import <WebCore/InputMode.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/LibWebRTCProvider.h>
#import <WebCore/MediaSessionManagerIOS.h>
#import <WebCore/Node.h>
#import <WebCore/NodeList.h>
#import <WebCore/NodeRenderStyle.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/Page.h>
#import <WebCore/PagePasteboardContext.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/PlatformKeyboardEvent.h>
#import <WebCore/PlatformMediaSessionManager.h>
#import <WebCore/PlatformMouseEvent.h>
#import <WebCore/PointerCaptureController.h>
#import <WebCore/PointerCharacteristics.h>
#import <WebCore/Quirks.h>
#import <WebCore/Range.h>
#import <WebCore/RenderBlock.h>
#import <WebCore/RenderImage.h>
#import <WebCore/RenderLayer.h>
#import <WebCore/RenderThemeIOS.h>
#import <WebCore/RenderVideo.h>
#import <WebCore/RenderView.h>
#import <WebCore/RenderedDocumentMarker.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/Settings.h>
#import <WebCore/ShadowRoot.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/StyleProperties.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/TextIterator.h>
#import <WebCore/TextPlaceholderElement.h>
#import <WebCore/UserAgent.h>
#import <WebCore/UserGestureIndicator.h>
#import <WebCore/VisibleUnits.h>
#import <WebCore/WebEvent.h>
#import <wtf/MathExtras.h>
#import <wtf/MemoryPressureHandler.h>
#import <wtf/Scope.h>
#import <wtf/SetForScope.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/text/StringToIntegerConversion.h>
#import <wtf/text/TextBreakIterator.h>
#import <wtf/text/TextStream.h>

#if ENABLE(ATTACHMENT_ELEMENT)
#import <WebCore/PromisedAttachmentInfo.h>
#endif

#import <pal/cocoa/RevealSoftLink.h>

#define WEBPAGE_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - WebPage::" fmt, this, ##__VA_ARGS__)
#define WEBPAGE_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - WebPage::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

enum class SelectionWasFlipped : bool { No, Yes };

// FIXME: Unclear if callers in this file are correctly choosing which of these two functions to use.

static String plainTextForContext(const SimpleRange& range)
{
    return WebCore::plainTextReplacingNoBreakSpace(range);
}

static String plainTextForContext(const std::optional<SimpleRange>& range)
{
    return range ? plainTextForContext(*range) : emptyString();
}

static String plainTextForDisplay(const SimpleRange& range)
{
    return WebCore::plainTextReplacingNoBreakSpace(range, { }, true);
}

static String plainTextForDisplay(const std::optional<SimpleRange>& range)
{
    return range ? plainTextForDisplay(*range) : emptyString();
}

// WebCore stores the page scale factor as float instead of double. When we get a scale from WebCore,
// we need to ignore differences that are within a small rounding error, with enough leeway
// to handle rounding differences that may result from round-tripping through UIScrollView.
bool scalesAreEssentiallyEqual(float a, float b)
{
    const auto scaleFactorEpsilon = 0.01f;
    return WTF::areEssentiallyEqual(a, b, scaleFactorEpsilon);
}

void WebPage::platformDetach()
{
    [m_mockAccessibilityElement setWebPage:nullptr];
}
    
void WebPage::platformInitializeAccessibility()
{
    m_mockAccessibilityElement = adoptNS([[WKAccessibilityWebPageObject alloc] init]);
    [m_mockAccessibilityElement setWebPage:this];

    accessibilityTransferRemoteToken(accessibilityRemoteTokenData());
}

void WebPage::platformReinitialize()
{
    accessibilityTransferRemoteToken(accessibilityRemoteTokenData());
}

RetainPtr<NSData> WebPage::accessibilityRemoteTokenData() const
{
    return newAccessibilityRemoteToken([NSUUID UUID]);
}

static void computeEditableRootHasContentAndPlainText(const VisibleSelection& selection, EditorState::PostLayoutData& data)
{
    data.hasContent = false;
    data.hasPlainText = false;
    if (!selection.isContentEditable())
        return;

    if (data.selectedTextLength || data.characterAfterSelection || data.characterBeforeSelection || data.twoCharacterBeforeSelection) {
        // If any of these variables have been previously set, the editable root must have plain text content, so we can bail from the remainder of the check.
        data.hasContent = true;
        data.hasPlainText = true;
        return;
    }

    auto* root = selection.rootEditableElement();
    if (!root)
        return;

    auto startInEditableRoot = firstPositionInNode(root);
    data.hasContent = root->hasChildNodes() && !isEndOfEditableOrNonEditableContent(startInEditableRoot);
    if (data.hasContent) {
        auto range = makeSimpleRange(VisiblePosition { startInEditableRoot }, VisiblePosition { lastPositionInNode(root) });
        data.hasPlainText = range && hasAnyPlainText(*range);
    }
}

bool WebPage::requiresPostLayoutDataForEditorState(const Frame& frame) const
{
    // If we have a composition or are using a hardware keyboard then we need to send the full
    // editor so that the UIProcess can update UI, including the position of the caret.
    bool needsLayout = frame.editor().hasComposition();
#if !PLATFORM(MACCATALYST)
    needsLayout |= m_keyboardIsAttached;
#endif
    return needsLayout;
}

static void convertContentToRootView(const FrameView& view, Vector<SelectionGeometry>& geometries)
{
    for (auto& geometry : geometries)
        geometry.setQuad(view.contentsToRootView(geometry.quad()));
}

void WebPage::getPlatformEditorState(Frame& frame, EditorState& result) const
{
    getPlatformEditorStateCommon(frame, result);

    if (!result.hasPostLayoutAndVisualData())
        return;

    ASSERT(frame.view());
    auto& postLayoutData = *result.postLayoutData;
    auto& visualData = *result.visualData;

    Ref view = *frame.view();

    if (frame.editor().hasComposition()) {
        if (auto compositionRange = frame.editor().compositionRange()) {
            visualData.markedTextRects = RenderObject::collectSelectionGeometries(*compositionRange);
            convertContentToRootView(view, visualData.markedTextRects);

            postLayoutData.markedText = plainTextForContext(*compositionRange);
            VisibleSelection compositionSelection(*compositionRange);
            visualData.markedTextCaretRectAtStart = view->contentsToRootView(compositionSelection.visibleStart().absoluteCaretBounds(nullptr /* insideFixed */));
            visualData.markedTextCaretRectAtEnd = view->contentsToRootView(compositionSelection.visibleEnd().absoluteCaretBounds(nullptr /* insideFixed */));

        }
    }

    const auto& selection = frame.selection().selection();
    std::optional<SimpleRange> selectedRange;
    postLayoutData.isStableStateUpdate = m_isInStableState;
    bool startNodeIsInsideFixedPosition = false;
    bool endNodeIsInsideFixedPosition = false;
    if (selection.isCaret()) {
        visualData.caretRectAtStart = view->contentsToRootView(frame.selection().absoluteCaretBounds(&startNodeIsInsideFixedPosition));
        endNodeIsInsideFixedPosition = startNodeIsInsideFixedPosition;
        visualData.caretRectAtEnd = visualData.caretRectAtStart;
        // FIXME: The following check should take into account writing direction.
        postLayoutData.isReplaceAllowed = result.isContentEditable && atBoundaryOfGranularity(selection.start(), TextGranularity::WordGranularity, SelectionDirection::Forward);

        selectedRange = wordRangeFromPosition(selection.start());
        postLayoutData.wordAtSelection = plainTextForContext(selectedRange);

        if (selection.isContentEditable())
            charactersAroundPosition(selection.start(), postLayoutData.characterAfterSelection, postLayoutData.characterBeforeSelection, postLayoutData.twoCharacterBeforeSelection);
    } else if (selection.isRange()) {
        visualData.caretRectAtStart = view->contentsToRootView(VisiblePosition(selection.start()).absoluteCaretBounds(&startNodeIsInsideFixedPosition));
        visualData.caretRectAtEnd = view->contentsToRootView(VisiblePosition(selection.end()).absoluteCaretBounds(&endNodeIsInsideFixedPosition));
        selectedRange = selection.toNormalizedRange();
        String selectedText;
        if (selectedRange) {
            visualData.selectionGeometries = RenderObject::collectSelectionGeometries(*selectedRange);
            convertContentToRootView(view, visualData.selectionGeometries);
            selectedText = plainTextForDisplay(*selectedRange);
            postLayoutData.selectedTextLength = selectedText.length();
            const int maxSelectedTextLength = 200;
            postLayoutData.wordAtSelection = selectedText.left(maxSelectedTextLength);
            auto findSelectedEditableImageElement = [&] {
                RefPtr<HTMLImageElement> foundImage;
                if (!result.isContentEditable)
                    return foundImage;

                for (TextIterator iterator { *selectedRange, { } }; !iterator.atEnd(); iterator.advance()) {
                    auto imageElement = dynamicDowncast<HTMLImageElement>(iterator.node());
                    if (!imageElement)
                        continue;

                    if (foundImage) {
                        foundImage = nullptr;
                        break;
                    }

                    foundImage = imageElement;
                }
                return foundImage;
            };

            if (auto imageElement = findSelectedEditableImageElement())
                postLayoutData.selectedEditableImage = contextForElement(*imageElement);
        }
        // FIXME: We should disallow replace when the string contains only CJ characters.
        postLayoutData.isReplaceAllowed = result.isContentEditable && !result.isInPasswordField && !selectedText.isAllSpecialCharacters<isHTMLSpace>();
    }

#if USE(DICTATION_ALTERNATIVES)
    if (selectedRange) {
        auto markers = frame.document()->markers().markersInRange(*selectedRange, DocumentMarker::MarkerType::DictationAlternatives);
        postLayoutData.dictationContextsForSelection = WTF::map(markers, [] (auto* marker) {
            return std::get<DocumentMarker::DictationData>(marker->data()).context;
        });
    }
#endif

    postLayoutData.insideFixedPosition = startNodeIsInsideFixedPosition || endNodeIsInsideFixedPosition;
    if (!selection.isNone()) {
        if (selection.hasEditableStyle()) {
            // FIXME: The caret color style should be computed using the selection caret's container
            // rather than the focused element. This causes caret colors in editable children to be
            // ignored in favor of the editing host's caret color. See: <https://webkit.org/b/229809>.
            if (RefPtr editableRoot = selection.rootEditableElement(); editableRoot && editableRoot->renderer())
                postLayoutData.caretColor = CaretBase::computeCaretColor(editableRoot->renderer()->style(), editableRoot.get());
        }

        if (RefPtr editableRootOrFormControl = enclosingTextFormControl(selection.start()) ?: selection.rootEditableElement()) {
            visualData.selectionClipRect = rootViewInteractionBounds(*editableRootOrFormControl);
            postLayoutData.editableRootIsTransparentOrFullyClipped = result.isContentEditable && isTransparentOrFullyClipped(*editableRootOrFormControl);
        }
        computeEditableRootHasContentAndPlainText(selection, postLayoutData);
        postLayoutData.selectionStartIsAtParagraphBoundary = atBoundaryOfGranularity(selection.visibleStart(), TextGranularity::ParagraphGranularity, SelectionDirection::Backward);
        postLayoutData.selectionEndIsAtParagraphBoundary = atBoundaryOfGranularity(selection.visibleEnd(), TextGranularity::ParagraphGranularity, SelectionDirection::Forward);
    }
}

void WebPage::platformWillPerformEditingCommand()
{
#if ENABLE(CONTENT_CHANGE_OBSERVER)
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    if (RefPtr document = frame->document()) {
        if (auto* holdingTank = document->domTimerHoldingTankIfExists())
            holdingTank->removeAll();
    }
#endif
}

FloatSize WebPage::screenSize() const
{
    return m_screenSize;
}

FloatSize WebPage::availableScreenSize() const
{
    return m_availableScreenSize;
}

FloatSize WebPage::overrideScreenSize() const
{
    return m_overrideScreenSize;
}

void WebPage::didReceiveMobileDocType(bool isMobileDoctype)
{
    resetViewportDefaultConfiguration(m_mainFrame.ptr(), isMobileDoctype);
}

void WebPage::savePageState(HistoryItem& historyItem)
{
    historyItem.setScaleIsInitial(!m_userHasChangedPageScaleFactor);
    historyItem.setMinimumLayoutSizeInScrollViewCoordinates(m_viewportConfiguration.minimumLayoutSize());
    historyItem.setContentSize(m_viewportConfiguration.contentsSize());
}

static double scaleAfterViewportWidthChange(double currentScale, bool scaleToFitContent, const ViewportConfiguration& viewportConfiguration, float unobscuredWidthInScrollViewCoordinates, const IntSize& newContentSize, const IntSize& oldContentSize, float visibleHorizontalFraction)
{
    double scale;
    if (!scaleToFitContent) {
        scale = viewportConfiguration.initialScale();
        LOG(VisibleRects, "scaleAfterViewportWidthChange using initial scale: %.2f", scale);
        return scale;
    }

    // When the content size changes, we keep the same relative horizontal content width in view, otherwise we would
    // end up zoomed too far in landscape->portrait, and too close in portrait->landscape.
    double widthToKeepInView = visibleHorizontalFraction * newContentSize.width();
    double newScale = unobscuredWidthInScrollViewCoordinates / widthToKeepInView;
    scale = std::max(std::min(newScale, viewportConfiguration.maximumScale()), viewportConfiguration.minimumScale());
    LOG(VisibleRects, "scaleAfterViewportWidthChange scaling content to fit: %.2f", scale);
    return scale;
}

static FloatPoint relativeCenterAfterContentSizeChange(const FloatRect& originalContentRect, IntSize oldContentSize, IntSize newContentSize)
{
    // If the content size has changed, keep the same relative position.
    FloatPoint oldContentCenter = originalContentRect.center();
    float relativeHorizontalPosition = oldContentCenter.x() / oldContentSize.width();
    float relativeVerticalPosition =  oldContentCenter.y() / oldContentSize.height();
    return FloatPoint(relativeHorizontalPosition * newContentSize.width(), relativeVerticalPosition * newContentSize.height());
}

static inline FloatRect adjustExposedRectForNewScale(const FloatRect& exposedRect, double exposedRectScale, double newScale)
{
    if (exposedRectScale == newScale)
        return exposedRect;

    float horizontalChange = exposedRect.width() * exposedRectScale / newScale - exposedRect.width();
    float verticalChange = exposedRect.height() * exposedRectScale / newScale - exposedRect.height();

    auto adjustedRect = exposedRect;
    adjustedRect.inflate({ horizontalChange / 2, verticalChange / 2 });
    return adjustedRect;
}

void WebPage::restorePageState(const HistoryItem& historyItem)
{
    // When a HistoryItem is cleared, its scale factor and scroll point are set to zero. We should not try to restore the other
    // parameters in those conditions.
    if (!historyItem.pageScaleFactor()) {
        send(Messages::WebPageProxy::CouldNotRestorePageState());
        return;
    }

    // We can restore the exposed rect and scale, but we cannot touch the scroll position since the obscured insets
    // may be changing in the UIProcess. The UIProcess can update the position from the information we send and will then
    // scroll to the correct position through a regular VisibleContentRectUpdate.

    m_userHasChangedPageScaleFactor = !historyItem.scaleIsInitial();

    FrameView& frameView = *m_page->mainFrame().view();

    FloatSize currentMinimumLayoutSizeInScrollViewCoordinates = m_viewportConfiguration.minimumLayoutSize();
    if (historyItem.minimumLayoutSizeInScrollViewCoordinates() == currentMinimumLayoutSizeInScrollViewCoordinates) {
        float boundedScale = historyItem.scaleIsInitial() ? m_viewportConfiguration.initialScale() : historyItem.pageScaleFactor();
        boundedScale = std::min<float>(m_viewportConfiguration.maximumScale(), std::max<float>(m_viewportConfiguration.minimumScale(), boundedScale));
        scalePage(boundedScale, IntPoint());

        std::optional<FloatPoint> scrollPosition;
        if (historyItem.shouldRestoreScrollPosition()) {
            m_drawingArea->setExposedContentRect(historyItem.exposedContentRect());
            m_hasRestoredExposedContentRectAfterDidCommitLoad = true;
            scrollPosition = FloatPoint(historyItem.scrollPosition());
        }

        send(Messages::WebPageProxy::RestorePageState(scrollPosition, frameView.scrollOrigin(), historyItem.obscuredInsets(), boundedScale));
    } else {
        IntSize oldContentSize = historyItem.contentSize();
        IntSize newContentSize = frameView.contentsSize();
        double visibleHorizontalFraction = static_cast<float>(historyItem.unobscuredContentRect().width()) / oldContentSize.width();

        double newScale = scaleAfterViewportWidthChange(historyItem.pageScaleFactor(), !historyItem.scaleIsInitial(), m_viewportConfiguration, currentMinimumLayoutSizeInScrollViewCoordinates.width(), newContentSize, oldContentSize, visibleHorizontalFraction);

        std::optional<FloatPoint> newCenter;
        if (historyItem.shouldRestoreScrollPosition()) {
            if (!oldContentSize.isEmpty() && !newContentSize.isEmpty() && newContentSize != oldContentSize)
                newCenter = relativeCenterAfterContentSizeChange(historyItem.unobscuredContentRect(), oldContentSize, newContentSize);
            else
                newCenter = FloatRect(historyItem.unobscuredContentRect()).center();
        }

        scalePage(newScale, IntPoint());
        send(Messages::WebPageProxy::RestorePageCenterAndScale(newCenter, newScale));
    }
}

double WebPage::minimumPageScaleFactor() const
{
    if (!m_viewportConfiguration.allowsUserScaling())
        return m_page->pageScaleFactor();
    return m_viewportConfiguration.minimumScale();
}

double WebPage::maximumPageScaleFactor() const
{
    if (!m_viewportConfiguration.allowsUserScaling())
        return m_page->pageScaleFactor();
    return m_viewportConfiguration.maximumScale();
}

double WebPage::maximumPageScaleFactorIgnoringAlwaysScalable() const
{
    if (!m_viewportConfiguration.allowsUserScalingIgnoringAlwaysScalable())
        return m_page->pageScaleFactor();
    return m_viewportConfiguration.maximumScaleIgnoringAlwaysScalable();
}

bool WebPage::allowsUserScaling() const
{
    return m_viewportConfiguration.allowsUserScaling();
}

bool WebPage::handleEditingKeyboardEvent(KeyboardEvent& event)
{
    auto* platformEvent = event.underlyingPlatformEvent();
    if (!platformEvent)
        return false;
    
    // Don't send synthetic events to the UIProcess. They are only
    // used for interacting with JavaScript.
    if (platformEvent->isSyntheticEvent())
        return false;

    if (handleKeyEventByRelinquishingFocusToChrome(event))
        return true;

    // FIXME: Interpret the event immediately upon receiving it in UI process, without sending to WebProcess first.
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::InterpretKeyEvent(editorState(ShouldPerformLayout::Yes), platformEvent->type() == PlatformKeyboardEvent::Type::Char), m_identifier);
    auto [eventWasHandled] = sendResult.takeReplyOr(false);
    return eventWasHandled;
}

static bool disableServiceWorkerEntitlementTestingOverride;

bool WebPage::parentProcessHasServiceWorkerEntitlement() const
{
    if (disableServiceWorkerEntitlementTestingOverride)
        return false;
    
    static bool hasEntitlement = WTF::hasEntitlement(WebProcess::singleton().parentProcessConnection()->xpcConnection(), "com.apple.developer.WebKit.ServiceWorkers"_s) || WTF::hasEntitlement(WebProcess::singleton().parentProcessConnection()->xpcConnection(), "com.apple.developer.web-browser"_s);
    return hasEntitlement;
}

void WebPage::disableServiceWorkerEntitlement()
{
    disableServiceWorkerEntitlementTestingOverride = true;
}

void WebPage::clearServiceWorkerEntitlementOverride(CompletionHandler<void()>&& completionHandler)
{
    disableServiceWorkerEntitlementTestingOverride = false;
    completionHandler();
}

bool WebPage::performNonEditingBehaviorForSelector(const String&, WebCore::KeyboardEvent*)
{
    notImplemented();
    return false;
}

void WebPage::getSelectionContext(CompletionHandler<void(const String&, const String&, const String&)>&& completionHandler)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    if (!frame->selection().isRange())
        return completionHandler({ }, { }, { });
    const int selectionExtendedContextLength = 350;

    auto& selection = frame->selection().selection();
    String selectedText = plainTextForContext(selection.firstRange());
    String textBefore = plainTextForDisplay(rangeExpandedByCharactersInDirectionAtWordBoundary(selection.start(), selectionExtendedContextLength, SelectionDirection::Backward));
    String textAfter = plainTextForDisplay(rangeExpandedByCharactersInDirectionAtWordBoundary(selection.end(), selectionExtendedContextLength, SelectionDirection::Forward));

    completionHandler(selectedText, textBefore, textAfter);
}

NSObject *WebPage::accessibilityObjectForMainFramePlugin()
{
    return nil;
}
    
void WebPage::registerUIProcessAccessibilityTokens(const IPC::DataReference& elementToken, const IPC::DataReference&)
{
    NSData *elementTokenData = [NSData dataWithBytes:elementToken.data() length:elementToken.size()];
    [m_mockAccessibilityElement setRemoteTokenData:elementTokenData];
}

void WebPage::getStringSelectionForPasteboard(CompletionHandler<void(String&&)>&& completionHandler)
{
    notImplemented();
    completionHandler({ });
}

void WebPage::getDataSelectionForPasteboard(const String, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
{
    notImplemented();
    completionHandler({ });
}

WKAccessibilityWebPageObject* WebPage::accessibilityRemoteObject()
{
    notImplemented();
    return 0;
}

bool WebPage::platformCanHandleRequest(const WebCore::ResourceRequest& request)
{
    return [NSURLConnection canHandleRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody)];
}

void WebPage::shouldDelayWindowOrderingEvent(const WebKit::WebMouseEvent&, CompletionHandler<void(bool)>&& completionHandler)
{
    notImplemented();
    completionHandler(false);
}

void WebPage::computePagesForPrintingPDFDocument(WebCore::FrameIdentifier, const PrintInfo&, Vector<IntRect>&)
{
    notImplemented();
}

void WebPage::drawPagesToPDFFromPDFDocument(CGContextRef, PDFDocument *, const PrintInfo&, uint32_t, uint32_t)
{
    notImplemented();
}

void WebPage::advanceToNextMisspelling(bool)
{
    notImplemented();
}

IntRect WebPage::rectForElementAtInteractionLocation() const
{
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowVisibleChildFrameContentOnly };
    HitTestResult result = m_page->mainFrame().eventHandler().hitTestResultAtPoint(m_lastInteractionLocation, hitType);
    Node* hitNode = result.innerNode();
    if (!hitNode || !hitNode->renderer())
        return IntRect();
    return result.innerNodeFrame()->view()->contentsToRootView(hitNode->renderer()->absoluteBoundingBoxRect(true));
}

void WebPage::updateSelectionAppearance()
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    auto& editor = frame->editor();
    if (editor.ignoreSelectionChanges())
        return;

    if (editor.client() && !editor.client()->shouldRevealCurrentSelectionAfterInsertion())
        return;

    if (!editor.hasComposition() && frame->selection().selection().isNone())
        return;

    didChangeSelection(frame.get());
}

static void dispatchSyntheticMouseMove(Frame& mainFrame, const WebCore::FloatPoint& location, OptionSet<WebEventModifier> modifiers, WebCore::PointerID pointerId = WebCore::mousePointerID)
{
    IntPoint roundedAdjustedPoint = roundedIntPoint(location);
    auto mouseEvent = PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, NoButton, PlatformEvent::Type::MouseMoved, 0, platform(modifiers), WallTime::now(), WebCore::ForceAtClick, WebCore::OneFingerTap, pointerId);
    // FIXME: Pass caps lock state.
    mainFrame.eventHandler().dispatchSyntheticMouseMove(mouseEvent);
}

void WebPage::generateSyntheticEditingCommand(SyntheticEditingCommandType command)
{
    PlatformKeyboardEvent keyEvent;
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    
    OptionSet<PlatformEvent::Modifier> modifiers;
    modifiers.add(PlatformEvent::Modifier::MetaKey);
    
    switch (command) {
    case SyntheticEditingCommandType::Undo:
        keyEvent = PlatformKeyboardEvent(PlatformEvent::Type::KeyDown, "z"_s, "z"_s,
        "z"_s, "KeyZ"_s,
        "U+005A"_s, 90, false, false, false, modifiers, WallTime::now());
        break;
    case SyntheticEditingCommandType::Redo:
        keyEvent = PlatformKeyboardEvent(PlatformEvent::Type::KeyDown, "y"_s, "y"_s,
        "y"_s, "KeyY"_s,
        "U+0059"_s, 89, false, false, false, modifiers, WallTime::now());
        break;
    case SyntheticEditingCommandType::ToggleBoldface:
        keyEvent = PlatformKeyboardEvent(PlatformEvent::Type::KeyDown, "b"_s, "b"_s,
        "b"_s, "KeyB"_s,
        "U+0042"_s, 66, false, false, false, modifiers, WallTime::now());
        break;
    case SyntheticEditingCommandType::ToggleItalic:
        keyEvent = PlatformKeyboardEvent(PlatformEvent::Type::KeyDown, "i"_s, "i"_s,
        "i"_s, "KeyI"_s,
        "U+0049"_s, 73, false, false, false, modifiers, WallTime::now());
        break;
    case SyntheticEditingCommandType::ToggleUnderline:
        keyEvent = PlatformKeyboardEvent(PlatformEvent::Type::KeyDown, "u"_s, "u"_s,
        "u"_s, "KeyU"_s,
        "U+0055"_s, 85, false, false, false, modifiers, WallTime::now());
        break;
    default:
        break;
    }

    keyEvent.setIsSyntheticEvent();
    
    PlatformKeyboardEvent::setCurrentModifierState(modifiers);
    
    frame->eventHandler().keyEvent(keyEvent);
}

void WebPage::handleSyntheticClick(Node& nodeRespondingToClick, const WebCore::FloatPoint& location, OptionSet<WebEventModifier> modifiers, WebCore::PointerID pointerId)
{
    auto& respondingDocument = nodeRespondingToClick.document();
    auto isFirstSyntheticClickOnPage = !m_hasHandledSyntheticClick;
    m_hasHandledSyntheticClick = true;

    if (!respondingDocument.settings().contentChangeObserverEnabled() || respondingDocument.quirks().shouldDisableContentChangeObserver() || respondingDocument.quirks().shouldIgnoreContentObservationForSyntheticClick(isFirstSyntheticClickOnPage)) {
        completeSyntheticClick(nodeRespondingToClick, location, modifiers, WebCore::OneFingerTap, pointerId);
        return;
    }

    auto& contentChangeObserver = respondingDocument.contentChangeObserver();
    auto targetNodeWentFromHiddenToVisible = contentChangeObserver.hiddenTouchTarget() == &nodeRespondingToClick && ContentChangeObserver::isConsideredVisible(nodeRespondingToClick);
    {
        LOG_WITH_STREAM(ContentObservation, stream << "handleSyntheticClick: node(" << &nodeRespondingToClick << ") " << location);
        ContentChangeObserver::MouseMovedScope observingScope(respondingDocument);
        auto& mainFrame = m_page->mainFrame();
        dispatchSyntheticMouseMove(mainFrame, location, modifiers, pointerId);
        mainFrame.document()->updateStyleIfNeeded();
        if (m_isClosed)
            return;
    }

    if (targetNodeWentFromHiddenToVisible) {
        LOG(ContentObservation, "handleSyntheticClick: target node was hidden and now is visible -> hover.");
        return;
    }

    auto nodeTriggersFastPath = [&](auto& targetNode) {
        if (!is<Element>(targetNode))
            return false;
        if (is<HTMLFormControlElement>(targetNode))
            return true;
        if (targetNode.document().quirks().shouldIgnoreAriaForFastPathContentObservationCheck())
            return false;
        auto ariaRole = AccessibilityObject::ariaRoleToWebCoreRole(downcast<Element>(targetNode).getAttribute(HTMLNames::roleAttr));
        return AccessibilityObject::isARIAControl(ariaRole);
    };
    auto targetNodeTriggersFastPath = nodeTriggersFastPath(nodeRespondingToClick);

    auto observedContentChange = contentChangeObserver.observedContentChange();
    auto continueContentObservation = !(observedContentChange == WKContentVisibilityChange || targetNodeTriggersFastPath);
    if (continueContentObservation) {
        // Wait for callback to completePendingSyntheticClickForContentChangeObserver() to decide whether to send the click event.
        const Seconds observationDuration = 32_ms;
        contentChangeObserver.startContentObservationForDuration(observationDuration);
        LOG(ContentObservation, "handleSyntheticClick: Can't decide it yet -> wait.");
        m_pendingSyntheticClickNode = &nodeRespondingToClick;
        m_pendingSyntheticClickLocation = location;
        m_pendingSyntheticClickModifiers = modifiers;
        m_pendingSyntheticClickPointerId = pointerId;
        return;
    }
    contentChangeObserver.stopContentObservation();
    callOnMainRunLoop([protectedThis = Ref { *this }, targetNode = Ref<Node>(nodeRespondingToClick), location, modifiers, observedContentChange, pointerId] {
        if (protectedThis->m_isClosed || !protectedThis->corePage())
            return;

        auto shouldStayAtHoverState = observedContentChange == WKContentVisibilityChange;
        if (shouldStayAtHoverState) {
            // The move event caused new contents to appear. Don't send synthetic click event, but just ensure that the mouse is on the most recent content.
            dispatchSyntheticMouseMove(protectedThis->corePage()->mainFrame(), location, modifiers, pointerId);
            LOG(ContentObservation, "handleSyntheticClick: Observed meaningful visible change -> hover.");
            return;
        }
        LOG(ContentObservation, "handleSyntheticClick: calling completeSyntheticClick -> click.");
        protectedThis->completeSyntheticClick(targetNode, location, modifiers, WebCore::OneFingerTap, pointerId);
    });
}

void WebPage::didFinishContentChangeObserving(WKContentChange observedContentChange)
{
    LOG_WITH_STREAM(ContentObservation, stream << "didFinishContentChangeObserving: pending target node(" << m_pendingSyntheticClickNode << ")");
    if (!m_pendingSyntheticClickNode)
        return;
    callOnMainRunLoop([protectedThis = Ref { *this }, targetNode = Ref<Node>(*m_pendingSyntheticClickNode), originalDocument = WeakPtr<Document, WeakPtrImplWithEventTargetData> { m_pendingSyntheticClickNode->document() }, observedContentChange, location = m_pendingSyntheticClickLocation, modifiers = m_pendingSyntheticClickModifiers, pointerId = m_pendingSyntheticClickPointerId] {
        if (protectedThis->m_isClosed || !protectedThis->corePage())
            return;
        if (!originalDocument || &targetNode->document() != originalDocument)
            return;

        // Only dispatch the click if the document didn't get changed by any timers started by the move event.
        if (observedContentChange == WKContentNoChange) {
            LOG(ContentObservation, "No chage was observed -> click.");
            protectedThis->completeSyntheticClick(targetNode, location, modifiers, WebCore::OneFingerTap, pointerId);
            return;
        }
        // Ensure that the mouse is on the most recent content.
        LOG(ContentObservation, "Observed meaningful visible change -> hover.");
        dispatchSyntheticMouseMove(protectedThis->corePage()->mainFrame(), location, modifiers, pointerId);
    });
    m_pendingSyntheticClickNode = nullptr;
    m_pendingSyntheticClickLocation = { };
    m_pendingSyntheticClickModifiers = { };
    m_pendingSyntheticClickPointerId = 0;
}

void WebPage::completeSyntheticClick(Node& nodeRespondingToClick, const WebCore::FloatPoint& location, OptionSet<WebEventModifier> modifiers, SyntheticClickType syntheticClickType, WebCore::PointerID pointerId)
{
    SetForScope completeSyntheticClickScope { m_completingSyntheticClick, true };
    IntPoint roundedAdjustedPoint = roundedIntPoint(location);
    Frame& mainframe = m_page->mainFrame();

    RefPtr<Frame> oldFocusedFrame = CheckedRef(m_page->focusController())->focusedFrame();
    RefPtr<Element> oldFocusedElement = oldFocusedFrame ? oldFocusedFrame->document()->focusedElement() : nullptr;

    SetForScope userIsInteractingChange { m_userIsInteracting, true };

    m_lastInteractionLocation = roundedAdjustedPoint;

    // FIXME: Pass caps lock state.
    auto platformModifiers = platform(modifiers);

    bool handledPress = mainframe.eventHandler().handleMousePressEvent(PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, LeftButton, PlatformEvent::Type::MousePressed, 1, platformModifiers, WallTime::now(), WebCore::ForceAtClick, syntheticClickType, pointerId));
    if (m_isClosed)
        return;

    if (auto selectionChangedHandler = std::exchange(m_selectionChangedHandler, {}))
        selectionChangedHandler();
    else if (!handledPress)
        clearSelectionAfterTapIfNeeded();

    bool handledRelease = mainframe.eventHandler().handleMouseReleaseEvent(PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, LeftButton, PlatformEvent::Type::MouseReleased, 1, platformModifiers, WallTime::now(), WebCore::ForceAtClick, syntheticClickType, pointerId));
    if (m_isClosed)
        return;

    RefPtr<Frame> newFocusedFrame = CheckedRef(m_page->focusController())->focusedFrame();
    RefPtr<Element> newFocusedElement = newFocusedFrame ? newFocusedFrame->document()->focusedElement() : nullptr;

    if (nodeRespondingToClick.document().settings().contentChangeObserverEnabled() && !nodeRespondingToClick.document().quirks().shouldDisableContentChangeObserver()) {
        auto& document = nodeRespondingToClick.document();
        // Dispatch mouseOut to dismiss tooltip content when tapping on the control bar buttons (cc, settings).
        if (document.quirks().needsYouTubeMouseOutQuirk()) {
            if (auto* frame = document.frame())
                frame->eventHandler().dispatchSyntheticMouseOut(PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, LeftButton, PlatformEvent::Type::NoType, 0, platformModifiers, WallTime::now(), 0, WebCore::NoTap, pointerId));
        }
    }

    if (m_isClosed)
        return;

    if ((!handledPress && !handledRelease) || !nodeRespondingToClick.isElementNode())
        send(Messages::WebPageProxy::DidNotHandleTapAsClick(roundedIntPoint(location)));

    send(Messages::WebPageProxy::DidCompleteSyntheticClick());
}

void WebPage::attemptSyntheticClick(const IntPoint& point, OptionSet<WebEventModifier> modifiers, TransactionID lastLayerTreeTransactionId)
{
    FloatPoint adjustedPoint;
    Node* nodeRespondingToClick = m_page->mainFrame().nodeRespondingToClickEvents(point, adjustedPoint);
    Frame* frameRespondingToClick = nodeRespondingToClick ? nodeRespondingToClick->document().frame() : nullptr;
    IntPoint adjustedIntPoint = roundedIntPoint(adjustedPoint);

    if (!frameRespondingToClick || lastLayerTreeTransactionId < WebFrame::fromCoreFrame(*frameRespondingToClick)->firstLayerTreeTransactionIDAfterDidCommitLoad())
        send(Messages::WebPageProxy::DidNotHandleTapAsClick(adjustedIntPoint));
    else if (m_interactionNode == nodeRespondingToClick)
        completeSyntheticClick(*nodeRespondingToClick, adjustedPoint, modifiers, WebCore::OneFingerTap);
    else
        handleSyntheticClick(*nodeRespondingToClick, adjustedPoint, modifiers);
}

void WebPage::handleDoubleTapForDoubleClickAtPoint(const IntPoint& point, OptionSet<WebEventModifier> modifiers, TransactionID lastLayerTreeTransactionId)
{
    FloatPoint adjustedPoint;
    auto* nodeRespondingToDoubleClick = m_page->mainFrame().nodeRespondingToDoubleClickEvent(point, adjustedPoint);
    if (!nodeRespondingToDoubleClick)
        return;

    auto* frameRespondingToDoubleClick = nodeRespondingToDoubleClick->document().frame();
    if (!frameRespondingToDoubleClick || lastLayerTreeTransactionId < WebFrame::fromCoreFrame(*frameRespondingToDoubleClick)->firstLayerTreeTransactionIDAfterDidCommitLoad())
        return;

    auto platformModifiers = platform(modifiers);
    auto roundedAdjustedPoint = roundedIntPoint(adjustedPoint);
    nodeRespondingToDoubleClick->document().frame()->eventHandler().handleMousePressEvent(PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, LeftButton, PlatformEvent::Type::MousePressed, 2, platformModifiers, WallTime::now(), 0, WebCore::OneFingerTap));
    if (m_isClosed)
        return;
    nodeRespondingToDoubleClick->document().frame()->eventHandler().handleMouseReleaseEvent(PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, LeftButton, PlatformEvent::Type::MouseReleased, 2, platformModifiers, WallTime::now(), 0, WebCore::OneFingerTap));
}

void WebPage::requestFocusedElementInformation(CompletionHandler<void(const std::optional<FocusedElementInformation>&)>&& completionHandler)
{
    std::optional<FocusedElementInformation> information;
    if (m_focusedElement)
        information = focusedElementInformation();

    completionHandler(information);
}

#if ENABLE(DRAG_SUPPORT)
void WebPage::requestDragStart(const IntPoint& clientPosition, const IntPoint& globalPosition, OptionSet<WebCore::DragSourceAction> allowedActionsMask)
{
    SetForScope allowedActionsForScope(m_allowedDragSourceActions, allowedActionsMask);
    bool didStart = m_page->mainFrame().eventHandler().tryToBeginDragAtPoint(clientPosition, globalPosition);
    send(Messages::WebPageProxy::DidHandleDragStartRequest(didStart));
}

void WebPage::requestAdditionalItemsForDragSession(const IntPoint& clientPosition, const IntPoint& globalPosition, OptionSet<WebCore::DragSourceAction> allowedActionsMask)
{
    SetForScope allowedActionsForScope(m_allowedDragSourceActions, allowedActionsMask);
    // To augment the platform drag session with additional items, end the current drag session and begin a new drag session with the new drag item.
    // This process is opaque to the UI process, which still maintains the old drag item in its drag session. Similarly, this persistent drag session
    // is opaque to the web process, which only sees that the current drag has ended, and that a new one is beginning.
    PlatformMouseEvent event(clientPosition, globalPosition, LeftButton, PlatformEvent::Type::MouseMoved, 0, { }, WallTime::now(), 0, NoTap);
    m_page->dragController().dragEnded();
    m_page->mainFrame().eventHandler().dragSourceEndedAt(event, { }, MayExtendDragSession::Yes);

    bool didHandleDrag = m_page->mainFrame().eventHandler().tryToBeginDragAtPoint(clientPosition, globalPosition);
    send(Messages::WebPageProxy::DidHandleAdditionalDragItemsRequest(didHandleDrag));
}

void WebPage::insertDroppedImagePlaceholders(const Vector<IntSize>& imageSizes, CompletionHandler<void(const Vector<IntRect>&, std::optional<WebCore::TextIndicatorData>)>&& reply)
{
    m_page->dragController().insertDroppedImagePlaceholdersAtCaret(imageSizes);
    auto placeholderRects = m_page->dragController().droppedImagePlaceholders().map([&] (auto& element) {
        return rootViewBounds(element);
    });

    auto imagePlaceholderRange = m_page->dragController().droppedImagePlaceholderRange();
    if (placeholderRects.size() != imageSizes.size()) {
        RELEASE_LOG(DragAndDrop, "Failed to insert dropped image placeholders: placeholder rect count (%tu) does not match image size count (%tu).", placeholderRects.size(), imageSizes.size());
        reply({ }, std::nullopt);
        return;
    }

    if (!imagePlaceholderRange) {
        RELEASE_LOG(DragAndDrop, "Failed to insert dropped image placeholders: no image placeholder range.");
        reply({ }, std::nullopt);
        return;
    }

    std::optional<TextIndicatorData> textIndicatorData;
    constexpr OptionSet<TextIndicatorOption> textIndicatorOptions {
        TextIndicatorOption::IncludeSnapshotOfAllVisibleContentWithoutSelection,
        TextIndicatorOption::ExpandClipBeyondVisibleRect,
        TextIndicatorOption::PaintAllContent,
        TextIndicatorOption::UseSelectionRectForSizing
    };

    if (auto textIndicator = TextIndicator::createWithRange(*imagePlaceholderRange, textIndicatorOptions, TextIndicatorPresentationTransition::None, { }))
        textIndicatorData = textIndicator->data();

    reply(WTFMove(placeholderRects), WTFMove(textIndicatorData));
}

void WebPage::didConcludeDrop()
{
    m_rangeForDropSnapshot = std::nullopt;
    m_pendingImageElementsForDropSnapshot.clear();
}

void WebPage::didConcludeEditDrag()
{
    send(Messages::WebPageProxy::WillReceiveEditDragSnapshot());

    layoutIfNeeded();

    m_pendingImageElementsForDropSnapshot.clear();

    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    if (auto selectionRange = frame->selection().selection().toNormalizedRange()) {
        m_pendingImageElementsForDropSnapshot = visibleImageElementsInRangeWithNonLoadedImages(*selectionRange);
        frame->selection().setSelectedRange(makeSimpleRange(selectionRange->end), Affinity::Downstream, FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
        m_rangeForDropSnapshot = WTFMove(selectionRange);
    }

    if (m_pendingImageElementsForDropSnapshot.isEmpty())
        computeAndSendEditDragSnapshot();
}

void WebPage::didFinishLoadingImageForElement(WebCore::HTMLImageElement& element)
{
    if (!m_pendingImageElementsForDropSnapshot.remove(&element))
        return;

    bool shouldSendSnapshot = m_pendingImageElementsForDropSnapshot.isEmpty();
    m_page->dragController().finalizeDroppedImagePlaceholder(element, [protectedThis = Ref { *this }, shouldSendSnapshot] {
        if (shouldSendSnapshot)
            protectedThis->computeAndSendEditDragSnapshot();
    });
}

void WebPage::computeAndSendEditDragSnapshot()
{
    std::optional<TextIndicatorData> textIndicatorData;
    constexpr OptionSet<TextIndicatorOption> defaultTextIndicatorOptionsForEditDrag {
        TextIndicatorOption::IncludeSnapshotOfAllVisibleContentWithoutSelection,
        TextIndicatorOption::ExpandClipBeyondVisibleRect,
        TextIndicatorOption::PaintAllContent,
        TextIndicatorOption::IncludeMarginIfRangeMatchesSelection,
        TextIndicatorOption::PaintBackgrounds,
        TextIndicatorOption::ComputeEstimatedBackgroundColor,
        TextIndicatorOption::UseSelectionRectForSizing,
        TextIndicatorOption::IncludeSnapshotWithSelectionHighlight
    };
    if (auto range = std::exchange(m_rangeForDropSnapshot, std::nullopt)) {
        if (auto textIndicator = TextIndicator::createWithRange(*range, defaultTextIndicatorOptionsForEditDrag, TextIndicatorPresentationTransition::None, { }))
            textIndicatorData = textIndicator->data();
    }
    send(Messages::WebPageProxy::DidReceiveEditDragSnapshot(WTFMove(textIndicatorData)));
}

#endif

void WebPage::sendTapHighlightForNodeIfNecessary(WebKit::TapIdentifier requestID, Node* node)
{
#if ENABLE(TOUCH_EVENTS)
    if (!node)
        return;

    if (m_page->isEditable() && node == m_page->mainFrame().document()->body())
        return;

    if (is<Element>(*node)) {
        ASSERT(m_page);
        m_page->mainFrame().loader().client().prefetchDNS(downcast<Element>(*node).absoluteLinkURL().host().toString());
    }

    if (is<HTMLAreaElement>(node)) {
        node = downcast<HTMLAreaElement>(node)->imageElement();
        if (!node)
            return;
    }

    Vector<FloatQuad> quads;
    if (RenderObject *renderer = node->renderer()) {
        renderer->absoluteQuads(quads);
        auto& style = renderer->style();
        auto highlightColor = style.colorResolvingCurrentColor(style.tapHighlightColor());
        if (!node->document().frame()->isMainFrame()) {
            FrameView* view = node->document().frame()->view();
            for (auto& quad : quads)
                quad = view->contentsToRootView(quad);
        }

        RoundedRect::Radii borderRadii;
        if (is<RenderBox>(*renderer))
            borderRadii = downcast<RenderBox>(*renderer).borderRadii();

        bool nodeHasBuiltInClickHandling = is<HTMLFormControlElement>(*node) || is<HTMLAnchorElement>(*node) || is<HTMLLabelElement>(*node) || is<HTMLSummaryElement>(*node) || node->isLink();
        send(Messages::WebPageProxy::DidGetTapHighlightGeometries(requestID, highlightColor, quads, roundedIntSize(borderRadii.topLeft()), roundedIntSize(borderRadii.topRight()), roundedIntSize(borderRadii.bottomLeft()), roundedIntSize(borderRadii.bottomRight()), nodeHasBuiltInClickHandling));
    }
#else
    UNUSED_PARAM(requestID);
    UNUSED_PARAM(node);
#endif
}

void WebPage::handleTwoFingerTapAtPoint(const WebCore::IntPoint& point, OptionSet<WebKit::WebEventModifier> modifiers, WebKit::TapIdentifier requestID)
{
    FloatPoint adjustedPoint;
    Node* nodeRespondingToClick = m_page->mainFrame().nodeRespondingToClickEvents(point, adjustedPoint);
    if (!nodeRespondingToClick || !nodeRespondingToClick->renderer()) {
        send(Messages::WebPageProxy::DidNotHandleTapAsClick(roundedIntPoint(adjustedPoint)));
        return;
    }
    sendTapHighlightForNodeIfNecessary(requestID, nodeRespondingToClick);
    completeSyntheticClick(*nodeRespondingToClick, adjustedPoint, modifiers, WebCore::TwoFingerTap);
}

void WebPage::potentialTapAtPosition(WebKit::TapIdentifier requestID, const WebCore::FloatPoint& position, bool shouldRequestMagnificationInformation)
{
    m_potentialTapNode = m_page->mainFrame().nodeRespondingToClickEvents(position, m_potentialTapLocation, m_potentialTapSecurityOrigin.get());
    m_wasShowingInputViewForFocusedElementDuringLastPotentialTap = m_isShowingInputViewForFocusedElement;

    if (shouldRequestMagnificationInformation && m_potentialTapNode && m_viewGestureGeometryCollector) {
        // FIXME: Could this be combined into tap highlight?
        FloatPoint origin = position;
        FloatRect renderRect;
        bool fitEntireRect;
        double viewportMinimumScale;
        double viewportMaximumScale;

        m_viewGestureGeometryCollector->computeZoomInformationForNode(*m_potentialTapNode, origin, renderRect, fitEntireRect, viewportMinimumScale, viewportMaximumScale);

        bool nodeIsRootLevel = is<WebCore::Document>(*m_potentialTapNode) || is<WebCore::HTMLBodyElement>(*m_potentialTapNode);
        send(Messages::WebPageProxy::HandleSmartMagnificationInformationForPotentialTap(requestID, renderRect, fitEntireRect, viewportMinimumScale, viewportMaximumScale, nodeIsRootLevel));
    }

    sendTapHighlightForNodeIfNecessary(requestID, m_potentialTapNode.get());
#if ENABLE(TOUCH_EVENTS)
    if (m_potentialTapNode && !m_potentialTapNode->allowsDoubleTapGesture())
        send(Messages::WebPageProxy::DisableDoubleTapGesturesDuringTapIfNecessary(requestID));
#endif
}

void WebPage::commitPotentialTap(OptionSet<WebEventModifier> modifiers, TransactionID lastLayerTreeTransactionId, WebCore::PointerID pointerId)
{
    auto invalidTargetForSingleClick = !m_potentialTapNode;
    if (!invalidTargetForSingleClick) {
        bool targetRenders = m_potentialTapNode->renderer();
        if (!targetRenders && is<Element>(m_potentialTapNode))
            targetRenders = downcast<Element>(*m_potentialTapNode).renderOrDisplayContentsStyle();
        if (!targetRenders && is<ShadowRoot>(m_potentialTapNode))
            targetRenders = downcast<ShadowRoot>(*m_potentialTapNode).host()->renderOrDisplayContentsStyle();
        invalidTargetForSingleClick = !targetRenders && !is<HTMLAreaElement>(m_potentialTapNode);
    }
    if (invalidTargetForSingleClick) {
        commitPotentialTapFailed();
        return;
    }

    FloatPoint adjustedPoint;
    Node* nodeRespondingToClick = m_page->mainFrame().nodeRespondingToClickEvents(m_potentialTapLocation, adjustedPoint, m_potentialTapSecurityOrigin.get());
    Frame* frameRespondingToClick = nodeRespondingToClick ? nodeRespondingToClick->document().frame() : nullptr;

    if (!frameRespondingToClick || lastLayerTreeTransactionId < WebFrame::fromCoreFrame(*frameRespondingToClick)->firstLayerTreeTransactionIDAfterDidCommitLoad()) {
        commitPotentialTapFailed();
        return;
    }

    if (m_potentialTapNode == nodeRespondingToClick)
        handleSyntheticClick(*nodeRespondingToClick, adjustedPoint, modifiers, pointerId);
    else
        commitPotentialTapFailed();

    m_potentialTapNode = nullptr;
    m_potentialTapLocation = FloatPoint();
    m_potentialTapSecurityOrigin = nullptr;
}

void WebPage::commitPotentialTapFailed()
{
    if (auto selectionChangedHandler = std::exchange(m_selectionChangedHandler, {}))
        selectionChangedHandler();

    ContentChangeObserver::didCancelPotentialTap(m_page->mainFrame());
    clearSelectionAfterTapIfNeeded();

    send(Messages::WebPageProxy::CommitPotentialTapFailed());
    send(Messages::WebPageProxy::DidNotHandleTapAsClick(roundedIntPoint(m_potentialTapLocation)));
}

void WebPage::clearSelectionAfterTapIfNeeded()
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    if (frame->selection().selection().isContentEditable())
        return;

    clearSelection();
}

void WebPage::cancelPotentialTap()
{
    ContentChangeObserver::didCancelPotentialTap(m_page->mainFrame());
    cancelPotentialTapInFrame(m_mainFrame);
}

void WebPage::cancelPotentialTapInFrame(WebFrame& frame)
{
    if (auto selectionChangedHandler = std::exchange(m_selectionChangedHandler, {}))
        selectionChangedHandler();

    if (m_potentialTapNode) {
        auto* potentialTapFrame = m_potentialTapNode->document().frame();
        if (potentialTapFrame && !potentialTapFrame->tree().isDescendantOf(frame.coreFrame()))
            return;
    }

    m_potentialTapNode = nullptr;
    m_potentialTapLocation = FloatPoint();
    m_potentialTapSecurityOrigin = nullptr;
}

void WebPage::didRecognizeLongPress()
{
    ContentChangeObserver::didRecognizeLongPress(m_page->mainFrame());
}

void WebPage::tapHighlightAtPosition(WebKit::TapIdentifier requestID, const FloatPoint& position)
{
    Frame& mainframe = m_page->mainFrame();
    FloatPoint adjustedPoint;
    sendTapHighlightForNodeIfNecessary(requestID, mainframe.nodeRespondingToClickEvents(position, adjustedPoint));
}

void WebPage::inspectorNodeSearchMovedToPosition(const FloatPoint& position)
{
    IntPoint adjustedPoint = roundedIntPoint(position);
    Frame& mainframe = m_page->mainFrame();

    mainframe.eventHandler().mouseMoved(PlatformMouseEvent(adjustedPoint, adjustedPoint, NoButton, PlatformEvent::Type::MouseMoved, 0, { }, { }, 0, WebCore::NoTap));
    mainframe.document()->updateStyleIfNeeded();
}

void WebPage::inspectorNodeSearchEndedAtPosition(const FloatPoint& position)
{
    if (Node* node = m_page->mainFrame().deepestNodeAtLocation(position))
        node->inspect();
}

void WebPage::updateInputContextAfterBlurringAndRefocusingElementIfNeeded(Element& element)
{
    if (m_recentlyBlurredElement != &element || !m_isShowingInputViewForFocusedElement)
        return;

    m_hasPendingInputContextUpdateAfterBlurringAndRefocusingElement = true;
    callOnMainRunLoop([this, protectedThis = Ref { *this }] {
        if (m_hasPendingInputContextUpdateAfterBlurringAndRefocusingElement)
            send(Messages::WebPageProxy::UpdateInputContextAfterBlurringAndRefocusingElement());
        m_hasPendingInputContextUpdateAfterBlurringAndRefocusingElement = false;
    });
}

void WebPage::blurFocusedElement()
{
    if (!m_focusedElement)
        return;

    m_focusedElement->blur();
}

void WebPage::setFocusedElementValue(const WebCore::ElementContext& context, const String& value)
{
    RefPtr<Element> element = elementForContext(context);
    // FIXME: should also handle the case of HTMLSelectElement.
    if (is<HTMLInputElement>(element))
        downcast<HTMLInputElement>(*element).setValue(value, DispatchInputAndChangeEvent);
}

void WebPage::setFocusedElementSelectedIndex(const WebCore::ElementContext& context, uint32_t index, bool allowMultipleSelection)
{
    RefPtr<Element> element = elementForContext(context);
    if (is<HTMLSelectElement>(element))
        downcast<HTMLSelectElement>(*element).optionSelectedByUser(index, true, allowMultipleSelection);
}

void WebPage::showInspectorHighlight(const WebCore::InspectorOverlay::Highlight& highlight)
{
    send(Messages::WebPageProxy::ShowInspectorHighlight(highlight));
}

void WebPage::hideInspectorHighlight()
{
    send(Messages::WebPageProxy::HideInspectorHighlight());
}

void WebPage::showInspectorIndication()
{
    send(Messages::WebPageProxy::ShowInspectorIndication());
}

void WebPage::hideInspectorIndication()
{
    send(Messages::WebPageProxy::HideInspectorIndication());
}

void WebPage::enableInspectorNodeSearch()
{
    send(Messages::WebPageProxy::EnableInspectorNodeSearch());
}

void WebPage::disableInspectorNodeSearch()
{
    send(Messages::WebPageProxy::DisableInspectorNodeSearch());
}

void WebPage::setForceAlwaysUserScalable(bool userScalable)
{
    m_forceAlwaysUserScalable = userScalable;
    m_viewportConfiguration.setForceAlwaysUserScalable(userScalable);
}

static IntRect elementBoundsInFrame(const Frame& frame, const Element& focusedElement)
{
    frame.document()->updateLayoutIgnorePendingStylesheets();
    
    if (focusedElement.hasTagName(HTMLNames::textareaTag) || focusedElement.hasTagName(HTMLNames::inputTag) || focusedElement.hasTagName(HTMLNames::selectTag))
        return WebPage::absoluteInteractionBounds(focusedElement);

    if (auto* rootEditableElement = focusedElement.rootEditableElement())
        return WebPage::absoluteInteractionBounds(*rootEditableElement);

    return { };
}

static IntPoint constrainPoint(const IntPoint& point, const Frame& frame, const Element& focusedElement)
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

static bool insideImageOverlay(const VisiblePosition& position)
{
    RefPtr container = position.deepEquivalent().containerNode();
    return container && ImageOverlay::isInsideOverlay(*container);
}

static std::optional<SimpleRange> expandForImageOverlay(const SimpleRange& range)
{
    VisiblePosition expandedStart(makeContainerOffsetPosition(&range.startContainer(), range.startOffset()));
    VisiblePosition expandedEnd(makeContainerOffsetPosition(&range.endContainer(), range.endOffset()));

    for (auto start = expandedStart; insideImageOverlay(start); start = start.previous()) {
        if (RefPtr container = start.deepEquivalent().containerNode(); is<Text>(container)) {
            expandedStart = firstPositionInNode(container.get()).downstream();
            break;
        }
    }

    for (auto end = expandedEnd; insideImageOverlay(end); end = end.next()) {
        if (RefPtr container = end.deepEquivalent().containerNode(); is<Text>(container)) {
            expandedEnd = lastPositionInNode(container.get()).upstream();
            break;
        }
    }

    return makeSimpleRange({ expandedStart, expandedEnd });
}

void WebPage::selectWithGesture(const IntPoint& point, GestureType gestureType, GestureRecognizerState gestureState, bool isInteractingWithFocusedElement, CompletionHandler<void(const WebCore::IntPoint&, GestureType, GestureRecognizerState, OptionSet<SelectionFlags>)>&& completionHandler)
{
    if (static_cast<GestureRecognizerState>(gestureState) == GestureRecognizerState::Began)
        setFocusedFrameBeforeSelectingTextAtLocation(point);

    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    VisiblePosition position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithFocusedElement);

    if (position.isNull()) {
        completionHandler(point, gestureType, gestureState, { });
        return;
    }
    std::optional<SimpleRange> range;
    OptionSet<SelectionFlags> flags;
    GestureRecognizerState wkGestureState = static_cast<GestureRecognizerState>(gestureState);
    switch (static_cast<GestureType>(gestureType)) {
    case GestureType::PhraseBoundary: {
        if (!frame->editor().hasComposition())
            break;
        auto markedRange = frame->editor().compositionRange();
        auto startPosition = VisiblePosition { makeDeprecatedLegacyPosition(markedRange->start) };
        position = std::clamp(position, startPosition, VisiblePosition { makeDeprecatedLegacyPosition(markedRange->end) });
        if (wkGestureState != GestureRecognizerState::Began)
            flags = distanceBetweenPositions(startPosition, frame->selection().selection().start()) != distanceBetweenPositions(startPosition, position) ? PhraseBoundaryChanged : OptionSet<SelectionFlags> { };
        else
            flags = PhraseBoundaryChanged;
        range = makeSimpleRange(position);
        break;
    }

    case GestureType::OneFingerTap: {
        auto [adjustedPosition, withinWordBoundary] = wordBoundaryForPositionWithoutCrossingLine(position);
        if (withinWordBoundary == WithinWordBoundary::Yes)
            flags = WordIsNearTap;
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
        frame->selection().setSelectedRange(range, position.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);

    completionHandler(point, gestureType, gestureState, flags);
}

static std::pair<std::optional<SimpleRange>, SelectionWasFlipped> rangeForPointInRootViewCoordinates(Frame& frame, const IntPoint& pointInRootViewCoordinates, bool baseIsStart, bool selectionFlippingEnabled)
{
    VisibleSelection existingSelection = frame.selection().selection();
    VisiblePosition selectionStart = existingSelection.visibleStart();
    VisiblePosition selectionEnd = existingSelection.visibleEnd();

    auto pointInDocument = frame.view()->rootViewToContents(pointInRootViewCoordinates.constrainedWithin(frame.mainFrame().view()->unobscuredContentRect()));

    if (!selectionFlippingEnabled) {
        auto node = selectionStart.deepEquivalent().containerNode();
        if (node && node->renderStyle() && node->renderStyle()->isVerticalWritingMode()) {
            if (baseIsStart) {
                int startX = selectionStart.absoluteCaretBounds().center().x();
                if (pointInDocument.x() > startX)
                    pointInDocument.setX(startX);
            } else {
                int endX = selectionEnd.absoluteCaretBounds().center().x();
                if (pointInDocument.x() < endX)
                    pointInDocument.setX(endX);
            }
        } else {
            if (baseIsStart) {
                int startY = selectionStart.absoluteCaretBounds().center().y();
                if (pointInDocument.y() < startY)
                    pointInDocument.setY(startY);
            } else {
                int endY = selectionEnd.absoluteCaretBounds().center().y();
                if (pointInDocument.y() > endY)
                    pointInDocument.setY(endY);
            }
        }
    }

    auto hitTest = frame.eventHandler().hitTestResultAtPoint(pointInDocument, {
        HitTestRequest::Type::ReadOnly,
        HitTestRequest::Type::Active,
        HitTestRequest::Type::AllowVisibleChildFrameContentOnly,
    });

    RefPtr targetNode = hitTest.targetNode();
    if (targetNode && !HTMLElement::shouldExtendSelectionToTargetNode(*targetNode, existingSelection))
        return { std::nullopt, SelectionWasFlipped::No };

    VisiblePosition result;
    std::optional<SimpleRange> range;
    SelectionWasFlipped selectionFlipped = SelectionWasFlipped::No;

    if (targetNode)
        result = frame.eventHandler().selectionExtentRespectingEditingBoundary(frame.selection().selection(), hitTest.localPoint(), targetNode.get()).deepEquivalent();
    else
        result = frame.visiblePositionForPoint(pointInDocument).deepEquivalent();
    
    if (baseIsStart) {
        bool flipSelection = result < selectionStart;

        if ((flipSelection && !selectionFlippingEnabled) || result == selectionStart)
            result = selectionStart.next();
        else if (RefPtr containerNode = selectionStart.deepEquivalent().containerNode(); containerNode && targetNode && &containerNode->treeScope() != &targetNode->treeScope())
            result = VisibleSelection::adjustPositionForEnd(result.deepEquivalent(), containerNode.get());
        
        if (selectionFlippingEnabled && flipSelection) {
            range = makeSimpleRange(result, selectionStart);
            selectionFlipped = SelectionWasFlipped::Yes;
        } else
            range = makeSimpleRange(selectionStart, result);
    } else {
        bool flipSelection = selectionEnd < result;
        
        if ((flipSelection && !selectionFlippingEnabled) || result == selectionEnd)
            result = selectionEnd.previous();
        else if (RefPtr containerNode = selectionEnd.deepEquivalent().containerNode(); containerNode && targetNode && &containerNode->treeScope() != &targetNode->treeScope())
            result = VisibleSelection::adjustPositionForStart(result.deepEquivalent(), containerNode.get());

        if (selectionFlippingEnabled && flipSelection) {
            range = makeSimpleRange(selectionEnd, result);
            selectionFlipped = SelectionWasFlipped::Yes;
        } else
            range = makeSimpleRange(result, selectionEnd);
    }
    
    if (range && ImageOverlay::isInsideOverlay(*range))
        return { expandForImageOverlay(*range), SelectionWasFlipped::No };

    return { range, selectionFlipped };
}

static std::optional<SimpleRange> rangeAtWordBoundaryForPosition(Frame* frame, const VisiblePosition& position, bool baseIsStart)
{
    SelectionDirection sameDirection = baseIsStart ? SelectionDirection::Forward : SelectionDirection::Backward;
    SelectionDirection oppositeDirection = baseIsStart ? SelectionDirection::Backward : SelectionDirection::Forward;
    VisiblePosition base = baseIsStart ? frame->selection().selection().visibleStart() : frame->selection().selection().visibleEnd();
    VisiblePosition extent = baseIsStart ? frame->selection().selection().visibleEnd() : frame->selection().selection().visibleStart();
    VisiblePosition initialExtent = position;

    if (atBoundaryOfGranularity(extent, TextGranularity::WordGranularity, sameDirection)) {
        // This is a word boundary. Leave selection where it is.
        return std::nullopt;
    }

    if (atBoundaryOfGranularity(extent, TextGranularity::WordGranularity, oppositeDirection)) {
        // This is a word boundary in the wrong direction. Nudge the selection to a character before proceeding.
        extent = baseIsStart ? extent.previous() : extent.next();
    }

    // Extend to the boundary of the word.

    VisiblePosition wordBoundary = positionOfNextBoundaryOfGranularity(extent, TextGranularity::WordGranularity, sameDirection);
    if (wordBoundary.isNotNull()
        && atBoundaryOfGranularity(wordBoundary, TextGranularity::WordGranularity, sameDirection)
        && initialExtent != wordBoundary) {
        extent = wordBoundary;
        if (!(base < extent))
            std::swap(base, extent);
        return makeSimpleRange(base, extent);
    }
    // Conversely, if the initial extent equals the current word boundary, then
    // run the rest of this function to see if the selection should extend
    // the other direction to the other word.

    // If this is where the extent was initially, then iterate in the other direction in the document until we hit the next word.
    while (extent.isNotNull()
        && !atBoundaryOfGranularity(extent, TextGranularity::WordGranularity, sameDirection)
        && extent != base
        && !atBoundaryOfGranularity(extent, TextGranularity::LineGranularity, sameDirection)
        && !atBoundaryOfGranularity(extent, TextGranularity::LineGranularity, oppositeDirection)) {
        extent = baseIsStart ? extent.next() : extent.previous();
    }

    // Don't let the smart extension make the extent equal the base.
    // Expand out to word boundary.
    if (extent.isNull() || extent == base)
        extent = wordBoundary;
    if (extent.isNull())
        return std::nullopt;

    if (!(base < extent))
        std::swap(base, extent);
    return makeSimpleRange(base, extent);
}

IntRect WebPage::rootViewBounds(const Node& node)
{
    RefPtr frame = node.document().frame();
    if (!frame)
        return { };

    RefPtr view = frame->view();
    if (!view)
        return { };

    auto* renderer = node.renderer();
    if (!renderer)
        return { };

    return view->contentsToRootView(renderer->absoluteBoundingBoxRect());
}

IntRect WebPage::absoluteInteractionBounds(const Node& node)
{
    RefPtr frame = node.document().frame();
    if (!frame)
        return { };

    RefPtr view = frame->view();
    if (!view)
        return { };

    auto* renderer = node.renderer();
    if (!renderer)
        return { };

    if (is<RenderBox>(*renderer)) {
        auto& box = downcast<RenderBox>(*renderer);

        FloatRect rect;
        // FIXME: want borders or not?
        if (box.style().isOverflowVisible())
            rect = box.layoutOverflowRect();
        else
            rect = box.clientBoxRect();
        return box.localToAbsoluteQuad(rect).enclosingBoundingBox();
    }

    auto& style = renderer->style();
    FloatRect boundingBox = renderer->absoluteBoundingBoxRect(true /* use transforms*/);
    // This is wrong. It's subtracting borders after converting to absolute coords on something that probably doesn't represent a rectangular element.
    boundingBox.move(style.borderLeftWidth(), style.borderTopWidth());
    boundingBox.setWidth(boundingBox.width() - style.borderLeftWidth() - style.borderRightWidth());
    boundingBox.setHeight(boundingBox.height() - style.borderBottomWidth() - style.borderTopWidth());
    return enclosingIntRect(boundingBox);
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

void WebPage::clearSelection()
{
    m_startingGestureRange = std::nullopt;
    CheckedRef(m_page->focusController())->focusedOrMainFrame().selection().clear();
}

void WebPage::dispatchSyntheticMouseEventsForSelectionGesture(SelectionTouch touch, const IntPoint& point)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    if (!frame->selection().selection().isContentEditable())
        return;

    IntRect focusedElementRect;
    if (m_focusedElement)
        focusedElementRect = rootViewInteractionBounds(*m_focusedElement);

    if (focusedElementRect.isEmpty())
        return;

    auto adjustedPoint = point.constrainedBetween(focusedElementRect.minXMinYCorner(), focusedElementRect.maxXMaxYCorner());
    auto& eventHandler = m_page->mainFrame().eventHandler();
    switch (touch) {
    case SelectionTouch::Started:
        eventHandler.handleMousePressEvent({ adjustedPoint, adjustedPoint, LeftButton, PlatformEvent::Type::MousePressed, 1, { }, WallTime::now(), WebCore::ForceAtClick, NoTap });
        break;
    case SelectionTouch::Moved:
        eventHandler.dispatchSyntheticMouseMove({ adjustedPoint, adjustedPoint, LeftButton, PlatformEvent::Type::MouseMoved, 0, { }, WallTime::now(), WebCore::ForceAtClick, NoTap });
        break;
    case SelectionTouch::Ended:
    case SelectionTouch::EndedMovingForward:
    case SelectionTouch::EndedMovingBackward:
    case SelectionTouch::EndedNotMoving:
        eventHandler.handleMouseReleaseEvent({ adjustedPoint, adjustedPoint, LeftButton, PlatformEvent::Type::MouseReleased, 1, { }, WallTime::now(), WebCore::ForceAtClick, NoTap });
        break;
    }
}

void WebPage::updateSelectionWithTouches(const IntPoint& point, SelectionTouch selectionTouch, bool baseIsStart, CompletionHandler<void(const WebCore::IntPoint&, SelectionTouch, OptionSet<SelectionFlags>)>&& completionHandler)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    IntPoint pointInDocument = RefPtr(frame->view())->rootViewToContents(point);
    VisiblePosition position = frame->visiblePositionForPoint(pointInDocument);
    if (position.isNull())
        return completionHandler(point, selectionTouch, { });

    if (shouldDispatchSyntheticMouseEventsWhenModifyingSelection())
        dispatchSyntheticMouseEventsForSelectionGesture(selectionTouch, point);

    std::optional<SimpleRange> range;
    OptionSet<SelectionFlags> flags;
    SelectionWasFlipped selectionFlipped = SelectionWasFlipped::No;

    switch (selectionTouch) {
    case SelectionTouch::Started:
    case SelectionTouch::EndedNotMoving:
        break;

    case SelectionTouch::Ended:
        if (frame->selection().selection().isContentEditable())
            range = makeSimpleRange(closestWordBoundaryForPosition(position));
        else
            std::tie(range, selectionFlipped) = rangeForPointInRootViewCoordinates(frame, point, baseIsStart, selectionFlippingEnabled());
        break;

    case SelectionTouch::EndedMovingForward:
    case SelectionTouch::EndedMovingBackward:
        range = rangeAtWordBoundaryForPosition(frame.ptr(), position, baseIsStart);
        break;

    case SelectionTouch::Moved:
        std::tie(range, selectionFlipped) = rangeForPointInRootViewCoordinates(frame, point, baseIsStart, selectionFlippingEnabled());
        break;
    }

    if (range)
        frame->selection().setSelectedRange(range, position.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    
    if (selectionFlipped == SelectionWasFlipped::Yes)
        flags = SelectionFlipped;

    completionHandler(point, selectionTouch, flags);
}

void WebPage::selectWithTwoTouches(const WebCore::IntPoint& from, const WebCore::IntPoint& to, GestureType gestureType, GestureRecognizerState gestureState, CompletionHandler<void(const WebCore::IntPoint&, GestureType, GestureRecognizerState, OptionSet<SelectionFlags>)>&& completionHandler)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    RefPtr view = frame->view();
    auto fromPosition = frame->visiblePositionForPoint(view->rootViewToContents(from));
    auto toPosition = frame->visiblePositionForPoint(view->rootViewToContents(to));
    if (auto range = makeSimpleRange(fromPosition, toPosition)) {
        if (!(fromPosition < toPosition))
            std::swap(range->start, range->end);
        frame->selection().setSelectedRange(range, fromPosition.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    }

    // We can use the same callback for the gestures with one point.
    completionHandler(from, gestureType, gestureState, { });
}

void WebPage::extendSelectionForReplacement(CompletionHandler<void()>&& completion)
{
    auto callCompletionHandlerOnExit = makeScopeExit(WTFMove(completion));

    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    RefPtr document = frame->document();
    if (!document)
        return;

    auto selectedRange = frame->selection().selection().range();
    if (!selectedRange || !selectedRange->collapsed())
        return;

    VisiblePosition position = frame->selection().selection().start();
    RefPtr container = position.deepEquivalent().containerNode();
    if (!container)
        return;

    auto markerRanges = document->markers().markersFor(*container, { DocumentMarker::DictationAlternatives }).map([&](auto* marker) {
        return makeSimpleRange(*container, *marker);
    });

    std::optional<SimpleRange> rangeToSelect;
    for (auto& markerRange : markerRanges) {
        if (contains(makeVisiblePositionRange(markerRange), position)) {
            // In practice, dictation markers should only span a single text node, so it's sufficient to
            // grab the first matching range (instead of taking the union of all intersecting ranges).
            rangeToSelect = markerRange;
            break;
        }
    }

    if (!rangeToSelect)
        rangeToSelect = wordRangeFromPosition(position);

    if (!rangeToSelect)
        return;

    setSelectedRangeDispatchingSyntheticMouseEventsIfNeeded(*rangeToSelect, position.affinity());
}

void WebPage::extendSelection(WebCore::TextGranularity granularity, CompletionHandler<void()>&& completionHandler)
{
    auto callCompletionHandlerOnExit = makeScopeExit(WTFMove(completionHandler));

    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    // For the moment we handle only TextGranularity::WordGranularity.
    if (granularity != TextGranularity::WordGranularity || !frame->selection().isCaret())
        return;

    VisiblePosition position = frame->selection().selection().start();
    auto wordRange = wordRangeFromPosition(position);
    if (!wordRange)
        return;

    setSelectedRangeDispatchingSyntheticMouseEventsIfNeeded(*wordRange, position.affinity());
}

void WebPage::setSelectedRangeDispatchingSyntheticMouseEventsIfNeeded(const SimpleRange& range, Affinity affinity)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    IntPoint endLocationForSyntheticMouseEvents;
    bool shouldDispatchMouseEvents = shouldDispatchSyntheticMouseEventsWhenModifyingSelection();
    if (shouldDispatchMouseEvents) {
        RefPtr view = frame->view();
        auto startLocationForSyntheticMouseEvents = view->contentsToRootView(VisiblePosition(makeDeprecatedLegacyPosition(range.start)).absoluteCaretBounds()).center();
        endLocationForSyntheticMouseEvents = view->contentsToRootView(VisiblePosition(makeDeprecatedLegacyPosition(range.end)).absoluteCaretBounds()).center();
        dispatchSyntheticMouseEventsForSelectionGesture(SelectionTouch::Started, startLocationForSyntheticMouseEvents);
        dispatchSyntheticMouseEventsForSelectionGesture(SelectionTouch::Moved, endLocationForSyntheticMouseEvents);
    }

    frame->selection().setSelectedRange(range, affinity, WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);

    if (shouldDispatchMouseEvents)
        dispatchSyntheticMouseEventsForSelectionGesture(SelectionTouch::Ended, endLocationForSyntheticMouseEvents);
}

void WebPage::platformDidSelectAll()
{
    if (!shouldDispatchSyntheticMouseEventsWhenModifyingSelection())
        return;

    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    RefPtr view = frame->view();
    auto startCaretRect = view->contentsToRootView(VisiblePosition(frame->selection().selection().start()).absoluteCaretBounds());
    auto endCaretRect = view->contentsToRootView(VisiblePosition(frame->selection().selection().end()).absoluteCaretBounds());
    dispatchSyntheticMouseEventsForSelectionGesture(SelectionTouch::Started, startCaretRect.center());
    dispatchSyntheticMouseEventsForSelectionGesture(SelectionTouch::Moved, endCaretRect.center());
    dispatchSyntheticMouseEventsForSelectionGesture(SelectionTouch::Ended, endCaretRect.center());
}

void WebPage::selectWordBackward()
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    if (!frame->selection().isCaret())
        return;

    auto position = frame->selection().selection().visibleStart();
    auto startPosition = positionOfNextBoundaryOfGranularity(position, TextGranularity::WordGranularity, SelectionDirection::Backward);
    if (startPosition.isNull() || startPosition == position)
        return;

    frame->selection().setSelectedRange(makeSimpleRange(startPosition, position), position.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
}

void WebPage::moveSelectionByOffset(int32_t offset, CompletionHandler<void()>&& completionHandler)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    
    VisiblePosition startPosition = frame->selection().selection().end();
    if (startPosition.isNull())
        return;
    SelectionDirection direction = offset < 0 ? SelectionDirection::Backward : SelectionDirection::Forward;
    VisiblePosition position = startPosition;
    for (int i = 0; i < abs(offset); ++i) {
        position = positionOfNextBoundaryOfGranularity(position, TextGranularity::CharacterGranularity, direction);
        if (position.isNull())
            break;
    }
    if (position.isNotNull() && startPosition != position)
        frame->selection().setSelectedRange(makeSimpleRange(position), position.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    completionHandler();
}
    
void WebPage::startAutoscrollAtPosition(const WebCore::FloatPoint& positionInWindow)
{
    if (m_focusedElement && m_focusedElement->renderer()) {
        m_page->mainFrame().eventHandler().startSelectionAutoscroll(m_focusedElement->renderer(), positionInWindow);
        return;
    }
    
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    auto& selection = frame->selection().selection();
    if (!selection.isRange())
        return;
    auto range = selection.toNormalizedRange();
    if (!range)
        return;
    auto* renderer = range->start.container->renderer();
    if (!renderer)
        return;

    Ref(m_page->mainFrame())->eventHandler().startSelectionAutoscroll(renderer, positionInWindow);
}
    
void WebPage::cancelAutoscroll()
{
    m_page->mainFrame().eventHandler().cancelSelectionAutoscroll();
}

void WebPage::requestEvasionRectsAboveSelection(CompletionHandler<void(const Vector<FloatRect>&)>&& reply)
{
    const Vector<FloatPoint> offsetsForHitTesting { { -30, -50 }, { 30, -50 }, { -60, -35 }, { 60, -35 }, { 0, -20 } };
    auto rects = getEvasionRectsAroundSelection(offsetsForHitTesting);
    reply(WTFMove(rects));
}

void WebPage::getRectsForGranularityWithSelectionOffset(WebCore::TextGranularity granularity, int32_t offset, CompletionHandler<void(const Vector<WebCore::SelectionGeometry>&)>&& completionHandler)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();

    auto selection = m_storedSelectionForAccessibility.isNone() ? frame->selection().selection() : m_storedSelectionForAccessibility;
    auto position = visiblePositionForPositionWithOffset(selection.visibleStart(), offset);
    auto direction = offset < 0 ? SelectionDirection::Backward : SelectionDirection::Forward;
    auto range = enclosingTextUnitOfGranularity(position, granularity, direction);
    if (!range || range->collapsed()) {
        completionHandler({ });
        return;
    }

    auto selectionGeometries = RenderObject::collectSelectionGeometriesWithoutUnionInteriorLines(*range);
    RefPtr view = frame->view();
    convertContentToRootView(*view, selectionGeometries);
    completionHandler(selectionGeometries);
}

void WebPage::storeSelectionForAccessibility(bool shouldStore)
{
    if (!shouldStore)
        m_storedSelectionForAccessibility = VisibleSelection();
    else {
        Frame& frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
        m_storedSelectionForAccessibility = frame.selection().selection();
    }
}

static std::optional<SimpleRange> rangeNearPositionMatchesText(const VisiblePosition& position, const String& matchText, const VisibleSelection& selection)
{
    auto liveRange = selection.firstRange();
    if (!liveRange)
        return std::nullopt;
    SimpleRange range { *liveRange };
    auto boundaryPoint = makeBoundaryPoint(position);
    if (!boundaryPoint)
        return std::nullopt;
    return findClosestPlainText(range, matchText, { }, characterCount({ range.start, *boundaryPoint }, TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions));
}

void WebPage::getRectsAtSelectionOffsetWithText(int32_t offset, const String& text, CompletionHandler<void(const Vector<WebCore::SelectionGeometry>&)>&& completionHandler)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    auto& selection = m_storedSelectionForAccessibility.isNone() ? frame->selection().selection() : m_storedSelectionForAccessibility;
    auto startPosition = visiblePositionForPositionWithOffset(selection.visibleStart(), offset);
    auto range = makeSimpleRange(startPosition, visiblePositionForPositionWithOffset(startPosition, text.length()));
    if (!range || range->collapsed()) {
        completionHandler({ });
        return;
    }

    if (plainTextForDisplay(*range) != text) {
        // Try to search for a range which is the closest to the position within the selection range that matches the passed in text.
        if (auto wordRange = rangeNearPositionMatchesText(startPosition, text, selection)) {
            if (!wordRange->collapsed())
                range = *wordRange;
        }
    }

    auto selectionGeometries = RenderObject::collectSelectionGeometriesWithoutUnionInteriorLines(*range);
    RefPtr view = frame->view();
    convertContentToRootView(*view, selectionGeometries);
    completionHandler(selectionGeometries);
}

VisiblePosition WebPage::visiblePositionInFocusedNodeForPoint(const Frame& frame, const IntPoint& point, bool isInteractingWithFocusedElement)
{
    IntPoint adjustedPoint(frame.view()->rootViewToContents(point));
    IntPoint constrainedPoint = m_focusedElement && isInteractingWithFocusedElement ? constrainPoint(adjustedPoint, frame, *m_focusedElement) : adjustedPoint;
    return frame.visiblePositionForPoint(constrainedPoint);
}

void WebPage::selectPositionAtPoint(const WebCore::IntPoint& point, bool isInteractingWithFocusedElement, CompletionHandler<void()>&& completionHandler)
{
    SetForScope userIsInteractingChange { m_userIsInteracting, true };

    setFocusedFrameBeforeSelectingTextAtLocation(point);

    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    VisiblePosition position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithFocusedElement);
    
    if (position.isNotNull())
        frame->selection().setSelectedRange(makeSimpleRange(position), position.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    completionHandler();
}

void WebPage::selectPositionAtBoundaryWithDirection(const WebCore::IntPoint& point, WebCore::TextGranularity granularity, WebCore::SelectionDirection direction, bool isInteractingWithFocusedElement, CompletionHandler<void()>&& completionHandler)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    VisiblePosition position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithFocusedElement);

    if (position.isNotNull()) {
        position = positionOfNextBoundaryOfGranularity(position, granularity, direction);
        if (position.isNotNull())
            frame->selection().setSelectedRange(makeSimpleRange(position), Affinity::Upstream, WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    }
    completionHandler();
}

void WebPage::moveSelectionAtBoundaryWithDirection(WebCore::TextGranularity granularity, WebCore::SelectionDirection direction, CompletionHandler<void()>&& completionHandler)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();

    if (!frame->selection().selection().isNone()) {
        bool isForward = (direction == SelectionDirection::Forward || direction == SelectionDirection::Right);
        VisiblePosition position = (isForward) ? frame->selection().selection().visibleEnd() : frame->selection().selection().visibleStart();
        position = positionOfNextBoundaryOfGranularity(position, granularity, direction);
        if (position.isNotNull())
            frame->selection().setSelectedRange(makeSimpleRange(position), isForward? Affinity::Upstream : Affinity::Downstream, WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    }
    completionHandler();
}

std::optional<SimpleRange> WebPage::rangeForGranularityAtPoint(Frame& frame, const WebCore::IntPoint& point, WebCore::TextGranularity granularity, bool isInteractingWithFocusedElement)
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
        frame.selection().selectAll();
        return std::nullopt;
    case TextGranularity::LineGranularity:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    case TextGranularity::LineBoundary:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    case TextGranularity::SentenceBoundary:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    case TextGranularity::ParagraphBoundary:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    case TextGranularity::DocumentBoundary:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

static inline bool rectIsTooBigForSelection(const IntRect& blockRect, const Frame& frame)
{
    const float factor = 0.97;
    return blockRect.height() > frame.view()->unobscuredContentRect().height() * factor;
}

void WebPage::setFocusedFrameBeforeSelectingTextAtLocation(const IntPoint& point)
{
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowVisibleChildFrameContentOnly };
    auto result = Ref(m_page->mainFrame())->eventHandler().hitTestResultAtPoint(point, hitType);
    auto* hitNode = result.innerNode();
    if (hitNode && hitNode->renderer()) {
        RefPtr frame = result.innerNodeFrame();
        CheckedRef(m_page->focusController())->setFocusedFrame(frame.get());
    }
}

void WebPage::setSelectionRange(const WebCore::IntPoint& point, WebCore::TextGranularity granularity, bool isInteractingWithFocusedElement)
{
    setFocusedFrameBeforeSelectingTextAtLocation(point);

    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    auto range = rangeForGranularityAtPoint(frame, point, granularity, isInteractingWithFocusedElement);
    if (range)
        frame->selection().setSelectedRange(*range, Affinity::Upstream, WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    m_initialSelection = range;
}

void WebPage::selectTextWithGranularityAtPoint(const WebCore::IntPoint& point, WebCore::TextGranularity granularity, bool isInteractingWithFocusedElement, CompletionHandler<void()>&& completionHandler)
{
    if (!m_potentialTapNode) {
        setSelectionRange(point, granularity, isInteractingWithFocusedElement);
        completionHandler();
        return;
    }
    
    ASSERT(!m_selectionChangedHandler);
    if (auto selectionChangedHandler = std::exchange(m_selectionChangedHandler, {}))
        selectionChangedHandler();

    m_selectionChangedHandler = [point, granularity, isInteractingWithFocusedElement, completionHandler = WTFMove(completionHandler), webPage = WeakPtr { *this }, this]() mutable {
        RefPtr<WebPage> strongPage = webPage.get();
        if (!strongPage) {
            completionHandler();
            return;
        }
        setSelectionRange(point, granularity, isInteractingWithFocusedElement);
        completionHandler();
    };

}

void WebPage::beginSelectionInDirection(WebCore::SelectionDirection direction, CompletionHandler<void(bool)>&& completionHandler)
{
    m_selectionAnchor = direction == SelectionDirection::Left ? Start : End;
    completionHandler(m_selectionAnchor == Start);
}

void WebPage::updateSelectionWithExtentPointAndBoundary(const WebCore::IntPoint& point, WebCore::TextGranularity granularity, bool isInteractingWithFocusedElement, CompletionHandler<void(bool)>&& callback)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    auto position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithFocusedElement);
    auto newRange = rangeForGranularityAtPoint(frame, point, granularity, isInteractingWithFocusedElement);
    
    if (position.isNull() || !m_initialSelection || !newRange)
        return callback(false);

    auto initialSelectionStartPosition = makeDeprecatedLegacyPosition(m_initialSelection->start);
    auto initialSelectionEndPosition = makeDeprecatedLegacyPosition(m_initialSelection->end);

    VisiblePosition selectionStart = initialSelectionStartPosition;
    VisiblePosition selectionEnd = initialSelectionEndPosition;
    if (position > initialSelectionEndPosition)
        selectionEnd = makeDeprecatedLegacyPosition(newRange->end);
    else if (position < initialSelectionStartPosition)
        selectionStart = makeDeprecatedLegacyPosition(newRange->start);

    if (auto range = makeSimpleRange(selectionStart, selectionEnd))
        frame->selection().setSelectedRange(range, Affinity::Upstream, WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);

    callback(selectionStart == initialSelectionStartPosition);
}

void WebPage::updateSelectionWithExtentPoint(const WebCore::IntPoint& point, bool isInteractingWithFocusedElement, RespectSelectionAnchor respectSelectionAnchor, CompletionHandler<void(bool)>&& callback)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    auto position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithFocusedElement);

    if (position.isNull())
        return callback(false);

    VisiblePosition selectionStart;
    VisiblePosition selectionEnd;
    
    if (respectSelectionAnchor == RespectSelectionAnchor::Yes) {
        if (m_selectionAnchor == Start) {
            selectionStart = frame->selection().selection().visibleStart();
            selectionEnd = position;
            if (position <= selectionStart) {
                selectionStart = selectionStart.previous();
                selectionEnd = frame->selection().selection().visibleEnd();
                m_selectionAnchor = End;
            }
        } else {
            selectionStart = position;
            selectionEnd = frame->selection().selection().visibleEnd();
            if (position >= selectionEnd) {
                selectionStart = frame->selection().selection().visibleStart();
                selectionEnd = selectionEnd.next();
                m_selectionAnchor = Start;
            }
        }
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
        frame->selection().setSelectedRange(range, Affinity::Upstream, WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);

    callback(m_selectionAnchor == Start);
}

void WebPage::requestDictationContext(CompletionHandler<void(const String&, const String&, const String&)>&& completionHandler)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    VisiblePosition startPosition = frame->selection().selection().start();
    VisiblePosition endPosition = frame->selection().selection().end();
    const unsigned dictationContextWordCount = 5;

    String selectedText = plainTextForContext(frame->selection().selection().toNormalizedRange());

    String contextBefore;
    if (startPosition != startOfEditableContent(startPosition)) {
        VisiblePosition currentPosition = startPosition;
        VisiblePosition lastPosition = startPosition;
        for (unsigned i = 0; i < dictationContextWordCount; ++i) {
            currentPosition = startOfWord(positionOfNextBoundaryOfGranularity(lastPosition, TextGranularity::WordGranularity, SelectionDirection::Backward));
            if (currentPosition.isNull())
                break;
            lastPosition = currentPosition;
        }
        contextBefore = plainTextForContext(makeSimpleRange(lastPosition, startPosition));
    }

    String contextAfter;
    if (endPosition != endOfEditableContent(endPosition)) {
        VisiblePosition currentPosition = endPosition;
        VisiblePosition lastPosition = endPosition;
        for (unsigned i = 0; i < dictationContextWordCount; ++i) {
            currentPosition = endOfWord(positionOfNextBoundaryOfGranularity(lastPosition, TextGranularity::WordGranularity, SelectionDirection::Forward));
            if (currentPosition.isNull())
                break;
            lastPosition = currentPosition;
        }
        contextAfter = plainTextForContext(makeSimpleRange(endPosition, lastPosition));
    }

    completionHandler(selectedText, contextBefore, contextAfter);
}
#if ENABLE(REVEAL)
RetainPtr<RVItem> WebPage::revealItemForCurrentSelection()
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    auto selection = frame->selection().selection();
    RetainPtr<RVItem> item;
    if (!selection.isNone()) {
        std::optional<SimpleRange> fullCharacterRange;
        if (selection.isRange()) {
            auto selectionStart = selection.visibleStart();
            auto selectionEnd = selection.visibleEnd();

            // As context, we are going to use the surrounding paragraphs of text.
            fullCharacterRange = makeSimpleRange(startOfParagraph(selectionStart), endOfParagraph(selectionEnd));
            if (!fullCharacterRange)
                fullCharacterRange = makeSimpleRange(selectionStart, selectionEnd);
            
            if (fullCharacterRange) {
                auto selectionRange = NSMakeRange(characterCount(*makeSimpleRange(fullCharacterRange->start, selectionStart)), characterCount(*makeSimpleRange(selectionStart, selectionEnd)));
                String itemString = plainText(*fullCharacterRange);
                item = adoptNS([PAL::allocRVItemInstance() initWithText:itemString selectedRange:selectionRange]);
            }
        }
    }
    return item;
}

void WebPage::requestRVItemInCurrentSelectedRange(CompletionHandler<void(const WebKit::RevealItem&)>&& completionHandler)
{
    completionHandler(RevealItem(revealItemForCurrentSelection()));
}

void WebPage::prepareSelectionForContextMenuWithLocationInView(IntPoint point, CompletionHandler<void(bool, const RevealItem&)>&& completionHandler)
{
    constexpr OptionSet hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowVisibleChildFrameContentOnly };
    Ref frame = m_page->mainFrame();
    auto result = frame->eventHandler().hitTestResultAtPoint(point, hitType);
    RefPtr hitNode = result.innerNonSharedNode();
    if (!hitNode)
        return completionHandler(false, { });

    auto sendEditorStateAndCallCompletionHandler = [this, completionHandler = WTFMove(completionHandler)](RevealItem&& item) mutable {
        layoutIfNeeded();
        sendEditorStateUpdate();
        completionHandler(true, WTFMove(item));
    };

    if (is<HTMLImageElement>(*hitNode) && hitNode->hasEditableStyle()) {
        auto range = makeRangeSelectingNode(*hitNode);
        if (range && frame->selection().setSelectedRange(range, Affinity::Upstream, FrameSelection::ShouldCloseTyping::Yes, UserTriggered))
            return sendEditorStateAndCallCompletionHandler({ });
    }

    frame->eventHandler().selectClosestContextualWordOrLinkFromHitTestResult(result, DontAppendTrailingWhitespace);
    sendEditorStateAndCallCompletionHandler(revealItemForCurrentSelection());
}
#endif

void WebPage::replaceSelectedText(const String& oldText, const String& newText)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    auto wordRange = frame->selection().isCaret() ? wordRangeFromPosition(frame->selection().selection().start()) : frame->selection().selection().toNormalizedRange();
    if (plainTextForContext(wordRange) != oldText)
        return;

    IgnoreSelectionChangeForScope ignoreSelectionChanges { frame };
    frame->selection().setSelectedRange(wordRange, Affinity::Upstream, WebCore::FrameSelection::ShouldCloseTyping::Yes);
    frame->editor().insertText(newText, 0);
}

void WebPage::replaceDictatedText(const String& oldText, const String& newText)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    if (frame->selection().isNone())
        return;
    
    if (frame->selection().isRange()) {
        frame->editor().deleteSelectionWithSmartDelete(false);
        return;
    }
    VisiblePosition position = frame->selection().selection().start();
    for (auto i = numGraphemeClusters(oldText); i; --i)
        position = position.previous();
    if (position.isNull())
        position = startOfDocument(frame->document());
    auto range = makeSimpleRange(position, frame->selection().selection().start());

    if (plainTextForContext(range) != oldText)
        return;

    // We don't want to notify the client that the selection has changed until we are done inserting the new text.
    IgnoreSelectionChangeForScope ignoreSelectionChanges { frame };
    frame->selection().setSelectedRange(range, Affinity::Upstream, WebCore::FrameSelection::ShouldCloseTyping::Yes);
    frame->editor().insertText(newText, 0);
}

void WebPage::willInsertFinalDictationResult()
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    if (frame->selection().isNone())
        return;

    m_ignoreSelectionChangeScopeForDictation = makeUnique<IgnoreSelectionChangeForScope>(frame);
}

void WebPage::didInsertFinalDictationResult()
{
    m_ignoreSelectionChangeScopeForDictation = nullptr;
    scheduleFullEditorStateUpdate();
}

void WebPage::requestAutocorrectionData(const String& textForAutocorrection, CompletionHandler<void(WebAutocorrectionData)>&& reply)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    if (!frame->selection().isCaret()) {
        reply({ });
        return;
    }

    auto range = wordRangeFromPosition(frame->selection().selection().visibleStart());
    if (!range) {
        reply({ });
        return;
    }

    auto textForRange = plainTextForContext(range);
    const unsigned maxSearchAttempts = 5;
    for (size_t i = 0;  i < maxSearchAttempts && textForRange != textForAutocorrection; ++i) {
        auto position = makeDeprecatedLegacyPosition(range->start).previous();
        if (position.isNull() || position == makeDeprecatedLegacyPosition(range->start))
            break;
        range = { { wordRangeFromPosition(position)->start, range->end } };
        textForRange = plainTextForContext(range);
    }

    Vector<SelectionGeometry> selectionGeometries;
    if (textForRange == textForAutocorrection)
        selectionGeometries = RenderObject::collectSelectionGeometries(*range);

    auto rootViewSelectionRects = selectionGeometries.map([&](const auto& selectionGeometry) -> FloatRect {
        return frame->view()->contentsToRootView(selectionGeometry.rect());
    });

    bool multipleFonts = false;
    CTFontRef font = nil;
    if (auto coreFont = frame->editor().fontForSelection(multipleFonts))
        font = coreFont->getCTFont();

    reply({ WTFMove(rootViewSelectionRects) , (__bridge UIFont *)font });
}

void WebPage::applyAutocorrection(const String& correction, const String& originalText, CompletionHandler<void(const String&)>&& callback)
{
    callback(applyAutocorrectionInternal(correction, originalText) ? correction : String());
}

Seconds WebPage::eventThrottlingDelay() const
{
    auto behaviorOverride = m_page->eventThrottlingBehaviorOverride();
    if (behaviorOverride) {
        switch (behaviorOverride.value()) {
        case EventThrottlingBehavior::Responsive:
            return 0_s;
        case EventThrottlingBehavior::Unresponsive:
            return 1_s;
        }
    }

    if (m_isInStableState || m_estimatedLatency <= Seconds(1.0 / 60))
        return 0_s;

    return std::min(m_estimatedLatency * 2, 1_s);
}

void WebPage::syncApplyAutocorrection(const String& correction, const String& originalText, CompletionHandler<void(bool)>&& reply)
{
    reply(applyAutocorrectionInternal(correction, originalText));
}

bool WebPage::applyAutocorrectionInternal(const String& correction, const String& originalText)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    if (!frame->selection().isCaretOrRange())
        return false;

    std::optional<SimpleRange> range;
    String textForRange;
    auto originalTextWithFoldedQuoteMarks = foldQuoteMarks(originalText);

    if (frame->selection().isCaret()) {
        auto position = frame->selection().selection().visibleStart();
        range = wordRangeFromPosition(position);
        textForRange = plainTextForContext(range);
        
        // If 'originalText' is not the same as 'textForRange' we need to move 'range'
        // forward such that it matches the original selection as much as possible.
        if (foldQuoteMarks(textForRange) != originalTextWithFoldedQuoteMarks) {
            // Search for the original text before the selection caret.
            // FIXME: Does this do the right thing in the case where `originalText` contains one or more grapheme clusters
            // that encompass multiple codepoints? Advancing backwards by grapheme cluster count here may also allow us to
            // sidestep the position adjustment logic below in some cases.
            for (size_t i = 0; i < originalText.length(); ++i)
                position = position.previous();
            if (position.isNull())
                position = startOfDocument(frame->document());
            range = makeSimpleRange(position, frame->selection().selection().start());
            textForRange = plainTextForContext(range);
            unsigned loopCount = 0;
            const unsigned maxPositionsAttempts = 10;
            while (textForRange.length() && textForRange.length() > originalText.length() && loopCount < maxPositionsAttempts) {
                position = position.next();
                if (position.isNotNull() && position >= frame->selection().selection().start())
                    range = std::nullopt;
                else
                    range = makeSimpleRange(position, frame->selection().selection().start());
                textForRange = plainTextForContext(range);
                loopCount++;
            }
        } else if (textForRange.isEmpty() && range && !range->collapsed()) {
            // If 'range' does not include any text but it is not collapsed, we need to set
            // 'range' to match the selection. Otherwise non-text nodes will be removed.
            range = makeSimpleRange(position);
            if (!range)
                return false;
        }
    } else {
        // Range selection.
        range = frame->selection().selection().toNormalizedRange();
        if (!range)
            return false;

        textForRange = plainTextForContext(range);
    }

    if (foldQuoteMarks(textForRange) != originalTextWithFoldedQuoteMarks)
        return false;
    
    // Correctly determine affinity, using logic currently only present in VisiblePosition
    auto affinity = Affinity::Downstream;
    if (range && range->collapsed())
        affinity = VisiblePosition(makeDeprecatedLegacyPosition(range->start), Affinity::Upstream).affinity();
    
    frame->selection().setSelectedRange(range, affinity, WebCore::FrameSelection::ShouldCloseTyping::Yes);
    if (correction.length())
        frame->editor().insertText(correction, 0, originalText.isEmpty() ? TextEventInputKeyboard : TextEventInputAutocompletion);
    else if (originalText.length())
        frame->editor().deleteWithDirection(SelectionDirection::Backward, TextGranularity::CharacterGranularity, false, true);
    return true;
}

WebAutocorrectionContext WebPage::autocorrectionContext()
{
    if (!m_focusedElement)
        return { };

    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    if (!frame->selection().selection().hasEditableStyle())
        return { };

    String contextBefore;
    String markedText;
    String selectedText;
    String contextAfter;
    EditingRange markedTextRange;

    VisiblePosition startPosition = frame->selection().selection().start();
    VisiblePosition endPosition = frame->selection().selection().end();
    const unsigned minContextWordCount = 10;
    const unsigned minContextLength = 40;
    const unsigned maxContextLength = 100;

    if (frame->selection().isRange())
        selectedText = plainTextForContext(frame->selection().selection().toNormalizedRange());

    if (auto compositionRange = frame->editor().compositionRange()) {
        auto markedTextBefore = plainTextForContext(makeSimpleRange(compositionRange->start, startPosition));
        auto markedTextAfter = plainTextForContext(makeSimpleRange(endPosition, compositionRange->end));
        markedText = markedTextBefore + selectedText + markedTextAfter;
        if (!markedText.isEmpty()) {
            markedTextRange.location = markedTextBefore.length();
            markedTextRange.length = selectedText.length();
        }
    } else {
        auto firstPositionInEditableContent = startOfEditableContent(startPosition);
        if (startPosition != firstPositionInEditableContent) {
            VisiblePosition contextStartPosition = startPosition;
            VisiblePosition previousPosition;
            unsigned totalContextLength = 0;
            for (unsigned i = 0; i < minContextWordCount; ++i) {
                if (contextBefore.length() >= minContextLength)
                    break;
                previousPosition = startOfWord(positionOfNextBoundaryOfGranularity(contextStartPosition, TextGranularity::WordGranularity, SelectionDirection::Backward));
                if (previousPosition.isNull())
                    break;
                String currentWord = plainTextForContext(makeSimpleRange(previousPosition, contextStartPosition));
                totalContextLength += currentWord.length();
                if (totalContextLength >= maxContextLength)
                    break;
                contextStartPosition = previousPosition;
            }
            VisiblePosition sentenceContextStartPosition = startOfSentence(startPosition);
            if (sentenceContextStartPosition.isNotNull() && sentenceContextStartPosition < contextStartPosition)
                contextStartPosition = sentenceContextStartPosition;
            if (contextStartPosition.isNotNull() && contextStartPosition < startPosition) {
                contextBefore = plainTextForContext(makeSimpleRange(contextStartPosition, startPosition));
                if (atBoundaryOfGranularity(contextStartPosition, TextGranularity::ParagraphGranularity, SelectionDirection::Backward) && firstPositionInEditableContent != contextStartPosition)
                    contextBefore = makeString("\n "_s, contextBefore);
            }
        }

        if (endPosition != endOfEditableContent(endPosition)) {
            VisiblePosition nextPosition = endOfSentence(endPosition);
            if (nextPosition.isNotNull() && nextPosition > endPosition)
                contextAfter = plainTextForContext(makeSimpleRange(endPosition, nextPosition));
        }
    }

    WebAutocorrectionContext correction;
    correction.contextBefore = WTFMove(contextBefore);
    correction.markedText = WTFMove(markedText);
    correction.selectedText = WTFMove(selectedText);
    correction.contextAfter = WTFMove(contextAfter);
    correction.markedTextRange = WTFMove(markedTextRange);
    return correction;
}

void WebPage::preemptivelySendAutocorrectionContext()
{
    send(Messages::WebPageProxy::HandleAutocorrectionContext(autocorrectionContext()));
}

void WebPage::handleAutocorrectionContextRequest()
{
    send(Messages::WebPageProxy::HandleAutocorrectionContext(autocorrectionContext()));
}

void WebPage::prepareToRunModalJavaScriptDialog()
{
    // When a modal dialog is presented while an editable element is focused, UIKit will attempt to request a
    // WebAutocorrectionContext, which triggers synchronous IPC back to the web process, resulting in deadlock.
    // To avoid this deadlock, we preemptively compute and send autocorrection context data to the UI process,
    // such that the UI process can immediately respond to UIKit without synchronous IPC to the web process.
    preemptivelySendAutocorrectionContext();
}

static HTMLAnchorElement* containingLinkAnchorElement(Element& element)
{
    // FIXME: There is code in the drag controller that supports any link, even if it's not an HTMLAnchorElement. Why is this different?
    for (auto& currentElement : lineageOfType<HTMLAnchorElement>(element)) {
        if (currentElement.isLink())
            return &currentElement;
    }
    return nullptr;
}

static inline bool isAssistableElement(Element& element)
{
    if (is<HTMLSelectElement>(element))
        return true;
    if (is<HTMLTextAreaElement>(element))
        return true;
    if (is<HTMLInputElement>(element)) {
        HTMLInputElement& inputElement = downcast<HTMLInputElement>(element);
        // FIXME: This laundry list of types is not a good way to factor this. Need a suitable function on HTMLInputElement itself.
#if ENABLE(INPUT_TYPE_COLOR)
        if (inputElement.isColorControl())
            return true;
#endif
        return inputElement.isTextField() || inputElement.isDateField() || inputElement.isDateTimeLocalField() || inputElement.isMonthField() || inputElement.isTimeField();
    }
    if (is<HTMLIFrameElement>(element))
        return false;
    return element.isContentEditable();
}

static inline bool isObscuredElement(Element& element)
{
    Ref topDocument = element.document().topDocument();
    auto elementRectInMainFrame = element.boundingBoxInRootViewCoordinates();

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowChildFrameContent, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::IgnoreClipping };
    HitTestResult result(elementRectInMainFrame.center());

    topDocument->hitTest(hitType, result);
    result.setToNonUserAgentShadowAncestor();

    if (result.targetElement() == &element)
        return false;

    return true;
}
    
static void focusedElementPositionInformation(WebPage& page, Element& focusedElement, const InteractionInformationRequest& request, InteractionInformationAtPosition& info)
{
    Ref frame = CheckedRef(page.corePage()->focusController())->focusedOrMainFrame();
    if (!frame->editor().hasComposition())
        return;

    const uint32_t kHitAreaWidth = 66;
    const uint32_t kHitAreaHeight = 66;
    Ref view = *frame->view();
    IntPoint adjustedPoint(view->rootViewToContents(request.point));
    IntPoint constrainedPoint = constrainPoint(adjustedPoint, frame, focusedElement);
    VisiblePosition position = frame->visiblePositionForPoint(constrainedPoint);

    auto compositionRange = frame->editor().compositionRange();
    if (!compositionRange)
        return;

    auto startPosition = makeDeprecatedLegacyPosition(compositionRange->start);
    auto endPosition = makeDeprecatedLegacyPosition(compositionRange->end);
    if (position < startPosition)
        position = startPosition;
    else if (position > endPosition)
        position = endPosition;
    IntRect caretRect = view->contentsToRootView(position.absoluteCaretBounds());
    float deltaX = abs(caretRect.x() + (caretRect.width() / 2) - request.point.x());
    float deltaYFromTheTop = abs(caretRect.y() - request.point.y());
    float deltaYFromTheBottom = abs(caretRect.y() + caretRect.height() - request.point.y());

    info.isNearMarkedText = !(deltaX > kHitAreaWidth || deltaYFromTheTop > kHitAreaHeight || deltaYFromTheBottom > kHitAreaHeight);
}

static void linkIndicatorPositionInformation(WebPage& page, Element& linkElement, const InteractionInformationRequest& request, InteractionInformationAtPosition& info)
{
    if (!request.includeLinkIndicator)
        return;

    auto linkRange = makeRangeSelectingNodeContents(linkElement);
    float deviceScaleFactor = page.corePage()->deviceScaleFactor();
    const float marginInPoints = request.linkIndicatorShouldHaveLegacyMargins ? 4 : 0;

    constexpr OptionSet<TextIndicatorOption> textIndicatorOptions {
        TextIndicatorOption::TightlyFitContent,
        TextIndicatorOption::RespectTextColor,
        TextIndicatorOption::PaintBackgrounds,
        TextIndicatorOption::UseBoundingRectAndPaintAllContentForComplexRanges,
        TextIndicatorOption::IncludeMarginIfRangeMatchesSelection,
        TextIndicatorOption::ComputeEstimatedBackgroundColor
    };
    auto textIndicator = TextIndicator::createWithRange(linkRange, textIndicatorOptions, TextIndicatorPresentationTransition::None, FloatSize(marginInPoints * deviceScaleFactor, marginInPoints * deviceScaleFactor));

    if (textIndicator)
        info.linkIndicator = textIndicator->data();
}
    
#if ENABLE(DATA_DETECTION)

static void dataDetectorLinkPositionInformation(Element& element, InteractionInformationAtPosition& info)
{
    if (!DataDetection::isDataDetectorLink(element))
        return;
    
    info.isDataDetectorLink = true;
    info.dataDetectorBounds = info.bounds;
    const int dataDetectionExtendedContextLength = 350;
    info.dataDetectorIdentifier = DataDetection::dataDetectorIdentifier(element);
    if (auto* results = element.document().frame()->dataDetectionResultsIfExists())
        info.dataDetectorResults = results->documentLevelResults();

    if (!DataDetection::requiresExtendedContext(element))
        return;
    
    auto range = makeRangeSelectingNodeContents(element);
    info.textBefore = plainTextForDisplay(rangeExpandedByCharactersInDirectionAtWordBoundary(makeDeprecatedLegacyPosition(range.start),
        dataDetectionExtendedContextLength, SelectionDirection::Backward));
    info.textAfter = plainTextForDisplay(rangeExpandedByCharactersInDirectionAtWordBoundary(makeDeprecatedLegacyPosition(range.end),
        dataDetectionExtendedContextLength, SelectionDirection::Forward));
}

static void dataDetectorImageOverlayPositionInformation(const HTMLElement& overlayHost, const InteractionInformationRequest& request, InteractionInformationAtPosition& info)
{
    RefPtr frame = overlayHost.document().frame();
    if (!frame)
        return;

    auto elementAndBounds = DataDetection::findDataDetectionResultElementInImageOverlay(request.point, overlayHost);
    if (!elementAndBounds)
        return;

    auto [foundElement, elementBounds] = *elementAndBounds;
    auto identifierValue = parseInteger<uint64_t>(foundElement->attributeWithoutSynchronization(HTMLNames::x_apple_data_detectors_resultAttr));
    if (!identifierValue)
        return;

    auto identifier = makeObjectIdentifier<ImageOverlayDataDetectionResultIdentifierType>(*identifierValue);
    if (!identifier.isValid())
        return;

    auto* dataDetectionResults = frame->dataDetectionResultsIfExists();
    if (!dataDetectionResults)
        return;

    auto dataDetectionResult = retainPtr(dataDetectionResults->imageOverlayDataDetectionResult(identifier));
    if (!dataDetectionResult)
        return;

    info.dataDetectorBounds = WTFMove(elementBounds);
    info.dataDetectorResults = @[ dataDetectionResult.get() ];
}

#endif // ENABLE(DATA_DETECTION)

static std::optional<std::pair<RenderImage&, Image&>> imageRendererAndImage(Element& element)
{
    if (!is<RenderImage>(element.renderer()))
        return std::nullopt;

    auto& renderImage = downcast<RenderImage>(*element.renderer());
    if (!renderImage.cachedImage() || renderImage.cachedImage()->errorOccurred())
        return std::nullopt;

    auto* image = renderImage.cachedImage()->imageForRenderer(&renderImage);
    if (!image || image->width() <= 1 || image->height() <= 1)
        return std::nullopt;

    return {{ renderImage, *image }};
}

static void videoPositionInformation(WebPage& page, HTMLVideoElement& element, const InteractionInformationRequest& request, InteractionInformationAtPosition& info)
{
    info.elementContainsImageOverlay = ImageOverlay::hasOverlay(element);

    if (!element.paused())
        return;

    auto renderVideo = element.renderer();
    if (!renderVideo)
        return;

    info.isPausedVideo = true;

    if (request.includeImageData)
        info.image = createShareableBitmap(*renderVideo);

    info.hostImageOrVideoElementContext = page.contextForElement(element);
}

static RefPtr<HTMLVideoElement> hostVideoElementIgnoringImageOverlay(Node& node)
{
    if (ImageOverlay::isInsideOverlay(node))
        return { };

    if (is<HTMLVideoElement>(node))
        return downcast<HTMLVideoElement>(&node);

    RefPtr shadowHost = node.shadowHost();
    if (!is<HTMLVideoElement>(shadowHost.get()))
        return { };

    return downcast<HTMLVideoElement>(shadowHost.get());
}

static void imagePositionInformation(WebPage& page, Element& element, const InteractionInformationRequest& request, InteractionInformationAtPosition& info)
{
    auto rendererAndImage = imageRendererAndImage(element);
    if (!rendererAndImage)
        return;

    auto& [renderImage, image] = *rendererAndImage;
    info.isImage = true;
    info.imageURL = page.sanitizeLookalikeCharacters(element.document().completeURL(renderImage.cachedImage()->url().string()), LookalikeCharacterSanitizationTrigger::Unspecified);
    info.imageMIMEType = image.mimeType();
    info.isAnimatedImage = image.isAnimated();
    info.isAnimating = image.isAnimating();
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    info.canShowAnimationControls = page.corePage() && page.corePage()->settings().imageAnimationControlEnabled();
#endif
    info.elementContainsImageOverlay = is<HTMLElement>(element) && ImageOverlay::hasOverlay(downcast<HTMLElement>(element));

    if (request.includeSnapshot || request.includeImageData)
        info.image = createShareableBitmap(renderImage, { screenSize() * page.corePage()->deviceScaleFactor(), AllowAnimatedImages::Yes, UseSnapshotForTransparentImages::Yes });

    info.hostImageOrVideoElementContext = page.contextForElement(element);
}

static void boundsPositionInformation(RenderObject& renderer, InteractionInformationAtPosition& info)
{
    if (renderer.isRenderImage())
        info.bounds = downcast<RenderImage>(renderer).absoluteContentQuad().enclosingBoundingBox();
    else
        info.bounds = renderer.absoluteBoundingBoxRect();

    if (!renderer.document().frame()->isMainFrame()) {
        FrameView *view = renderer.document().frame()->view();
        info.bounds = view->contentsToRootView(info.bounds);
    }
}

static void elementPositionInformation(WebPage& page, Element& element, const InteractionInformationRequest& request, const Node* innerNonSharedNode, InteractionInformationAtPosition& info)
{
    Element* linkElement = nullptr;
    if (element.renderer() && element.renderer()->isRenderImage())
        linkElement = containingLinkAnchorElement(element);
    else if (element.isLink())
        linkElement = &element;

    info.isElement = true;
    info.idAttribute = element.getIdAttribute();
    info.isImageOverlayText = ImageOverlay::isOverlayText(innerNonSharedNode);

    info.title = element.attributeWithoutSynchronization(HTMLNames::titleAttr).string();
    if (linkElement && info.title.isEmpty())
        info.title = element.innerText();
    if (element.renderer())
        info.touchCalloutEnabled = element.renderer()->style().touchCalloutEnabled();

    if (linkElement && !info.isImageOverlayText) {
        info.isLink = true;
        info.url = page.sanitizeLookalikeCharacters(linkElement->document().completeURL(stripLeadingAndTrailingHTMLSpaces(linkElement->getAttribute(HTMLNames::hrefAttr))), LookalikeCharacterSanitizationTrigger::Unspecified);

        linkIndicatorPositionInformation(page, *linkElement, request, info);
#if ENABLE(DATA_DETECTION)
        dataDetectorLinkPositionInformation(element, info);
#endif
    }

    auto* elementForScrollTesting = linkElement ? linkElement : &element;
    if (auto* renderer = elementForScrollTesting->renderer()) {
#if ENABLE(ASYNC_SCROLLING)
        if (auto* scrollingCoordinator = page.scrollingCoordinator())
            info.containerScrollingNodeID = scrollingCoordinator->scrollableContainerNodeID(*renderer);
#endif
    }

    if (auto* renderer = element.renderer()) {
        bool shouldCollectImagePositionInformation = renderer->isRenderImage();
        if (shouldCollectImagePositionInformation && info.isImageOverlayText) {
            shouldCollectImagePositionInformation = false;
            if (request.includeImageData) {
                if (auto rendererAndImage = imageRendererAndImage(element)) {
                    auto& [renderImage, image] = *rendererAndImage;
                    info.imageURL = page.sanitizeLookalikeCharacters(element.document().completeURL(renderImage.cachedImage()->url().string()), LookalikeCharacterSanitizationTrigger::Unspecified);
                    info.imageMIMEType = image.mimeType();
                    info.image = createShareableBitmap(renderImage, { screenSize() * page.corePage()->deviceScaleFactor(), AllowAnimatedImages::Yes, UseSnapshotForTransparentImages::Yes });
                }
            }
        }
        if (shouldCollectImagePositionInformation) {
            if (auto video = hostVideoElementIgnoringImageOverlay(element))
                videoPositionInformation(page, *video, request, info);
            else
                imagePositionInformation(page, element, request, info);
        }
        boundsPositionInformation(*renderer, info);
    }

    info.elementContext = page.contextForElement(element);
}
    
static void selectionPositionInformation(WebPage& page, const InteractionInformationRequest& request, InteractionInformationAtPosition& info)
{
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowVisibleChildFrameContentOnly };
    HitTestResult result = page.corePage()->mainFrame().eventHandler().hitTestResultAtPoint(request.point, hitType);
    Node* hitNode = result.innerNode();

    // Hit test could return HTMLHtmlElement that has no renderer, if the body is smaller than the document.
    if (!hitNode || !hitNode->renderer())
        return;

    auto* renderer = hitNode->renderer();

    info.selectability = ([&] {
        if (renderer->style().effectiveUserSelect() == UserSelect::None)
            return InteractionInformationAtPosition::Selectability::UnselectableDueToUserSelectNone;

        if (is<Element>(*hitNode)) {
            if (isAssistableElement(downcast<Element>(*hitNode)))
                return InteractionInformationAtPosition::Selectability::UnselectableDueToFocusableElement;

            if (rectIsTooBigForSelection(info.bounds, *result.innerNodeFrame())) {
                // We don't want to select blocks that are larger than 97% of the visible area of the document.
                // FIXME: Is this heuristic still needed, now that block selection has been removed?
                return InteractionInformationAtPosition::Selectability::UnselectableDueToLargeElementBounds;
            }

            if (hostVideoElementIgnoringImageOverlay(*hitNode))
                return InteractionInformationAtPosition::Selectability::UnselectableDueToMediaControls;
        }

        return InteractionInformationAtPosition::Selectability::Selectable;
    })();
    info.isSelected = result.isSelected();

    if (info.isLink || info.isImage)
        return;

    boundsPositionInformation(*renderer, info);
    
    if (is<Element>(*hitNode)) {
        Element& element = downcast<Element>(*hitNode);
        info.idAttribute = element.getIdAttribute();
    }
    
    if (is<HTMLAttachmentElement>(*hitNode)) {
        info.isAttachment = true;
        HTMLAttachmentElement& attachment = downcast<HTMLAttachmentElement>(*hitNode);
        info.title = attachment.attachmentTitle();
        linkIndicatorPositionInformation(page, attachment, request, info);
        if (attachment.file())
            info.url = URL::fileURLWithFileSystemPath(downcast<HTMLAttachmentElement>(*hitNode).file()->path());
    }

    for (RefPtr currentNode = hitNode; currentNode; currentNode = currentNode->parentOrShadowHostNode()) {
        auto* renderer = currentNode->renderer();
        if (!renderer)
            continue;

        auto& style = renderer->style();
        if (style.effectiveUserSelect() == UserSelect::None && style.userDrag() == UserDrag::Element) {
            info.prefersDraggingOverTextSelection = true;
            break;
        }
    }
#if PLATFORM(MACCATALYST)
    bool isInsideFixedPosition;
    VisiblePosition caretPosition(renderer->positionForPoint(request.point, nullptr));
    info.caretRect = caretPosition.absoluteCaretBounds(&isInsideFixedPosition);
#endif
}

#if ENABLE(DATALIST_ELEMENT)
static void textInteractionPositionInformation(WebPage& page, const HTMLInputElement& input, const InteractionInformationRequest& request, InteractionInformationAtPosition& info)
{
    if (!input.list())
        return;

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowVisibleChildFrameContentOnly };
    HitTestResult result = page.corePage()->mainFrame().eventHandler().hitTestResultAtPoint(request.point, hitType);
    if (result.innerNode() == input.dataListButtonElement())
        info.preventTextInteraction = true;
}
#endif

RefPtr<ShareableBitmap> WebPage::shareableBitmapSnapshotForNode(Element& element)
{
    // Ensure that the image contains at most 600K pixels, so that it is not too big.
    if (auto snapshot = snapshotNode(element, SnapshotOptionsShareable, 600 * 1024))
        return snapshot->bitmap();
    return nullptr;
}

static bool canForceCaretForPosition(const VisiblePosition& position)
{
    auto* node = position.deepEquivalent().anchorNode();
    if (!node)
        return false;

    auto* renderer = node->renderer();
    auto* style = renderer ? &renderer->style() : nullptr;
    auto cursorType = style ? style->cursor() : CursorType::Auto;

    if (cursorType == CursorType::Text)
        return true;

    if (cursorType != CursorType::Auto)
        return false;

    if (node->hasEditableStyle())
        return true;

    if (!renderer)
        return false;

    return renderer->isText() && node->canStartSelection();
}

static void populateCaretContext(const HitTestResult& hitTestResult, const InteractionInformationRequest& request, InteractionInformationAtPosition& info)
{
    RefPtr frame = hitTestResult.innerNodeFrame();
    if (!frame)
        return;

    RefPtr view = frame->view();
    if (!view)
        return;

    auto node = hitTestResult.innerNode();
    if (!node)
        return;

    auto* renderer = node->renderer();
    if (!renderer)
        return;

    while (renderer && !is<RenderBlockFlow>(*renderer))
        renderer = renderer->parent();

    if (!renderer)
        return;

    // FIXME: We should be able to retrieve this geometry information without
    // forcing the text to fall out of Simple Line Layout.
    auto& blockFlow = downcast<RenderBlockFlow>(*renderer);
    auto position = frame->visiblePositionForPoint(view->rootViewToContents(request.point));
    auto lineRect = position.absoluteSelectionBoundsForLine();
    bool isEditable = node->hasEditableStyle();

    if (isEditable)
        lineRect.setWidth(blockFlow.contentWidth());

    info.isVerticalWritingMode = !renderer->isHorizontalWritingMode();
    info.lineCaretExtent = view->contentsToRootView(lineRect);
    info.caretLength = info.isVerticalWritingMode ? info.lineCaretExtent.width() : info.lineCaretExtent.height();

    bool lineContainsRequestPoint = info.lineCaretExtent.contains(request.point);
    // Force an I-beam cursor if the page didn't request a hand, and we're inside the bounds of the line.
    if (lineContainsRequestPoint && info.cursor->type() != Cursor::Hand && canForceCaretForPosition(position))
        info.cursor = Cursor::fromType(Cursor::IBeam);

    if (!lineContainsRequestPoint && info.cursor->type() == Cursor::IBeam) {
        auto approximateLineRectInContentCoordinates = renderer->absoluteBoundingBoxRect();
        approximateLineRectInContentCoordinates.setHeight(renderer->style().computedLineHeight());
        info.lineCaretExtent = view->contentsToRootView(approximateLineRectInContentCoordinates);
        if (!info.lineCaretExtent.contains(request.point) || !isEditable)
            info.lineCaretExtent.setY(request.point.y() - info.lineCaretExtent.height() / 2);
        info.caretLength = info.isVerticalWritingMode ? info.lineCaretExtent.width() : info.lineCaretExtent.height();
    }

    auto nodeShouldNotUseIBeam = ^(Node* node) {
        if (!node)
            return false;
        RenderObject *renderer = node->renderer();
        if (!renderer)
            return false;
        return is<RenderReplaced>(*renderer);
    };

    const auto& deepPosition = position.deepEquivalent();
    info.shouldNotUseIBeamInEditableContent = nodeShouldNotUseIBeam(node) || nodeShouldNotUseIBeam(deepPosition.computeNodeBeforePosition()) || nodeShouldNotUseIBeam(deepPosition.computeNodeAfterPosition());
}

InteractionInformationAtPosition WebPage::positionInformation(const InteractionInformationRequest& request)
{
    InteractionInformationAtPosition info;
    info.request = request;

    FloatPoint adjustedPoint;
    auto* nodeRespondingToClickEvents = m_page->mainFrame().nodeRespondingToClickEvents(request.point, adjustedPoint);

    info.isContentEditable = nodeRespondingToClickEvents && nodeRespondingToClickEvents->isContentEditable();
    info.adjustedPointForNodeRespondingToClickEvents = adjustedPoint;

    if (request.includeHasDoubleClickHandler)
        info.nodeAtPositionHasDoubleClickHandler = m_page->mainFrame().nodeRespondingToDoubleClickEvent(request.point, adjustedPoint);

    auto& eventHandler = m_page->mainFrame().eventHandler();
    auto hitTestRequestTypes = OptionSet<HitTestRequest::Type> {
        HitTestRequest::Type::ReadOnly,
        HitTestRequest::Type::AllowFrameScrollbars,
        HitTestRequest::Type::AllowVisibleChildFrameContentOnly,
    };

    if (request.disallowUserAgentShadowContent)
        hitTestRequestTypes.add(HitTestRequest::Type::DisallowUserAgentShadowContent);

    auto hitTestResult = eventHandler.hitTestResultAtPoint(request.point, hitTestRequestTypes);
    if (auto* hitFrame = hitTestResult.innerNodeFrame()) {
        info.cursor = hitFrame->eventHandler().selectCursor(hitTestResult, false);
        if (request.includeCaretContext)
            populateCaretContext(hitTestResult, request, info);
    }

    if (m_focusedElement)
        focusedElementPositionInformation(*this, *m_focusedElement, request, info);

    RefPtr hitTestNode = hitTestResult.innerNonSharedNode();
    if (is<Element>(nodeRespondingToClickEvents)) {
        auto& element = downcast<Element>(*nodeRespondingToClickEvents);
        elementPositionInformation(*this, element, request, hitTestNode.get(), info);

        if (info.isLink && !info.isImage && request.includeSnapshot)
            info.image = shareableBitmapSnapshotForNode(element);
    }

#if ENABLE(DATA_DETECTION)
    auto hitTestedImageOverlayHost = ([&]() -> RefPtr<HTMLElement> {
        if (!hitTestNode || !info.isImageOverlayText)
            return nullptr;

        RefPtr shadowHost = hitTestNode->shadowHost();
        if (!is<HTMLElement>(shadowHost.get()))
            return nullptr;

        RefPtr htmlElement = downcast<HTMLElement>(shadowHost.get());
        if (!ImageOverlay::hasOverlay(*htmlElement))
            return nullptr;

        return htmlElement;
    })();

    if (hitTestedImageOverlayHost)
        dataDetectorImageOverlayPositionInformation(*hitTestedImageOverlayHost, request, info);
#endif // ENABLE(DATA_DETECTION)

    if (!info.isImage && request.includeImageData && hitTestNode) {
        if (auto video = hostVideoElementIgnoringImageOverlay(*hitTestNode))
            videoPositionInformation(*this, *video, request, info);
        else if (is<HTMLImageElement>(hitTestNode))
            imagePositionInformation(*this, downcast<HTMLImageElement>(*hitTestNode), request, info);
    }

    selectionPositionInformation(*this, request, info);

    // Prevent the callout bar from showing when tapping on the datalist button.
#if ENABLE(DATALIST_ELEMENT)
    if (is<HTMLInputElement>(nodeRespondingToClickEvents))
        textInteractionPositionInformation(*this, downcast<HTMLInputElement>(*nodeRespondingToClickEvents), request, info);
#endif

    return info;
}

void WebPage::requestPositionInformation(const InteractionInformationRequest& request)
{
    sendEditorStateUpdate();
    send(Messages::WebPageProxy::DidReceivePositionInformation(positionInformation(request)));
}

void WebPage::startInteractionWithElementContextOrPosition(std::optional<WebCore::ElementContext>&& elementContext, WebCore::IntPoint&& point)
{
    if (elementContext) {
        m_interactionNode = elementForContext(*elementContext);
        if (m_interactionNode)
            return;
    }

    FloatPoint adjustedPoint;
    m_interactionNode = m_page->mainFrame().nodeRespondingToInteraction(point, adjustedPoint);
}

void WebPage::stopInteraction()
{
    m_interactionNode = nullptr;
}

void WebPage::performActionOnElement(uint32_t action, const String& authorizationToken, CompletionHandler<void()>&& completionHandler)
{
    CompletionHandlerCallingScope callCompletionHandler(WTFMove(completionHandler));

    if (!is<HTMLElement>(m_interactionNode))
        return;

    HTMLElement& element = downcast<HTMLElement>(*m_interactionNode);
    if (!element.renderer())
        return;

    if (static_cast<SheetAction>(action) == SheetAction::Copy) {
        if (is<RenderImage>(*element.renderer())) {
            URL urlToCopy;
            String titleToCopy;
            if (RefPtr linkElement = containingLinkAnchorElement(element)) {
                if (auto url = linkElement->href(); !url.isEmpty() && !url.protocolIsJavaScript()) {
                    urlToCopy = url;
                    titleToCopy = linkElement->attributeWithoutSynchronization(HTMLNames::titleAttr);
                    if (!titleToCopy.length())
                        titleToCopy = linkElement->textContent();
                    titleToCopy = stripLeadingAndTrailingHTMLSpaces(titleToCopy);
                }
            }
            m_interactionNode->document().editor().writeImageToPasteboard(*Pasteboard::createForCopyAndPaste(PagePasteboardContext::create(element.document().pageID())), element, urlToCopy, titleToCopy);
        } else if (element.isLink())
            m_interactionNode->document().editor().copyURL(element.document().completeURL(stripLeadingAndTrailingHTMLSpaces(element.attributeWithoutSynchronization(HTMLNames::hrefAttr))), element.textContent());
#if ENABLE(ATTACHMENT_ELEMENT)
        else if (auto attachmentInfo = element.document().editor().promisedAttachmentInfo(element))
            send(Messages::WebPageProxy::WritePromisedAttachmentToPasteboard(WTFMove(attachmentInfo), authorizationToken));
#endif
    } else if (static_cast<SheetAction>(action) == SheetAction::SaveImage) {
        if (!is<RenderImage>(*element.renderer()))
            return;
        CachedImage* cachedImage = downcast<RenderImage>(*element.renderer()).cachedImage();
        if (!cachedImage)
            return;
        RefPtr<FragmentedSharedBuffer> buffer = cachedImage->resourceBuffer();
        if (!buffer)
            return;
        SharedMemory::Handle handle;
        {
            auto sharedMemoryBuffer = SharedMemory::copyBuffer(*buffer);
            if (!sharedMemoryBuffer)
                return;
            if (auto memoryHandle = sharedMemoryBuffer->createHandle(SharedMemory::Protection::ReadOnly))
                handle = WTFMove(*memoryHandle);
        }
        send(Messages::WebPageProxy::SaveImageToLibrary(WTFMove(handle), authorizationToken));
    }

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    if (static_cast<SheetAction>(action) == SheetAction::PlayAnimation) {
        if (auto* imageElement = dynamicDowncast<HTMLImageElement>(element))
            imageElement->setAllowsAnimation(true);
    } else if (static_cast<SheetAction>(action) == SheetAction::PauseAnimation) {
        if (auto* imageElement = dynamicDowncast<HTMLImageElement>(element))
            imageElement->setAllowsAnimation(false);
    }
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
}

static inline RefPtr<Element> nextAssistableElement(Node* startNode, Page& page, bool isForward)
{
    if (!is<Element>(startNode))
        return nullptr;

    RefPtr nextElement = downcast<Element>(startNode);
    CheckedRef focusController { page.focusController() };
    do {
        nextElement = isForward ? focusController->nextFocusableElement(*nextElement) : focusController->previousFocusableElement(*nextElement);
    } while (nextElement && (!isAssistableElement(*nextElement) || isObscuredElement(*nextElement)));

    return nextElement;
}

void WebPage::focusNextFocusedElement(bool isForward, CompletionHandler<void()>&& completionHandler)
{
    auto nextElement = nextAssistableElement(m_focusedElement.get(), *m_page, isForward);
    m_userIsInteracting = true;
    if (nextElement)
        nextElement->focus();
    m_userIsInteracting = false;
    completionHandler();
}

std::optional<FocusedElementInformation> WebPage::focusedElementInformation()
{
    RefPtr<Document> document = CheckedRef(m_page->focusController())->focusedOrMainFrame().document();
    if (!document || !document->view())
        return std::nullopt;

    auto focusedElement = m_focusedElement.copyRef();
    layoutIfNeeded();

    // Layout may have detached the document or caused a change of focus.
    if (!document->view() || focusedElement != m_focusedElement)
        return std::nullopt;

    scheduleFullEditorStateUpdate();

    FocusedElementInformation information;

    information.frameID = CheckedRef(m_page->focusController())->focusedOrMainFrame().frameID();

    information.lastInteractionLocation = m_lastInteractionLocation;
    if (auto elementContext = contextForElement(*focusedElement))
        information.elementContext = WTFMove(*elementContext);

    if (auto* renderer = focusedElement->renderer()) {
        information.interactionRect = rootViewInteractionBounds(*focusedElement);
        information.nodeFontSize = renderer->style().fontDescription().computedSize();

        bool inFixed = false;
        renderer->localToContainerPoint(FloatPoint(), nullptr, UseTransforms, &inFixed);
        information.insideFixedPosition = inFixed;
        information.isRTL = renderer->style().direction() == TextDirection::RTL;

#if ENABLE(ASYNC_SCROLLING)
        if (auto* scrollingCoordinator = this->scrollingCoordinator())
            information.containerScrollingNodeID = scrollingCoordinator->scrollableContainerNodeID(*renderer);
#endif
    } else
        information.interactionRect = { };

    if (is<HTMLElement>(focusedElement))
        information.isSpellCheckingEnabled = downcast<HTMLElement>(*focusedElement).spellcheck();

    if (is<HTMLFormControlElement>(focusedElement))
        information.isFocusingWithValidationMessage = downcast<HTMLFormControlElement>(*focusedElement).isFocusingWithValidationMessage();

    information.minimumScaleFactor = minimumPageScaleFactor();
    information.maximumScaleFactor = maximumPageScaleFactor();
    information.maximumScaleFactorIgnoringAlwaysScalable = maximumPageScaleFactorIgnoringAlwaysScalable();
    information.allowsUserScaling = m_viewportConfiguration.allowsUserScaling();
    information.allowsUserScalingIgnoringAlwaysScalable = m_viewportConfiguration.allowsUserScalingIgnoringAlwaysScalable();
    if (auto nextElement = nextAssistableElement(focusedElement.get(), *m_page, true)) {
        information.nextNodeRect = rootViewBounds(*nextElement);
        information.hasNextNode = true;
    }
    if (auto previousElement = nextAssistableElement(focusedElement.get(), *m_page, false)) {
        information.previousNodeRect = rootViewBounds(*previousElement);
        information.hasPreviousNode = true;
    }
    information.identifier = m_lastFocusedElementInformationIdentifier.increment();

    if (is<HTMLElement>(*focusedElement)) {
        if (auto labels = downcast<HTMLElement>(*focusedElement).labels()) {
            Vector<Ref<Element>> associatedLabels;
            for (unsigned index = 0; index < labels->length(); ++index) {
                if (is<Element>(labels->item(index)) && labels->item(index)->renderer())
                    associatedLabels.append(downcast<Element>(*labels->item(index)));
            }
            for (auto& labelElement : associatedLabels) {
                auto text = labelElement->innerText();
                if (!text.isEmpty()) {
                    information.label = WTFMove(text);
                    break;
                }
            }
        }
    }

    information.title = focusedElement->title();
    information.ariaLabel = focusedElement->attributeWithoutSynchronization(HTMLNames::aria_labelAttr);

    if (is<HTMLSelectElement>(*focusedElement)) {
        HTMLSelectElement& element = downcast<HTMLSelectElement>(*focusedElement);
        information.elementType = InputType::Select;

        int parentGroupID = 0;
        // The parent group ID indicates the group the option belongs to and is 0 for group elements.
        // If there are option elements in between groups, they are given it's own group identifier.
        // If a select does not have groups, all the option elements have group ID 0.
        for (auto& item : element.listItems()) {
            if (auto* optionElement = dynamicDowncast<HTMLOptionElement>(item.get()))
                information.selectOptions.append(OptionItem(optionElement->text(), false, optionElement->selected(), optionElement->hasAttributeWithoutSynchronization(WebCore::HTMLNames::disabledAttr), parentGroupID));
            else if (auto* optGroupElement = dynamicDowncast<HTMLOptGroupElement>(item.get())) {
                parentGroupID++;
                information.selectOptions.append(OptionItem(optGroupElement->groupLabelText(), true, false, optGroupElement->hasAttributeWithoutSynchronization(WebCore::HTMLNames::disabledAttr), 0));
            }
        }
        information.selectedIndex = element.selectedIndex();
        information.isMultiSelect = element.multiple();
    } else if (is<HTMLTextAreaElement>(*focusedElement)) {
        HTMLTextAreaElement& element = downcast<HTMLTextAreaElement>(*focusedElement);
        information.autocapitalizeType = element.autocapitalizeType();
        information.isAutocorrect = element.shouldAutocorrect();
        information.elementType = InputType::TextArea;
        information.isReadOnly = element.isReadOnly();
        information.value = element.value();
        information.autofillFieldName = WebCore::toAutofillFieldName(element.autofillData().fieldName);
        information.nonAutofillCredentialType = element.autofillData().nonAutofillCredentialType;
        information.placeholder = element.attributeWithoutSynchronization(HTMLNames::placeholderAttr);
        information.inputMode = element.canonicalInputMode();
        information.enterKeyHint = element.canonicalEnterKeyHint();
    } else if (is<HTMLInputElement>(*focusedElement)) {
        HTMLInputElement& element = downcast<HTMLInputElement>(*focusedElement);
        HTMLFormElement* form = element.form();
        if (form)
            information.formAction = form->getURLAttribute(WebCore::HTMLNames::actionAttr).string();
        if (auto autofillElements = WebCore::AutofillElements::computeAutofillElements(element)) {
            information.acceptsAutofilledLoginCredentials = true;
            information.isAutofillableUsernameField = autofillElements->username() == focusedElement;
        }
        information.representingPageURL = element.document().urlForBindings();
        information.autocapitalizeType = element.autocapitalizeType();
        information.isAutocorrect = element.shouldAutocorrect();
        information.placeholder = element.attributeWithoutSynchronization(HTMLNames::placeholderAttr);
        if (element.isPasswordField())
            information.elementType = InputType::Password;
        else if (element.isSearchField())
            information.elementType = InputType::Search;
        else if (element.isEmailField())
            information.elementType = InputType::Email;
        else if (element.isTelephoneField())
            information.elementType = InputType::Phone;
        else if (element.isNumberField())
            information.elementType = element.getAttribute(HTMLNames::patternAttr) == "\\d*"_s || element.getAttribute(HTMLNames::patternAttr) == "[0-9]*"_s ? InputType::NumberPad : InputType::Number;
        else if (element.isDateTimeLocalField())
            information.elementType = InputType::DateTimeLocal;
        else if (element.isDateField())
            information.elementType = InputType::Date;
        else if (element.isTimeField())
            information.elementType = InputType::Time;
        else if (element.isWeekField())
            information.elementType = InputType::Week;
        else if (element.isMonthField())
            information.elementType = InputType::Month;
        else if (element.isURLField())
            information.elementType = InputType::URL;
        else if (element.isText()) {
            const AtomString& pattern = element.attributeWithoutSynchronization(HTMLNames::patternAttr);
            if (pattern == "\\d*"_s || pattern == "[0-9]*"_s)
                information.elementType = InputType::NumberPad;
            else {
                information.elementType = InputType::Text;
                if (!information.formAction.isEmpty()
                    && (element.getNameAttribute().contains("search"_s) || element.getIdAttribute().contains("search"_s) || element.attributeWithoutSynchronization(HTMLNames::titleAttr).contains("search"_s)))
                    information.elementType = InputType::Search;
            }
        }
#if ENABLE(INPUT_TYPE_COLOR)
        else if (element.isColorControl()) {
            information.elementType = InputType::Color;
            information.colorValue = element.valueAsColor();
#if ENABLE(DATALIST_ELEMENT)
            information.suggestedColors = element.suggestedColors();
#endif
        }
#endif

#if ENABLE(DATALIST_ELEMENT)
        information.isFocusingWithDataListDropdown = element.isFocusingWithDataListDropdown();
        information.hasSuggestions = !!element.list();
#endif
        information.inputMode = element.canonicalInputMode();
        information.enterKeyHint = element.canonicalEnterKeyHint();
        information.isReadOnly = element.isReadOnly();
        information.value = element.value();
        information.valueAsNumber = element.valueAsNumber();
        information.autofillFieldName = WebCore::toAutofillFieldName(element.autofillData().fieldName);
        information.nonAutofillCredentialType = element.autofillData().nonAutofillCredentialType;
    } else if (focusedElement->hasEditableStyle()) {
        information.elementType = InputType::ContentEditable;
        if (is<HTMLElement>(*focusedElement)) {
            auto& focusedHTMLElement = downcast<HTMLElement>(*focusedElement);
            information.isAutocorrect = focusedHTMLElement.shouldAutocorrect();
            information.autocapitalizeType = focusedHTMLElement.autocapitalizeType();
            information.inputMode = focusedHTMLElement.canonicalInputMode();
            information.enterKeyHint = focusedHTMLElement.canonicalEnterKeyHint();
            information.shouldSynthesizeKeyEventsForEditing = focusedHTMLElement.document().settings().syntheticEditingCommandsEnabled();
        } else {
            information.isAutocorrect = true;
            information.autocapitalizeType = WebCore::AutocapitalizeType::Default;
        }
        information.isReadOnly = false;
    }

    if (focusedElement->document().quirks().shouldSuppressAutocorrectionAndAutocapitalizationInHiddenEditableAreas() && isTransparentOrFullyClipped(*focusedElement)) {
        information.autocapitalizeType = WebCore::AutocapitalizeType::None;
        information.isAutocorrect = false;
    }

    auto& quirks = focusedElement->document().quirks();
    information.shouldAvoidResizingWhenInputViewBoundsChange = quirks.shouldAvoidResizingWhenInputViewBoundsChange();
    information.shouldAvoidScrollingWhenFocusedContentIsVisible = quirks.shouldAvoidScrollingWhenFocusedContentIsVisible();
    information.shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation = quirks.shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation();

    return information;
}

void WebPage::autofillLoginCredentials(const String& username, const String& password)
{
    if (is<HTMLInputElement>(m_focusedElement)) {
        if (auto autofillElements = AutofillElements::computeAutofillElements(downcast<HTMLInputElement>(*m_focusedElement)))
            autofillElements->autofill(username, password);
    }
}

void WebPage::setViewportConfigurationViewLayoutSize(const FloatSize& size, double scaleFactor, double minimumEffectiveDeviceWidth)
{
    LOG_WITH_STREAM(VisibleRects, stream << "WebPage " << m_identifier << " setViewportConfigurationViewLayoutSize " << size << " scaleFactor " << scaleFactor << " minimumEffectiveDeviceWidth " << minimumEffectiveDeviceWidth);

    if (!m_viewportConfiguration.isKnownToLayOutWiderThanViewport())
        m_viewportConfiguration.setMinimumEffectiveDeviceWidthForShrinkToFit(0);

    auto previousLayoutSizeScaleFactor = m_viewportConfiguration.layoutSizeScaleFactor();
    if (!m_viewportConfiguration.setViewLayoutSize(size, scaleFactor, minimumEffectiveDeviceWidth))
        return;

    auto zoomToInitialScale = ZoomToInitialScale::No;
    auto newInitialScale = m_viewportConfiguration.initialScale();
    auto currentPageScaleFactor = pageScaleFactor();
    if (scaleFactor > previousLayoutSizeScaleFactor && newInitialScale > currentPageScaleFactor)
        zoomToInitialScale = ZoomToInitialScale::Yes;
    else if (scaleFactor < previousLayoutSizeScaleFactor && newInitialScale < currentPageScaleFactor)
        zoomToInitialScale = ZoomToInitialScale::Yes;

    viewportConfigurationChanged(zoomToInitialScale);
}

void WebPage::setDeviceOrientation(int32_t deviceOrientation)
{
    if (deviceOrientation == m_deviceOrientation)
        return;
    m_deviceOrientation = deviceOrientation;
#if ENABLE(ORIENTATION_EVENTS)
    m_page->mainFrame().orientationChanged();
#endif
}

void WebPage::setOverrideViewportArguments(const std::optional<WebCore::ViewportArguments>& arguments)
{
    m_page->setOverrideViewportArguments(arguments);
}

void WebPage::dynamicViewportSizeUpdate(const FloatSize& viewLayoutSize, const WebCore::FloatSize& minimumUnobscuredSize, const WebCore::FloatSize& maximumUnobscuredSize, const FloatRect& targetExposedContentRect, const FloatRect& targetUnobscuredRect, const WebCore::FloatRect& targetUnobscuredRectInScrollViewCoordinates, const WebCore::FloatBoxExtent& targetUnobscuredSafeAreaInsets, double targetScale, int32_t deviceOrientation, double minimumEffectiveDeviceWidth, DynamicViewportSizeUpdateID dynamicViewportSizeUpdateID)
{
    SetForScope dynamicSizeUpdateGuard(m_inDynamicSizeUpdate, true);
    // FIXME: this does not handle the cases where the content would change the content size or scroll position from JavaScript.
    // To handle those cases, we would need to redo this computation on every change until the next visible content rect update.
    LOG_WITH_STREAM(VisibleRects, stream << "\nWebPage::dynamicViewportSizeUpdate - viewLayoutSize " << viewLayoutSize << " targetUnobscuredRect " << targetUnobscuredRect << " targetExposedContentRect " << targetExposedContentRect << " targetScale " << targetScale);

    FrameView& frameView = *m_page->mainFrame().view();
    IntSize oldContentSize = frameView.contentsSize();
    float oldPageScaleFactor = m_page->pageScaleFactor();
    auto oldUnobscuredContentRect = frameView.unobscuredContentRect();
    bool wasAtInitialScale = scalesAreEssentiallyEqual(oldPageScaleFactor, m_viewportConfiguration.initialScale());

    m_dynamicSizeUpdateHistory.add(std::make_pair(oldContentSize, oldPageScaleFactor), frameView.scrollPosition());

    RefPtr<Node> oldNodeAtCenter;
    double visibleHorizontalFraction = 1;
    float relativeHorizontalPositionInNodeAtCenter = 0;
    float relativeVerticalPositionInNodeAtCenter = 0;
    if (!shouldEnableViewportBehaviorsForResizableWindows()) {
        visibleHorizontalFraction = frameView.unobscuredContentSize().width() / oldContentSize.width();
        IntPoint unobscuredContentRectCenter = frameView.unobscuredContentRect().center();

        HitTestResult hitTestResult = HitTestResult(unobscuredContentRectCenter);

        auto* localFrame = dynamicDowncast<WebCore::LocalFrame>(frameView.frame());
        if (auto* document = localFrame ? localFrame->document() : nullptr)
            document->hitTest(HitTestRequest(), hitTestResult);

        if (Node* node = hitTestResult.innerNode()) {
            if (RenderObject* renderer = node->renderer()) {
                FrameView& containingView = *node->document().frame()->view();
                FloatRect boundingBox = containingView.contentsToRootView(renderer->absoluteBoundingBoxRect(true));
                relativeHorizontalPositionInNodeAtCenter = (unobscuredContentRectCenter.x() - boundingBox.x()) / boundingBox.width();
                relativeVerticalPositionInNodeAtCenter = (unobscuredContentRectCenter.y() - boundingBox.y()) / boundingBox.height();
                oldNodeAtCenter = node;
            }
        }
    }

    LOG_WITH_STREAM(VisibleRects, stream << "WebPage::dynamicViewportSizeUpdate setting view layout size to " << viewLayoutSize);
    bool viewportChanged = m_viewportConfiguration.setIsKnownToLayOutWiderThanViewport(false);
    viewportChanged |= m_viewportConfiguration.setViewLayoutSize(viewLayoutSize, std::nullopt, minimumEffectiveDeviceWidth);
    if (viewportChanged)
        viewportConfigurationChanged();

    IntSize newLayoutSize = m_viewportConfiguration.layoutSize();

    if (setFixedLayoutSize(newLayoutSize))
        resetTextAutosizing();

    setDefaultUnobscuredSize(maximumUnobscuredSize);
    setMinimumUnobscuredSize(minimumUnobscuredSize);
    setMaximumUnobscuredSize(maximumUnobscuredSize);
    m_page->setUnobscuredSafeAreaInsets(targetUnobscuredSafeAreaInsets);

    frameView.updateLayoutAndStyleIfNeededRecursive();

    IntSize newContentSize = frameView.contentsSize();

    bool scaleToFitContent = (!shouldEnableViewportBehaviorsForResizableWindows() || !wasAtInitialScale) && m_userHasChangedPageScaleFactor;
    double scale = scaleAfterViewportWidthChange(targetScale, scaleToFitContent, m_viewportConfiguration, targetUnobscuredRectInScrollViewCoordinates.width(), newContentSize, oldContentSize, visibleHorizontalFraction);
    FloatRect newUnobscuredContentRect = targetUnobscuredRect;
    FloatRect newExposedContentRect = targetExposedContentRect;

    bool scaleChanged = !scalesAreEssentiallyEqual(scale, targetScale);
    if (scaleChanged) {
        // The target scale the UI is using cannot be reached by the content. We need to compute new targets based
        // on the viewport constraint and report everything back to the UIProcess.

        // 1) Compute a new unobscured rect centered around the original one.
        double scaleDifference = targetScale / scale;
        double newUnobscuredRectWidth = targetUnobscuredRect.width() * scaleDifference;
        double newUnobscuredRectHeight = targetUnobscuredRect.height() * scaleDifference;
        double newUnobscuredRectX;
        double newUnobscuredRectY;
        if (shouldEnableViewportBehaviorsForResizableWindows()) {
            newUnobscuredRectX = oldUnobscuredContentRect.x();
            newUnobscuredRectY = oldUnobscuredContentRect.y();
        } else {
            newUnobscuredRectX = targetUnobscuredRect.x() - (newUnobscuredRectWidth - targetUnobscuredRect.width()) / 2;
            newUnobscuredRectY = targetUnobscuredRect.y() - (newUnobscuredRectHeight - targetUnobscuredRect.height()) / 2;
        }
        newUnobscuredContentRect = FloatRect(newUnobscuredRectX, newUnobscuredRectY, newUnobscuredRectWidth, newUnobscuredRectHeight);

        // 2) Extend our new unobscuredRect by the obscured margins to get a new exposed rect.
        double obscuredTopMargin = (targetUnobscuredRect.y() - targetExposedContentRect.y()) * scaleDifference;
        double obscuredLeftMargin = (targetUnobscuredRect.x() - targetExposedContentRect.x()) * scaleDifference;
        double obscuredBottomMargin = (targetExposedContentRect.maxY() - targetUnobscuredRect.maxY()) * scaleDifference;
        double obscuredRightMargin = (targetExposedContentRect.maxX() - targetUnobscuredRect.maxX()) * scaleDifference;
        newExposedContentRect = FloatRect(newUnobscuredRectX - obscuredLeftMargin,
                                          newUnobscuredRectY - obscuredTopMargin,
                                          newUnobscuredRectWidth + obscuredLeftMargin + obscuredRightMargin,
                                          newUnobscuredRectHeight + obscuredTopMargin + obscuredBottomMargin);
    }

    if (oldContentSize != newContentSize || scaleChanged) {
        // Snap the new unobscured rect back into the content rect.
        newUnobscuredContentRect.setWidth(std::min(static_cast<float>(newContentSize.width()), newUnobscuredContentRect.width()));
        newUnobscuredContentRect.setHeight(std::min(static_cast<float>(newContentSize.height()), newUnobscuredContentRect.height()));

        bool positionWasRestoredFromSizeUpdateHistory = false;
        const auto& previousPosition = m_dynamicSizeUpdateHistory.find(std::pair<IntSize, float>(newContentSize, scale));
        if (previousPosition != m_dynamicSizeUpdateHistory.end()) {
            IntPoint restoredPosition = previousPosition->value;
            FloatPoint deltaPosition(restoredPosition.x() - newUnobscuredContentRect.x(), restoredPosition.y() - newUnobscuredContentRect.y());
            newUnobscuredContentRect.moveBy(deltaPosition);
            newExposedContentRect.moveBy(deltaPosition);
            positionWasRestoredFromSizeUpdateHistory = true;
        } else if (oldContentSize != newContentSize) {
            FloatPoint newRelativeContentCenter;

            if (RenderObject* renderer = oldNodeAtCenter ? oldNodeAtCenter->renderer() : nullptr) {
                FrameView& containingView = *oldNodeAtCenter->document().frame()->view();
                FloatRect newBoundingBox = containingView.contentsToRootView(renderer->absoluteBoundingBoxRect(true));
                newRelativeContentCenter = FloatPoint(newBoundingBox.x() + relativeHorizontalPositionInNodeAtCenter * newBoundingBox.width(), newBoundingBox.y() + relativeVerticalPositionInNodeAtCenter * newBoundingBox.height());
            } else
                newRelativeContentCenter = relativeCenterAfterContentSizeChange(targetUnobscuredRect, oldContentSize, newContentSize);

            FloatPoint newUnobscuredContentRectCenter = newUnobscuredContentRect.center();
            FloatPoint positionDelta(newRelativeContentCenter.x() - newUnobscuredContentRectCenter.x(), newRelativeContentCenter.y() - newUnobscuredContentRectCenter.y());
            newUnobscuredContentRect.moveBy(positionDelta);
            newExposedContentRect.moveBy(positionDelta);
        }

        // Make the top/bottom edges "sticky" within 1 pixel.
        if (!positionWasRestoredFromSizeUpdateHistory) {
            if (targetUnobscuredRect.maxY() > oldContentSize.height() - 1) {
                float bottomVerticalPosition = newContentSize.height() - newUnobscuredContentRect.height();
                newUnobscuredContentRect.setY(bottomVerticalPosition);
                newExposedContentRect.setY(bottomVerticalPosition);
            }
            if (targetUnobscuredRect.y() < 1) {
                newUnobscuredContentRect.setY(0);
                newExposedContentRect.setY(0);
            }

            bool likelyResponsiveDesignViewport = newLayoutSize.width() == viewLayoutSize.width() && scalesAreEssentiallyEqual(scale, 1);
            bool contentBleedsOutsideLayoutWidth = newContentSize.width() > newLayoutSize.width();
            bool originalScrollPositionWasOnTheLeftEdge = targetUnobscuredRect.x() <= 0;
            if (likelyResponsiveDesignViewport && contentBleedsOutsideLayoutWidth && originalScrollPositionWasOnTheLeftEdge) {
                // This is a special heuristics for "responsive" design with odd layout. It is quite common for responsive design
                // to have content "bleeding" outside of the minimal layout width, usually from an image or table larger than expected.
                // In those cases, the design usually does not adapt to the new width and remain at the newLayoutSize except for the
                // large boxes.
                // It is worth revisiting this special case as web developers get better with responsive design.
                newExposedContentRect.setX(0);
                newUnobscuredContentRect.setX(0);
            }
        }

        float horizontalAdjustment = 0;
        if (newUnobscuredContentRect.maxX() > newContentSize.width())
            horizontalAdjustment -= newUnobscuredContentRect.maxX() - newContentSize.width();
        float verticalAdjustment = 0;
        if (newUnobscuredContentRect.maxY() > newContentSize.height())
            verticalAdjustment -= newUnobscuredContentRect.maxY() - newContentSize.height();
        if (newUnobscuredContentRect.x() < 0)
            horizontalAdjustment += - newUnobscuredContentRect.x();
        if (newUnobscuredContentRect.y() < 0)
            verticalAdjustment += - newUnobscuredContentRect.y();

        FloatPoint adjustmentDelta(horizontalAdjustment, verticalAdjustment);
        newUnobscuredContentRect.moveBy(adjustmentDelta);
        newExposedContentRect.moveBy(adjustmentDelta);
    }

    frameView.setScrollVelocity({ 0, 0, 0, MonotonicTime::now() });

    IntPoint roundedUnobscuredContentRectPosition = roundedIntPoint(newUnobscuredContentRect.location());
    frameView.setUnobscuredContentSize(newUnobscuredContentRect.size());
    m_drawingArea->setExposedContentRect(newExposedContentRect);

    scalePage(scale, roundedUnobscuredContentRectPosition);

    frameView.updateLayoutAndStyleIfNeededRecursive();

    // FIXME: Move settings from Frame to AbstractFrame and remove this check.
    auto* localFrame = dynamicDowncast<LocalFrame>(frameView.frame());
    if (!localFrame)
        return;
    auto& settings = localFrame->settings();
    LayoutRect documentRect = IntRect(frameView.scrollOrigin(), frameView.contentsSize());
    auto layoutViewportSize = FrameView::expandedLayoutViewportSize(frameView.baseLayoutViewportSize(), LayoutSize(documentRect.size()), settings.layoutViewportHeightExpansionFactor());
    LayoutRect layoutViewportRect = FrameView::computeUpdatedLayoutViewportRect(frameView.layoutViewportRect(), documentRect, LayoutSize(newUnobscuredContentRect.size()), LayoutRect(newUnobscuredContentRect), layoutViewportSize, frameView.minStableLayoutViewportOrigin(), frameView.maxStableLayoutViewportOrigin(), FrameView::LayoutViewportConstraint::ConstrainedToDocumentRect);
    frameView.setLayoutViewportOverrideRect(layoutViewportRect);
    frameView.layoutOrVisualViewportChanged();

    frameView.setCustomSizeForResizeEvent(expandedIntSize(targetUnobscuredRectInScrollViewCoordinates.size()));
    setDeviceOrientation(deviceOrientation);
    frameView.setScrollOffset(roundedUnobscuredContentRectPosition);

    m_page->isolatedUpdateRendering();

#if ENABLE(VIEWPORT_RESIZING)
    shrinkToFitContent();
#endif

    m_drawingArea->triggerRenderingUpdate();

    m_pendingDynamicViewportSizeUpdateID = dynamicViewportSizeUpdateID;
}

void WebPage::resetViewportDefaultConfiguration(WebFrame* frame, bool hasMobileDocType)
{
    LOG_WITH_STREAM(VisibleRects, stream << "WebPage " << m_identifier << " resetViewportDefaultConfiguration");
    if (m_useTestingViewportConfiguration) {
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::testingParameters());
        return;
    }

    auto parametersForStandardFrame = [&] {
        if (shouldIgnoreMetaViewport())
            return m_viewportConfiguration.nativeWebpageParameters();
        return ViewportConfiguration::webpageParameters();
    };

    if (!frame) {
        m_viewportConfiguration.setDefaultConfiguration(parametersForStandardFrame());
        return;
    }

    if (hasMobileDocType) {
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::xhtmlMobileParameters());
        return;
    }

    auto* document = frame->coreFrame()->document();
    if (document->isImageDocument())
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::imageDocumentParameters());
    else if (document->isTextDocument())
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::textDocumentParameters());
    else
        m_viewportConfiguration.setDefaultConfiguration(parametersForStandardFrame());
}

#if ENABLE(TEXT_AUTOSIZING)
void WebPage::resetIdempotentTextAutosizingIfNeeded(double previousInitialScale)
{
    if (!m_page->settings().textAutosizingEnabled() || !m_page->settings().textAutosizingUsesIdempotentMode())
        return;

    const float minimumScaleChangeBeforeRecomputingTextAutosizing = 0.01;
    if (std::abs(previousInitialScale - m_page->initialScaleIgnoringContentSize()) < minimumScaleChangeBeforeRecomputingTextAutosizing)
        return;

    if (m_page->initialScaleIgnoringContentSize() >= 1 && previousInitialScale >= 1)
        return;

    if (!m_page->mainFrame().view())
        return;

    auto textAutoSizingDelay = [&] {
        auto& frameView = *m_page->mainFrame().view();
        if (!frameView.isVisuallyNonEmpty()) {
            // We don't anticipate any painting after the next upcoming layout.
            const Seconds longTextAutoSizingDelayOnViewportChange = 100_ms;
            return longTextAutoSizingDelayOnViewportChange;
        }
        const Seconds defaultTextAutoSizingDelayOnViewportChange = 80_ms;
        return defaultTextAutoSizingDelayOnViewportChange;
    };

    // We don't need to update text sizing eagerly. There might be multiple incoming dynamic viewport changes.
    m_textAutoSizingAdjustmentTimer.startOneShot(textAutoSizingDelay());
}
#endif // ENABLE(TEXT_AUTOSIZING)

void WebPage::resetTextAutosizing()
{
#if ENABLE(TEXT_AUTOSIZING)
    for (AbstractFrame* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        auto* document = localFrame->document();
        if (!document || !document->renderView())
            continue;
        document->renderView()->resetTextAutosizing();
    }
#endif
}

#if ENABLE(VIEWPORT_RESIZING)

void WebPage::shrinkToFitContent(ZoomToInitialScale zoomToInitialScale)
{
    if (m_isClosed)
        return;

    if (!m_page->settings().allowViewportShrinkToFitContent())
        return;

    if (m_useTestingViewportConfiguration)
        return;

    if (!shouldIgnoreMetaViewport())
        return;

    if (!m_viewportConfiguration.viewportArguments().shrinkToFit)
        return;

    if (m_viewportConfiguration.canIgnoreScalingConstraints())
        return;

    RefPtr mainFrame = m_mainFrame->coreFrame();
    if (!mainFrame)
        return;

    RefPtr view = mainFrame->view();
    RefPtr mainDocument = mainFrame->document();
    if (!view || !mainDocument)
        return;

    mainDocument->updateLayout();

    static const int toleratedHorizontalScrollingDistance = 20;
    static const int maximumExpandedLayoutWidth = 1280;
    static const int maximumContentWidthBeforeAvoidingShrinkToFit = 1920;

    auto scaledViewWidth = [&] () -> int {
        return std::round(m_viewportConfiguration.viewLayoutSize().width() / m_viewportConfiguration.initialScale());
    };

    int originalContentWidth = view->contentsWidth();
    int originalViewWidth = scaledViewWidth();
    int originalLayoutWidth = m_viewportConfiguration.layoutWidth();
    int originalHorizontalOverflowAmount = originalContentWidth - originalViewWidth;
    if (originalHorizontalOverflowAmount <= toleratedHorizontalScrollingDistance || originalLayoutWidth >= maximumExpandedLayoutWidth || originalContentWidth <= originalViewWidth || originalContentWidth > maximumContentWidthBeforeAvoidingShrinkToFit)
        return;

    auto changeMinimumEffectiveDeviceWidth = [this, mainDocument] (int targetLayoutWidth) -> bool {
        if (m_viewportConfiguration.setMinimumEffectiveDeviceWidthForShrinkToFit(targetLayoutWidth)) {
            viewportConfigurationChanged();
            mainDocument->updateLayout();
            return true;
        }
        return false;
    };

    m_viewportConfiguration.setIsKnownToLayOutWiderThanViewport(true);
    double originalMinimumDeviceWidth = m_viewportConfiguration.minimumEffectiveDeviceWidth();
    if (changeMinimumEffectiveDeviceWidth(std::min(maximumExpandedLayoutWidth, originalContentWidth)) && view->contentsWidth() - scaledViewWidth() > originalHorizontalOverflowAmount) {
        changeMinimumEffectiveDeviceWidth(originalMinimumDeviceWidth);
        m_viewportConfiguration.setIsKnownToLayOutWiderThanViewport(false);
    }

    // FIXME (197429): Consider additionally logging an error message to the console if a responsive meta viewport tag was used.
    RELEASE_LOG(ViewportSizing, "Shrink-to-fit: content width %d => %d; layout width %d => %d", originalContentWidth, view->contentsWidth(), originalLayoutWidth, m_viewportConfiguration.layoutWidth());
    viewportConfigurationChanged(zoomToInitialScale);
}

#endif // ENABLE(VIEWPORT_RESIZING)

bool WebPage::shouldIgnoreMetaViewport() const
{
    if (auto* mainDocument = m_page->mainFrame().document()) {
        auto* loader = mainDocument->loader();
        if (loader && loader->metaViewportPolicy() == WebCore::MetaViewportPolicy::Ignore)
            return true;
    }
    return m_page->settings().shouldIgnoreMetaViewport();
}

bool WebPage::shouldEnableViewportBehaviorsForResizableWindows() const
{
#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    return shouldIgnoreMetaViewport() && m_isWindowResizingEnabled;
#else
    return false;
#endif
}

void WebPage::viewportConfigurationChanged(ZoomToInitialScale zoomToInitialScale)
{
    double initialScale = m_viewportConfiguration.initialScale();
    double initialScaleIgnoringContentSize = m_viewportConfiguration.initialScaleIgnoringContentSize();
#if ENABLE(TEXT_AUTOSIZING)
    double previousInitialScaleIgnoringContentSize = m_page->initialScaleIgnoringContentSize();
    m_page->setInitialScaleIgnoringContentSize(initialScaleIgnoringContentSize);
    resetIdempotentTextAutosizingIfNeeded(previousInitialScaleIgnoringContentSize);
#endif
    if (setFixedLayoutSize(m_viewportConfiguration.layoutSize()))
        resetTextAutosizing();

    double scale;
    if (m_userHasChangedPageScaleFactor && zoomToInitialScale == ZoomToInitialScale::No)
        scale = std::max(std::min(pageScaleFactor(), m_viewportConfiguration.maximumScale()), m_viewportConfiguration.minimumScale());
    else
        scale = initialScale;

    LOG_WITH_STREAM(VisibleRects, stream << "WebPage " << m_identifier << " viewportConfigurationChanged - setting zoomedOutPageScaleFactor to " << m_viewportConfiguration.minimumScale() << " and scale to " << scale);

    m_page->setZoomedOutPageScaleFactor(m_viewportConfiguration.minimumScale());

    updateSizeForCSSDefaultViewportUnits();
    updateSizeForCSSSmallViewportUnits();
    updateSizeForCSSLargeViewportUnits();

    FrameView& frameView = *mainFrameView();
    IntPoint scrollPosition = frameView.scrollPosition();
    if (!m_hasReceivedVisibleContentRectsAfterDidCommitLoad) {
        FloatSize minimumLayoutSizeInScrollViewCoordinates = m_viewportConfiguration.viewLayoutSize();
        minimumLayoutSizeInScrollViewCoordinates.scale(1 / scale);
        IntSize minimumLayoutSizeInDocumentCoordinates = roundedIntSize(minimumLayoutSizeInScrollViewCoordinates);
        frameView.setUnobscuredContentSize(minimumLayoutSizeInDocumentCoordinates);
        frameView.setScrollVelocity({ 0, 0, 0, MonotonicTime::now() });

        // FIXME: We could send down the obscured margins to find a better exposed rect and unobscured rect.
        // It is not a big deal at the moment because the tile coverage will always extend past the obscured bottom inset.
        if (!m_hasRestoredExposedContentRectAfterDidCommitLoad)
            m_drawingArea->setExposedContentRect(FloatRect(scrollPosition, minimumLayoutSizeInDocumentCoordinates));
    }
    scalePage(scale, scrollPosition);
    
    if (!m_hasReceivedVisibleContentRectsAfterDidCommitLoad) {
        // This takes scale into account, so do after the scale change.
        frameView.setCustomFixedPositionLayoutRect(enclosingIntRect(frameView.viewportConstrainedObjectsRect()));

        frameView.setCustomSizeForResizeEvent(expandedIntSize(m_viewportConfiguration.minimumLayoutSize()));
    }
}

void WebPage::applicationWillResignActive()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebUIApplicationWillResignActiveNotification object:nil];

    // FIXME(224775): Move to WebProcess
    if (auto* manager = PlatformMediaSessionManager::sharedManagerIfExists())
        manager->applicationWillBecomeInactive();

    if (m_page)
        m_page->applicationWillResignActive();
}

void WebPage::applicationDidEnterBackground(bool isSuspendedUnderLock)
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebUIApplicationDidEnterBackgroundNotification object:nil userInfo:@{@"isSuspendedUnderLock": @(isSuspendedUnderLock)}];

    m_isSuspendedUnderLock = isSuspendedUnderLock;
    freezeLayerTree(LayerTreeFreezeReason::BackgroundApplication);

    // FIXME(224775): Move to WebProcess
    if (auto* manager = PlatformMediaSessionManager::sharedManagerIfExists())
        manager->applicationDidEnterBackground(isSuspendedUnderLock);

    if (m_page)
        m_page->applicationDidEnterBackground();
}

void WebPage::applicationDidFinishSnapshottingAfterEnteringBackground()
{
    markLayersVolatile();
}

void WebPage::applicationWillEnterForeground(bool isSuspendedUnderLock)
{
    m_isSuspendedUnderLock = false;
    cancelMarkLayersVolatile();

    unfreezeLayerTree(LayerTreeFreezeReason::BackgroundApplication);

    [[NSNotificationCenter defaultCenter] postNotificationName:WebUIApplicationWillEnterForegroundNotification object:nil userInfo:@{@"isSuspendedUnderLock": @(isSuspendedUnderLock)}];

    // FIXME(224775): Move to WebProcess
    if (auto* manager = PlatformMediaSessionManager::sharedManagerIfExists())
        manager->applicationWillEnterForeground(isSuspendedUnderLock);

    if (m_page)
        m_page->applicationWillEnterForeground();
}

void WebPage::applicationDidBecomeActive()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebUIApplicationDidBecomeActiveNotification object:nil];

    // FIXME(224775): Move to WebProcess
    if (auto* manager = PlatformMediaSessionManager::sharedManagerIfExists())
        manager->applicationDidBecomeActive();

    if (m_page)
        m_page->applicationDidBecomeActive();
}

void WebPage::applicationDidEnterBackgroundForMedia(bool isSuspendedUnderLock)
{
    if (auto* manager = PlatformMediaSessionManager::sharedManagerIfExists())
        manager->applicationDidEnterBackground(isSuspendedUnderLock);
}

void WebPage::applicationWillEnterForegroundForMedia(bool isSuspendedUnderLock)
{
    if (auto* manager = PlatformMediaSessionManager::sharedManagerIfExists())
        manager->applicationWillEnterForeground(isSuspendedUnderLock);
}

static inline void adjustVelocityDataForBoundedScale(VelocityData& velocityData, double exposedRectScale, double minimumScale, double maximumScale)
{
    if (velocityData.scaleChangeRate) {
        velocityData.horizontalVelocity = 0;
        velocityData.verticalVelocity = 0;
    }

    if (exposedRectScale >= maximumScale || exposedRectScale <= minimumScale)
        velocityData.scaleChangeRate = 0;
}

std::optional<float> WebPage::scaleFromUIProcess(const VisibleContentRectUpdateInfo& visibleContentRectUpdateInfo) const
{
    auto transactionIDForLastScaleFromUIProcess = visibleContentRectUpdateInfo.lastLayerTreeTransactionID();
    if (m_lastTransactionIDWithScaleChange > transactionIDForLastScaleFromUIProcess)
        return std::nullopt;

    float scaleFromUIProcess = visibleContentRectUpdateInfo.scale();
    float currentScale = m_page->pageScaleFactor();

    double scaleNoiseThreshold = 0.005;
    if (!m_isInStableState && fabs(scaleFromUIProcess - currentScale) < scaleNoiseThreshold) {
        // Tiny changes of scale during interactive zoom cause content to jump by one pixel, creating
        // visual noise. We filter those useless updates.
        scaleFromUIProcess = currentScale;
    }
    
    scaleFromUIProcess = std::min<float>(m_viewportConfiguration.maximumScale(), std::max<float>(m_viewportConfiguration.minimumScale(), scaleFromUIProcess));
    if (scalesAreEssentiallyEqual(currentScale, scaleFromUIProcess))
        return std::nullopt;

    return scaleFromUIProcess;
}

static bool selectionIsInsideFixedPositionContainer(Frame& frame)
{
    auto& selection = frame.selection().selection();
    if (selection.isNone())
        return false;

    bool isInsideFixedPosition = false;
    if (selection.isCaret()) {
        frame.selection().absoluteCaretBounds(&isInsideFixedPosition);
        return isInsideFixedPosition;
    }

    selection.visibleStart().absoluteCaretBounds(&isInsideFixedPosition);
    if (isInsideFixedPosition)
        return true;

    selection.visibleEnd().absoluteCaretBounds(&isInsideFixedPosition);
    return isInsideFixedPosition;
}

void WebPage::updateVisibleContentRects(const VisibleContentRectUpdateInfo& visibleContentRectUpdateInfo, MonotonicTime oldestTimestamp)
{
    LOG_WITH_STREAM(VisibleRects, stream << "\nWebPage " << m_identifier << " updateVisibleContentRects " << visibleContentRectUpdateInfo);

    // Skip any VisibleContentRectUpdate that have been queued before DidCommitLoad suppresses the updates in the UIProcess.
    if (visibleContentRectUpdateInfo.lastLayerTreeTransactionID() < m_mainFrame->firstLayerTreeTransactionIDAfterDidCommitLoad() && !visibleContentRectUpdateInfo.isFirstUpdateForNewViewSize())
        return;

    m_hasReceivedVisibleContentRectsAfterDidCommitLoad = true;
    m_isInStableState = visibleContentRectUpdateInfo.inStableState();

    auto scaleFromUIProcess = this->scaleFromUIProcess(visibleContentRectUpdateInfo);

    // Skip progressively redrawing tiles if pinch-zooming while the system is under memory pressure.
    if (scaleFromUIProcess && !m_isInStableState && MemoryPressureHandler::singleton().isUnderMemoryPressure())
        return;

    if (m_isInStableState)
        m_hasStablePageScaleFactor = true;
    else {
        if (!m_oldestNonStableUpdateVisibleContentRectsTimestamp)
            m_oldestNonStableUpdateVisibleContentRectsTimestamp = oldestTimestamp;
    }

    float scaleToUse = scaleFromUIProcess.value_or(m_page->pageScaleFactor());
    FloatRect exposedContentRect = visibleContentRectUpdateInfo.exposedContentRect();
    FloatRect adjustedExposedContentRect = adjustExposedRectForNewScale(exposedContentRect, visibleContentRectUpdateInfo.scale(), scaleToUse);
    m_drawingArea->setExposedContentRect(adjustedExposedContentRect);

    auto& frame = m_page->mainFrame();
    FrameView& frameView = *frame.view();

    if (auto* scrollingCoordinator = this->scrollingCoordinator()) {
        auto& remoteScrollingCoordinator = downcast<RemoteScrollingCoordinator>(*scrollingCoordinator);
        if (auto mainFrameScrollingNodeID = frameView.scrollingNodeID()) {
            if (visibleContentRectUpdateInfo.viewStability().contains(ViewStabilityFlag::ScrollViewRubberBanding))
                remoteScrollingCoordinator.addNodeWithActiveRubberBanding(mainFrameScrollingNodeID);
            else
                remoteScrollingCoordinator.removeNodeWithActiveRubberBanding(mainFrameScrollingNodeID);
        }
    }

    IntPoint scrollPosition = roundedIntPoint(visibleContentRectUpdateInfo.unobscuredContentRect().location());

    bool pageHasBeenScaledSinceLastLayerTreeCommitThatChangedPageScale = ([&] {
        if (!m_lastLayerTreeTransactionIdAndPageScaleBeforeScalingPage)
            return false;

        if (scalesAreEssentiallyEqual(scaleToUse, m_page->pageScaleFactor()))
            return false;

        auto [transactionIdBeforeScalingPage, scaleBeforeScalingPage] = *m_lastLayerTreeTransactionIdAndPageScaleBeforeScalingPage;
        if (!scalesAreEssentiallyEqual(scaleBeforeScalingPage, scaleToUse))
            return false;

        return transactionIdBeforeScalingPage >= visibleContentRectUpdateInfo.lastLayerTreeTransactionID();
    })();

    if (!pageHasBeenScaledSinceLastLayerTreeCommitThatChangedPageScale) {
        bool hasSetPageScale = false;
        if (scaleFromUIProcess) {
            m_scaleWasSetByUIProcess = true;
            m_hasStablePageScaleFactor = m_isInStableState;

            m_dynamicSizeUpdateHistory.clear();

            m_page->setPageScaleFactor(scaleFromUIProcess.value(), scrollPosition, m_isInStableState);
            hasSetPageScale = true;
            send(Messages::WebPageProxy::PageScaleFactorDidChange(scaleFromUIProcess.value()));
        }

        if (!hasSetPageScale && m_isInStableState) {
            m_page->setPageScaleFactor(scaleToUse, scrollPosition, true);
            hasSetPageScale = true;
        }
    }

    if (scrollPosition != frameView.scrollPosition())
        m_dynamicSizeUpdateHistory.clear();

    if (m_viewportConfiguration.setCanIgnoreScalingConstraints(visibleContentRectUpdateInfo.allowShrinkToFit()))
        viewportConfigurationChanged();

    double minimumEffectiveDeviceWidthWhenIgnoringScalingConstraints = ([&] {
        RefPtr document = frame.document();
        if (!document)
            return 0;

        if (!document->quirks().shouldLayOutAtMinimumWindowWidthWhenIgnoringScalingConstraints())
            return 0;

        // This value is chosen to be close to the minimum width of a Safari window on macOS.
        return 500;
    })();

    if (m_viewportConfiguration.setMinimumEffectiveDeviceWidthWhenIgnoringScalingConstraints(minimumEffectiveDeviceWidthWhenIgnoringScalingConstraints))
        viewportConfigurationChanged();

    frameView.setUnobscuredContentSize(visibleContentRectUpdateInfo.unobscuredContentRect().size());
    m_page->setContentInsets(visibleContentRectUpdateInfo.contentInsets());
    m_page->setObscuredInsets(visibleContentRectUpdateInfo.obscuredInsets());
    m_page->setUnobscuredSafeAreaInsets(visibleContentRectUpdateInfo.unobscuredSafeAreaInsets());
    m_page->setEnclosedInScrollableAncestorView(visibleContentRectUpdateInfo.enclosedInScrollableAncestorView());

    VelocityData scrollVelocity = visibleContentRectUpdateInfo.scrollVelocity();
    adjustVelocityDataForBoundedScale(scrollVelocity, visibleContentRectUpdateInfo.scale(), m_viewportConfiguration.minimumScale(), m_viewportConfiguration.maximumScale());
    frameView.setScrollVelocity(scrollVelocity);

    if (m_isInStableState) {
        if (visibleContentRectUpdateInfo.unobscuredContentRect() != visibleContentRectUpdateInfo.unobscuredContentRectRespectingInputViewBounds())
            frameView.setVisualViewportOverrideRect(LayoutRect(visibleContentRectUpdateInfo.unobscuredContentRectRespectingInputViewBounds()));
        else
            frameView.setVisualViewportOverrideRect(std::nullopt);

        LOG_WITH_STREAM(VisibleRects, stream << "WebPage::updateVisibleContentRects - setLayoutViewportOverrideRect " << visibleContentRectUpdateInfo.layoutViewportRect());
        frameView.setLayoutViewportOverrideRect(LayoutRect(visibleContentRectUpdateInfo.layoutViewportRect()));
        if (selectionIsInsideFixedPositionContainer(frame)) {
            // Ensure that the next layer tree commit contains up-to-date caret/selection rects.
            if (auto* localFrame = dynamicDowncast<LocalFrame>(frameView.frame()))
                localFrame->selection().setCaretRectNeedsUpdate();
            scheduleFullEditorStateUpdate();
        }

        frameView.layoutOrVisualViewportChanged();
    }

    bool isChangingObscuredInsetsInteractively = visibleContentRectUpdateInfo.viewStability().contains(ViewStabilityFlag::ChangingObscuredInsetsInteractively);
    if (!isChangingObscuredInsetsInteractively)
        frameView.setCustomSizeForResizeEvent(expandedIntSize(visibleContentRectUpdateInfo.unobscuredRectInScrollViewCoordinates().size()));

    if (auto* scrollingCoordinator = this->scrollingCoordinator()) {
        auto viewportStability = ViewportRectStability::Stable;
        auto layerAction = ScrollingLayerPositionAction::Sync;
        
        if (isChangingObscuredInsetsInteractively) {
            viewportStability = ViewportRectStability::ChangingObscuredInsetsInteractively;
            layerAction = ScrollingLayerPositionAction::SetApproximate;
        } else if (!m_isInStableState) {
            viewportStability = ViewportRectStability::Unstable;
            layerAction = ScrollingLayerPositionAction::SetApproximate;
        }
        scrollingCoordinator->reconcileScrollingState(frameView, scrollPosition, visibleContentRectUpdateInfo.layoutViewportRect(), ScrollType::User, viewportStability, layerAction);
    }
}

void WebPage::willStartUserTriggeredZooming()
{
    m_page->diagnosticLoggingClient().logDiagnosticMessage(DiagnosticLoggingKeys::webViewKey(), DiagnosticLoggingKeys::userZoomActionKey(), ShouldSample::No);
    m_userHasChangedPageScaleFactor = true;
}

#if ENABLE(IOS_TOUCH_EVENTS)
void WebPage::dispatchAsynchronousTouchEvents(Vector<std::pair<WebTouchEvent, CompletionHandler<void(bool)>>, 1>&& queue)
{
    for (auto& eventAndCallbackID : queue) {
        bool handled = dispatchTouchEvent(eventAndCallbackID.first);
        if (auto& completionHandler = eventAndCallbackID.second)
            completionHandler(handled);
    }
}

void WebPage::cancelAsynchronousTouchEvents(Vector<std::pair<WebTouchEvent, CompletionHandler<void(bool)>>, 1>&& queue)
{
    for (auto& eventAndCallbackID : queue) {
        if (auto& completionHandler = eventAndCallbackID.second)
            completionHandler(true);
    }
}
#endif

void WebPage::computePagesForPrintingiOS(WebCore::FrameIdentifier frameID, const PrintInfo& printInfo, CompletionHandler<void(size_t)>&& reply)
{
    ASSERT_WITH_MESSAGE(!printInfo.snapshotFirstPage, "If we are just snapshotting the first page, we don't need a synchronous message to determine the page count, which is 1.");

    Vector<WebCore::IntRect> pageRects;
    double totalScaleFactor;
    auto margin = printInfo.margin;
    computePagesForPrintingImpl(frameID, printInfo, pageRects, totalScaleFactor, margin);

    ASSERT(pageRects.size() >= 1);
    reply(pageRects.size());
}

void WebPage::drawToPDFiOS(WebCore::FrameIdentifier frameID, const PrintInfo& printInfo, size_t pageCount, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& reply)
{
    if (printInfo.snapshotFirstPage) {
        IntSize snapshotSize { FloatSize { printInfo.availablePaperWidth, printInfo.availablePaperHeight } };
        IntRect snapshotRect { {0, 0}, snapshotSize };

        auto& frameView = *m_page->mainFrame().view();
        auto originalLayoutViewportOverrideRect = frameView.layoutViewportOverrideRect();
        frameView.setLayoutViewportOverrideRect(LayoutRect(snapshotRect));

        auto pdfData = pdfSnapshotAtSize(snapshotRect, snapshotSize, 0);

        frameView.setLayoutViewportOverrideRect(originalLayoutViewportOverrideRect);
        reply(SharedBuffer::create(pdfData.get()));
        return;
    }

    RetainPtr<CFMutableDataRef> pdfPageData;
    drawPagesToPDFImpl(frameID, printInfo, 0, pageCount, pdfPageData);
    reply(SharedBuffer::create(pdfPageData.get()));

    endPrinting();
}

void WebPage::contentSizeCategoryDidChange(const String& contentSizeCategory)
{
    setContentSizeCategory(contentSizeCategory);
    FontCache::invalidateAllFontCaches();
}

String WebPage::platformUserAgent(const URL&) const
{
    if (!m_page->settings().needsSiteSpecificQuirks())
        return String();

    auto document = m_mainFrame->coreFrame()->document();
    if (!document)
        return String();

    if (document->quirks().shouldAvoidUsingIOS13ForGmail() && osNameForUserAgent() == "iPhone OS"_s)
        return standardUserAgentWithApplicationName({ }, "12_1_3"_s);

    return String();
}

static bool isMousePrimaryPointingDevice()
{
#if PLATFORM(MACCATALYST)
    return true;
#else
    return false;
#endif
}

static bool hasAccessoryMousePointingDevice()
{
    if (isMousePrimaryPointingDevice())
        return true;

#if HAVE(MOUSE_DEVICE_OBSERVATION)
    if (WebProcess::singleton().hasMouseDevice())
        return true;
#endif

    return false;
}

static bool hasAccessoryStylusPointingDevice()
{
#if HAVE(STYLUS_DEVICE_OBSERVATION)
    if (WebProcess::singleton().hasStylusDevice())
        return true;
#endif

    return false;
}

bool WebPage::hoverSupportedByPrimaryPointingDevice() const
{
    return isMousePrimaryPointingDevice();
}

bool WebPage::hoverSupportedByAnyAvailablePointingDevice() const
{
    return hasAccessoryMousePointingDevice();
}

std::optional<PointerCharacteristics> WebPage::pointerCharacteristicsOfPrimaryPointingDevice() const
{
    return isMousePrimaryPointingDevice() ? PointerCharacteristics::Fine : PointerCharacteristics::Coarse;
}

OptionSet<PointerCharacteristics> WebPage::pointerCharacteristicsOfAllAvailablePointingDevices() const
{
    OptionSet<PointerCharacteristics> result;
    if (auto pointerCharacteristicsOfPrimaryPointingDevice = this->pointerCharacteristicsOfPrimaryPointingDevice())
        result.add(*pointerCharacteristicsOfPrimaryPointingDevice);
    if (hasAccessoryMousePointingDevice() || hasAccessoryStylusPointingDevice())
        result.add(PointerCharacteristics::Fine);
    return result;
}

void WebPage::hardwareKeyboardAvailabilityChanged(bool keyboardIsAttached)
{
    m_keyboardIsAttached = keyboardIsAttached;

    if (RefPtr focusedFrame = CheckedRef(m_page->focusController())->focusedFrame())
        focusedFrame->eventHandler().capsLockStateMayHaveChanged();
}

void WebPage::updateStringForFind(const String& findString)
{
    send(Messages::WebPageProxy::UpdateStringForFind(findString));
}

bool WebPage::platformPrefersTextLegibilityBasedZoomScaling() const
{
#if PLATFORM(WATCHOS)
    return true;
#else
    return false;
#endif
}

void WebPage::insertTextPlaceholder(const IntSize& size, CompletionHandler<void(const std::optional<WebCore::ElementContext>&)>&& completionHandler)
{
    // Inserting the placeholder may run JavaScript, which can do anything, including frame destruction.
    Ref<Frame> frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    auto placeholder = frame->editor().insertTextPlaceholder(size);
    completionHandler(placeholder ? contextForElement(*placeholder) : std::nullopt);
}

void WebPage::removeTextPlaceholder(const ElementContext& placeholder, CompletionHandler<void()>&& completionHandler)
{
    if (auto element = elementForContext(placeholder)) {
        RELEASE_ASSERT(is<TextPlaceholderElement>(element));
        if (RefPtr<Frame> frame = element->document().frame())
            frame->editor().removeTextPlaceholder(downcast<TextPlaceholderElement>(*element));
    }
    completionHandler();
}

void WebPage::updateSelectionWithDelta(int64_t locationDelta, int64_t lengthDelta, CompletionHandler<void()>&& completionHandler)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    RefPtr root = frame->selection().rootEditableElementOrDocumentElement();
    auto selectionRange = frame->selection().selection().toNormalizedRange();
    if (!root || !selectionRange) {
        completionHandler();
        return;
    }

    auto scope = makeRangeSelectingNodeContents(*root);
    auto selectionCharacterRange = characterRange(scope, *selectionRange);
    CheckedInt64 newSelectionLocation { selectionCharacterRange.location };
    CheckedInt64 newSelectionLength { selectionCharacterRange.length };
    newSelectionLocation += locationDelta;
    newSelectionLength += lengthDelta;
    if (newSelectionLocation.hasOverflowed() || newSelectionLength.hasOverflowed()) {
        completionHandler();
        return;
    }

    auto newSelectionRange = CharacterRange(newSelectionLocation, newSelectionLength);
    auto updatedSelectionRange = resolveCharacterRange(makeRangeSelectingNodeContents(*root), newSelectionRange);
    frame->selection().setSelectedRange(updatedSelectionRange, Affinity::Downstream, WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    completionHandler();
}

static VisiblePosition moveByGranularityRespectingWordBoundary(const VisiblePosition& position, TextGranularity granularity, uint64_t granularityCount, SelectionDirection direction)
{
    ASSERT(granularityCount);
    ASSERT(position.isNotNull());
    bool backwards = direction == SelectionDirection::Backward;
    auto farthestPositionInDirection = backwards ? startOfEditableContent(position) : endOfEditableContent(position);
    if (position == farthestPositionInDirection)
        return backwards ? startOfWord(position) : endOfWord(position);
    VisiblePosition currentPosition = position;
    VisiblePosition nextPosition;
    do {
        nextPosition = positionOfNextBoundaryOfGranularity(currentPosition, granularity, direction);
        if (nextPosition.isNull())
            break;
        currentPosition = nextPosition;
        if (atBoundaryOfGranularity(currentPosition, granularity, direction))
            --granularityCount;
    } while (granularityCount);
    if (granularity == TextGranularity::SentenceGranularity) {
        ASSERT(atBoundaryOfGranularity(currentPosition, TextGranularity::SentenceGranularity, direction));
        return currentPosition;
    }
    // Note that this rounds to the nearest word, which may cross a line boundary when using line granularity.
    // For example, suppose the text is laid out as follows and the insertion point is at |:
    //     |This is the first sen
    //      tence in a paragraph.
    // Then moving 1 line of granularity forward will return the postion after the 'e' in sentence.
    return backwards ? startOfWord(currentPosition) : endOfWord(currentPosition);
}

static VisiblePosition visiblePositionForPointInRootViewCoordinates(Frame& frame, FloatPoint pointInRootViewCoordinates)
{
    auto pointInDocument = frame.view()->rootViewToContents(roundedIntPoint(pointInRootViewCoordinates));
    return frame.visiblePositionForPoint(pointInDocument);
}

static VisiblePositionRange constrainRangeToSelection(const VisiblePositionRange& selection, const VisiblePositionRange& range)
{
    if (intersects(selection, range))
        return intersection(selection, range);
    auto rangeMidpoint = midpoint(range);
    auto position = startOfWord(rangeMidpoint);
    if (!contains(range, position)) {
        position = endOfWord(rangeMidpoint);
        if (!contains(range, position))
            position = rangeMidpoint;
    }
    return { position, position };
}

void WebPage::requestDocumentEditingContext(DocumentEditingContextRequest request, CompletionHandler<void(DocumentEditingContext)>&& completionHandler)
{
    if (!request.options.contains(DocumentEditingContextRequest::Options::Text) && !request.options.contains(DocumentEditingContextRequest::Options::AttributedText)) {
        completionHandler({ });
        return;
    }

    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    RefPtr { frame->document() }->updateLayoutIgnorePendingStylesheets();

    VisibleSelection selection = frame->selection().selection();

    VisiblePositionRange rangeOfInterest;
    auto selectionRange = VisiblePositionRange { selection.visibleStart(), selection.visibleEnd() };

    bool isSpatialRequest = request.options.containsAny({ DocumentEditingContextRequest::Options::Spatial, DocumentEditingContextRequest::Options::SpatialAndCurrentSelection });
    bool wantsRects = request.options.contains(DocumentEditingContextRequest::Options::Rects);
    bool wantsMarkedTextRects = request.options.contains(DocumentEditingContextRequest::Options::MarkedTextRects);

    if (auto textInputContext = request.textInputContext) {
        auto element = elementForContext(*textInputContext);
        if (!element) {
            completionHandler({ });
            return;
        }

        if (!request.rect.isEmpty()) {
            rangeOfInterest.start = closestEditablePositionInElementForAbsolutePoint(*element, roundedIntPoint(request.rect.minXMinYCorner()));
            rangeOfInterest.end = closestEditablePositionInElementForAbsolutePoint(*element, roundedIntPoint(request.rect.maxXMaxYCorner()));
        } else if (is<HTMLTextFormControlElement>(element)) {
            auto& textFormControlElement = downcast<HTMLTextFormControlElement>(*element);
            rangeOfInterest.start = textFormControlElement.visiblePositionForIndex(0);
            rangeOfInterest.end = textFormControlElement.visiblePositionForIndex(textFormControlElement.value().length());
        } else {
            rangeOfInterest.start = firstPositionInOrBeforeNode(element.get());
            rangeOfInterest.end = lastPositionInOrAfterNode(element.get());
        }
    } else if (isSpatialRequest) {
        // FIXME: We might need to be a bit more careful that we get something useful (test the other corners?).
        rangeOfInterest.start = visiblePositionForPointInRootViewCoordinates(frame.get(), request.rect.minXMinYCorner());
        rangeOfInterest.end = visiblePositionForPointInRootViewCoordinates(frame.get(), request.rect.maxXMaxYCorner());
        if (request.options.contains(DocumentEditingContextRequest::Options::SpatialAndCurrentSelection)) {
            if (RefPtr rootEditableElement = selection.rootEditableElement()) {
                VisiblePosition startOfEditableRoot { firstPositionInOrBeforeNode(rootEditableElement.get()) };
                VisiblePosition endOfEditableRoot { lastPositionInOrAfterNode(rootEditableElement.get()) };
                if (rangeOfInterest.start < startOfEditableRoot)
                    rangeOfInterest.start = WTFMove(startOfEditableRoot);
                if (rangeOfInterest.end > endOfEditableRoot)
                    rangeOfInterest.end = WTFMove(endOfEditableRoot);
            }
        }
    } else if (!selection.isNone())
        rangeOfInterest = selectionRange;

    if (rangeOfInterest.end < rangeOfInterest.start)
        std::exchange(rangeOfInterest.start, rangeOfInterest.end);

    if (request.options.contains(DocumentEditingContextRequest::Options::SpatialAndCurrentSelection)) {
        if (selectionRange.start < rangeOfInterest.start)
            rangeOfInterest.start = selectionRange.start;
        if (selectionRange.end > rangeOfInterest.end)
            rangeOfInterest.end = selectionRange.end;
    }

    if (rangeOfInterest.start.isNull() || rangeOfInterest.start.isOrphan() || rangeOfInterest.end.isNull() || rangeOfInterest.end.isOrphan()) {
        completionHandler({ });
        return;
    }

    // The subset of the selection that is inside the range of interest.
    auto rangeOfInterestInSelection = constrainRangeToSelection(selection, rangeOfInterest);
    if (rangeOfInterestInSelection.isNull()) {
        completionHandler({ });
        return;
    }

    VisiblePosition contextBeforeStart;
    VisiblePosition contextAfterEnd;
    auto compositionRange = frame->editor().compositionRange();
    if (request.granularityCount) {
        contextBeforeStart = moveByGranularityRespectingWordBoundary(rangeOfInterest.start, request.surroundingGranularity, request.granularityCount, SelectionDirection::Backward);
        contextAfterEnd = moveByGranularityRespectingWordBoundary(rangeOfInterest.end, request.surroundingGranularity, request.granularityCount, SelectionDirection::Forward);
    } else {
        contextBeforeStart = rangeOfInterest.start;
        contextAfterEnd = rangeOfInterest.end;
        if (wantsMarkedTextRects && compositionRange) {
            // In the case where the client has requested marked text rects make sure that the context
            // range encompasses the entire marked text range so that we don't return a truncated result.
            auto compositionStart = makeDeprecatedLegacyPosition(compositionRange->start);
            auto compositionEnd = makeDeprecatedLegacyPosition(compositionRange->end);
            if (contextBeforeStart > compositionStart)
                contextBeforeStart = compositionStart;
            if (contextAfterEnd < compositionEnd)
                contextAfterEnd = compositionEnd;
        }
    }

    auto makeString = [] (const VisiblePosition& start, const VisiblePosition& end) -> AttributedString {
        auto range = makeSimpleRange(start, end);
        if (!range || range->collapsed())
            return { };
        // FIXME: This should return editing-offset-compatible attributed strings if that option is requested.
        return { adoptNS([[NSAttributedString alloc] initWithString:WebCore::plainTextReplacingNoBreakSpace(*range, TextIteratorBehavior::EmitsOriginalText)]), nil };
    };

    DocumentEditingContext context;
    context.contextBefore = makeString(contextBeforeStart, rangeOfInterestInSelection.start);
    context.selectedText = makeString(rangeOfInterestInSelection.start, rangeOfInterestInSelection.end);
    context.contextAfter = makeString(rangeOfInterestInSelection.end, contextAfterEnd);
    if (auto compositionVisiblePositionRange = makeVisiblePositionRange(compositionRange); intersects(rangeOfInterest, compositionVisiblePositionRange)) {
        context.markedText = makeString(compositionVisiblePositionRange.start, compositionVisiblePositionRange.end);
        context.selectedRangeInMarkedText.location = distanceBetweenPositions(rangeOfInterestInSelection.start, compositionVisiblePositionRange.start);
        context.selectedRangeInMarkedText.length = [context.selectedText.string length];
    }

    auto characterRectsForRange = [](const SimpleRange& range, unsigned startOffset) {
        Vector<DocumentEditingContext::TextRectAndRange> rects;
        CharacterIterator iterator { range };
        unsigned offsetSoFar = startOffset;
        const int stride = 1;
        while (!iterator.atEnd()) {
            if (!iterator.text().isEmpty()) {
                IntRect absoluteBoundingBox;
                if (iterator.range().collapsed())
                    absoluteBoundingBox = VisiblePosition(makeContainerOffsetPosition(iterator.range().start)).absoluteCaretBounds();
                else
                    absoluteBoundingBox = unionRectIgnoringZeroRects(RenderObject::absoluteTextRects(iterator.range(), RenderObject::BoundingRectBehavior::IgnoreEmptyTextSelections));
                rects.append({ iterator.range().start.document().view()->contentsToRootView(absoluteBoundingBox), { offsetSoFar++, stride } });
            }
            iterator.advance(stride);
        }
        return rects;
    };

    if (wantsRects) {
        if (auto contextRange = makeSimpleRange(contextBeforeStart, contextAfterEnd))
            context.textRects = characterRectsForRange(*contextRange, 0);
    } else if (wantsMarkedTextRects && compositionRange) {
        unsigned compositionStartOffset = 0;
        if (auto range = makeSimpleRange(contextBeforeStart, compositionRange->start))
            compositionStartOffset = characterCount(*range);
        context.textRects = characterRectsForRange(*compositionRange, compositionStartOffset);
    }

#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    if (request.options.contains(DocumentEditingContextRequest::Options::Annotation))
        context.annotatedText = m_textCheckingControllerProxy->annotatedSubstringBetweenPositions(contextBeforeStart, contextAfterEnd);
#endif

    completionHandler(context);
}

bool WebPage::shouldAllowSingleClickToChangeSelection(WebCore::Node& targetNode, const WebCore::VisibleSelection& newSelection)
{
    if (RefPtr editableRoot = newSelection.rootEditableElement(); editableRoot && editableRoot == targetNode.rootEditableElement()) {
        // Text interaction gestures will handle selection in the case where we are already editing the node. In the case where we're
        // just starting to focus an editable element by tapping on it, only change the selection if we weren't already showing an
        // input view prior to handling the tap.
        return !(m_completingSyntheticClick ? m_wasShowingInputViewForFocusedElementDuringLastPotentialTap : m_isShowingInputViewForFocusedElement);
    }
    return true;
}

void WebPage::setShouldRevealCurrentSelectionAfterInsertion(bool shouldRevealCurrentSelectionAfterInsertion)
{
    if (m_shouldRevealCurrentSelectionAfterInsertion == shouldRevealCurrentSelectionAfterInsertion)
        return;
    m_shouldRevealCurrentSelectionAfterInsertion = shouldRevealCurrentSelectionAfterInsertion;
    if (!shouldRevealCurrentSelectionAfterInsertion)
        return;
    m_page->revealCurrentSelection();
    scheduleFullEditorStateUpdate();
}

void WebPage::setScreenIsBeingCaptured(bool captured)
{
    m_screenIsBeingCaptured = captured;
}

void WebPage::textInputContextsInRect(FloatRect searchRect, CompletionHandler<void(const Vector<ElementContext>&)>&& completionHandler)
{
    auto contexts = m_page->editableElementsInRect(searchRect).map([&] (const auto& element) {
        auto& document = element->document();

        ElementContext context;
        context.webPageIdentifier = m_identifier;
        context.documentIdentifier = document.identifier();
        context.elementIdentifier = element->identifier();
        context.boundingRect = element->boundingBoxInRootViewCoordinates();
        return context;
    });
    completionHandler(contexts);
#if ENABLE(EDITABLE_REGION)
    m_page->setEditableRegionEnabled();
#endif
}

void WebPage::focusTextInputContextAndPlaceCaret(const ElementContext& elementContext, const IntPoint& point, CompletionHandler<void(bool)>&& completionHandler)
{
    auto target = elementForContext(elementContext);
    if (!target) {
        completionHandler(false);
        return;
    }

    ASSERT(target->document().frame());
    Ref targetFrame = *target->document().frame();

    targetFrame->document()->updateLayoutIgnorePendingStylesheets();

    // Performing layout could have could torn down the element's renderer. Check that we still
    // have one. Otherwise, bail out as this function only focuses elements that have a visual
    // representation.
    if (!target->renderer() || !target->isFocusable()) {
        completionHandler(false);
        return;
    }

    // FIXME: Do not focus an element if it moved or the caret point is outside its bounds
    // because we only want to do so if the caret can be placed.
    UserGestureIndicator gestureIndicator { ProcessingUserGesture, &target->document() };
    SetForScope userIsInteractingChange { m_userIsInteracting, true };
    CheckedRef(m_page->focusController())->setFocusedElement(target.get(), targetFrame);

    // Setting the focused element could tear down the element's renderer. Check that we still have one.
    if (m_focusedElement != target || !target->renderer()) {
        completionHandler(false);
        return;
    }

    ASSERT(targetFrame->view());
    auto position = closestEditablePositionInElementForAbsolutePoint(*target, targetFrame->view()->rootViewToContents(point));
    if (position.isNull()) {
        completionHandler(false);
        return;
    }
    targetFrame->selection().setSelectedRange(makeSimpleRange(position), position.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    completionHandler(true);
}

void WebPage::platformDidScalePage()
{
    auto transactionID = downcast<RemoteLayerTreeDrawingArea>(*m_drawingArea).lastCommittedTransactionID();
    m_lastLayerTreeTransactionIdAndPageScaleBeforeScalingPage = {{ transactionID, m_lastTransactionPageScaleFactor }};
}

#if USE(QUICK_LOOK)

void WebPage::didStartLoadForQuickLookDocumentInMainFrame(const String& fileName, const String& uti)
{
    send(Messages::WebPageProxy::DidStartLoadForQuickLookDocumentInMainFrame(fileName, uti));
}

void WebPage::didFinishLoadForQuickLookDocumentInMainFrame(const FragmentedSharedBuffer& buffer)
{
    ASSERT(!buffer.isEmpty());

    // FIXME: In some cases, buffer contains a single segment that wraps an existing ShareableResource.
    // If we could create a handle from that existing resource then we could avoid this extra
    // allocation and copy.

    auto sharedMemory = SharedMemory::copyBuffer(buffer);
    if (!sharedMemory)
        return;

    auto shareableResource = ShareableResource::create(sharedMemory.releaseNonNull(), 0, buffer.size());
    if (!shareableResource)
        return;

    auto handle = shareableResource->createHandle();
    if (!handle)
        return;

    send(Messages::WebPageProxy::DidFinishLoadForQuickLookDocumentInMainFrame(*handle));
}

void WebPage::requestPasswordForQuickLookDocumentInMainFrame(const String& fileName, CompletionHandler<void(const String&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebPageProxy::RequestPasswordForQuickLookDocumentInMainFrame(fileName), WTFMove(completionHandler));
}

#endif

void WebPage::animationDidFinishForElement(const WebCore::Element& animatedElement)
{
    Ref frame = CheckedRef(m_page->focusController())->focusedOrMainFrame();
    auto& selection = frame->selection().selection();
    if (selection.isNoneOrOrphaned())
        return;

    if (selection.isCaret() && !selection.hasEditableStyle())
        return;

    auto scheduleEditorStateUpdateForStartOrEndContainerNodeIfNeeded = [&](const Node* container) {
        if (!animatedElement.containsIncludingShadowDOM(container))
            return false;

        frame->selection().setCaretRectNeedsUpdate();
        scheduleFullEditorStateUpdate();
        return true;
    };

    RefPtr startContainer = selection.start().containerNode();
    if (scheduleEditorStateUpdateForStartOrEndContainerNodeIfNeeded(startContainer.get()))
        return;

    RefPtr endContainer = selection.end().containerNode();
    if (startContainer != endContainer)
        scheduleEditorStateUpdateForStartOrEndContainerNodeIfNeeded(endContainer.get());
}

} // namespace WebKit

#undef WEBPAGE_RELEASE_LOG
#undef WEBPAGE_RELEASE_LOG_ERROR

#endif // PLATFORM(IOS_FAMILY)
