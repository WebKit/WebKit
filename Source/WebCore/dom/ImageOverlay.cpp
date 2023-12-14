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
#include "ElementAncestorIteratorInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ElementRareData.h"
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
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderImage.h"
#include "RenderText.h"
#include "ShadowRoot.h"
#include "SharedBuffer.h"
#include "SimpleRange.h"
#include "Text.h"
#include "TextIterator.h"
#include "TextRecognitionResult.h"
#include "TreeScopeInlines.h"
#include "UserAgentStyleSheets.h"
#include "VisibleSelection.h"
#include <wtf/Range.h>
#include <wtf/Scope.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>

#if ENABLE(DATA_DETECTION)
#include "DataDetection.h"
#endif

namespace WebCore {
namespace ImageOverlay {

static const AtomString& imageOverlayElementIdentifier()
{
    static MainThreadNeverDestroyed<const AtomString> identifier("image-overlay"_s);
    return identifier;
}

static const AtomString& imageOverlayDataDetectorClass()
{
    static MainThreadNeverDestroyed<const AtomString> className("image-overlay-data-detector-result"_s);
    return className;
}

#if ENABLE(IMAGE_ANALYSIS)

static const AtomString& imageOverlayLineClass()
{
    static MainThreadNeverDestroyed<const AtomString> className("image-overlay-line"_s);
    return className;
}

static const AtomString& imageOverlayTextClass()
{
    static MainThreadNeverDestroyed<const AtomString> className("image-overlay-text"_s);
    return className;
}

static const AtomString& imageOverlayBlockClass()
{
    static MainThreadNeverDestroyed<const AtomString> className("image-overlay-block"_s);
    return className;
}

#endif // ENABLE(IMAGE_ANALYSIS)

bool hasOverlay(const HTMLElement& element)
{
    RefPtr shadowRoot = element.shadowRoot();
    if (LIKELY(!shadowRoot || shadowRoot->mode() != ShadowRootMode::UserAgent))
        return false;

    return shadowRoot->hasElementWithId(imageOverlayElementIdentifier());
}

static RefPtr<HTMLElement> imageOverlayHost(const Node& node)
{
    RefPtr host = dynamicDowncast<HTMLElement>(node.shadowHost());
    if (!host)
        return nullptr;

    return hasOverlay(*host) ? host : nullptr;
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
    for (Ref ancestor : ancestorsOfType<HTMLDivElement>(*selection.start().containerNode())) {
        if (ancestor->getIdAttribute() == imageOverlayElementIdentifier()) {
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
    RefPtr host = imageOverlayHost(node);
    return host && host->userAgentShadowRoot()->contains(node);
}

bool isOverlayText(const Node* node)
{
    return node && isOverlayText(*node);
}

bool isOverlayText(const Node& node)
{
    RefPtr host = imageOverlayHost(node);
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

    element.protectedDocument()->eventLoop().queueTask(TaskSource::InternalAsyncTask, [weakElement = WeakPtr { element }] {
        RefPtr element = weakElement.get();
        if (!element)
            return;

        RefPtr shadowRoot = element->userAgentShadowRoot();
        if (!shadowRoot)
            return;

        if (RefPtr overlay = shadowRoot->getElementById(imageOverlayElementIdentifier()))
            overlay->remove();

#if ENABLE(IMAGE_ANALYSIS)
        if (RefPtr page = element->document().page())
            page->resetTextRecognitionResult(*element);
#endif
    });
}

IntRect containerRect(HTMLElement& element)
{
    CheckedPtr renderer = dynamicDowncast<RenderImage>(element.renderer());
    if (!renderer)
        return { };

    if (!renderer->opacity())
        return { 0, 0, element.offsetWidth(), element.offsetHeight() };

    return enclosingIntRect(renderer->replacedContentRect());
}

#if ENABLE(IMAGE_ANALYSIS)

static void installImageOverlayStyleSheet(ShadowRoot& shadowRoot)
{
    static MainThreadNeverDestroyed<const String> shadowStyle(StringImpl::createWithoutCopying(imageOverlayUserAgentStyleSheet, sizeof(imageOverlayUserAgentStyleSheet)));
    Ref style = HTMLStyleElement::create(HTMLNames::styleTag, shadowRoot.protectedDocument(), false);
    style->setTextContent(String { shadowStyle });
    shadowRoot.appendChild(WTFMove(style));
}

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
        if (CheckedPtr renderer = dynamicDowncast<RenderImage>(element.renderer()))
            renderer->setHasImageOverlay();

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
                    if (childResults[childIndex].text != StringView(childTextElements[childIndex]->textContent()).trim(deprecatedIsSpaceOrNewline))
                        return false;
                }
            }

