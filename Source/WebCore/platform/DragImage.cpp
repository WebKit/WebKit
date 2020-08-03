/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DragImage.h"

#include "Frame.h"
#include "FrameSnapshotting.h"
#include "FrameView.h"
#include "ImageBuffer.h"
#include "NotImplemented.h"
#include "RenderElement.h"
#include "RenderView.h"
#include "SimpleRange.h"
#include "TextIndicator.h"

namespace WebCore {

#if PLATFORM(COCOA)
const float ColorSwatchCornerRadius = 4;
const float ColorSwatchStrokeSize = 4;
const float ColorSwatchWidth = 24;
#endif

DragImageRef fitDragImageToMaxSize(DragImageRef image, const IntSize& layoutSize, const IntSize& maxSize)
{
    float heightResizeRatio = 0.0f;
    float widthResizeRatio = 0.0f;
    float resizeRatio = -1.0f;
    IntSize originalSize = dragImageSize(image);

    if (layoutSize.width() > maxSize.width()) {
        widthResizeRatio = maxSize.width() / (float)layoutSize.width();
        resizeRatio = widthResizeRatio;
    }

    if (layoutSize.height() > maxSize.height()) {
        heightResizeRatio = maxSize.height() / (float)layoutSize.height();
        if ((resizeRatio < 0.0f) || (resizeRatio > heightResizeRatio))
            resizeRatio = heightResizeRatio;
    }

    if (layoutSize == originalSize)
        return resizeRatio > 0.0f ? scaleDragImage(image, FloatSize(resizeRatio, resizeRatio)) : image;

    // The image was scaled in the webpage so at minimum we must account for that scaling.
    float scaleX = layoutSize.width() / (float)originalSize.width();
    float scaleY = layoutSize.height() / (float)originalSize.height();
    if (resizeRatio > 0.0f) {
        scaleX *= resizeRatio;
        scaleY *= resizeRatio;
    }

    return scaleDragImage(image, FloatSize(scaleX, scaleY));
}

struct ScopedNodeDragEnabler {
    ScopedNodeDragEnabler(Frame& frame, Node& node)
        : element(is<Element>(node) ? &downcast<Element>(node) : nullptr)
    {
        if (element)
            element->setBeingDragged(true);
        frame.document()->updateLayout();
    }

    ~ScopedNodeDragEnabler()
    {
        if (element)
            element->setBeingDragged(false);
    }

    RefPtr<Element> element;
};

static DragImageRef createDragImageFromSnapshot(std::unique_ptr<ImageBuffer> snapshot, Node* node)
{
    if (!snapshot)
        return nullptr;

    ImageOrientation orientation;
    if (node) {
        RenderObject* renderer = node->renderer();
        if (!renderer || !is<RenderElement>(renderer))
            return nullptr;

        orientation = downcast<RenderElement>(*renderer).imageOrientation();
    }

    RefPtr<Image> image = ImageBuffer::sinkIntoImage(WTFMove(snapshot), PreserveResolution::Yes);
    if (!image)
        return nullptr;
    return createDragImageFromImage(image.get(), orientation);
}

DragImageRef createDragImageForNode(Frame& frame, Node& node)
{
    ScopedNodeDragEnabler enableDrag(frame, node);
    return createDragImageFromSnapshot(snapshotNode(frame, node), &node);
}

#if !PLATFORM(IOS_FAMILY) || !ENABLE(DRAG_SUPPORT)

DragImageRef createDragImageForSelection(Frame& frame, TextIndicatorData&, bool forceBlackText)
{
    SnapshotOptions options = forceBlackText ? SnapshotOptionsForceBlackText : SnapshotOptionsNone;
    return createDragImageFromSnapshot(snapshotSelection(frame, options), nullptr);
}

#endif

struct ScopedFrameSelectionState {
    ScopedFrameSelectionState(Frame& frame)
        : frame(frame)
    {
        if (auto* renderView = frame.contentRenderer())
            selection = renderView->selection().get();
    }

    ~ScopedFrameSelectionState()
    {
        if (auto* renderView = frame.contentRenderer()) {
            ASSERT(selection);
            renderView->selection().setSelection(selection.value(), HighlightData::RepaintMode::Nothing);
        }
    }

