/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ImageOverlay.h"

#include "Blob.h"
#include "CharacterRange.h"
#include "DOMTokenList.h"
#include "DOMURL.h"
#include "Document.h"
#include "ElementChildIterator.h"
#include "EventHandler.h"
#include "EventLoop.h"
#include "FloatRect.h"
#include "FloatSize.h"
#include "GeometryUtilities.h"
#include "HTMLBRElement.h"
#include "HTMLDivElement.h"
#include "HTMLImageElement.h"
#include "HTMLMediaElement.h"
#include "HTMLStyleElement.h"
#include "ImageOverlayController.h"
#include "MediaControlsHost.h"
#include "Page.h"
#include "Quirks.h"
#include "RenderImage.h"
#include "RenderText.h"
#include "ShadowRoot.h"
#include "SharedBuffer.h"
#include "SimpleRange.h"
#include "Text.h"
#include "TextIterator.h"
#include "TextRecognitionResult.h"
#include "UserAgentStyleSheets.h"
#include "VisibleSelection.h"
#include <wtf/Range.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>

#if ENABLE(DATA_DETECTION)
#include "DataDetection.h"
#endif

namespace WebCore {
namespace ImageOverlay {

static const AtomString& imageOverlayElementIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("image-overlay", AtomString::ConstructFromLiteral);
    return identifier;
}

static const AtomString& imageOverlayDataDetectorClass()
{
    static MainThreadNeverDestroyed<const AtomString> className("image-overlay-data-detector-result", AtomString::ConstructFromLiteral);
    return className;
}

#if ENABLE(IMAGE_ANALYSIS)

static const AtomString& imageOverlayLineClass()
{
    static MainThreadNeverDestroyed<const AtomString> className("image-overlay-line", AtomString::ConstructFromLiteral);
    return className;
}

static const AtomString& imageOverlayTextClass()
{
    static MainThreadNeverDestroyed<const AtomString> className("image-overlay-text", AtomString::ConstructFromLiteral);
    return className;
}

static const AtomString& imageOverlayBlockClass()
{
    static MainThreadNeverDestroyed<const AtomString> className("image-overlay-block", AtomString::ConstructFromLiteral);
    return className;
}

#endif // ENABLE(IMAGE_ANALYSIS)

static const AtomString& imageOverlayCroppedImageIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("image-overlay-cropped-image", AtomString::ConstructFromLiteral);
    return identifier;
}

static const AtomString& imageOverlayCroppedImageBackdropIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("image-overlay-cropped-image-backdrop", AtomString::ConstructFromLiteral);
    return identifier;
}

bool hasOverlay(const HTMLElement& element)
{
    auto shadowRoot = element.shadowRoot();
    if (LIKELY(!shadowRoot || shadowRoot->mode() != ShadowRootMode::UserAgent))
        return false;

    return shadowRoot->hasElementWithId(*imageOverlayElementIdentifier().impl());
}

static RefPtr<HTMLElement> imageOverlayHost(const Node& node)
{
    auto host = node.shadowHost();
    if (!is<HTMLElement>(host))
        return nullptr;

    RefPtr element { &downcast<HTMLElement>(*host) };
    return hasOverlay(*element) ? element : nullptr;
}

bool isDataDetectorResult(const HTMLElement& element)
{
    return imageOverlayHost(element) && element.hasClass() && element.classNames().contains(imageOverlayDataDetectorClass());
}

std::optional<CharacterRange> characterRange(const VisibleSelection& selection)
{
    auto selectionRange = selection.range();
    if (!selectionRange)
        return std::nullopt;

    if (!isInsideOverlay(selection))
        return std::nullopt;

    std::optional<SimpleRange> imageOverlayRange;
    for (auto& ancestor : ancestorsOfType<HTMLDivElement>(*selection.start().containerNode())) {
        if (ancestor.getIdAttribute() == imageOverlayElementIdentifier()) {
            imageOverlayRange = makeRangeSelectingNodeContents(ancestor);
            break;
        }
    }

    if (!imageOverlayRange)
        return std::nullopt;

    return characterRange(resolveCharacterLocation(*imageOverlayRange, 0), *selectionRange);
}

bool isInsideOverlay(const VisibleSelection& selection)
{
    auto range = selection.range();
    return range && isInsideOverlay(*range);
}