            for (size_t index = 0; index < result.blocks.size(); ++index) {
                auto textContentByLine = result.blocks[index].text.split(newlineCharacter);
                size_t lineIndex = 0;
                for (auto& text : childrenOfType<Text>(elements.blocks[index])) {
                    if (textContentByLine.size() <= lineIndex)
                        return false;

                    if (StringView(textContentByLine[lineIndex++]).trim(deprecatedIsSpaceOrNewline) != StringView(text.wholeText()).trim(deprecatedIsSpaceOrNewline))
                        return false;
                }
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
            Ref rootContainer = HTMLDivElement::create(document.get());
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
            Ref lineContainer = HTMLDivElement::create(document.get());
            lineContainer->classList().add(imageOverlayLineClass());
            elements.root->appendChild(lineContainer);
            LineElements lineElements { lineContainer, { }, { } };
            lineElements.children.reserveInitialCapacity(line.children.size());
            for (size_t childIndex = 0; childIndex < line.children.size(); ++childIndex) {
                auto& child = line.children[childIndex];
                Ref textContainer = HTMLDivElement::create(document.get());
                textContainer->classList().add(imageOverlayTextClass());
                lineContainer->appendChild(textContainer);
                textContainer->appendChild(Text::create(document.get(), child.hasLeadingWhitespace ? makeString('\n', child.text) : String { child.text }));
                lineElements.children.append(WTFMove(textContainer));
            }

            if (line.hasTrailingNewline) {
                Ref lineBreak = HTMLBRElement::create(document.get());
                lineContainer->appendChild(lineBreak.get());
                lineElements.lineBreak = WTFMove(lineBreak);
            }

            elements.lines.append(WTFMove(lineElements));
        }

#if ENABLE(DATA_DETECTION)
        elements.dataDetectors.reserveInitialCapacity(result.dataDetectors.size());
        for (auto& dataDetector : result.dataDetectors) {
            auto dataDetectorContainer = DataDetection::createElementForImageOverlay(document.get(), dataDetector);
            dataDetectorContainer->classList().add(imageOverlayDataDetectorClass());
            elements.root->appendChild(dataDetectorContainer);
            elements.dataDetectors.append(WTFMove(dataDetectorContainer));
        }
#endif // ENABLE(DATA_DETECTION)

        elements.blocks.reserveInitialCapacity(result.blocks.size());
        for (auto& block : result.blocks) {
            Ref blockContainer = HTMLDivElement::create(document.get());
            blockContainer->classList().add(imageOverlayBlockClass());
            auto lines = block.text.split(newlineCharacter);
            for (auto&& textContent : WTFMove(lines)) {
                if (blockContainer->hasChildNodes())
                    blockContainer->appendChild(HTMLBRElement::create(document.get()));
                blockContainer->appendChild(Text::create(document.get(), WTFMove(textContent)));
            }

            constexpr auto maxLineCountForCenterAlignedText = 2;
            if (lines.size() > maxLineCountForCenterAlignedText)
                blockContainer->setInlineStyleProperty(CSSPropertyTextAlign, CSSValueStart);

            elements.root->appendChild(blockContainer);
            elements.blocks.append(WTFMove(blockContainer));
        }
    }

    if (!hadExistingElements)
        installImageOverlayStyleSheet(shadowRoot.get());

    return elements;
}

