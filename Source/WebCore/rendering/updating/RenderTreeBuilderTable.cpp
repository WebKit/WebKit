/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "RenderTreeBuilderTable.h"

#include "RenderTableCell.h"
#include "RenderTableRow.h"
#include "RenderTreeBuilder.h"

namespace WebCore {

RenderTreeBuilder::Table::Table(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

RenderElement& RenderTreeBuilder::Table::findOrCreateParentForChild(RenderTableRow& parent, const RenderObject& child, RenderObject*& beforeChild)
{
    if (is<RenderTableCell>(child))
        return parent;

    if (beforeChild && !beforeChild->isAnonymous() && beforeChild->parent() == &parent) {
        auto* previousSibling = beforeChild->previousSibling();
        if (is<RenderTableCell>(previousSibling) && previousSibling->isAnonymous()) {
            beforeChild = nullptr;
            return downcast<RenderElement>(*previousSibling);
        }
    }

    auto* lastChild = beforeChild ? beforeChild : parent.lastCell();
    if (lastChild) {
        if (is<RenderTableCell>(*lastChild) && lastChild->isAnonymous() && !lastChild->isBeforeOrAfterContent()) {
            if (beforeChild == lastChild)
                beforeChild = downcast<RenderElement>(*lastChild).firstChild();
            return downcast<RenderElement>(*lastChild);
        }

        // Try to find an anonymous container for the child.
        if (auto* lastChildParent = lastChild->parent()) {
            if (lastChildParent->isAnonymous() && !lastChildParent->isBeforeOrAfterContent()) {
                // If beforeChild is inside an anonymous cell, insert into the cell.
                if (!is<RenderTableCell>(*lastChild))
                    return *lastChildParent;
                // If beforeChild is inside an anonymous row, insert into the row.
                if (is<RenderTableRow>(*lastChildParent)) {
                    auto newCell = RenderTableCell::createAnonymousWithParentRenderer(parent);
                    auto& cell = *newCell;
                    m_builder.insertChild(*lastChildParent, WTFMove(newCell), beforeChild);
                    beforeChild = nullptr;
                    return cell;
                }
            }
        }
    }
    auto newCell = RenderTableCell::createAnonymousWithParentRenderer(parent);
    auto& cell = *newCell;
    m_builder.insertChild(parent, WTFMove(newCell), beforeChild);
    beforeChild = nullptr;
    return cell;
}

}
