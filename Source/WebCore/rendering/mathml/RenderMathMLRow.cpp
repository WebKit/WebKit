/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MATHML)

#include "RenderMathMLRow.h"

#include "MathMLNames.h"
#include "RenderMathMLOperator.h"

namespace WebCore {

using namespace MathMLNames;

RenderMathMLRow::RenderMathMLRow(Node* node)
    : RenderMathMLBlock(node)
{
}

// FIXME: Change all these createAnonymous... routines to return a PassOwnPtr<>.
RenderMathMLRow* RenderMathMLRow::createAnonymousWithParentRenderer(const RenderObject* parent)
{
    RefPtr<RenderStyle> newStyle = RenderStyle::createAnonymousStyle(parent->style());
    newStyle->setDisplay(INLINE_BLOCK);

    RenderMathMLRow* newMRow = new (parent->renderArena()) RenderMathMLRow(parent->document() /* is anonymous */);
    newMRow->setStyle(newStyle.release());
    return newMRow;
}

void RenderMathMLRow::layout() 
{
    RenderBlock::layout();
    
    int maxHeight = 0;

    // Calculate the non-operator max height of the row.
    for (RenderObject* current = firstChild(); current; current = current->nextSibling()) {
        if (current->isRenderMathMLBlock()) {
            RenderMathMLBlock* block = toRenderMathMLBlock(current);
            if (!block->unembellishedOperator() && block->offsetHeight() > maxHeight)
                maxHeight = block->offsetHeight();
        } else if (current->isBoxModelObject()) {
            RenderBoxModelObject* box = toRenderBoxModelObject(current);
            // Check to see if this box has a larger height.
            if (box->pixelSnappedOffsetHeight() > maxHeight)
                maxHeight = box->pixelSnappedOffsetHeight();
        }
    }
    
    if (!maxHeight)
        maxHeight = style()->fontSize();
    
    // Stretch everything to the same height (blocks can ignore the request).
    if (maxHeight > 0) {
        bool didStretch = false;
        for (RenderObject* current = firstChild(); current; current = current->nextSibling()) {
            if (current->isRenderMathMLBlock()) {
                RenderMathMLBlock* block = toRenderMathMLBlock(current);
                block->stretchToHeight(maxHeight);
                didStretch = true;
            }
        }
        if (didStretch) {
            setNeedsLayout(true);
            setPreferredLogicalWidthsDirty(true, MarkOnlyThis);
            RenderBlock::layout();
        }
    }
    
}    
    
}

#endif // ENABLE(MATHML)
