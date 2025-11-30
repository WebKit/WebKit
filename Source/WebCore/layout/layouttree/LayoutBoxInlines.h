/**
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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

#pragma once

#include "LayoutBox.h"
#include "RenderStyleInlines.h"

namespace WebCore {

namespace Layout {

inline bool Box::isRuby() const { return style().display() == DisplayType::Ruby; }
inline bool Box::isRubyBase() const { return style().display() == DisplayType::RubyBase; }
inline bool Box::isRubyInlineBox() const { return isRuby() || isRubyBase(); }
inline bool Box::isTableCaption() const { return style().display() == DisplayType::TableCaption; }
inline bool Box::isTableHeader() const { return style().display() == DisplayType::TableHeaderGroup; }
inline bool Box::isTableBody() const { return style().display() == DisplayType::TableRowGroup; }
inline bool Box::isTableFooter() const { return style().display() == DisplayType::TableFooterGroup; }
inline bool Box::isTableRow() const { return style().display() == DisplayType::TableRow; }
inline bool Box::isTableColumnGroup() const { return style().display() == DisplayType::TableColumnGroup; }
inline bool Box::isTableColumn() const { return style().display() == DisplayType::TableColumn; }
inline bool Box::isTableCell() const { return style().display() == DisplayType::TableCell; }
inline bool Box::isFlexBox() const { return style().display() == DisplayType::Flex || style().display() == DisplayType::InlineFlex || m_nodeType == NodeType::ImplicitFlexBox; }
inline bool Box::isGridFormattingContext() const { return isGridBox() || isGridLanesBox(); }
inline bool Box::isGridBox() const { return style().display() == DisplayType::Grid || style().display() == DisplayType::InlineGrid; }
inline bool Box::isGridLanesBox() const { return style().display() == DisplayType::GridLanes || style().display() == DisplayType::InlineGridLanes; }
inline bool Box::isListItem() const { return style().display() == DisplayType::ListItem; }

inline bool Box::isContainingBlockForFixedPosition() const
{
    return isInitialContainingBlock() || isLayoutContainmentBox() || style().hasTransform();
}

inline bool Box::isContainingBlockForOutOfFlowPosition() const
{
    return isInitialContainingBlock() || isPositioned() || isLayoutContainmentBox() || style().hasTransform();
}

}

}
