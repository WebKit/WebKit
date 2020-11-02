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

#include "DisplayBoxPainter.h"
#include "DisplayContainerBox.h"
#include "DisplayPaintingContext.h"
#include "DisplayStyle.h"
#include "DisplayTree.h"
#include "GraphicsContext.h"
#include "IntRect.h"

namespace WebCore {
namespace Display {

// FIXME: Make this an iterator.
void CSSPainter::recursivePaintDescendants(const ContainerBox& containerBox, PaintingContext& paintingContext, PaintPhase paintPhase)
{
    for (const auto* child = containerBox.firstChild(); child; child = child->nextSibling()) {
        auto& box = *child;
        if (isStackingContextPaintingBoundary(box))
            continue;

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
            recursivePaintDescendants(downcast<ContainerBox>(box), paintingContext, paintPhase);
    }
}

void CSSPainter::paintStackingContext(const BoxModelBox& contextRoot, PaintingContext& paintingContext, const IntRect& dirtyRect)
{
    UNUSED_PARAM(dirtyRect);
    
    BoxPainter::paintBoxDecorations(contextRoot, paintingContext);

    auto paintDescendants = [&](const ContainerBox& containerBox) {
        // For all its in-flow, non-positioned, block-level descendants in tree order: If the element is a block, list-item, or other block equivalent:
        // Box decorations.
        // Table decorations.
        recursivePaintDescendants(containerBox, paintingContext, PaintPhase::BlockBackgrounds);

        // All non-positioned floating descendants, in tree order. For each one of these, treat the element as if it created a new stacking context,
        // but any positioned descendants and descendants which actually create a new stacking context should be considered part of the parent
        // stacking context, not this new one.
        recursivePaintDescendants(containerBox, paintingContext, PaintPhase::Floats);

        // If the element is an inline element that generates a stacking context, then:
        // FIXME: Handle this case.
        
        // Otherwise: first for the element, then for all its in-flow, non-positioned, block-level descendants in tree order:
        // 1. If the element is a block-level replaced element, then: the replaced content, atomically.
        // 2. Otherwise, for each line box of that element...
        recursivePaintDescendants(containerBox, paintingContext, PaintPhase::BlockForegrounds);
    };

    if (is<ContainerBox>(contextRoot)) {
        auto& containerBox = downcast<ContainerBox>(contextRoot);

        Vector<const BoxModelBox*> negativeZOrderList;
        Vector<const BoxModelBox*> positiveZOrderList;
    
        recursiveCollectLayers(containerBox, negativeZOrderList, positiveZOrderList);

        auto compareZIndex = [] (const BoxModelBox* a, const BoxModelBox* b) {
            return a->style().zIndex().valueOr(0) < b->style().zIndex().valueOr(0);
        };

        std::stable_sort(positiveZOrderList.begin(), positiveZOrderList.end(), compareZIndex);
        std::stable_sort(negativeZOrderList.begin(), negativeZOrderList.end(), compareZIndex);

        // Stacking contexts formed by positioned descendants with negative z-indices (excluding 0) in z-index order (most negative first) then tree order.
        for (auto* box : negativeZOrderList)
            paintStackingContext(*box, paintingContext, dirtyRect);

        paintDescendants(containerBox);

        // All positioned descendants with 'z-index: auto' or 'z-index: 0', in tree order. For those with 'z-index: auto', treat the element
        // as if it created a new stacking context, but any positioned descendants and descendants which actually create a new stacking context
        // should be considered part of the parent stacking context, not this new one. For those with 'z-index: 0', treat the stacking context
        // generated atomically.
        for (auto* box : positiveZOrderList) {
            if (box->style().isStackingContext())
                paintStackingContext(*box, paintingContext, dirtyRect);
            else if (is<ContainerBox>(*box)) {
                BoxPainter::paintBoxDecorations(*box, paintingContext);
                paintDescendants(downcast<ContainerBox>(*box));
            } else
                BoxPainter::paintBox(*box, paintingContext, dirtyRect);
        }
    }
}

bool CSSPainter::isStackingContextPaintingBoundary(const Box& box)
{
    return box.style().isStackingContext();
}

void CSSPainter::recursiveCollectLayers(const ContainerBox& containerBox, Vector<const BoxModelBox*>& negativeZOrderList, Vector<const BoxModelBox*>& positiveZOrderList)
{
    for (const auto* child = containerBox.firstChild(); child; child = child->nextSibling()) {
        if (child->style().participatesInZOrderSorting() && is<BoxModelBox>(*child)) {
            auto& childBox = downcast<BoxModelBox>(*child);

            auto zIndex = childBox.style().zIndex().valueOr(0);

            if (zIndex < 0)
                negativeZOrderList.append(&childBox);
            else
                positiveZOrderList.append(&childBox);
        }

        if (isStackingContextPaintingBoundary(*child))
            continue;

        if (is<ContainerBox>(*child))
            recursiveCollectLayers(downcast<ContainerBox>(*child), negativeZOrderList, positiveZOrderList);
    }
}

void CSSPainter::paintTree(const Tree& displayTree, PaintingContext& paintingContext, const IntRect& dirtyRect)
{
    paintStackingContext(displayTree.rootBox(), paintingContext, dirtyRect);
}

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
