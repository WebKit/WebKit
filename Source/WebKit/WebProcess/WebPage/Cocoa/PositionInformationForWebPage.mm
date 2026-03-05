/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "PositionInformationForWebPage.h"

#if PLATFORM(COCOA)

#import "InteractionInformationAtPosition.h"
#import "InteractionInformationRequest.h"
#import "PluginView.h"
#import "ShareableBitmapUtilities.h"
#import "WebPage.h"
#import <WebCore/ContainerNodeInlines.h>
#import <WebCore/DataDetection.h>
#import <WebCore/DataDetectionResultsStorage.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentQuirks.h>
#import <WebCore/Editor.h>
#import <WebCore/Element.h>
#import <WebCore/ElementInlines.h>
#import <WebCore/EventHandler.h>
#import <WebCore/EventNames.h>
#import <WebCore/EventTargetInlines.h>
#import <WebCore/FocusController.h>
#import <WebCore/FrameDestructionObserverInlines.h>
#import <WebCore/HTMLAnchorElement.h>
#import <WebCore/HTMLAttachmentElement.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLImageElement.h>
#import <WebCore/HTMLInputElement.h>
#import <WebCore/HTMLModelElement.h>
#import <WebCore/HTMLSelectElement.h>
#import <WebCore/HTMLTextAreaElement.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/HitTestRequest.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/ImageOverlay.h>
#import <WebCore/LocalDOMWindow.h>
#import <WebCore/LocalFrameInlines.h>
#import <WebCore/Model.h>
#import <WebCore/NodeDocument.h>
#import <WebCore/Page.h>
#import <WebCore/Quirks.h>
#import <WebCore/RenderBlockFlow.h>
#import <WebCore/RenderBoxInlines.h>
#import <WebCore/RenderImage.h>
#import <WebCore/RenderObjectDocument.h>
#import <WebCore/RenderObjectStyle.h>
#import <WebCore/RenderVideo.h>
#import <WebCore/RenderVideoInlines.h>
#import <WebCore/ScrollingCoordinator.h>
#import <WebCore/VisibleUnits.h>
#import <wtf/text/StringToIntegerConversion.h>

