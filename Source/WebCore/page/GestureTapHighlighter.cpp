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

inline LayoutPoint ownerFrameToMainFrameOffset(const RenderObject* o)
{
    ASSERT(o->node());
    Frame* containingFrame = o->frame();
    if (!containingFrame)
        return LayoutPoint();

    Frame* mainFrame = containingFrame->page()->mainFrame();

    LayoutPoint mainFramePoint = mainFrame->view()->rootViewToContents(containingFrame->view()->contentsToRootView(LayoutPoint()));
    return mainFramePoint;
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
    const int rounding = 4;

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
    rect.inflate(rounding);

    rect.move(toLayoutSize(ownerFrameToMainFrameOffset(o)));

    Path path;
    FloatSize rounded(rounding * 1.8, rounding * 1.8);
    path.addRoundedRect(rect, rounded);

    return path;
}

void addRectWithRoundedCorners(Path& path, const LayoutRect& rect, bool topLeft, bool topRight, bool bottomLeft, bool bottomRight)
{
    const int rounding = 4;

    FloatRect copy(rect);
    copy.inflateX(rounding);
    copy.inflateY(rounding / 2);

    FloatSize rounded(rounding * 1.8, rounding * 1.8);
    FloatSize squared(0, 0);

    path.addBeziersForRoundedRect(copy,
            topLeft ? rounded : squared, topRight ? rounded : squared,
            bottomLeft ? rounded : squared, bottomRight ? rounded : squared);
}

inline bool contains(LayoutRect rect, int x)
{
    return !rect.isEmpty() && x >= rect.x() && x <= rect.maxX();
}

Path pathForRenderInline(RenderInline* o)
{
    ASSERT(o);
    Path path;

    Vector<LayoutRect> rects;
    o->absoluteRects(rects, /* acc. offset */ ownerFrameToMainFrameOffset(o));

    LayoutRect first = rects.size() ? rects.first() : LayoutRect();
    LayoutRect last = rects.size() > 1 ? rects.last() : LayoutRect();
    LayoutRect middle;
    for (int i = 1; i < rects.size() - 1; ++i)
        middle.uniteIfNonZero(rects.at(i));

    if (!middle.isEmpty()) {
        int leftSide = middle.x();
        int rightSide = middle.maxX();

        if (!first.isEmpty()) {
            leftSide = std::min(leftSide, first.x());
            rightSide = std::max(rightSide, first.maxX());
        }
        if (!last.isEmpty()) {
            leftSide = std::min(leftSide, last.x());
            rightSide = std::max(rightSide, last.maxX());
        }

        middle.setX(leftSide);
        middle.setWidth(rightSide - leftSide);
    }

    if (!first.isEmpty()) {
        bool roundBottomLeft = !contains(middle, first.x()) && !contains(last, first.x());
        bool roundBottomRight = !contains(middle, first.maxX()) && !contains(last, first.maxX());
        addRectWithRoundedCorners(path, first, /* roundTopLeft */ true, /* roundTopRight */ true, roundBottomLeft, roundBottomRight);
    }

    if (!middle.isEmpty()) {
        bool roundTopLeft = !contains(first, middle.x());
        bool roundBottomRight = !contains(last, middle.maxX());
        addRectWithRoundedCorners(path, middle, roundTopLeft, /* roundTopRight */ false, /* roundBottomLeft */ false, roundBottomRight);
    }

    if (!last.isEmpty()) {
        bool roundTopLeft = !contains(middle, last.x()) && !contains(first, last.x());
        bool roundTopRight = !contains(middle, last.maxX()) && !contains(first, last.maxX());
        addRectWithRoundedCorners(path, last, roundTopLeft, roundTopRight, /* roundBottomLeft */ true, /* roundBottomRight */ true);
    }

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
