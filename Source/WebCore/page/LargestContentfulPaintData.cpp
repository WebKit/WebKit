/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "LargestContentfulPaintData.h"

#include "CachedImage.h"
#include "ContainerNodeInlines.h"
#include "DocumentView.h"
#include "ElementInlines.h"
#include "FloatQuad.h"
#include "LargestContentfulPaint.h"
#include "LegacyRenderSVGImage.h"
#include "LocalDOMWindow.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "Page.h"
#include "Performance.h"
#include "RenderBlockFlow.h"
#include "RenderBox.h"
#include "RenderElementInlines.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"
#include "RenderObjectInlines.h"
#include "RenderReplaced.h"
#include "RenderSVGImage.h"
#include "RenderText.h"
#include "RenderView.h"
#include "VisibleRectContext.h"
#include <wtf/CheckedRef.h>
#include <wtf/Ref.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LargestContentfulPaintData);

LargestContentfulPaintData::LargestContentfulPaintData() = default;
LargestContentfulPaintData::~LargestContentfulPaintData() = default;

// https://w3c.github.io/paint-timing/#exposed-for-paint-timing
bool LargestContentfulPaintData::isExposedForPaintTiming(const Element& element)
{
    if (!element.protectedDocument()->isFullyActive())
        return false;

    if (!element.isInDocumentTree()) // Also checks isConnected().
        return false;

    return true;
}

// https://w3c.github.io/largest-contentful-paint/#largest-contentful-paint-candidate
bool LargestContentfulPaintData::isEligibleForLargestContentfulPaint(const Element& element, float effectiveVisualArea)
{
    CheckedPtr renderer = element.renderer();
    if (!renderer)
        return false;

    if (renderer->style().isEffectivelyTransparent())
        return false;

    // FIXME: Need to implement the response length vs. image size logic: webkit.org/b/299558.
    UNUSED_PARAM(effectiveVisualArea);
    return true;
}

// https://w3c.github.io/largest-contentful-paint/#sec-effective-visual-size
std::optional<float> LargestContentfulPaintData::effectiveVisualArea(const Element& element, CachedImage* image, FloatRect imageLocalRect, FloatRect intersectionRect, FloatSize viewportSize)
{
    RefPtr frameView = element.document().view();
    if (!frameView)
        return { };

    auto area = intersectionRect.area();
    if (area >= viewportSize.area())
        return { };

    if (image) {
        CheckedPtr renderer = element.renderer();
        if (!renderer)
            return { };

        auto absoluteContentRect = renderer->localToAbsoluteQuad(FloatRect(imageLocalRect)).boundingBox();

        auto intersectingContentRect = intersection(absoluteContentRect, intersectionRect);
        area = intersectingContentRect.area();

        auto naturalSize = image->imageSizeForRenderer(renderer.get(), 1);
        if (naturalSize.isEmpty())
            return { };

        auto scaleFactor = absoluteContentRect.area() / FloatSize { naturalSize }.area();
        if (scaleFactor > 1)
            area /= scaleFactor;

        return area;
    }

    return area;
}

// https://w3c.github.io/largest-contentful-paint/#sec-add-lcp-entry
void LargestContentfulPaintData::potentiallyAddLargestContentfulPaintEntry(Element& element, CachedImage* image, FloatRect imageLocalRect, FloatRect intersectionRect, MonotonicTime loadTime, DOMHighResTimeStamp paintTimestamp, std::optional<FloatSize>& viewportSize)
{
    bool isNewCandidate = false;
    if (image) {
        isNewCandidate = m_imageContentSet.ensure(element, [] {
            return WeakHashSet<CachedImage> { };
        }).iterator->value.add(*image).isNewEntry;
    } else
        isNewCandidate = m_textContentSet.add(element).isNewEntry;

    LOG_WITH_STREAM(LargestContentfulPaint, stream << "LargestContentfulPaintData " << this << " potentiallyAddLargestContentfulPaintEntry() " << element << " image " << (image ? image->url().string() : emptyString()) << " rect " << intersectionRect << " - isNewCandidate " << isNewCandidate);

    if (!isNewCandidate)
        return;

    Ref document = element.document();
    RefPtr window = document->window();
    if (!window)
        return;

    RefPtr view = document->view();
    if (!view)
        return;

    // The spec talks about trusted scroll events, but the intent is to detect user scrolls: https://github.com/w3c/largest-contentful-paint/issues/105
    if ((view->wasEverScrolledExplicitlyByUser() || window->hasDispatchedInputEvent()))
        return;

    if (!viewportSize)
        viewportSize = FloatSize { view->visualViewportRect().size() };

    auto elementArea = effectiveVisualArea(element, image, imageLocalRect, intersectionRect, *viewportSize);
    if (!elementArea)
        return;

    if (*elementArea <= m_largestPaintArea) {
        LOG_WITH_STREAM(LargestContentfulPaint, stream << " element area " << elementArea << " less than LCP " << m_largestPaintArea);
        return;
    }

    if (!isEligibleForLargestContentfulPaint(element, *elementArea))
        return;

    m_largestPaintArea = *elementArea;

    Ref pendingEntry = LargestContentfulPaint::create(0);
    pendingEntry->setElement(&element);
    pendingEntry->setSize(std::round<unsigned>(m_largestPaintArea));

    if (image) {
        pendingEntry->setURLString(image->url().string());
        auto loadTimestamp = window->protectedPerformance()->relativeTimeFromTimeOriginInReducedResolution(loadTime);
        pendingEntry->setLoadTime(loadTimestamp);
    }

    if (element.hasID())
        pendingEntry->setID(element.getIdAttribute().string());

    pendingEntry->setRenderTime(paintTimestamp);

    LOG_WITH_STREAM(LargestContentfulPaint, stream << " making new entry for " << element << " image " << (image ? image->url().string() : emptyString()) << " id " << pendingEntry->id() <<
        ": entry size " << pendingEntry->size() << ", loadTime " << pendingEntry->loadTime() << ", renderTime " << pendingEntry->renderTime());

    m_pendingEntry = RefPtr { WTFMove(pendingEntry) };
}

