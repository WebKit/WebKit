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

#include "DOMTokenList.h"
#include "Document.h"
#include "ElementChildIterator.h"
#include "EventHandler.h"
#include "EventLoop.h"
#include "FloatSize.h"
#include "GeometryUtilities.h"
#include "HTMLBRElement.h"
#include "HTMLDivElement.h"
#include "HTMLMediaElement.h"
#include "HTMLStyleElement.h"
#include "MediaControlsHost.h"
#include "Page.h"
#include "Quirks.h"
#include "RenderImage.h"
#include "ShadowRoot.h"
#include "SimpleRange.h"
#include "Text.h"
#include "TextRecognitionResult.h"
#include "UserAgentStyleSheets.h"
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

static const AtomString& imageOverlayDataDetectorClassName()
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

#endif // ENABLE(IMAGE_ANALYSIS)

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
    return imageOverlayHost(element) && element.hasClass() && element.classNames().contains(imageOverlayDataDetectorClassName());
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

    if (RefPtr overlay = static_cast<TreeScope&>(*host->userAgentShadowRoot()).getElementById(imageOverlayElementIdentifier()))
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

        if (RefPtr overlay = static_cast<TreeScope&>(*shadowRoot).getElementById(imageOverlayElementIdentifier()))
            overlay->remove();

#if ENABLE(IMAGE_ANALYSIS)
        if (auto page = protectedElement->document().page())
            page->resetTextRecognitionResult(*protectedElement);
#endif
    });
}

#if ENABLE(IMAGE_ANALYSIS)

IntRect containerRect(HTMLElement& element)
{
    auto* renderer = element.renderer();
    if (!is<RenderImage>(renderer))
        return { };

    if (!renderer->opacity())
        return { 0, 0, element.offsetWidth(), element.offsetHeight() };

    return enclosingIntRect(downcast<RenderImage>(*renderer).replacedContentRect());
}

struct LineElements {
    Ref<HTMLDivElement> line;
    Vector<Ref<HTMLElement>> children;
};

struct Elements {
    RefPtr<HTMLDivElement> root;
    Vector<LineElements> lines;
    Vector<Ref<HTMLDivElement>> dataDetectors;
};

