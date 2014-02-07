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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "Range.h"
#include "RenderObject.h"
#include "RenderView.h"

namespace WebCore {

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
        : frame(frame)
        , node(node)
    {
        if (node.renderer())
            node.renderer()->updateDragState(true);
        frame.document()->updateLayout();
    }

    ~ScopedNodeDragEnabler()
    {
        if (node.renderer())
            node.renderer()->updateDragState(false);
    }

    const Frame& frame;
    const Node& node;
};

static DragImageRef createDragImageFromSnapshot(std::unique_ptr<ImageBuffer> snapshot, Node* node)
{
    if (!snapshot)
        return nullptr;

    ImageOrientationDescription orientation;
#if ENABLE(CSS_IMAGE_ORIENTATION)
    if (node) {
        RenderObject* renderer = node->renderer();
        if (!renderer)
            return nullptr;

        orientation.setRespectImageOrientation(renderer->shouldRespectImageOrientation());
        orientation.setImageOrientationEnum(renderer->style().imageOrientation());
    }
#else
    UNUSED_PARAM(node);
#endif
    RefPtr<Image> image = snapshot->copyImage(ImageBuffer::fastCopyImageMode(), Unscaled);
    if (!image)
        return nullptr;
    return createDragImageFromImage(image.get(), orientation);
}

DragImageRef createDragImageForNode(Frame& frame, Node& node)
{
    ScopedNodeDragEnabler enableDrag(frame, node);
    return createDragImageFromSnapshot(snapshotNode(frame, node), &node);
}

DragImageRef createDragImageForSelection(Frame& frame, bool forceBlackText)
{
    SnapshotOptions options = forceBlackText ? SnapshotOptionsForceBlackText : SnapshotOptionsNone;
    return createDragImageFromSnapshot(snapshotSelection(frame, options), nullptr);
}

struct ScopedFrameSelectionState {
    ScopedFrameSelectionState(Frame& frame)
        : frame(frame)
    {
        if (RenderView* root = frame.contentRenderer())
            root->getSelection(startRenderer, startOffset, endRenderer, endOffset);
    }

    ~ScopedFrameSelectionState()
    {
        if (RenderView* root = frame.contentRenderer())
            root->setSelection(startRenderer, startOffset, endRenderer, endOffset, RenderView::RepaintNothing);
    }

    const Frame& frame;
    RenderObject* startRenderer;
    RenderObject* endRenderer;
    int startOffset;
    int endOffset;
};

DragImageRef createDragImageForRange(Frame& frame, Range& range, bool forceBlackText)
{
    frame.document()->updateLayout();
    RenderView* view = frame.contentRenderer();
    if (!view)
        return nullptr;

    // To snapshot the range, temporarily select it and take selection snapshot.
    Position start = range.startPosition();
    Position candidate = start.downstream();
    if (candidate.deprecatedNode() && candidate.deprecatedNode()->renderer())
        start = candidate;

    Position end = range.endPosition();
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
    view->setSelection(startRenderer, start.deprecatedEditingOffset(), endRenderer, end.deprecatedEditingOffset(), RenderView::RepaintNothing);
    // We capture using snapshotFrameRect() because we fake up the selection using
    // FrameView but snapshotSelection() uses the selection from the Frame itself.
    return createDragImageFromSnapshot(snapshotFrameRect(frame, view->selectionBounds(), options), nullptr);
}

DragImageRef createDragImageForImage(Frame& frame, Node& node, IntRect& imageRect, IntRect& elementRect)
{
    ScopedNodeDragEnabler enableDrag(frame, node);

    RenderObject* renderer = node.renderer();
    if (!renderer)
        return nullptr;

    // Calculate image and element metrics for the client, then create drag image.
    LayoutRect topLevelRect;
    IntRect paintingRect = pixelSnappedIntRect(renderer->paintingRootRect(topLevelRect));

    if (paintingRect.isEmpty())
        return nullptr;

    elementRect = pixelSnappedIntRect(topLevelRect);
    imageRect = paintingRect;

    return createDragImageFromSnapshot(snapshotNode(frame, node), &node);
}

#if !PLATFORM(COCOA) && (!PLATFORM(WIN) || OS(WINCE))
DragImageRef createDragImageForLink(URL&, const String&, FontRenderingMode)
{
    return nullptr;
}
#endif

} // namespace WebCore