enum class ConstrainHeight : bool { No, Yes };
static RotatedRect fitElementToQuad(HTMLElement& container, const FloatQuad& quad, ConstrainHeight constrainHeight = ConstrainHeight::Yes)
{
    auto bounds = rotatedBoundingRectWithMinimumAngleOfRotation(quad, 0.01);
    container.setInlineStyleProperty(CSSPropertyWidth, bounds.size.width(), CSSUnitType::CSS_PX);
    if (constrainHeight == ConstrainHeight::Yes)
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
    Ref document = element.document();
    document->updateLayoutIgnorePendingStylesheets();

    auto containerRect = ImageOverlay::containerRect(element);
    auto convertToContainerCoordinates = [&](const FloatQuad& normalizedQuad) {
        auto quad = normalizedQuad;
        quad.scale(containerRect.width(), containerRect.height());
        quad.move(containerRect.x(), containerRect.y());
        return quad;
    };

    bool smallImageWithSingleWord = [&] {
        constexpr auto smallImageDimensionThreshold = 64;
        if (containerRect.width() > smallImageDimensionThreshold || containerRect.height() > smallImageDimensionThreshold)
            return false;

        return result.blocks.isEmpty() && result.lines.size() == 1 && result.lines[0].children.size() == 1;
    }();

    if (smallImageWithSingleWord)
        return;

    auto elements = updateSubtree(element, result);
    if (!elements.root)
        return;

    {
        document->updateLayoutIgnorePendingStylesheets();
        CheckedPtr renderer = dynamicDowncast<RenderImage>(element.renderer());
        if (!renderer)
            return;

        renderer->setHasImageOverlay();
    }

    bool applyUserSelectAll = [&] {
        auto* renderer = dynamicDowncast<RenderImage>(element.renderer());
        return document->isImageDocument() || (renderer && renderer->style().userSelect() != UserSelect::None);
    }();

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
            auto startOffset = offsetAlongHorizontalAxis(textQuad.p1(), textQuad.p4());
            auto endOffset = offsetAlongHorizontalAxis(textQuad.p2(), textQuad.p3());
            return { std::min(startOffset, endOffset), std::max(startOffset, endOffset) };
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
                textContainer->setInlineStyleProperty(CSSPropertyTransform, "scale(0, 0)"_s);
                continue;
            }

            document->updateLayoutIfDimensionsOutOfDate(textContainer);

            FloatSize sizeBeforeTransform;
            if (CheckedPtr renderer = textContainer->renderBoxModelObject()) {
                sizeBeforeTransform = {
                    adjustLayoutUnitForAbsoluteZoom(renderer->offsetWidth(), *renderer).toFloat(),
                    adjustLayoutUnitForAbsoluteZoom(renderer->offsetHeight(), *renderer).toFloat(),
                };
            }

            if (sizeBeforeTransform.isEmpty()) {
                textContainer->setInlineStyleProperty(CSSPropertyTransform, "scale(0, 0)"_s);
                continue;
            }

            textContainer->setInlineStyleProperty(CSSPropertyTransform, makeString(
                "translate("_s,
                horizontalOffset + (targetSize.width() - sizeBeforeTransform.width()) / 2, "px, "_s,
                (targetSize.height() - sizeBeforeTransform.height()) / 2, "px) "_s,
                "scale("_s, targetSize.width() / sizeBeforeTransform.width(), ", "_s, targetSize.height() / sizeBeforeTransform.height(), ") "_s
            ));

            textContainer->setInlineStyleProperty(CSSPropertyWebkitUserSelect, applyUserSelectAll ? CSSValueAll : CSSValueNone);

            if (line.isVertical)
                textContainer->setInlineStyleProperty(CSSPropertyWritingMode, CSSValueVerticalRl);
        }

        if (document->isImageDocument())
            lineContainer->setInlineStyleProperty(CSSPropertyCursor, line.isVertical ? CSSValueVerticalText : CSSValueText);
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
        RefPtr page = document->page();
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

    auto setInlineStylesForBlock = [&](HTMLElement& block, float scale, float targetHeight) {
        float fontSize = scale * targetHeight;
        float borderRadius = fontSize / 5 + (targetHeight - fontSize) / 50;
        block.setInlineStyleProperty(CSSPropertyFontSize, fontSize, CSSUnitType::CSS_PX);
        block.setInlineStyleProperty(CSSPropertyBorderRadius, makeString(borderRadius, "px"_s));
        block.setInlineStyleProperty(CSSPropertyPaddingLeft, 2 * borderRadius, CSSUnitType::CSS_PX);
        block.setInlineStyleProperty(CSSPropertyPaddingRight, 2 * borderRadius, CSSUnitType::CSS_PX);
        block.setInlineStyleProperty(CSSPropertyPaddingTop, borderRadius, CSSUnitType::CSS_PX);
        block.setInlineStyleProperty(CSSPropertyPaddingBottom, borderRadius, CSSUnitType::CSS_PX);
    };

    ASSERT(result.blocks.size() == elements.blocks.size());

    size_t index = 0;
    auto elementsToAdjust = WTF::compactMap(result.blocks, [&](auto& block) -> std::optional<FontSizeAdjustmentState> {
        auto incrementIndex = makeScopeExit([&index] { ++index; });
        if (block.normalizedQuad.isEmpty())
            return std::nullopt;

        auto blockContainer = elements.blocks[index];
        auto bounds = fitElementToQuad(blockContainer.get(), convertToContainerCoordinates(block.normalizedQuad), ConstrainHeight::No);
        setInlineStylesForBlock(blockContainer.get(), initialScaleForFontSize, bounds.size.height());
        return FontSizeAdjustmentState { WTFMove(blockContainer), bounds.size };
    });

    unsigned currentIteration = 0;
    while (!elementsToAdjust.isEmpty()) {
        document->updateLayoutIgnorePendingStylesheets();

        for (auto& state : elementsToAdjust) {
            CheckedPtr box = state.container->renderBox();
            if (!box)
                continue;

            auto targetHeight = state.targetSize.height();
            auto currentScore = box->contentHeight() / targetHeight;
            bool hasHorizontalOverflow = box->hasHorizontalOverflow();
            if (currentScore < minTargetScore && !hasHorizontalOverflow)
                state.minScale = state.scale;
            else if (currentScore > maxTargetScore || hasHorizontalOverflow)
                state.maxScale = state.scale;
            else {
                state.mayRequireAdjustment = false;
                continue;
            }

            state.scale = (state.minScale + state.maxScale) / 2;
            setInlineStylesForBlock(state.container.get(), state.scale, targetHeight);
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
        if (RefPtr page = document->page())
            page->cacheTextRecognitionResult(element, containerRect, result);
    }
}

#endif // ENABLE(IMAGE_ANALYSIS)

} // namespace ImageOverlay
} // namespace WebCore
