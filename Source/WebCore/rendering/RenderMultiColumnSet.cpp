/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnBlock.h"

namespace WebCore {

RenderMultiColumnSet::RenderMultiColumnSet(Node* node, RenderFlowThread* flowThread)
    : RenderRegionSet(node, flowThread)
    , m_columnCount(1)
    , m_columnWidth(ZERO_LAYOUT_UNIT)
    , m_columnHeight(ZERO_LAYOUT_UNIT)
{
}

void RenderMultiColumnSet::computeLogicalWidth()
{
    // Our logical width starts off matching the column block itself.
    // This width will be fixed up after the flow thread lays out once it is determined exactly how many
    // columns we ended up holding.
    // FIXME: When we add regions support, we'll start it off at the width of the multi-column
    // block in that particular region.
    setLogicalWidth(parentBox()->contentLogicalWidth());
    
    RenderMultiColumnBlock* parentBlock = toRenderMultiColumnBlock(parent());
    setColumnWidthAndCount(parentBlock->columnWidth(), parentBlock->columnCount()); // FIXME: This will eventually vary if we are contained inside regions.
}

void RenderMultiColumnSet::computeLogicalHeight()
{
    // Make sure our column height is up to date.
    RenderMultiColumnBlock* parentBlock = toRenderMultiColumnBlock(parent());
    setColumnHeight(parentBlock->columnHeight()); // FIXME: Once we make more than one column set, this will become variable.
    
    // Our logical height is always just the height of our columns.
    setLogicalHeight(columnHeight());
}

const char* RenderMultiColumnSet::renderName() const
{    
    return "RenderMultiColumnSet";
}

}
