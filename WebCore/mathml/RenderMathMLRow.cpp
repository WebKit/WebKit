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

RenderMathMLRow::RenderMathMLRow(Node* row)
    : RenderMathMLBlock(row)
{
}

int RenderMathMLRow::nonOperatorHeight() const
{
    int maxHeight = 0;
    for (RenderObject* current = firstChild(); current; current = current->nextSibling()) {
        if (current->isRenderMathMLBlock()) {
            RenderMathMLBlock* block = toRenderMathMLBlock(current);
            int blockHeight = block->nonOperatorHeight();
            // Check to see if this box has a larger height
            if (blockHeight > maxHeight)
                maxHeight = blockHeight;
        } else if (current->isBoxModelObject()) {
            RenderBoxModelObject* box = toRenderBoxModelObject(current);
            // Check to see if this box has a larger height
            if (box->offsetHeight() > maxHeight)
                maxHeight = box->offsetHeight();
        }
        
    }
    return maxHeight;
}

void RenderMathMLRow::layout() 
{
    RenderBlock::layout();
    
    // Calculate the maximum height of the row without the operators.
    int maxHeight = nonOperatorHeight();
    
    // Set the maximum height of the row for intermediate layouts.
    style()->setHeight(Length(maxHeight, Fixed));

    // Notify contained operators they may need to re-layout their stretched operators.
    // We need to keep track of the number of children and operators because a row of
    // operators needs some special handling.
    int childCount = 0;
    int operatorCount = 0;
    for (RenderObject* current = firstChild(); current; current = current->nextSibling()) {
        childCount++;
        if (current->isRenderMathMLBlock()) {
            RenderMathMLBlock* block = toRenderMathMLBlock(current);
            block->stretchToHeight(maxHeight);
            if (block->isRenderMathMLOperator()) 
                operatorCount++;
        }
    }
    
    // Layout the non-operators which have just been stretched.
    setNeedsLayoutAndPrefWidthsRecalc();
    markContainingBlocksForLayout();
    RenderBlock::layout();

    // Make a second pass with the real height of the operators.
    int operatorHeight = 0;
    for (RenderObject* current = firstChild(); current; current = current->nextSibling()) {
        if (current->isRenderMathMLBlock()) {
            RenderMathMLBlock* block = toRenderMathMLBlock(current);
            if (!block->hasBase() && !block->isRenderMathMLOperator()) {
                // Check to see if this box has a larger height.
                if (block->offsetHeight() > maxHeight)
                    maxHeight = block->offsetHeight();
            }
            if (block->isRenderMathMLOperator())
                if (block->offsetHeight() > operatorHeight)
                    operatorHeight = block->offsetHeight();
        } else if (current->isBoxModelObject()) {
            RenderBoxModelObject* box = toRenderBoxModelObject(current);
            // Check to see if this box has a larger height.
            if (box->offsetHeight() > maxHeight)
                maxHeight = box->offsetHeight();
        }
    }
    
    if (childCount > 0 && childCount == operatorCount) {
        // We have only operators and so set the max height to the operator height.
        maxHeight = operatorHeight;
    }
    
    int stretchHeight = maxHeight;
    
    // Stretch the operators again and re-calculate the row height.
    for (RenderObject* current = firstChild(); current; current = current->nextSibling()) {
        if (current->isRenderMathMLBlock()) {
            RenderMathMLBlock* block = toRenderMathMLBlock(current);
            if (block->isRenderMathMLOperator()) {
                RenderMathMLOperator* mathop = toRenderMathMLOperator(block);
                mathop->stretchToHeight(stretchHeight);
            } else {
                block->stretchToHeight(stretchHeight);
                RenderBoxModelObject* box = toRenderBoxModelObject(current);
                // Check to see if this box has a larger height
                if (box->offsetHeight() > maxHeight)
                    maxHeight = box->offsetHeight();
            }
        } else if (current->isBoxModelObject()) {
            RenderBoxModelObject* box = toRenderBoxModelObject(current);
            // Check to see if this box has a larger height
            if (box->offsetHeight() > maxHeight)
                maxHeight = box->offsetHeight();
        }
    }

    // Set the maximum height of the row based on the calculations.
    style()->setHeight(Length(maxHeight, Fixed));
    
    // Do the final layout by calling our parent's layout again.
    setNeedsLayoutAndPrefWidthsRecalc();
    markContainingBlocksForLayout();
    RenderBlock::layout();
}

}

#endif // ENABLE(MATHML)