    const Frame& frame;
    Optional<HighlightData::RenderRange> selection;
};

#if !PLATFORM(IOS_FAMILY)

DragImageRef createDragImageForRange(Frame& frame, const SimpleRange& range, bool forceBlackText)
{
    frame.document()->updateLayout();
    RenderView* view = frame.contentRenderer();
    if (!view)
        return nullptr;

    // To snapshot the range, temporarily select it and take selection snapshot.
    Position start = createLegacyEditingPosition(range.start);
    Position candidate = start.downstream();
    if (candidate.deprecatedNode() && candidate.deprecatedNode()->renderer())
        start = candidate;

    Position end = createLegacyEditingPosition(range.end);
    candidate = end.upstream();
    if (candidate.deprecatedNode() && candidate.deprecatedNode()->renderer())
        end = candidate;

    if (start.isNull() || end.isNull() || start == end)
        return nullptr;

    const ScopedFrameSelectionState selectionState(frame);

    RenderObject* startRenderer = start.deprecatedNode()->renderer();
    RenderObject* endRenderer = end.deprecatedNode()->renderer();
    if (!startRenderer || !endRenderer)
        return nullptr;

    SnapshotOptions options = SnapshotOptionsPaintSelectionOnly | (forceBlackText ? SnapshotOptionsForceBlackText : SnapshotOptionsNone);
    int startOffset = start.deprecatedEditingOffset();
    int endOffset = end.deprecatedEditingOffset();
    ASSERT(startOffset >= 0 && endOffset >= 0);
    view->selection().setSelection({ startRenderer, endRenderer, static_cast<unsigned>(startOffset), static_cast<unsigned>(endOffset) }, HighlightData::RepaintMode::Nothing);
    // We capture using snapshotFrameRect() because we fake up the selection using
    // FrameView but snapshotSelection() uses the selection from the Frame itself.
    return createDragImageFromSnapshot(snapshotFrameRect(frame, view->selection().boundsClippedToVisibleContent(), options), nullptr);
}

#endif

DragImageRef createDragImageForImage(Frame& frame, Node& node, IntRect& imageRect, IntRect& elementRect)
{
    ScopedNodeDragEnabler enableDrag(frame, node);

    RenderObject* renderer = node.renderer();
    if (!renderer)
        return nullptr;

    // Calculate image and element metrics for the client, then create drag image.
    LayoutRect topLevelRect;
    IntRect paintingRect = snappedIntRect(renderer->paintingRootRect(topLevelRect));

    if (paintingRect.isEmpty())
        return nullptr;

    elementRect = snappedIntRect(topLevelRect);
    imageRect = paintingRect;

    return createDragImageFromSnapshot(snapshotNode(frame, node), &node);
}

#if !PLATFORM(IOS_FAMILY) || !ENABLE(DRAG_SUPPORT)
DragImageRef platformAdjustDragImageForDeviceScaleFactor(DragImageRef image, float deviceScaleFactor)
{
    // Later code expects the drag image to be scaled by device's scale factor.
    return scaleDragImage(image, { deviceScaleFactor, deviceScaleFactor });
}
#endif

#if !PLATFORM(MAC)
const int linkDragBorderInset = 2;

IntPoint dragOffsetForLinkDragImage(DragImageRef dragImage)
{
    IntSize size = dragImageSize(dragImage);
    return { -size.width() / 2, -linkDragBorderInset };
}

FloatPoint anchorPointForLinkDragImage(DragImageRef dragImage)
{
    IntSize size = dragImageSize(dragImage);
    return { 0.5, static_cast<float>((size.height() - linkDragBorderInset) / size.height()) };
}
#endif

DragImage::DragImage()
    : m_dragImageRef { nullptr }
{
}

DragImage::DragImage(DragImageRef dragImageRef)
    : m_dragImageRef { dragImageRef }
{
}

DragImage::DragImage(DragImage&& other)
    : m_dragImageRef { std::exchange(other.m_dragImageRef, nullptr) }
{
    m_indicatorData = other.m_indicatorData;
    m_visiblePath = other.m_visiblePath;
}

DragImage& DragImage::operator=(DragImage&& other)
{
    if (m_dragImageRef)
        deleteDragImage(m_dragImageRef);

    m_dragImageRef = std::exchange(other.m_dragImageRef, nullptr);
    m_indicatorData = other.m_indicatorData;
    m_visiblePath = other.m_visiblePath;

    return *this;
}

DragImage::~DragImage()
{
    if (m_dragImageRef)
        deleteDragImage(m_dragImageRef);
}

#if !PLATFORM(COCOA) && !PLATFORM(GTK) && !PLATFORM(WIN)

IntSize dragImageSize(DragImageRef)
{
    notImplemented();
    return { 0, 0 };
}

void deleteDragImage(DragImageRef)
{
    notImplemented();
}

DragImageRef scaleDragImage(DragImageRef, FloatSize)
{
    notImplemented();
    return nullptr;
}

DragImageRef dissolveDragImageToFraction(DragImageRef, float)
{
    notImplemented();
    return nullptr;
}

DragImageRef createDragImageFromImage(Image*, ImageOrientation)
{
    notImplemented();
    return nullptr;
}

DragImageRef createDragImageIconForCachedImageFilename(const String&)
{
    notImplemented();
    return nullptr;
}

DragImageRef createDragImageForLink(Element&, URL&, const String&, TextIndicatorData&, FontRenderingMode, float)
{
    notImplemented();
    return nullptr;
}

#endif

} // namespace WebCore

