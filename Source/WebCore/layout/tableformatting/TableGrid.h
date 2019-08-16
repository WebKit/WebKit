/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "IntPointHash.h"
#include "LayoutBox.h"
#include <wtf/HashMap.h>
#include <wtf/IsoMalloc.h>
#include <wtf/ListHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
namespace Layout {

class TableGrid {
    WTF_MAKE_ISO_ALLOCATED(TableGrid);
public:
    TableGrid();

    void appendCell(const Box&);
    void insertCell(const Box&, const Box& before);
    void removeCell(const Box&);

    using SlotPosition = IntPoint;
    using CellSize = IntSize;
    struct CellInfo : public CanMakeWeakPtr<CellInfo> {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        CellInfo(const Box& tableCellBox, SlotPosition, CellSize);

        const Box& tableCellBox;
        SlotPosition position;
        CellSize size;
    };
    using CellList = WTF::ListHashSet<std::unique_ptr<CellInfo>>;
    CellList& cells() { return m_cellList; }

    using SlotLogicalSize = LayoutSize;
    struct SlotInfo {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        SlotInfo() = default;
        SlotInfo(CellInfo&);

        WeakPtr<CellInfo> cell;
        FormattingContext::IntrinsicWidthConstraints widthConstraints;
        SlotLogicalSize size;
    };
    SlotInfo* slot(SlotPosition);

private:
    using SlotMap = WTF::HashMap<SlotPosition, std::unique_ptr<SlotInfo>>;
    SlotMap m_slotMap;
    CellList m_cellList;
};

}
}
#endif