namespace WebKit {

static void focusedElementPositionInformation(WebPage& page, WebCore::Element& focusedElement, const InteractionInformationRequest& request, InteractionInformationAtPosition& info)
{
    RefPtr frame = page.corePage()->focusController().focusedOrMainFrame();
    if (!frame || !frame->editor().hasComposition())
        return;

    const uint32_t kHitAreaWidth = 66;
    const uint32_t kHitAreaHeight = 66;
    Ref view = *frame->view();
    WebCore::IntPoint adjustedPoint(view->rootViewToContents(request.point));
    WebCore::IntPoint constrainedPoint = WebPage::constrainPoint(adjustedPoint, *frame, focusedElement);
    WebCore::VisiblePosition position = frame->visiblePositionForPoint(constrainedPoint);

    auto compositionRange = protect(frame->editor())->compositionRange();
    if (!compositionRange)
        return;

    auto startPosition = makeDeprecatedLegacyPosition(compositionRange->start);
    auto endPosition = makeDeprecatedLegacyPosition(compositionRange->end);
    if (position < startPosition)
        position = startPosition;
    else if (position > endPosition)
        position = endPosition;
    WebCore::IntRect caretRect = view->contentsToRootView(position.absoluteCaretBounds());
    float deltaX = std::abs(caretRect.x() + (caretRect.width() / 2) - request.point.x());
    float deltaYFromTheTop = std::abs(caretRect.y() - request.point.y());
    float deltaYFromTheBottom = std::abs(caretRect.y() + caretRect.height() - request.point.y());

    info.isNearMarkedText = !(deltaX > kHitAreaWidth || deltaYFromTheTop > kHitAreaHeight || deltaYFromTheBottom > kHitAreaHeight);
}

static void linkIndicatorPositionInformation(WebPage& page, WebCore::Element& linkElement, const InteractionInformationRequest& request, InteractionInformationAtPosition& info)
{
    if (!request.includeLinkIndicator)
        return;

    auto linkRange = makeRangeSelectingNodeContents(linkElement);
    float deviceScaleFactor = page.corePage()->deviceScaleFactor();
    const float marginInPoints = request.linkIndicatorShouldHaveLegacyMargins ? 4 : 0;

    constexpr OptionSet<WebCore::TextIndicatorOption> textIndicatorOptions {
        WebCore::TextIndicatorOption::TightlyFitContent,
        WebCore::TextIndicatorOption::RespectTextColor,
        WebCore::TextIndicatorOption::PaintBackgrounds,
        WebCore::TextIndicatorOption::UseBoundingRectAndPaintAllContentForComplexRanges,
        WebCore::TextIndicatorOption::IncludeMarginIfRangeMatchesSelection,
        WebCore::TextIndicatorOption::ComputeEstimatedBackgroundColor
    };
    auto textIndicator = WebCore::TextIndicator::createWithRange(linkRange, textIndicatorOptions, WebCore::TextIndicatorPresentationTransition::None, WebCore::FloatSize(marginInPoints * deviceScaleFactor, marginInPoints * deviceScaleFactor));

    info.textIndicator = WTF::move(textIndicator);
}

#if ENABLE(DATA_DETECTION) && PLATFORM(IOS_FAMILY)

static void dataDetectorLinkPositionInformation(WebCore::Element& element, InteractionInformationAtPosition& info)
{
    if (!WebCore::DataDetection::isDataDetectorLink(element))
        return;

    info.isDataDetectorLink = true;
    info.dataDetectorBounds = info.bounds;
    const int dataDetectionExtendedContextLength = 350;
    info.dataDetectorIdentifier = WebCore::DataDetection::dataDetectorIdentifier(element);
    if (auto* results = element.document().frame()->dataDetectionResultsIfExists())
        info.dataDetectorResults = results->documentLevelResults();

    if (!WebCore::DataDetection::requiresExtendedContext(element))
        return;

    auto range = WebCore::makeRangeSelectingNodeContents(element);
    info.textBefore = plainTextForDisplay(WebCore::rangeExpandedByCharactersInDirectionAtWordBoundary(makeDeprecatedLegacyPosition(range.start),
        dataDetectionExtendedContextLength, WebCore::SelectionDirection::Backward));
    info.textAfter = plainTextForDisplay(WebCore::rangeExpandedByCharactersInDirectionAtWordBoundary(makeDeprecatedLegacyPosition(range.end),
        dataDetectionExtendedContextLength, WebCore::SelectionDirection::Forward));
}

static void dataDetectorImageOverlayPositionInformation(const WebCore::HTMLElement& overlayHost, const InteractionInformationRequest& request, InteractionInformationAtPosition& info)
{
    RefPtr frame = overlayHost.document().frame();
    if (!frame)
        return;

    auto elementAndBounds = WebCore::DataDetection::findDataDetectionResultElementInImageOverlay(request.point, overlayHost);
    if (!elementAndBounds)
        return;

    auto [foundElement, elementBounds] = *elementAndBounds;
    auto identifierValue = parseInteger<uint64_t>(foundElement->attributeWithoutSynchronization(WebCore::HTMLNames::x_apple_data_detectors_resultAttr));
    if (!identifierValue || !*identifierValue)
        return;

    auto identifier = ObjectIdentifier<WebCore::ImageOverlayDataDetectionResultIdentifierType>(*identifierValue);

    auto* dataDetectionResults = frame->dataDetectionResultsIfExists();
    if (!dataDetectionResults)
        return;

    auto dataDetectionResult = retainPtr(dataDetectionResults->imageOverlayDataDetectionResult(identifier));
    if (!dataDetectionResult)
        return;

    info.dataDetectorBounds = WTF::move(elementBounds);
    info.dataDetectorResults = @[ dataDetectionResult.get() ];
}

#endif // ENABLE(DATA_DETECTION) && PLATFORM(IOS_FAMILY)

static std::optional<std::pair<WebCore::RenderImage&, WebCore::Image&>> imageRendererAndImage(WebCore::Element& element)
{
    CheckedPtr renderImage = dynamicDowncast<WebCore::RenderImage>(element.renderer());
    if (!renderImage)
        return std::nullopt;

    if (!renderImage->cachedImage() || renderImage->cachedImage()->errorOccurred())
        return std::nullopt;

    RefPtr image = renderImage->cachedImage()->imageForRenderer(renderImage);
    if (!image || image->width() <= 1 || image->height() <= 1)
        return std::nullopt;

    return { { *renderImage, *image } };
}

#if PLATFORM(IOS_FAMILY)
static void videoPositionInformation(WebPage& page, WebCore::HTMLVideoElement& element, const InteractionInformationRequest& request, InteractionInformationAtPosition& info)
{
    info.elementContainsImageOverlay = WebCore::ImageOverlay::hasOverlay(element);

    if (!element.paused())
        return;

    CheckedPtr renderVideo = element.renderer();
    if (!renderVideo)
        return;

    info.isPausedVideo = true;

    if (request.includeImageData)
        info.image = createShareableBitmap(*renderVideo);

    info.hostImageOrVideoElementContext = page.contextForElement(element);
}
#endif // PLATFORM(IOS_FAMILY)

static RefPtr<WebCore::HTMLVideoElement> hostVideoElementIgnoringImageOverlay(WebCore::Node& node)
{
    if (WebCore::ImageOverlay::isInsideOverlay(node))
        return { };

    if (RefPtr video = dynamicDowncast<WebCore::HTMLVideoElement>(node))
        return video;

    return dynamicDowncast<WebCore::HTMLVideoElement>(node.shadowHost());
}

#if PLATFORM(IOS_FAMILY)
static void imagePositionInformation(WebPage& page, WebCore::Element& element, const InteractionInformationRequest& request, InteractionInformationAtPosition& info)
{
    auto rendererAndImage = imageRendererAndImage(element);
    if (!rendererAndImage)
        return;

    auto& [renderImage, image] = *rendererAndImage;
    info.isImage = true;
    info.imageURL = page.applyLinkDecorationFiltering(protect(element.document())->completeURL(renderImage.cachedImage()->url().string()), WebCore::LinkDecorationFilteringTrigger::Unspecified);
    info.imageMIMEType = image.mimeType();
    info.isAnimatedImage = image.isAnimated();
    info.isAnimating = image.isAnimating();
    RefPtr htmlElement = dynamicDowncast<WebCore::HTMLElement>(element);
    info.elementContainsImageOverlay = htmlElement && WebCore::ImageOverlay::hasOverlay(*htmlElement);
#if ENABLE(SPATIAL_IMAGE_DETECTION)
    info.isSpatialImage = image.isSpatial();
#endif

    if (request.includeSnapshot || request.includeImageData)
        info.image = createShareableBitmap(renderImage, { WebCore::screenSize() * page.corePage()->deviceScaleFactor(), AllowAnimatedImages::Yes, UseSnapshotForTransparentImages::Yes });

    info.hostImageOrVideoElementContext = page.contextForElement(element);
}
#endif // PLATFORM(IOS_FAMILY)

static void boundsPositionInformation(WebCore::RenderObject& renderer, InteractionInformationAtPosition& info)
{
    if (CheckedPtr renderImage = dynamicDowncast<WebCore::RenderImage>(renderer))
        info.bounds = renderImage->absoluteContentQuad().enclosingBoundingBox();
    else
        info.bounds = renderer.absoluteBoundingBoxRect();

    if (!renderer.document().frame()->isMainFrame()) {
        RefPtr view = renderer.document().frame()->view();
        info.bounds = view->contentsToRootView(info.bounds);
    }
}

static void elementPositionInformation(WebPage& page, WebCore::Element& element, const InteractionInformationRequest& request, const WebCore::Node* innerNonSharedNode, InteractionInformationAtPosition& info)
{
    Ref document = element.document();
    RefPtr<WebCore::Element> linkElement;
    if (element.renderer() && element.renderer()->isRenderImage())
        linkElement = WebPage::containingLinkAnchorElement(element);
    else if (element.isLink())
        linkElement = &element;

    info.isElement = true;
    info.idAttribute = element.getIdAttribute();
    info.isImageOverlayText = WebCore::ImageOverlay::isOverlayText(innerNonSharedNode);

    info.title = element.attributeWithoutSynchronization(WebCore::HTMLNames::titleAttr).string();
    if (linkElement && info.title.isEmpty())
        info.title = element.innerText();
#if PLATFORM(IOS_FAMILY)
    if (element.renderer())
        info.touchCalloutEnabled = element.renderer()->style().touchCallout() == WebCore::Style::WebkitTouchCallout::Default;
#endif

    if (linkElement && !info.isImageOverlayText) {
        info.isLink = true;
        info.url = page.applyLinkDecorationFiltering(document->completeURL(linkElement->getAttribute(WebCore::HTMLNames::hrefAttr)), WebCore::LinkDecorationFilteringTrigger::Unspecified);

        linkIndicatorPositionInformation(page, *linkElement, request, info);
#if ENABLE(DATA_DETECTION) && PLATFORM(IOS_FAMILY)
        dataDetectorLinkPositionInformation(element, info);
#endif
    }

    RefPtr elementForScrollTesting = linkElement ? linkElement.get() : &element;
    if (CheckedPtr renderer = elementForScrollTesting->renderer()) {
        if (RefPtr scrollingCoordinator = page.scrollingCoordinator())
            info.containerScrollingNodeID = scrollingCoordinator->scrollableContainerNodeID(*renderer);
    }

#if PLATFORM(IOS_FAMILY)
    info.needsPointerTouchCompatibilityQuirk = document->quirks().needsPointerTouchCompatibility(element);
#endif

    if (CheckedPtr renderer = element.renderer()) {
#if PLATFORM(IOS_FAMILY)
        bool shouldCollectImagePositionInformation = renderer->isRenderImage();
        if (shouldCollectImagePositionInformation && info.isImageOverlayText) {
            shouldCollectImagePositionInformation = false;
            if (request.includeImageData) {
                if (auto rendererAndImage = imageRendererAndImage(element)) {
                    auto& [renderImage, image] = *rendererAndImage;
                    info.imageURL = page.applyLinkDecorationFiltering(document->completeURL(renderImage.cachedImage()->url().string()), WebCore::LinkDecorationFilteringTrigger::Unspecified);
                    info.imageMIMEType = image.mimeType();
                    info.image = createShareableBitmap(renderImage, { WebCore::screenSize() * page.corePage()->deviceScaleFactor(), AllowAnimatedImages::Yes, UseSnapshotForTransparentImages::Yes });
                }
            }
        }
        if (shouldCollectImagePositionInformation) {
            if (auto video = hostVideoElementIgnoringImageOverlay(element))
                videoPositionInformation(page, *video, request, info);
            else
                imagePositionInformation(page, element, request, info);
        }
#endif // PLATFORM(IOS_FAMILY)
        boundsPositionInformation(*renderer, info);
    }

    info.elementContext = page.contextForElement(element);
}

static void selectionPositionInformation(WebPage& page, const InteractionInformationRequest& request, InteractionInformationAtPosition& info)
{
    RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(page.corePage()->mainFrame());
    if (!localMainFrame)
        return;

    constexpr OptionSet<WebCore::HitTestRequest::Type> hitType {
        WebCore::HitTestRequest::Type::ReadOnly,
        WebCore::HitTestRequest::Type::Active,
        WebCore::HitTestRequest::Type::AllowVisibleChildFrameContentOnly
    };
    WebCore::HitTestResult result = localMainFrame->eventHandler().hitTestResultAtPoint(request.point, hitType);
    RefPtr hitNode = result.innerNode();

    // Hit test could return HTMLHtmlElement that has no renderer, if the body is smaller than the document.
    if (!hitNode || !hitNode->renderer())
        return;

    CheckedPtr renderer = hitNode->renderer();

    info.selectability = ([&] {
        if (protect(renderer->style())->usedUserSelect() == WebCore::UserSelect::None)
            return InteractionInformationAtPosition::Selectability::UnselectableDueToUserSelectNoneOrQuirk;

        if (RefPtr element = dynamicDowncast<WebCore::Element>(*hitNode)) {
            if (WebPage::isAssistableElement(*element))
                return InteractionInformationAtPosition::Selectability::UnselectableDueToFocusableElement;

            if (hostVideoElementIgnoringImageOverlay(*hitNode))
                return InteractionInformationAtPosition::Selectability::UnselectableDueToMediaControls;
        }

        if (protect(hitNode->document())->quirks().shouldAvoidStartingSelectionOnMouseDownOverPointerCursor(*hitNode))
            return InteractionInformationAtPosition::Selectability::UnselectableDueToUserSelectNoneOrQuirk;

        return InteractionInformationAtPosition::Selectability::Selectable;
    })();
    info.isSelected = result.isSelected();

    if (info.isLink || info.isImage)
        return;

    boundsPositionInformation(*renderer, info);

    if (RefPtr element = dynamicDowncast<WebCore::Element>(*hitNode))
        info.idAttribute = element->getIdAttribute();

    if (RefPtr attachment = dynamicDowncast<WebCore::HTMLAttachmentElement>(*hitNode)) {
        info.isAttachment = true;
        info.title = attachment->attachmentTitle();
        linkIndicatorPositionInformation(page, *attachment, request, info);
        if (attachment->file())
            info.url = URL::fileURLWithFileSystemPath(attachment->file()->path());
    }

    for (RefPtr currentNode = hitNode; currentNode; currentNode = currentNode->parentOrShadowHostNode()) {
        CheckedPtr renderer = currentNode->renderer();
        if (!renderer)
            continue;

        CheckedRef style = renderer->style();
        if (style->usedUserSelect() == WebCore::UserSelect::None && style->userDrag() == WebCore::UserDrag::Element) {
            info.prefersDraggingOverTextSelection = true;
            break;
        }
    }
#if PLATFORM(MACCATALYST)
    bool isInsideFixedPosition;
    WebCore::VisiblePosition caretPosition(renderer->visiblePositionForPoint(request.point, WebCore::HitTestSource::User));
    info.caretRect = caretPosition.absoluteCaretBounds(&isInsideFixedPosition);
#endif

#if ENABLE(MODEL_PROCESS)
    if (is<WebCore::HTMLModelElement>(*hitNode))
        info.prefersDraggingOverTextSelection = true;
#endif
}

static void textInteractionPositionInformation(WebPage& page, const WebCore::HTMLInputElement& input, const InteractionInformationRequest& request, InteractionInformationAtPosition& info)
{
    if (!input.list())
        return;

    constexpr OptionSet<WebCore::HitTestRequest::Type> hitType { WebCore::HitTestRequest::Type::ReadOnly, WebCore::HitTestRequest::Type::Active, WebCore::HitTestRequest::Type::AllowVisibleChildFrameContentOnly };
    RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(page.corePage()->mainFrame());
    if (!localMainFrame)
        return;
    WebCore::HitTestResult result = localMainFrame->eventHandler().hitTestResultAtPoint(request.point, hitType);
    if (result.innerNode() == input.dataListButtonElement())
        info.preventTextInteraction = true;
}

static bool canForceCaretForPosition(const WebCore::VisiblePosition& position)
{
    RefPtr node = position.deepEquivalent().anchorNode();
    if (!node)
        return false;

    CheckedPtr renderer = node->renderer();
    CheckedPtr style = renderer ? &renderer->style() : nullptr;
    auto cursorType = style ? style->cursorType() : WebCore::CursorType::Auto;

    if (cursorType == WebCore::CursorType::Text)
        return true;

    if (cursorType != WebCore::CursorType::Auto)
        return false;

    if (node->hasEditableStyle())
        return true;

    if (!renderer)
        return false;

    return renderer->isRenderText() && node->canStartSelection();
}

static CursorContext cursorContext(const WebCore::HitTestResult& hitTestResult, const InteractionInformationRequest& request)
{
    CursorContext context;
    RefPtr frame = hitTestResult.innerNodeFrame();
    if (!frame)
        return context;

    // FIXME: Do we really need to set the cursor here if `includeCursorContext` is not set?
    context.cursor = frame->eventHandler().selectCursor(hitTestResult, false);

    if (!request.includeCursorContext)
        return context;

    RefPtr view = frame->view();
    if (!view)
        return context;

    RefPtr node = hitTestResult.innerNode();
    if (!node)
        return context;

    CheckedPtr renderer = node->renderer();
    if (!renderer)
        return context;

    while (renderer && !is<WebCore::RenderBlockFlow>(*renderer))
        renderer = renderer->parent();

    if (!renderer)
        return context;

    // FIXME: We should be able to retrieve this geometry information without
    // forcing the text to fall out of Simple Line Layout.
    CheckedRef blockFlow = downcast<WebCore::RenderBlockFlow>(*renderer);
    auto position = frame->visiblePositionForPoint(view->rootViewToContents(request.point));
    auto lineRect = position.absoluteSelectionBoundsForLine();
    bool isEditable = node->hasEditableStyle();

    if (isEditable)
        lineRect.setWidth(blockFlow->contentBoxWidth());

    context.isVerticalWritingMode = !renderer->isHorizontalWritingMode();
    context.lineCaretExtent = view->contentsToRootView(lineRect);

    auto cursorTypeIs = [](const auto& maybeCursor, auto type) {
        return maybeCursor.transform([type](const auto& cursor) {
            return cursor.type() == type;
        }).value_or(false);
    };

    bool lineContainsRequestPoint = context.lineCaretExtent.contains(request.point);
    // Force an I-beam cursor if the page didn't request a hand, and we're inside the bounds of the line.
    if (lineContainsRequestPoint && !cursorTypeIs(context.cursor, WebCore::Cursor::Type::Hand) && canForceCaretForPosition(position))
        context.cursor = WebCore::Cursor::fromType(WebCore::Cursor::Type::IBeam);

    if (!lineContainsRequestPoint && cursorTypeIs(context.cursor, WebCore::Cursor::Type::IBeam)) {
        auto approximateLineRectInContentCoordinates = renderer->absoluteBoundingBoxRect();
        approximateLineRectInContentCoordinates.setHeight(protect(renderer->style())->computedLineHeight());
        context.lineCaretExtent = view->contentsToRootView(approximateLineRectInContentCoordinates);
        if (!context.lineCaretExtent.contains(request.point) || !isEditable)
            context.lineCaretExtent.setY(request.point.y() - context.lineCaretExtent.height() / 2);
    }

    auto nodeShouldNotUseIBeam = ^(WebCore::Node* node) {
        if (!node)
            return false;
        WebCore::RenderObject *renderer = node->renderer();
        if (!renderer)
            return false;
        return is<WebCore::RenderReplaced>(*renderer);
    };

    const auto& deepPosition = position.deepEquivalent();
    context.shouldNotUseIBeamInEditableContent = nodeShouldNotUseIBeam(node) || nodeShouldNotUseIBeam(deepPosition.computeNodeBeforePosition()) || nodeShouldNotUseIBeam(deepPosition.computeNodeAfterPosition());
    return context;
}

static void animationPositionInformation(WebPage& page, const InteractionInformationRequest& request, const WebCore::HitTestResult& hitTestResult, InteractionInformationAtPosition& info)
{
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    if (!request.gatherAnimations)
        return;

    for (const auto& node : hitTestResult.listBasedTestResult()) {
        RefPtr element = dynamicDowncast<WebCore::Element>(node.ptr());
        if (!element)
            continue;

        auto rendererAndImage = imageRendererAndImage(*element);
        if (!rendererAndImage)
            continue;

        Ref image = rendererAndImage->second;
        if (!image->isAnimated())
            continue;

        if (auto elementContext = page.contextForElement(*element))
            info.animationsAtPoint.append({ WTF::move(*elementContext), image->isAnimating() });
    }
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(request);
    UNUSED_PARAM(info);
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
}

static RefPtr<WebCore::LocalDOMWindow> windowWithDoubleClickEventListener(RefPtr<WebCore::LocalFrame> frame)
{
    if (!frame)
        return nullptr;

    RefPtr window = frame->window();
    if (!window || !window->hasEventListeners(WebCore::eventNames().dblclickEvent))
        return nullptr;

    return window;
}

InteractionInformationAtPosition positionInformationForWebPage(WebPage& page, const InteractionInformationRequest& request)
{
    InteractionInformationAtPosition info;
    info.request = request;

    WebCore::FloatPoint adjustedPoint;
    RefPtr localMainFrame = WTF::protect(page.corePage())->localMainFrame();
    if (!localMainFrame)
        return info;

    RefPtr nodeRespondingToClickEvents = localMainFrame->nodeRespondingToClickEvents(request.point, adjustedPoint);

    info.isContentEditable = nodeRespondingToClickEvents && nodeRespondingToClickEvents->isContentEditable();
    info.adjustedPointForNodeRespondingToClickEvents = adjustedPoint;

    if (request.includeHasDoubleClickHandler)
        info.hitNodeOrWindowHasDoubleClickListener = localMainFrame->nodeRespondingToDoubleClickEvent(request.point, adjustedPoint) || windowWithDoubleClickEventListener(localMainFrame);

    auto hitTestRequestTypes = OptionSet<WebCore::HitTestRequest::Type> {
        WebCore::HitTestRequest::Type::ReadOnly,
        WebCore::HitTestRequest::Type::AllowFrameScrollbars,
        WebCore::HitTestRequest::Type::AllowVisibleChildFrameContentOnly,
    };

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    if (request.gatherAnimations) {
        hitTestRequestTypes.add(WebCore::HitTestRequest::Type::IncludeAllElementsUnderPoint);
        hitTestRequestTypes.add(WebCore::HitTestRequest::Type::CollectMultipleElements);
    }
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)

