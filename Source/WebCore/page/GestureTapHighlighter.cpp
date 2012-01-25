/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GestureTapHighlighter.h"

#include "Element.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsTypes.h"
#include "Node.h"
#include "Page.h"
#include "RenderBoxModelObject.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderObject.h"

namespace WebCore {

namespace {

inline LayoutSize ownerFrameToMainFrameOffset(const RenderObject* o)
{
    ASSERT(o->node());
    Frame* containingFrame = o->frame();
    if (!containingFrame)
        return LayoutSize();

    Frame* mainFrame = containingFrame->page()->mainFrame();

    LayoutPoint mainFramePoint = mainFrame->view()->rootViewToContents(containingFrame->view()->contentsToRootView(LayoutPoint()));
    return toLayoutSize(mainFramePoint);
}

AffineTransform localToAbsoluteTransform(const RenderObject* o)
{
    AffineTransform transform;
    LayoutPoint referencePoint;

    while (o) {
        RenderObject* nextContainer = o->container();
        if (!nextContainer)
            break;

        LayoutSize containerOffset = o->offsetFromContainer(nextContainer, referencePoint);
        TransformationMatrix t;
        o->getTransformFromContainer(nextContainer, containerOffset, t);

        transform = t.toAffineTransform() * transform;
        referencePoint.move(containerOffset);
        o = nextContainer;
    }

    return transform;
}

Path pathForRenderBox(RenderBox* o)
{
    ASSERT(o);

    LayoutRect contentBox;
    LayoutRect paddingBox;
    LayoutRect borderBox;

    contentBox = o->contentBoxRect();
    paddingBox = LayoutRect(
            contentBox.x() - o->paddingLeft(),
            contentBox.y() - o->paddingTop(),
            contentBox.width() + o->paddingLeft() + o->paddingRight(),
            contentBox.height() + o->paddingTop() + o->paddingBottom());
    borderBox = LayoutRect(
            paddingBox.x() - o->borderLeft(),
            paddingBox.y() - o->borderTop(),
            paddingBox.width() + o->borderLeft() + o->borderRight(),
            paddingBox.height() + o->borderTop() + o->borderBottom());

    FloatRect rect(borderBox);
    rect.inflate(5);

    rect.move(ownerFrameToMainFrameOffset(o));

    Path path;
    path.addRoundedRect(rect, FloatSize(10, 10));

    return path;
}

Path pathForRenderInline(RenderInline* o)
{
    // FIXME: Adapt this to not just use the bounding box.
    LayoutRect borderBox = o->linesBoundingBox();

    FloatRect rect(borderBox);
    rect.inflate(5);

    rect.move(ownerFrameToMainFrameOffset(o));

    Path path;
    path.addRoundedRect(rect, FloatSize(10, 10));

    return path;
}

} // anonymous namespace

namespace GestureTapHighlighter {

Path pathForNodeHighlight(const Node* node)
{
    RenderObject* renderer = node->renderer();

    Path path;
    if (!renderer || (!renderer->isBox() && !renderer->isRenderInline()))
        return path;

    if (renderer->isBox())
        path = pathForRenderBox(toRenderBox(renderer));
    else
        path = pathForRenderInline(toRenderInline(renderer));

    path.transform(localToAbsoluteTransform(renderer));
    return path;
}

} // namespace GestureTapHighlighter

} // namespace WebCore
