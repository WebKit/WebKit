/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 *  THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once

#include "GridArea.h"
#include "GridTrackSize.h"
#include "RenderStyleConstants.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

typedef HashMap<String, Vector<unsigned>> NamedGridLinesMap;
typedef HashMap<unsigned, Vector<String>, WTF::IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> OrderedNamedGridLinesMap;

class StyleGridData : public RefCounted<StyleGridData> {
public:
    static Ref<StyleGridData> create() { return adoptRef(*new StyleGridData); }
    Ref<StyleGridData> copy() const;

    bool operator==(const StyleGridData& o) const
    {
        // FIXME: comparing two hashes doesn't look great for performance. Something to keep in mind going forward.
        return gridColumns == o.gridColumns && gridRows == o.gridRows
            && gridAutoFlow == o.gridAutoFlow && gridAutoRows == o.gridAutoRows && gridAutoColumns == o.gridAutoColumns
            && namedGridColumnLines == o.namedGridColumnLines && namedGridRowLines == o.namedGridRowLines
            && autoRepeatNamedGridColumnLines == o.autoRepeatNamedGridColumnLines && autoRepeatNamedGridRowLines == o.autoRepeatNamedGridRowLines
            && autoRepeatOrderedNamedGridColumnLines == o.autoRepeatOrderedNamedGridColumnLines && autoRepeatOrderedNamedGridRowLines == o.autoRepeatOrderedNamedGridRowLines
            && namedGridArea == o.namedGridArea && namedGridArea == o.namedGridArea
            && namedGridAreaRowCount == o.namedGridAreaRowCount && namedGridAreaColumnCount == o.namedGridAreaColumnCount
            && orderedNamedGridRowLines == o.orderedNamedGridRowLines && orderedNamedGridColumnLines == o.orderedNamedGridColumnLines
            && gridAutoRepeatColumns == o.gridAutoRepeatColumns && gridAutoRepeatRows == o.gridAutoRepeatRows
            && autoRepeatColumnsInsertionPoint == o.autoRepeatColumnsInsertionPoint && autoRepeatRowsInsertionPoint == o.autoRepeatRowsInsertionPoint
            && autoRepeatColumnsType == o.autoRepeatColumnsType && autoRepeatRowsType == o.autoRepeatRowsType;
    }

    bool operator!=(const StyleGridData& o) const
    {
        return !(*this == o);
    }

    Vector<GridTrackSize> gridColumns;
    Vector<GridTrackSize> gridRows;

    NamedGridLinesMap namedGridColumnLines;
    NamedGridLinesMap namedGridRowLines;

    OrderedNamedGridLinesMap orderedNamedGridColumnLines;
    OrderedNamedGridLinesMap orderedNamedGridRowLines;

    NamedGridLinesMap autoRepeatNamedGridColumnLines;
    NamedGridLinesMap autoRepeatNamedGridRowLines;
    OrderedNamedGridLinesMap autoRepeatOrderedNamedGridColumnLines;
    OrderedNamedGridLinesMap autoRepeatOrderedNamedGridRowLines;

    unsigned gridAutoFlow : GridAutoFlowBits;

    Vector<GridTrackSize> gridAutoRows;
    Vector<GridTrackSize> gridAutoColumns;

    NamedGridAreaMap namedGridArea;
    // Because namedGridArea doesn't store the unnamed grid areas, we need to keep track
    // of the explicit grid size defined by both named and unnamed grid areas.
    unsigned namedGridAreaRowCount;
    unsigned namedGridAreaColumnCount;

    Vector<GridTrackSize> gridAutoRepeatColumns;
    Vector<GridTrackSize> gridAutoRepeatRows;

    unsigned autoRepeatColumnsInsertionPoint;
    unsigned autoRepeatRowsInsertionPoint;

    AutoRepeatType autoRepeatColumnsType;
    AutoRepeatType autoRepeatRowsType;

private:
    StyleGridData();
    StyleGridData(const StyleGridData&);
};

} // namespace WebCore