    auto& eventHandler = localMainFrame->eventHandler();
    auto hitTestResult = eventHandler.hitTestResultAtPoint(request.point, hitTestRequestTypes);

#if ENABLE(PDF_PLUGIN)
    RefPtr pluginView = hitTestResult.isOverWidget() ? WebPage::pluginViewForFrame(WTF::protect(hitTestResult.innerNodeFrame())) : nullptr;
#endif

    info.cursorContext = [&] {
        if (request.includeCursorContext) {
#if ENABLE(PDF_PLUGIN) && PLATFORM(IOS_FAMILY)
            if (pluginView)
                return pluginView->cursorContext(request.point);
#endif
        }
        return cursorContext(hitTestResult, request);
    }();

    if (page.focusedElement())
        focusedElementPositionInformation(page, *page.focusedElement(), request, info);

    RefPtr hitTestNode = hitTestResult.innerNonSharedNode();
    if (RefPtr element = dynamicDowncast<WebCore::Element>(nodeRespondingToClickEvents)) {
        elementPositionInformation(page, *element, request, hitTestNode.get(), info);

        if (info.isLink && !info.isImage && request.includeSnapshot)
            info.image = page.shareableBitmapSnapshotForNode(*element);
    }

#if ENABLE(DATA_DETECTION) && PLATFORM(IOS_FAMILY)
    auto hitTestedImageOverlayHost = ([&]() -> RefPtr<WebCore::HTMLElement> {
        if (!hitTestNode || !info.isImageOverlayText)
            return nullptr;

        RefPtr htmlElement = dynamicDowncast<WebCore::HTMLElement>(hitTestNode->shadowHost());
        if (!htmlElement || !WebCore::ImageOverlay::hasOverlay(*htmlElement))
            return nullptr;

        return htmlElement;
    })();