RefPtr<LargestContentfulPaint> LargestContentfulPaintData::takePendingEntry(DOMHighResTimeStamp paintTimestamp)
{
    std::optional<FloatSize> viewportSize;

    auto imageRecords = std::exchange(m_pendingImageRecords, { });
    for (auto [weakElement, imageAndData] : imageRecords) {
        RefPtr element = weakElement;
        if (!element)
            continue;

        // FIXME: This is doing multiple localToAbsolute on the same element, but multiple images per element is rare.
        for (auto [image, imageData] : imageAndData) {
            if (imageData.rect.isEmpty())
                continue;
            auto intersectionRect = computeViewportIntersectionRect(*element, imageData.rect);
            auto loadTimeSeconds = imageData.loadTime ? *imageData.loadTime : MonotonicTime::now();
            potentiallyAddLargestContentfulPaintEntry(*element, &image, imageData.rect, intersectionRect, loadTimeSeconds, paintTimestamp, viewportSize);
        }
    }

    auto textRecords = std::exchange(m_paintedTextRecords, { });
    for (auto [weakElement, rect] : textRecords) {
        RefPtr element = weakElement;
        if (!element)
            continue;

        auto intersectionRect = computeViewportIntersectionRect(*element, rect);
        potentiallyAddLargestContentfulPaintEntry(*element, nullptr, { }, intersectionRect, { }, paintTimestamp, viewportSize);
    }

    return std::exchange(m_pendingEntry, nullptr);
}

// This is a simplified version of IntersectionObserver::computeIntersectionState(). Some code should be shared.
FloatRect LargestContentfulPaintData::computeViewportIntersectionRect(Element& element, FloatRect localRect)
{
    RefPtr frameView = element.document().view();
    if (!frameView)
        return { };

    CheckedPtr targetRenderer = element.renderer();
    if (!targetRenderer)
        return { };

    if (targetRenderer->isSkippedContent())
        return { };

    CheckedPtr rootRenderer = frameView->renderView();
    auto layoutViewport = frameView->layoutViewportRect();

    auto localTargetBounds = LayoutRect { localRect };
    auto absoluteRects = targetRenderer->computeVisibleRectsInContainer(
        { localTargetBounds },
        &targetRenderer->checkedView().get(),
        {
            .hasPositionFixedDescendant = false,
            .dirtyRectIsFlipped = false,
            .options = {
                VisibleRectContext::Option::UseEdgeInclusiveIntersection,
                VisibleRectContext::Option::ApplyCompositedClips,
                VisibleRectContext::Option::ApplyCompositedContainerScrolls
            },
        }
    );

    if (!absoluteRects)
        return { };

    auto intersectionRect = layoutViewport;
    intersectionRect.edgeInclusiveIntersect(absoluteRects->clippedOverflowRect);
    return intersectionRect;
}

