/*
 *  Copyright (C) 2013 University of Washington. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"

#include "FrameSnapshotting.h"

#include "Document.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "ImageBuffer.h"
#include "Page.h"
#include "RenderObject.h"

namespace WebCore {

struct ScopedFramePaintingState {
    ScopedFramePaintingState(Frame& frame, Node* node)
    : frame(frame)
    , node(node)
    , paintBehavior(frame.view()->paintBehavior())
    , backgroundColor(frame.view()->baseBackgroundColor())
    {
        ASSERT(!node || node->renderer());
    }

    ~ScopedFramePaintingState()
    {
        frame.view()->setPaintBehavior(paintBehavior);
        frame.view()->setBaseBackgroundColor(backgroundColor);
        frame.view()->setNodeToDraw(nullptr);
    }

    Frame& frame;
    Node* node;
    PaintBehavior paintBehavior;
    Color backgroundColor;
};

std::unique_ptr<ImageBuffer> snapshotFrameRect(Frame& frame, const IntRect& imageRect, SnapshotOptions options)
{
    FrameView::SelectionInSnapshot shouldIncludeSelection = FrameView::IncludeSelection;
    if (options & SnapshotOptionsExcludeSelectionHighlighting)
        shouldIncludeSelection = FrameView::ExcludeSelection;

    FrameView::CoordinateSpaceForSnapshot coordinateSpace = FrameView::DocumentCoordinates;
    if (options & SnapshotOptionsInViewCoordinates)
        coordinateSpace = FrameView::ViewCoordinates;

    PaintBehavior textPaintBehavior = 0;
    if (options & SnapshotOptionsForceBlackText)
        textPaintBehavior = PaintBehaviorForceBlackText;

    const ScopedFramePaintingState state(frame, nullptr);

    // Other paint behaviors are set by paintContentsForSnapshot.
    PaintBehavior existingBehavior = frame.view()->paintBehavior();
    frame.view()->setPaintBehavior(existingBehavior | textPaintBehavior);
    frame.document()->updateLayout();

    float deviceScaleFactor = frame.page() ? frame.page()->deviceScaleFactor() : 1;
    IntRect usedRect(imageRect);
    usedRect.scale(deviceScaleFactor);

    std::unique_ptr<ImageBuffer> buffer = std::unique_ptr<ImageBuffer>(ImageBuffer::create(usedRect.size(), deviceScaleFactor, ColorSpaceDeviceRGB).leakPtr());
    if (!buffer)
        return nullptr;
    buffer->context()->translate(-usedRect.x(), -usedRect.y());
    buffer->context()->clip(FloatRect(0, 0, usedRect.maxX(), usedRect.maxY()));

    frame.view()->paintContentsForSnapshot(buffer->context(), usedRect, shouldIncludeSelection, coordinateSpace);
    return buffer;
}

std::unique_ptr<ImageBuffer> snapshotSelection(Frame& frame, SnapshotOptions options)
{
    if (!frame.selection().isRange())
        return nullptr;

    // Force selection highlighting to be included.
    options &= ~SnapshotOptionsExcludeSelectionHighlighting;
    IntRect selectionRect = enclosingIntRect(frame.selection().bounds());
    return snapshotFrameRect(frame, selectionRect, options);
}

std::unique_ptr<ImageBuffer> snapshotNode(Frame& frame, Node& node)
{
    if (!node.renderer())
        return nullptr;

    const ScopedFramePaintingState state(frame, &node);

    frame.view()->setBaseBackgroundColor(Color::transparent);
    frame.document()->updateLayout();
    frame.view()->setNodeToDraw(&node);

    // Document::updateLayout may have blown away the original renderer.
    RenderObject* renderer = node.renderer();
    if (!renderer)
        return nullptr;

    LayoutRect topLevelRect;
    IntRect paintingRect = pixelSnappedIntRect(renderer->paintingRootRect(topLevelRect));
    return snapshotFrameRect(frame, paintingRect);
}

} // namespace WebCore
