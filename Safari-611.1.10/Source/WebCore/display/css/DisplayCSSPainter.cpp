/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DisplayCSSPainter.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBoxClip.h"
#include "DisplayBoxPainter.h"
#include "DisplayBoxRareGeometry.h"
#include "DisplayContainerBox.h"
#include "DisplayPaintingContext.h"
#include "DisplayStackingItem.h"
#include "DisplayStyle.h"
#include "DisplayTree.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "TransformationMatrix.h"

#define SHOW_ITEM_EXTENTS 0

namespace WebCore {
namespace Display {

static void applyClipIfNecessary(const Box& box, PaintingContext& paintingContext, GraphicsContextStateSaver& stateSaver)
{
    if (is<BoxModelBox>(box) && box.style().hasClippedOverflow()) {
        stateSaver.save();
        auto roundedInnerBorder = downcast<BoxModelBox>(box).innerBorderRoundedRect();
        paintingContext.context.clipRoundedRect(roundedInnerBorder);
    }
}

static void applyAncestorClip(const BoxModelBox& box, PaintingContext& paintingContext)
{
    auto* boxClip = box.ancestorClip();
    if (!boxClip || !boxClip->clipRect())
        return;

    if (!boxClip->affectedByBorderRadius()) {
        paintingContext.context.clip(*boxClip->clipRect());
        return;
    }

    for (auto& roundedRect : boxClip->clipStack())
        paintingContext.context.clipRoundedRect(roundedRect);
}

static void applyEffects(const StackingItem& stackingItem, const Box& box, PaintingContext& paintingContext, TransparencyLayerScope& transparencyScope)
{
    if (box.style().opacity() < 1) {
        paintingContext.context.clip(stackingItem.paintedBoundsIncludingDescendantItems());
        transparencyScope.beginLayer(box.style().opacity());
    }
}

// FIXME: Make this an iterator.
void CSSPainter::recursivePaintDescendantsForPhase(const ContainerBox& containerBox, PaintingContext& paintingContext, PaintPhase paintPhase)
{
    auto stateSaver = GraphicsContextStateSaver { paintingContext.context, false };
    applyClipIfNecessary(containerBox, paintingContext, stateSaver);

    for (const auto* child = containerBox.firstChild(); child; child = child->nextSibling()) {
        auto& box = *child;
        ASSERT(!box.participatesInZOrderSorting());

        switch (paintPhase) {
        case PaintPhase::BlockBackgrounds:
            if (!box.style().isFloating() && !box.style().isPositioned() && is<BoxModelBox>(box))
                BoxPainter::paintBoxDecorations(downcast<BoxModelBox>(box), paintingContext);
            break;
        case PaintPhase::Floats:
            if (box.style().isFloating() && !box.style().isPositioned() && is<BoxModelBox>(box))
                BoxPainter::paintBoxDecorations(downcast<BoxModelBox>(box), paintingContext);
            break;
        case PaintPhase::BlockForegrounds:
            if (!box.style().isFloating() && !box.style().isPositioned())
                BoxPainter::paintBoxContent(box, paintingContext);
        };
        if (is<ContainerBox>(box))
            recursivePaintDescendantsForPhase(downcast<ContainerBox>(box), paintingContext, paintPhase);
    }
}

void CSSPainter::recursivePaintDescendants(const ContainerBox& containerBox, PaintingContext& paintingContext)
{
    // For all its in-flow, non-positioned, block-level descendants in tree order: If the element is a block, list-item, or other block equivalent:
    // Box decorations.
    // Table decorations.
    recursivePaintDescendantsForPhase(containerBox, paintingContext, PaintPhase::BlockBackgrounds);

    // All non-positioned floating descendants, in tree order. For each one of these, treat the element as if it created a new stacking context,
    // but any positioned descendants and descendants which actually create a new stacking context should be considered part of the parent
    // stacking context, not this new one.
    recursivePaintDescendantsForPhase(containerBox, paintingContext, PaintPhase::Floats);

    // If the element is an inline element that generates a stacking context, then:
    // FIXME: Handle this case.
    
    // Otherwise: first for the element, then for all its in-flow, non-positioned, block-level descendants in tree order:
    // 1. If the element is a block-level replaced element, then: the replaced content, atomically.
    // 2. Otherwise, for each line box of that element...
    recursivePaintDescendantsForPhase(containerBox, paintingContext, PaintPhase::BlockForegrounds);
}

void CSSPainter::paintAtomicallyPaintedBox(const StackingItem& stackingItem, PaintingContext& paintingContext, const IntRect& dirtyRect, IncludeStackingContextDescendants includeStackingContextDescendants)
{
    UNUSED_PARAM(dirtyRect);

    auto& box = stackingItem.box();

    auto needToSaveState = [](const Box& box) {
        if (!is<BoxModelBox>(box))
            return false;
        
        auto& boxModelBox = downcast<BoxModelBox>(box);
        return boxModelBox.hasAncestorClip() || boxModelBox.style().hasTransform() || boxModelBox.style().opacity() < 1;
    };

    auto stateSaver = GraphicsContextStateSaver { paintingContext.context, needToSaveState(box) };

    if (is<BoxModelBox>(box))
        applyAncestorClip(downcast<BoxModelBox>(box), paintingContext);

    if (is<BoxModelBox>(box) && box.style().hasTransform()) {
        auto transformationMatrix = downcast<BoxModelBox>(box).rareGeometry()->transform();
        auto absoluteBorderBox = box.absoluteBoxRect();

        // Equivalent to adjusting the CTM so that the origin is in the top left of the border box.
        transformationMatrix.translateRight(absoluteBorderBox.x(), absoluteBorderBox.y());
        // Allow descendants rendered using absolute coordinates to paint relative to this box.
        transformationMatrix.translate(-absoluteBorderBox.x(), -absoluteBorderBox.y());

        auto affineTransform = transformationMatrix.toAffineTransform();
        paintingContext.context.concatCTM(affineTransform);
    }

    auto transparencyScope = TransparencyLayerScope { paintingContext.context, 1, false };
    applyEffects(stackingItem, box, paintingContext, transparencyScope);

#if SHOW_ITEM_EXTENTS
    {
        GraphicsContextStateSaver saver(paintingContext.context);
        paintingContext.context.setStrokeColor(Color::gray);
        paintingContext.context.strokeRect(stackingItem.paintedBoundsIncludingDescendantItems(), 1);
    }
#endif

    BoxPainter::paintBox(box, paintingContext, dirtyRect);
    if (!is<ContainerBox>(box))
        return;

    if (includeStackingContextDescendants == IncludeStackingContextDescendants::Yes) {
        // Stacking contexts formed by positioned descendants with negative z-indices (excluding 0) in z-index order (most negative first) then tree order.
        for (auto& stackingItem : stackingItem.negativeZOrderList())
            paintStackingContext(*stackingItem, paintingContext, dirtyRect);
    }

    auto& containerBox = downcast<ContainerBox>(box);
    recursivePaintDescendants(containerBox, paintingContext);

    if (includeStackingContextDescendants == IncludeStackingContextDescendants::Yes) {
        // All positioned descendants with 'z-index: auto' or 'z-index: 0', in tree order. For those with 'z-index: auto', treat the element
        // as if it created a new stacking context, but any positioned descendants and descendants which actually create a new stacking context
        // should be considered part of the parent stacking context, not this new one. For those with 'z-index: 0', treat the stacking context
        // generated atomically.
        for (auto& stackingItem : stackingItem.positiveZOrderList()) {
            if (stackingItem->isStackingContext())
                paintStackingContext(*stackingItem, paintingContext, dirtyRect);
            else
                paintAtomicallyPaintedBox(*stackingItem, paintingContext, dirtyRect);
        }
    }
}

void CSSPainter::paintStackingContext(const StackingItem& stackingItem, PaintingContext& paintingContext, const IntRect& dirtyRect)
{
    paintAtomicallyPaintedBox(stackingItem, paintingContext, dirtyRect, IncludeStackingContextDescendants::Yes);
}

void CSSPainter::paintTree(const Tree& displayTree, PaintingContext& paintingContext, const IntRect& dirtyRect)
{
    paintStackingContext(displayTree.rootStackingItem(), paintingContext, dirtyRect);
}

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