bool isInsideOverlay(const SimpleRange& range)
{
    RefPtr commonAncestor = commonInclusiveAncestor<ComposedTree>(range);
    if (!commonAncestor)
        return false;

    return isInsideOverlay(*commonAncestor);
}

bool isInsideOverlay(const Node& node)
{
    auto host = imageOverlayHost(node);
    if (!host)
        return false;

    return host->userAgentShadowRoot()->contains(node);
}

bool isOverlayText(const Node* node)
{
    return node && isOverlayText(*node);
}

bool isOverlayText(const Node& node)
{
    auto host = imageOverlayHost(node);
    if (!host)
        return false;

    if (RefPtr overlay = host->userAgentShadowRoot()->getElementById(imageOverlayElementIdentifier()))
        return node.isDescendantOf(*overlay);

    return false;
}

void removeOverlaySoonIfNeeded(HTMLElement& element)
{
    if (!hasOverlay(element))
        return;

    element.document().eventLoop().queueTask(TaskSource::InternalAsyncTask, [weakElement = WeakPtr { element }] {
        RefPtr protectedElement = weakElement.get();
        if (!protectedElement)
            return;

        RefPtr shadowRoot = protectedElement->userAgentShadowRoot();
        if (!shadowRoot)
            return;

        if (RefPtr overlay = shadowRoot->getElementById(imageOverlayElementIdentifier()))
            overlay->remove();

#if ENABLE(IMAGE_ANALYSIS)
        if (auto page = protectedElement->document().page())
            page->resetTextRecognitionResult(*protectedElement);
#endif
    });
}

static void installImageOverlayStyleSheet(ShadowRoot& shadowRoot)
{
    static MainThreadNeverDestroyed<const String> shadowStyle(StringImpl::createWithoutCopying(imageOverlayUserAgentStyleSheet, sizeof(imageOverlayUserAgentStyleSheet)));
    auto style = HTMLStyleElement::create(HTMLNames::styleTag, shadowRoot.document(), false);
    style->setTextContent(shadowStyle);
    shadowRoot.appendChild(WTFMove(style));
}

IntRect containerRect(HTMLElement& element)
{
    auto* renderer = element.renderer();
    if (!is<RenderImage>(renderer))
        return { };

    if (!renderer->opacity())
        return { 0, 0, element.offsetWidth(), element.offsetHeight() };

    return enclosingIntRect(downcast<RenderImage>(*renderer).replacedContentRect());
}

#if ENABLE(IMAGE_ANALYSIS)

struct LineElements {
    Ref<HTMLDivElement> line;
    Vector<Ref<HTMLElement>> children;
    RefPtr<HTMLBRElement> lineBreak;
};

struct Elements {
    RefPtr<HTMLDivElement> root;
    Vector<LineElements> lines;
    Vector<Ref<HTMLDivElement>> dataDetectors;
    Vector<Ref<HTMLDivElement>> blocks;
};