    if (hitTestedImageOverlayHost)
        dataDetectorImageOverlayPositionInformation(*hitTestedImageOverlayHost, request, info);
#endif // ENABLE(DATA_DETECTION) && PLATFORM(IOS_FAMILY)

#if PLATFORM(IOS_FAMILY)
    if (!info.isImage && request.includeImageData && hitTestNode) {
        if (auto video = hostVideoElementIgnoringImageOverlay(*hitTestNode))
            videoPositionInformation(page, *video, request, info);
        else if (RefPtr img = dynamicDowncast<WebCore::HTMLImageElement>(hitTestNode))
            imagePositionInformation(page, *img, request, info);
    }
#endif // PLATFORM(IOS_FAMILY)

    animationPositionInformation(page, request, hitTestResult, info);
    selectionPositionInformation(page, request, info);

    // Prevent the callout bar from showing when tapping on the datalist button.
    if (RefPtr input = dynamicDowncast<WebCore::HTMLInputElement>(nodeRespondingToClickEvents))
        textInteractionPositionInformation(page, *input, request, info);

#if ENABLE(MODEL_PROCESS)
    if (RefPtr modelElement = dynamicDowncast<WebCore::HTMLModelElement>(hitTestNode))
        info.isInteractiveModel = modelElement->model() && modelElement->supportsStageModeInteraction();
#endif

#if ENABLE(PDF_PLUGIN) && PLATFORM(IOS_FAMILY)
    if (pluginView) {
        if (auto&& [url, bounds, textIndicator] = pluginView->linkDataAtPoint(request.point); !url.isEmpty()) {
            info.isLink = true;
            info.url = WTF::move(url);
            info.bounds = enclosingIntRect(bounds);
            info.textIndicator = WTF::move(textIndicator);
        }
        info.isInPlugin = true;
    }
#endif // ENABLE(PDF_PLUGIN) && PLATFORM(IOS_FAMILY)

    return info;
}

};

#endif // PLATFORM(COCOA)
