/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Google Inc.
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

#include "Document.h"
#include "FloatRect.h"
#include "ImageBuffer.h"
#include "RenderView.h"
#include "Settings.h"

namespace WebCore {

namespace {

struct ScopedState {
    ScopedState(Frame* theFrame, RenderObject* theRenderer)
        : frame(theFrame)
        , renderer(theRenderer)
        , paintBehavior(frame->view()->paintBehavior())
        , backgroundColor(frame->view()->baseBackgroundColor())
    {
    }

    ~ScopedState()
    {
        if (renderer)
            renderer->updateDragState(false);
        frame->view()->setPaintBehavior(paintBehavior);
        frame->view()->setBaseBackgroundColor(backgroundColor);
        frame->view()->setNodeToDraw(0);
    }

    Frame* frame;
    RenderObject* renderer;
    PaintBehavior paintBehavior;
    Color backgroundColor;
};

} // namespace

DragImageRef Frame::nodeImage(Node* node)
{
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return 0;

    const ScopedState state(this, renderer);

    renderer->updateDragState(true);
    m_view->setPaintBehavior(state.paintBehavior | PaintBehaviorFlattenCompositingLayers);
    // When generating the drag image for an element, ignore the document background.
    m_view->setBaseBackgroundColor(colorWithOverrideAlpha(Color::white, 1.0));
    m_doc->updateLayout();
    m_view->setNodeToDraw(node); // Enable special sub-tree drawing mode.

    IntRect topLevelRect;
    IntRect paintingRect = renderer->paintingRootRect(topLevelRect);

    OwnPtr<ImageBuffer> buffer(ImageBuffer::create(paintingRect.size()));
    if (!buffer)
        return 0;
    buffer->context()->translate(-paintingRect.x(), -paintingRect.y());
    buffer->context()->clip(FloatRect(0, 0, paintingRect.right(), paintingRect.bottom()));

    m_view->paint(buffer->context(), paintingRect);

    RefPtr<Image> image = buffer->copyImage();
    return createDragImageFromImage(image.get());
}

DragImageRef Frame::dragImageForSelection()
{
    if (!selection()->isRange())
        return 0;

    const ScopedState state(this, 0);
    m_view->setPaintBehavior(PaintBehaviorSelectionOnly);
    m_doc->updateLayout();

    IntRect paintingRect = enclosingIntRect(selection()->bounds());

    OwnPtr<ImageBuffer> buffer(ImageBuffer::create(paintingRect.size()));
    if (!buffer)
        return 0;
    buffer->context()->translate(-paintingRect.x(), -paintingRect.y());
    buffer->context()->clip(FloatRect(0, 0, paintingRect.right(), paintingRect.bottom()));

    m_view->paint(buffer->context(), paintingRect);

    RefPtr<Image> image = buffer->copyImage();
    return createDragImageFromImage(image.get());
}

} // namespace WebCore
