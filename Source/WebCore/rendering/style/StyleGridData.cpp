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

#include "config.h"
#include "StyleGridData.h"

#include "RenderStyle.h"

namespace WebCore {

StyleGridData::StyleGridData()
    : gridColumns(RenderStyle::initialGridColumns())
    , gridRows(RenderStyle::initialGridRows())
    , namedGridColumnLines(RenderStyle::initialNamedGridColumnLines())
    , namedGridRowLines(RenderStyle::initialNamedGridRowLines())
    , orderedNamedGridColumnLines(RenderStyle::initialOrderedNamedGridColumnLines())
    , orderedNamedGridRowLines(RenderStyle::initialOrderedNamedGridRowLines())
    , autoRepeatNamedGridColumnLines(RenderStyle::initialNamedGridColumnLines())
    , autoRepeatNamedGridRowLines(RenderStyle::initialNamedGridRowLines())
    , autoRepeatOrderedNamedGridColumnLines(RenderStyle::initialOrderedNamedGridColumnLines())
    , autoRepeatOrderedNamedGridRowLines(RenderStyle::initialOrderedNamedGridRowLines())
    , implicitNamedGridColumnLines(RenderStyle::initialNamedGridColumnLines())
    , implicitNamedGridRowLines(RenderStyle::initialNamedGridRowLines())
    , gridAutoFlow(RenderStyle::initialGridAutoFlow())
    , gridAutoRows(RenderStyle::initialGridAutoRows())
    , gridAutoColumns(RenderStyle::initialGridAutoColumns())
    , namedGridArea(RenderStyle::initialNamedGridArea())
    , namedGridAreaRowCount(RenderStyle::initialNamedGridAreaCount())
    , namedGridAreaColumnCount(RenderStyle::initialNamedGridAreaCount())
    , gridAutoRepeatColumns(RenderStyle::initialGridAutoRepeatTracks())
    , gridAutoRepeatRows(RenderStyle::initialGridAutoRepeatTracks())
    , autoRepeatColumnsInsertionPoint(RenderStyle::initialGridAutoRepeatInsertionPoint())
    , autoRepeatRowsInsertionPoint(RenderStyle::initialGridAutoRepeatInsertionPoint())
    , autoRepeatColumnsType(RenderStyle::initialGridAutoRepeatType())
    , autoRepeatRowsType(RenderStyle::initialGridAutoRepeatType())
{
}

inline StyleGridData::StyleGridData(const StyleGridData& o)
    : RefCounted<StyleGridData>()
    , gridColumns(o.gridColumns)
    , gridRows(o.gridRows)
    , namedGridColumnLines(o.namedGridColumnLines)
    , namedGridRowLines(o.namedGridRowLines)
    , orderedNamedGridColumnLines(o.orderedNamedGridColumnLines)
    , orderedNamedGridRowLines(o.orderedNamedGridRowLines)
    , autoRepeatNamedGridColumnLines(o.autoRepeatNamedGridColumnLines)
    , autoRepeatNamedGridRowLines(o.autoRepeatNamedGridRowLines)
    , autoRepeatOrderedNamedGridColumnLines(o.autoRepeatOrderedNamedGridColumnLines)
    , autoRepeatOrderedNamedGridRowLines(o.autoRepeatOrderedNamedGridRowLines)
    , implicitNamedGridColumnLines(o.implicitNamedGridColumnLines)
    , implicitNamedGridRowLines(o.implicitNamedGridRowLines)
    , gridAutoFlow(o.gridAutoFlow)
    , gridAutoRows(o.gridAutoRows)
    , gridAutoColumns(o.gridAutoColumns)
    , namedGridArea(o.namedGridArea)
    , namedGridAreaRowCount(o.namedGridAreaRowCount)
    , namedGridAreaColumnCount(o.namedGridAreaColumnCount)
    , gridAutoRepeatColumns(o.gridAutoRepeatColumns)
    , gridAutoRepeatRows(o.gridAutoRepeatRows)
    , autoRepeatColumnsInsertionPoint(o.autoRepeatColumnsInsertionPoint)
    , autoRepeatRowsInsertionPoint(o.autoRepeatRowsInsertionPoint)
    , autoRepeatColumnsType(o.autoRepeatColumnsType)
    , autoRepeatRowsType(o.autoRepeatRowsType)
{
}

Ref<StyleGridData> StyleGridData::copy() const
{
    return adoptRef(*new StyleGridData(*this));
}

} // namespace WebCore