FloatRect LargestContentfulPaintData::computeViewportIntersectionRectForTextContainer(Element& element, const WeakHashSet<Text, WeakPtrImplWithEventTargetData>& textNodes)
{
    RefPtr frameView = element.document().view();
    if (!frameView)
        return { };

    CheckedPtr rootRenderer = frameView->renderView();
    auto layoutViewport = frameView->layoutViewportRect();

    IntRect absoluteTextBounds;
    for (RefPtr node : textNodes) {
        if (!node)
            continue;

        CheckedPtr renderer = node->renderer();
        if (!renderer)
            continue;

        if (renderer->isSkippedContent())
            continue;

        static constexpr bool useTransforms = true;
        auto absoluteBounds = renderer->absoluteBoundingBoxRect(useTransforms);
        absoluteTextBounds.unite(absoluteBounds);
    }

    auto intersectionRect = layoutViewport;
    intersectionRect.edgeInclusiveIntersect(absoluteTextBounds);

    return intersectionRect;
}

void LargestContentfulPaintData::didLoadImage(Element& element, CachedImage* image)
{
    LOG_WITH_STREAM(LargestContentfulPaint, stream << "LargestContentfulPaintData " << this << " didLoadImage() " << element << " image " << (image ? image->url().string() : emptyString()));

    if (!image)
        return;

    auto it = m_imageContentSet.find(element);
    if (it != m_imageContentSet.end()) {
        auto& imageSet = it->value;
        if (imageSet.contains(*image))
            return;
    }

    if (!isExposedForPaintTiming(element))
        return;

    auto addResult = m_pendingImageRecords.ensure(element, [] {
        return WeakHashMap<CachedImage, PendingImageData> { };
    });

    auto& imageRectMap = addResult.iterator->value;
    imageRectMap.ensure(*image, [] {
        return PendingImageData { { }, MonotonicTime::now() };
    });
}

void LargestContentfulPaintData::didPaintImage(Element& element, CachedImage* image, FloatRect localRect)
{
    LOG_WITH_STREAM(LargestContentfulPaint, stream << "LargestContentfulPaintData " << this << " didPaintImage() " << element << " image " << (image ? image->url().string() : emptyString()) << " localRect " << localRect);

    if (!image)
        return;

    if (localRect.isEmpty())
        return;

    auto it = m_imageContentSet.find(element);
    if (it != m_imageContentSet.end()) {
        auto& imageSet = it->value;
        if (imageSet.contains(*image))
            return;
    }

    if (!isExposedForPaintTiming(element))
        return;

    if (m_pendingImageRecords.isEmptyIgnoringNullReferences()) {
        if (RefPtr page = element.document().page())
            page->scheduleRenderingUpdate(RenderingUpdateStep::PaintTiming);
    }

    auto& imageRectMap = m_pendingImageRecords.ensure(element, [] {
        return WeakHashMap<CachedImage, PendingImageData> { };
    }).iterator->value;

    auto addResult = imageRectMap.ensure(*image, [&] {
        return PendingImageData { localRect, MonotonicTime::now() };
    });

    if (!addResult.isNewEntry) {
        auto& pendingImageData = addResult.iterator->value;
        if (localRect.area() > pendingImageData.rect.area())
            pendingImageData.rect = localRect;

        if (!pendingImageData.loadTime)
            pendingImageData.loadTime = MonotonicTime::now();
    }
}

void LargestContentfulPaintData::didPaintText(const RenderBlockFlow& formattingContextRoot, FloatRect localRect)
{
    if (localRect.isEmpty())
        return;

    auto& renderBlockFlow = const_cast<RenderBlockFlow&>(formattingContextRoot);
    // https://w3c.github.io/paint-timing/#sec-modifications-dom says to get the containing block.
    CheckedPtr<RenderBlock> containingBlock = &renderBlockFlow;
    if (containingBlock->isAnonymous()) {
        CheckedPtr ancestor = containingBlock->firstNonAnonymousAncestor();
        if (CheckedPtr ancestorBlock = dynamicDowncast<RenderBlock>(ancestor.get()))
            containingBlock = ancestorBlock;
        else
            containingBlock = containingBlock->containingBlock();
    }

    if (!containingBlock)
        return;

    RefPtr element = containingBlock->element();
    if (!element)
        return;

    if (m_textContentSet.contains(*element))
        return;

    if (!isExposedForPaintTiming(*element))
        return;

    if (containingBlock != &formattingContextRoot)
        localRect = formattingContextRoot.localToContainerQuad({ localRect }, containingBlock.get()).boundingBox();

    if (m_paintedTextRecords.isEmptyIgnoringNullReferences()) {
        if (RefPtr page = element->document().page())
            page->scheduleRenderingUpdate(RenderingUpdateStep::PaintTiming);
    }

    m_paintedTextRecords.ensure(*element, [] {
        return FloatRect { };
    }).iterator->value.unite(localRect);
}

} // namespace WebCore