static Elements updateSubtree(HTMLElement& element, const TextRecognitionResult& result)
{
    bool hadExistingElements = false;
    Elements elements;
    RefPtr<HTMLElement> mediaControlsContainer;
    if (RefPtr shadowRoot = element.shadowRoot()) {
#if ENABLE(MODERN_MEDIA_CONTROLS)
        if (is<HTMLMediaElement>(element)) {
            if (RefPtr controlsHost = downcast<HTMLMediaElement>(element).mediaControlsHost()) {
                auto& containerClass = controlsHost->mediaControlsContainerClassName();
                for (auto& child : childrenOfType<HTMLDivElement>(*shadowRoot)) {
                    if (child.hasClass() && child.classNames().contains(containerClass)) {
                        mediaControlsContainer = &child;
                        break;
                    }
                }
            }
        }
#endif
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

    if (elements.root) {
        for (auto& lineOrDataDetector : childrenOfType<HTMLDivElement>(*elements.root)) {
            if (!lineOrDataDetector.hasClass())
                continue;

            if (lineOrDataDetector.classList().contains(imageOverlayLineClass())) {
                LineElements lineElements { lineOrDataDetector, { } };
                for (auto& text : childrenOfType<HTMLDivElement>(lineOrDataDetector))
                    lineElements.children.append(text);
                elements.lines.append(WTFMove(lineElements));
            } else if (lineOrDataDetector.classList().contains(imageOverlayDataDetectorClassName()))
                elements.dataDetectors.append(lineOrDataDetector);
        }

        bool canUseExistingElements = ([&] {
            if (result.dataDetectors.size() != elements.dataDetectors.size())
                return false;

            if (result.lines.size() != elements.lines.size())
                return false;

            for (size_t lineIndex = 0; lineIndex < result.lines.size(); ++lineIndex) {
                auto& childResults = result.lines[lineIndex].children;
                auto& childTextElements = elements.lines[lineIndex].children;
                if (childResults.size() != childTextElements.size())
                    return false;

                for (size_t childIndex = 0; childIndex < childResults.size(); ++childIndex) {
                    if (childResults[childIndex].text != childTextElements[childIndex]->textContent().stripWhiteSpace())
                        return false;
                }
            }

            return true;
        })();

        if (!canUseExistingElements) {
            elements.root->remove();
            elements = { };
        }
    }

    if (result.isEmpty())
        return { };

    Ref document = element.document();
    Ref shadowRoot = element.ensureUserAgentShadowRoot();
    if (!elements.root) {
        auto rootContainer = HTMLDivElement::create(document.get());
        rootContainer->setIdAttribute(imageOverlayElementIdentifier());
        if (document->isImageDocument())
            rootContainer->setInlineStyleProperty(CSSPropertyWebkitUserSelect, CSSValueText);

        if (mediaControlsContainer)
            mediaControlsContainer->appendChild(rootContainer);
        else
            shadowRoot->appendChild(rootContainer);
        elements.root = rootContainer.copyRef();
        elements.lines.reserveInitialCapacity(result.lines.size());
        for (auto& line : result.lines) {
            auto lineContainer = HTMLDivElement::create(document.get());
            lineContainer->classList().add(imageOverlayLineClass());
            rootContainer->appendChild(lineContainer);
            LineElements lineElements { lineContainer, { } };
            lineElements.children.reserveInitialCapacity(line.children.size());
            for (size_t childIndex = 0; childIndex < line.children.size(); ++childIndex) {
                auto& child = line.children[childIndex];
                auto textContainer = HTMLDivElement::create(document.get());
                textContainer->classList().add(imageOverlayTextClass());
                lineContainer->appendChild(textContainer);
                textContainer->appendChild(Text::create(document.get(), child.hasLeadingWhitespace ? makeString('\n', child.text) : child.text));
                lineElements.children.uncheckedAppend(WTFMove(textContainer));
            }

            lineContainer->appendChild(HTMLBRElement::create(document.get()));
            elements.lines.uncheckedAppend(WTFMove(lineElements));
        }

#if ENABLE(DATA_DETECTION)
        elements.dataDetectors.reserveInitialCapacity(result.dataDetectors.size());
        for (auto& dataDetector : result.dataDetectors) {
            auto dataDetectorContainer = DataDetection::createElementForImageOverlay(document.get(), dataDetector);
            dataDetectorContainer->classList().add(imageOverlayDataDetectorClassName());
            rootContainer->appendChild(dataDetectorContainer);
            elements.dataDetectors.uncheckedAppend(WTFMove(dataDetectorContainer));
        }
#endif // ENABLE(DATA_DETECTION)

        if (document->quirks().needsToForceUserSelectWhenInstallingImageOverlay())
            element.setInlineStyleProperty(CSSPropertyWebkitUserSelect, CSSValueText);
    }

    if (!hadExistingElements) {
        static MainThreadNeverDestroyed<const String> shadowStyle(StringImpl::createWithoutCopying(imageOverlayUserAgentStyleSheet, sizeof(imageOverlayUserAgentStyleSheet)));
        auto style = HTMLStyleElement::create(HTMLNames::styleTag, document.get(), false);
        style->setTextContent(shadowStyle);
        shadowRoot->appendChild(WTFMove(style));
    }

    return elements;
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

        auto lineBounds = rotatedBoundingRectWithMinimumAngleOfRotation(lineQuad, 0.01);
        lineContainer->setInlineStyleProperty(CSSPropertyWidth, lineBounds.size.width(), CSSUnitType::CSS_PX);
        lineContainer->setInlineStyleProperty(CSSPropertyHeight, lineBounds.size.height(), CSSUnitType::CSS_PX);
        lineContainer->setInlineStyleProperty(CSSPropertyTransform, makeString(
            "translate("_s,
            std::round(lineBounds.center.x() - (lineBounds.size.width() / 2)), "px, "_s,
            std::round(lineBounds.center.y() - (lineBounds.size.height() / 2)), "px) "_s,
            lineBounds.angleInRadians ? makeString("rotate("_s, lineBounds.angleInRadians, "rad) "_s) : emptyString()
        ));

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

        // FIXME: We should come up with a way to coalesce the bounding quads into one or more rotated rects with the same angle of rotation.
        auto targetQuad = convertToContainerCoordinates(dataDetector.normalizedQuads.first());
        auto targetBounds = rotatedBoundingRectWithMinimumAngleOfRotation(targetQuad, 0.01);
        dataDetectorContainer->setInlineStyleProperty(CSSPropertyWidth, targetBounds.size.width(), CSSUnitType::CSS_PX);
        dataDetectorContainer->setInlineStyleProperty(CSSPropertyHeight, targetBounds.size.height(), CSSUnitType::CSS_PX);
        dataDetectorContainer->setInlineStyleProperty(CSSPropertyTransform, makeString(
            "translate("_s,
            std::round(targetBounds.center.x() - (targetBounds.size.width() / 2)), "px, "_s,
            std::round(targetBounds.center.y() - (targetBounds.size.height() / 2)), "px) "_s,
            targetBounds.angleInRadians ? makeString("rotate("_s, targetBounds.angleInRadians, "rad) "_s) : emptyString()
        ));
    }
#endif // ENABLE(DATA_DETECTION)

    if (RefPtr frame = document->frame())
        frame->eventHandler().scheduleCursorUpdate();

    if (cacheTextRecognitionResults == CacheTextRecognitionResults::Yes) {
        if (auto* page = document->page())
            page->cacheTextRecognitionResult(element, containerRect, result);
    }
}

#endif // ENABLE(IMAGE_ANALYSIS)

} // namespace ImageOverlay
} // namespace WebCore
