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

inline bool contains(const LayoutRect& rect, int x)
{
    return !rect.isEmpty() && x >= rect.x() && x <= rect.maxX();
}

inline bool strikes(const LayoutRect& a, const LayoutRect& b)
{
    return !a.isEmpty() && !b.isEmpty()
        && a.x() <= b.maxX() && b.x() <= a.maxX()
        && a.y() <= b.maxY() && b.y() <= a.maxY();
}

inline void shiftXEdgesToContainIfStrikes(LayoutRect& rect, const LayoutRect& other)
{
    int leftSide = rect.x();
    int rightSide = rect.maxX();

    if (!other.isEmpty() && strikes(rect, other)) {
        leftSide = std::min(leftSide, other.x());
        rightSide = std::max(rightSide, other.maxX());
    }

    rect.setX(leftSide);
    rect.setWidth(rightSide - leftSide);
}

inline void addHighlightRect(Path& path, const LayoutRect& rect, const LayoutRect& prev, const LayoutRect& next)
{
    // The rounding check depends on the rects not intersecting eachother,
    // or being contained for that matter.
    ASSERT(!rect.intersects(prev));
    ASSERT(!rect.intersects(next));

    if (rect.isEmpty())
        return;

    const int rounding = 4;

    FloatRect copy(rect);
    copy.inflateX(rounding);
    copy.inflateY(rounding / 2);

    FloatSize rounded(rounding * 1.8, rounding * 1.8);
    FloatSize squared(0, 0);

    path.addBeziersForRoundedRect(copy,
            contains(prev, rect.x()) ? squared : rounded,
            contains(prev, rect.maxX()) ? squared : rounded,
            contains(next, rect.x()) ? squared : rounded,
            contains(next, rect.maxX()) ? squared : rounded);
}

Path pathForRenderer(RenderObject* o)
{
    ASSERT(o);
    Path path;

    Vector<LayoutRect> rects;
    o->addFocusRingRects(rects, /* acc. offset */ ownerFrameToMainFrameOffset(o));

    // The basic idea is to allow up to three different boxes in order to highlight
    // text with line breaks more nicer than using a bounding box.

    // Merge all center boxes (all but the first and the last).
    LayoutRect mid;
    for (int i = 1; i < rects.size() - 1; ++i)
        mid.uniteIfNonZero(rects.at(i));

    Vector<LayoutRect> drawableRects;

    if (!mid.isEmpty())
        drawableRects.append(mid);

    // Add the first box, but merge it with the center boxes if it intersects.
    if (rects.size() && !rects.first().isEmpty()) {
        if (drawableRects.size() && drawableRects.last().intersects(rects.first()))
            drawableRects.last().unite(rects.first());
        else
            drawableRects.prepend(rects.first());
    }

    // Add the last box, but merge it with the center boxes if it intersects.
    if (rects.size() > 1 && !rects.last().isEmpty()) {
        if (drawableRects.size() && drawableRects.last().intersects(rects.last()))
            drawableRects.last().unite(rects.last());
        else
            drawableRects.append(rects.last());
    }

    // Adjust middle to boundaries of first and last.
    if (drawableRects.size() == 3) {
        LayoutRect& middle = drawableRects.at(1);
        shiftXEdgesToContainIfStrikes(middle, drawableRects.at(0));
        shiftXEdgesToContainIfStrikes(middle, drawableRects.at(2));
    }

    for (int i = 0; i < drawableRects.size(); ++i) {
        LayoutRect prev = (i - 1) >= 0 ? drawableRects.at(i - 1) : LayoutRect();
        LayoutRect next = (i + 1) < drawableRects.size() ? drawableRects.at(i + 1) : LayoutRect();
        addHighlightRect(path, drawableRects.at(i), prev, next);
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

    path = pathForRenderer(renderer);
    path.transform(localToAbsoluteTransform(renderer));

    return path;
}

} // namespace GestureTapHighlighter

} // namespace WebCore