static Elements updateSubtree(HTMLElement& element, const TextRecognitionResult& result)
{
    bool hadExistingElements = false;
    Elements elements;
    RefPtr<HTMLElement> mediaControlsContainer;
#if ENABLE(MODERN_MEDIA_CONTROLS)
    mediaControlsContainer = ([&]() -> RefPtr<HTMLElement> {
        RefPtr mediaElement = dynamicDowncast<HTMLMediaElement>(element);
        if (!mediaElement)
            return nullptr;

        Ref shadowRoot = mediaElement->ensureUserAgentShadowRoot();
        RefPtr controlsHost = mediaElement->mediaControlsHost();
        if (!controlsHost) {
            ASSERT_NOT_REACHED();
            return nullptr;
        }

        auto& containerClass = controlsHost->mediaControlsContainerClassName();
        for (auto& child : childrenOfType<HTMLDivElement>(shadowRoot.get())) {
            if (child.hasClass() && child.classNames().contains(containerClass))
                return &child;
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    })();
#endif // ENABLE(MODERN_MEDIA_CONTROLS)

    if (RefPtr shadowRoot = element.shadowRoot()) {
        if (hasOverlay(element)) {
            RefPtr<ContainerNode> containerForImageOverlay;
            if (mediaControlsContainer)
                containerForImageOverlay = mediaControlsContainer;
            else
                containerForImageOverlay = shadowRoot;
            for (auto& child : childrenOfType<HTMLDivElement>(*containerForImageOverlay)) {
                if (child.getIdAttribute() == imageOverlayElementIdentifier()) {
                    elements.root = &child;
                    hadExistingElements = true;
                    continue;
                }
            }
        }
    }

    bool canUseExistingElements = false;
    if (elements.root) {
        for (auto& childElement : childrenOfType<HTMLDivElement>(*elements.root)) {
            if (!childElement.hasClass())
                continue;

            auto& classes = childElement.classList();
            if (classes.contains(imageOverlayDataDetectorClass())) {
                elements.dataDetectors.append(childElement);
                continue;
            }

            if (classes.contains(imageOverlayBlockClass())) {
                elements.blocks.append(childElement);
                continue;
            }

            ASSERT(classes.contains(imageOverlayLineClass()));
            Vector<Ref<HTMLElement>> lineChildren;
            for (auto& text : childrenOfType<HTMLDivElement>(childElement))
                lineChildren.append(text);
            elements.lines.append({ childElement, WTFMove(lineChildren), childrenOfType<HTMLBRElement>(childElement).first() });
        }

        canUseExistingElements = ([&] {
            if (result.dataDetectors.size() != elements.dataDetectors.size())
                return false;

            if (result.lines.size() != elements.lines.size())
                return false;

            if (result.blocks.size() != elements.blocks.size())
                return false;

            for (size_t lineIndex = 0; lineIndex < result.lines.size(); ++lineIndex) {
                auto& lineResult = result.lines[lineIndex];
                auto& childResults = lineResult.children;

                auto& lineElements = elements.lines[lineIndex];
                auto& childTextElements = lineElements.children;
                if (lineResult.hasTrailingNewline != static_cast<bool>(lineElements.lineBreak))
                    return false;

                if (childResults.size() != childTextElements.size())
                    return false;

                for (size_t childIndex = 0; childIndex < childResults.size(); ++childIndex) {
                    if (childResults[childIndex].text != childTextElements[childIndex]->textContent().stripWhiteSpace())
                        return false;
                }
            }

            for (size_t index = 0; index < result.blocks.size(); ++index) {
                if (result.blocks[index].text != elements.blocks[index]->textContent())
                    return false;
            }

            return true;
        })();

        if (!canUseExistingElements) {
            elements.root->removeChildren();
            elements = { elements.root, { }, { }, { } };
        }
    }

    if (result.isEmpty())
        return { };

    Ref document = element.document();
    Ref shadowRoot = element.ensureUserAgentShadowRoot();
    if (!canUseExistingElements) {
        if (!elements.root) {
            auto rootContainer = HTMLDivElement::create(document.get());
            rootContainer->setIdAttribute(imageOverlayElementIdentifier());
            rootContainer->setTranslate(false);
            if (document->isImageDocument())
                rootContainer->setInlineStyleProperty(CSSPropertyWebkitUserSelect, CSSValueText);

            if (mediaControlsContainer)
                mediaControlsContainer->appendChild(rootContainer);
            else
                shadowRoot->appendChild(rootContainer);
            elements.root = rootContainer.copyRef();
        }
        elements.lines.reserveInitialCapacity(result.lines.size());
        for (auto& line : result.lines) {
            auto lineContainer = HTMLDivElement::create(document.get());
            lineContainer->classList().add(imageOverlayLineClass());
            elements.root->appendChild(lineContainer);
            LineElements lineElements { lineContainer, { }, { } };
            lineElements.children.reserveInitialCapacity(line.children.size());
            for (size_t childIndex = 0; childIndex < line.children.size(); ++childIndex) {
                auto& child = line.children[childIndex];
                auto textContainer = HTMLDivElement::create(document.get());
                textContainer->classList().add(imageOverlayTextClass());
                lineContainer->appendChild(textContainer);
                textContainer->appendChild(Text::create(document.get(), child.hasLeadingWhitespace ? makeString('\n', child.text) : child.text));
                lineElements.children.uncheckedAppend(WTFMove(textContainer));
            }

            if (line.hasTrailingNewline) {
                lineElements.lineBreak = HTMLBRElement::create(document.get());
                lineContainer->appendChild(*lineElements.lineBreak);
            }

            elements.lines.uncheckedAppend(WTFMove(lineElements));
        }

#if ENABLE(DATA_DETECTION)
        elements.dataDetectors.reserveInitialCapacity(result.dataDetectors.size());
        for (auto& dataDetector : result.dataDetectors) {
            auto dataDetectorContainer = DataDetection::createElementForImageOverlay(document.get(), dataDetector);
            dataDetectorContainer->classList().add(imageOverlayDataDetectorClass());
            elements.root->appendChild(dataDetectorContainer);
            elements.dataDetectors.uncheckedAppend(WTFMove(dataDetectorContainer));
        }
#endif // ENABLE(DATA_DETECTION)

        elements.blocks.reserveInitialCapacity(result.blocks.size());
        for (auto& block : result.blocks) {
            auto blockContainer = HTMLDivElement::create(document.get());
            blockContainer->classList().add(imageOverlayBlockClass());
            elements.root->appendChild(blockContainer);
            blockContainer->appendChild(Text::create(document.get(), makeString('\n', block.text)));
            elements.blocks.uncheckedAppend(WTFMove(blockContainer));
        }

        if (document->quirks().needsToForceUserSelectAndUserDragWhenInstallingImageOverlay()) {
            element.setInlineStyleProperty(CSSPropertyWebkitUserSelect, CSSValueText);
            element.setInlineStyleProperty(CSSPropertyWebkitUserDrag, CSSValueAuto);
        }
    }

    if (!hadExistingElements)
        installImageOverlayStyleSheet(shadowRoot.get());

    return elements;
}

static RotatedRect fitElementToQuad(HTMLElement& container, const FloatQuad& quad)
{
    auto bounds = rotatedBoundingRectWithMinimumAngleOfRotation(quad, 0.01);
    container.setInlineStyleProperty(CSSPropertyWidth, bounds.size.width(), CSSUnitType::CSS_PX);
    container.setInlineStyleProperty(CSSPropertyHeight, bounds.size.height(), CSSUnitType::CSS_PX);
    container.setInlineStyleProperty(CSSPropertyTransform, makeString(
        "translate("_s,
        std::round(bounds.center.x() - (bounds.size.width() / 2)), "px, "_s,
        std::round(bounds.center.y() - (bounds.size.height() / 2)), "px) "_s,
        bounds.angleInRadians ? makeString("rotate("_s, bounds.angleInRadians, "rad) "_s) : emptyString()
    ));
    return bounds;
}

void updateWithTextRecognitionResult(HTMLElement& element, const TextRecognitionResult& result, CacheTextRecognitionResults cacheTextRecognitionResults)
{
    auto elements = updateSubtree(element, result);
    if (!elements.root)
        return;

    Ref document = element.document();
    document->updateLayoutIgnorePendingStylesheets();

    auto* renderer = element.renderer();
    if (!is<RenderImage>(renderer))
        return;

    downcast<RenderImage>(*renderer).setHasImageOverlay();

    auto containerRect = ImageOverlay::containerRect(element);
    auto convertToContainerCoordinates = [&](const FloatQuad& normalizedQuad) {
        auto quad = normalizedQuad;
        quad.scale(containerRect.width(), containerRect.height());
        quad.move(containerRect.x(), containerRect.y());
        return quad;
    };

    bool applyUserSelectAll = document->isImageDocument() || renderer->style().userSelect() != UserSelect::None;
    for (size_t lineIndex = 0; lineIndex < result.lines.size(); ++lineIndex) {
        auto& lineElements = elements.lines[lineIndex];
        auto& lineContainer = lineElements.line;
        auto& line = result.lines[lineIndex];
        auto lineQuad = convertToContainerCoordinates(line.normalizedQuad);
        if (lineQuad.isEmpty())
            continue;

        auto lineBounds = fitElementToQuad(lineContainer.get(), lineQuad);
        auto offsetAlongHorizontalAxis = [&](const FloatPoint& quadPoint1, const FloatPoint& quadPoint2) {
            auto intervalLength = lineBounds.size.width();
            auto mid = midPoint(quadPoint1, quadPoint2);
            mid.moveBy(-lineBounds.center);
            mid.rotate(-lineBounds.angleInRadians);
            return intervalLength * clampTo<float>(0.5 + mid.x() / intervalLength, 0, 1);
        };

        auto offsetsAlongHorizontalAxis = line.children.map([&](auto& child) -> WTF::Range<float> {
            auto textQuad = convertToContainerCoordinates(child.normalizedQuad);
            return {
                offsetAlongHorizontalAxis(textQuad.p1(), textQuad.p4()),
                offsetAlongHorizontalAxis(textQuad.p2(), textQuad.p3())
            };
        });

        for (size_t childIndex = 0; childIndex < line.children.size(); ++childIndex) {
            auto& textContainer = lineElements.children[childIndex];
            bool lineHasOneChild = line.children.size() == 1;
            float horizontalMarginToMinimizeSelectionGaps = lineHasOneChild ? 0 : 0.125;
            float horizontalOffset = lineHasOneChild ? 0 : -horizontalMarginToMinimizeSelectionGaps;
            float horizontalExtent = lineHasOneChild ? 0 : horizontalMarginToMinimizeSelectionGaps;

            if (lineHasOneChild) {
                horizontalOffset += offsetsAlongHorizontalAxis[childIndex].begin();
                horizontalExtent += offsetsAlongHorizontalAxis[childIndex].end();
            } else if (!childIndex) {
                horizontalOffset += offsetsAlongHorizontalAxis[childIndex].begin();
                horizontalExtent += (offsetsAlongHorizontalAxis[childIndex].end() + offsetsAlongHorizontalAxis[childIndex + 1].begin()) / 2;
            } else if (childIndex == line.children.size() - 1) {
                horizontalOffset += (offsetsAlongHorizontalAxis[childIndex - 1].end() + offsetsAlongHorizontalAxis[childIndex].begin()) / 2;
                horizontalExtent += offsetsAlongHorizontalAxis[childIndex].end();
            } else {
                horizontalOffset += (offsetsAlongHorizontalAxis[childIndex - 1].end() + offsetsAlongHorizontalAxis[childIndex].begin()) / 2;
                horizontalExtent += (offsetsAlongHorizontalAxis[childIndex].end() + offsetsAlongHorizontalAxis[childIndex + 1].begin()) / 2;
            }

            FloatSize targetSize { horizontalExtent - horizontalOffset, lineBounds.size.height() };
            if (targetSize.isEmpty()) {
                textContainer->setInlineStyleProperty(CSSPropertyTransform, "scale(0, 0)");
                continue;
            }

            document->updateLayoutIfDimensionsOutOfDate(textContainer);

            FloatSize sizeBeforeTransform;
            if (auto* renderer = textContainer->renderBoxModelObject()) {
                sizeBeforeTransform = {
                    adjustLayoutUnitForAbsoluteZoom(renderer->offsetWidth(), *renderer).toFloat(),
                    adjustLayoutUnitForAbsoluteZoom(renderer->offsetHeight(), *renderer).toFloat(),
                };
            }

            if (sizeBeforeTransform.isEmpty()) {
                textContainer->setInlineStyleProperty(CSSPropertyTransform, "scale(0, 0)");
                continue;
            }

            textContainer->setInlineStyleProperty(CSSPropertyTransform, makeString(
                "translate("_s,
                horizontalOffset + (targetSize.width() - sizeBeforeTransform.width()) / 2, "px, "_s,
                (targetSize.height() - sizeBeforeTransform.height()) / 2, "px) "_s,
                "scale("_s, targetSize.width() / sizeBeforeTransform.width(), ", "_s, targetSize.height() / sizeBeforeTransform.height(), ") "_s
            ));

            textContainer->setInlineStyleProperty(CSSPropertyWebkitUserSelect, applyUserSelectAll ? CSSValueAll : CSSValueNone);
        }

        if (document->isImageDocument())
            lineContainer->setInlineStyleProperty(CSSPropertyCursor, CSSValueText);
    }

#if ENABLE(DATA_DETECTION)
    for (size_t index = 0; index < result.dataDetectors.size(); ++index) {
        auto dataDetectorContainer = elements.dataDetectors[index];
        auto& dataDetector = result.dataDetectors[index];
        if (dataDetector.normalizedQuads.isEmpty())
            continue;

        auto firstQuad = dataDetector.normalizedQuads.first();
        if (firstQuad.isEmpty())
            continue;

        // FIXME: We should come up with a way to coalesce the bounding quads into one or more rotated rects with the same angle of rotation.
        fitElementToQuad(dataDetectorContainer.get(), convertToContainerCoordinates(firstQuad));
    }

    if (!result.dataDetectors.isEmpty()) {
        auto* page = document->page();
        if (auto* overlayController = page ? page->imageOverlayControllerIfExists() : nullptr)
            overlayController->textRecognitionResultsChanged(element);
    }
#endif // ENABLE(DATA_DETECTION)

    constexpr float minScaleForFontSize = 0;
    constexpr float initialScaleForFontSize = 0.8;
    constexpr float maxScaleForFontSize = 1;
    constexpr unsigned iterationLimit = 10;
    constexpr float minTargetScore = 0.9;
    constexpr float maxTargetScore = 1.02;

    struct FontSizeAdjustmentState {
        Ref<HTMLElement> container;
        FloatSize targetSize;
        float scale { initialScaleForFontSize };
        float minScale { minScaleForFontSize };
        float maxScale { maxScaleForFontSize };
        bool mayRequireAdjustment { true };
    };

    Vector<FontSizeAdjustmentState> elementsToAdjust;
    elementsToAdjust.reserveInitialCapacity(result.blocks.size());

    ASSERT(result.blocks.size() == elements.blocks.size());
    for (size_t index = 0; index < result.blocks.size(); ++index) {
        auto& block = result.blocks[index];
        if (block.normalizedQuad.isEmpty())
            continue;

        auto blockContainer = elements.blocks[index];
        auto bounds = fitElementToQuad(blockContainer.get(), convertToContainerCoordinates(block.normalizedQuad));
        blockContainer->setInlineStyleProperty(CSSPropertyFontSize, initialScaleForFontSize * bounds.size.height(), CSSUnitType::CSS_PX);
        elementsToAdjust.uncheckedAppend({ WTFMove(blockContainer), bounds.size });
    }

    unsigned currentIteration = 0;
    while (!elementsToAdjust.isEmpty()) {
        document->updateLayoutIgnorePendingStylesheets();

        for (auto& state : elementsToAdjust) {
            RefPtr textNode = state.container->firstChild();
            if (!is<Text>(textNode)) {
                ASSERT_NOT_REACHED();
                state.mayRequireAdjustment = false;
                continue;
            }

            auto* textRenderer = downcast<Text>(*textNode).renderer();
            if (!textRenderer) {
                ASSERT_NOT_REACHED();
                state.mayRequireAdjustment = false;
                continue;
            }

            auto currentScore = (textRenderer->linesBoundingBox().size() / state.targetSize).maxDimension();
            if (currentScore < minTargetScore)
                state.minScale = state.scale;
            else if (currentScore > maxTargetScore)
                state.maxScale = state.scale;
            else {
                state.mayRequireAdjustment = false;
                continue;
            }

            state.scale = (state.minScale + state.maxScale) / 2;
            state.container->setInlineStyleProperty(CSSPropertyFontSize, state.targetSize.height() * state.scale, CSSUnitType::CSS_PX);
        }

        elementsToAdjust.removeAllMatching([](auto& state) {
            return !state.mayRequireAdjustment;
        });

        if (++currentIteration > iterationLimit) {
            // Fall back to the largest font size that still vertically fits within the container.
            for (auto& state : elementsToAdjust)
                state.container->setInlineStyleProperty(CSSPropertyFontSize, state.targetSize.height() * state.minScale, CSSUnitType::CSS_PX);
            break;
        }
    }

    if (RefPtr frame = document->frame())
        frame->eventHandler().scheduleCursorUpdate();

    if (cacheTextRecognitionResults == CacheTextRecognitionResults::Yes) {
        if (auto* page = document->page())
            page->cacheTextRecognitionResult(element, containerRect, result);
    }
}

#endif // ENABLE(IMAGE_ANALYSIS)

std::unique_ptr<CroppedImage> CroppedImage::install(HTMLElement& host, Ref<SharedBuffer>&& imageData, const String& mimeType, FloatRect normalizedRect)
{
    Ref document = host.document();
    Ref shadowRoot = host.ensureUserAgentShadowRoot();
    RefPtr imageOverlayRoot = dynamicDowncast<HTMLDivElement>(shadowRoot->getElementById(imageOverlayElementIdentifier()));
    if (!imageOverlayRoot) {
        imageOverlayRoot = HTMLDivElement::create(document.get());
        imageOverlayRoot->setIdAttribute(imageOverlayElementIdentifier());
        imageOverlayRoot->setTranslate(false);
        shadowRoot->appendChild(*imageOverlayRoot);
        installImageOverlayStyleSheet(shadowRoot.get());
    }

    document->updateLayoutIgnorePendingStylesheets();

    if (auto* renderer = dynamicDowncast<RenderImage>(host.renderer()))
        renderer->setHasImageOverlay();

    auto containerRect = ImageOverlay::containerRect(host);
    auto cropRect = normalizedRect;
    cropRect.scale(containerRect.width(), containerRect.height());
    cropRect.move(containerRect.x(), containerRect.y());

    auto croppedImageBackdrop = HTMLDivElement::create(document.get());
    croppedImageBackdrop->setIdAttribute(imageOverlayCroppedImageBackdropIdentifier());
    imageOverlayRoot->appendChild(croppedImageBackdrop.get());

    auto croppedImage = HTMLImageElement::create(document.get());
    auto croppedImageURL = DOMURL::createObjectURL(document.get(), Blob::create(document.ptr(), imageData->extractData(), mimeType));
    croppedImage->setIdAttribute(imageOverlayCroppedImageIdentifier());
    croppedImage->setAttributeWithoutSynchronization(HTMLNames::srcAttr, croppedImageURL);
    croppedImage->setInlineStyleProperty(CSSPropertyLeft, cropRect.x(), CSSUnitType::CSS_PX);
    croppedImage->setInlineStyleProperty(CSSPropertyTop, cropRect.y(), CSSUnitType::CSS_PX);
    croppedImage->setInlineStyleProperty(CSSPropertyWidth, cropRect.width(), CSSUnitType::CSS_PX);
    croppedImage->setInlineStyleProperty(CSSPropertyHeight, cropRect.height(), CSSUnitType::CSS_PX);
    imageOverlayRoot->appendChild(croppedImage.get());

    document->updateLayoutIgnorePendingStylesheets();
    croppedImageBackdrop->setInlineStyleProperty(CSSPropertyOpacity, 0.5, CSSUnitType::CSS_NUMBER);

    return makeUnique<CroppedImage>(document.get(), host, croppedImageBackdrop.get(), croppedImageURL);
}

CroppedImage::CroppedImage(Document& document, HTMLElement& host, HTMLElement& croppedImageBackdrop, const String& imageURL)
    : m_document(document)
    , m_host(host)
    , m_croppedImageBackdrop(croppedImageBackdrop)
    , m_imageURL(imageURL)
{
    setVisibility(true);
}

CroppedImage::~CroppedImage()
{
    if (RefPtr document = m_document.get())
        DOMURL::revokeObjectURL(*document, m_imageURL);

    RefPtr host = m_host.get();
    if (!host)
        return;

    RefPtr shadowRoot = host->shadowRoot();
    if (!shadowRoot || shadowRoot->mode() != ShadowRootMode::UserAgent)
        return;

    if (RefPtr croppedImage = shadowRoot->getElementById(imageOverlayCroppedImageIdentifier()))
        croppedImage->remove();

    if (RefPtr croppedImageBackdrop = shadowRoot->getElementById(imageOverlayCroppedImageBackdropIdentifier()))
        croppedImageBackdrop->remove();
}

void CroppedImage::setVisibility(bool visible)
{
    RefPtr croppedImageBackdrop = m_croppedImageBackdrop.get();
    if (!croppedImageBackdrop)
        return;

    croppedImageBackdrop->document().updateLayoutIgnorePendingStylesheets();
    croppedImageBackdrop->setInlineStyleProperty(CSSPropertyOpacity, visible ? 0.5 : 0, CSSUnitType::CSS_NUMBER);
}

} // namespace ImageOverlay
} // namespace WebCore
