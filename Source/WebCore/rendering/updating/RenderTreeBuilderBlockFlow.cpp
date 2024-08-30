/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "RenderTreeBuilderBlockFlow.h"

#include "RenderMultiColumnFlow.h"
#include "RenderTreeBuilderBlock.h"
#include "RenderTreeBuilderMultiColumn.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(RenderTreeBuilderBlockFlow, RenderTreeBuilder::BlockFlow);

RenderTreeBuilder::BlockFlow::BlockFlow(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

void RenderTreeBuilder::BlockFlow::attach(RenderBlockFlow& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (auto* multicolumnFlow = parent.multiColumnFlow()) {
        auto legendAvoidsMulticolumn = parent.isFieldset() && child->isLegend();
        if (legendAvoidsMulticolumn)
            return m_builder.blockBuilder().attach(parent, WTFMove(child), nullptr);

        auto legendBeforeChildIsIncorrect = parent.isFieldset() && beforeChild && beforeChild->isLegend();
        if (legendBeforeChildIsIncorrect)
            return m_builder.blockBuilder().attach(*multicolumnFlow, WTFMove(child), nullptr);

        // When the before child is set to be the first child of the RenderBlockFlow, we need to readjust it to be the first
        // child of the multicol conainter.
        return m_builder.attach(*multicolumnFlow, WTFMove(child), beforeChild == multicolumnFlow ? multicolumnFlow->firstChild() : beforeChild);
    }

    auto* beforeChildOrPlaceholder = beforeChild;
    if (auto* containingFragmentedFlow = parent.enclosingFragmentedFlow())
        beforeChildOrPlaceholder = m_builder.multiColumnBuilder().resolveMovedChild(*containingFragmentedFlow, beforeChild);
    m_builder.blockBuilder().attach(parent, WTFMove(child), beforeChildOrPlaceholder);
}

void RenderTreeBuilder::BlockFlow::moveAllChildrenIncludingFloats(RenderBlockFlow& from, RenderBlock& to, RenderTreeBuilder::NormalizeAfterInsertion normalizeAfterInsertion)
{
    m_builder.moveAllChildren(from, to, normalizeAfterInsertion);
    from.addFloatsToNewParent(downcast<RenderBlockFlow>(to));
}

}
