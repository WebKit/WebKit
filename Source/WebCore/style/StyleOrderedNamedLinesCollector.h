/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "RenderStyleInlines.h"
#include "StyleExtractorState.h"
#include "StyleGridData.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
namespace Style {

class OrderedNamedLinesCollector {
    WTF_MAKE_NONCOPYABLE(OrderedNamedLinesCollector);
public:
    OrderedNamedLinesCollector(ExtractorState& state, bool isRowAxis)
        : m_orderedNamedGridLines(isRowAxis ? state.style.gridTemplateColumns().orderedNamedLines : state.style.gridTemplateRows().orderedNamedLines)
        , m_orderedNamedAutoRepeatGridLines(isRowAxis ? state.style.gridTemplateColumns().autoRepeatOrderedNamedLines : state.style.gridTemplateRows().autoRepeatOrderedNamedLines)
    {
    }
    virtual ~OrderedNamedLinesCollector() = default;

    bool isEmpty() const { return m_orderedNamedGridLines.map.isEmpty() && m_orderedNamedAutoRepeatGridLines.map.isEmpty(); }

    virtual void collectLineNamesForIndex(Vector<String>&, unsigned index) const = 0;
    virtual int namedGridLineCount() const { return m_orderedNamedGridLines.map.size(); }

protected:
    enum class NamedLinesType : bool { NamedLines, AutoRepeatNamedLines };
    void appendLines(Vector<String>&, unsigned index, NamedLinesType) const;

    const GridOrderedNamedLinesMap& m_orderedNamedGridLines;
    const GridOrderedNamedLinesMap& m_orderedNamedAutoRepeatGridLines;
};

class OrderedNamedLinesCollectorInGridLayout : public OrderedNamedLinesCollector {
public:
    OrderedNamedLinesCollectorInGridLayout(ExtractorState& state, bool isRowAxis, unsigned autoRepeatTracksCount, unsigned autoRepeatTrackListLength)
        : OrderedNamedLinesCollector(state, isRowAxis)
        , m_insertionPoint(isRowAxis ? state.style.gridTemplateColumns().autoRepeatInsertionPoint : state.style.gridTemplateRows().autoRepeatInsertionPoint)
        , m_autoRepeatTotalTracks(autoRepeatTracksCount)
        , m_autoRepeatTrackListLength(autoRepeatTrackListLength)
    {
    }

    void collectLineNamesForIndex(Vector<String>&, unsigned index) const override;

private:
    unsigned m_insertionPoint { 0 };
    unsigned m_autoRepeatTotalTracks { 0 };
    unsigned m_autoRepeatTrackListLength { 0 };
};

class OrderedNamedLinesCollectorInSubgridLayout : public OrderedNamedLinesCollector {
public:
    OrderedNamedLinesCollectorInSubgridLayout(ExtractorState& state, bool isRowAxis, unsigned totalTracksCount)
        : OrderedNamedLinesCollector(state, isRowAxis)
        , m_insertionPoint(isRowAxis ? state.style.gridTemplateColumns().autoRepeatInsertionPoint : state.style.gridTemplateRows().autoRepeatInsertionPoint)
        , m_autoRepeatLineSetListLength((isRowAxis ? state.style.gridTemplateColumns().autoRepeatOrderedNamedLines : state.style.gridTemplateRows().autoRepeatOrderedNamedLines).map.size())
        , m_totalLines(totalTracksCount + 1)
    {
        if (!m_autoRepeatLineSetListLength) {
            m_autoRepeatTotalLineSets = 0;
            return;
        }
        unsigned named = (isRowAxis ? state.style.gridTemplateColumns().orderedNamedLines : state.style.gridTemplateRows().orderedNamedLines).map.size();
        if (named >= m_totalLines) {
            m_autoRepeatTotalLineSets = 0;
            return;
        }
        m_autoRepeatTotalLineSets = (m_totalLines - named) / m_autoRepeatLineSetListLength;
        m_autoRepeatTotalLineSets *= m_autoRepeatLineSetListLength;
    }

    void collectLineNamesForIndex(Vector<String>&, unsigned index) const override;
    int namedGridLineCount() const override { return m_totalLines; }

private:
    unsigned m_insertionPoint { 0 };
    unsigned m_autoRepeatTotalLineSets { 0 };
    unsigned m_autoRepeatLineSetListLength { 0 };
    unsigned m_totalLines { 0 };
};

inline void OrderedNamedLinesCollector::appendLines(Vector<String>& lineNames, unsigned index, NamedLinesType type) const
{
    auto& map = (type == NamedLinesType::NamedLines ? m_orderedNamedGridLines : m_orderedNamedAutoRepeatGridLines).map;
    auto it = map.find(index);
    if (it == map.end())
        return;
    for (auto& name : it->value)
        lineNames.append(name);
}

inline void OrderedNamedLinesCollectorInGridLayout::collectLineNamesForIndex(Vector<String>& lineNamesValue, unsigned i) const
{
    ASSERT(!isEmpty());
    if (!m_autoRepeatTrackListLength || i < m_insertionPoint) {
        appendLines(lineNamesValue, i, NamedLinesType::NamedLines);
        return;
    }

    ASSERT(m_autoRepeatTotalTracks);

    if (i > m_insertionPoint + m_autoRepeatTotalTracks) {
        appendLines(lineNamesValue, i - (m_autoRepeatTotalTracks - 1), NamedLinesType::NamedLines);
        return;
    }

    if (i == m_insertionPoint) {
        appendLines(lineNamesValue, i, NamedLinesType::NamedLines);
        appendLines(lineNamesValue, 0, NamedLinesType::AutoRepeatNamedLines);
        return;
    }

    if (i == m_insertionPoint + m_autoRepeatTotalTracks) {
        appendLines(lineNamesValue, m_autoRepeatTrackListLength, NamedLinesType::AutoRepeatNamedLines);
        appendLines(lineNamesValue, m_insertionPoint + 1, NamedLinesType::NamedLines);
        return;
    }

    unsigned autoRepeatIndexInFirstRepetition = (i - m_insertionPoint) % m_autoRepeatTrackListLength;
    if (!autoRepeatIndexInFirstRepetition && i > m_insertionPoint)
        appendLines(lineNamesValue, m_autoRepeatTrackListLength, NamedLinesType::AutoRepeatNamedLines);
    appendLines(lineNamesValue, autoRepeatIndexInFirstRepetition, NamedLinesType::AutoRepeatNamedLines);
}

inline void OrderedNamedLinesCollectorInSubgridLayout::collectLineNamesForIndex(Vector<String>& lineNamesValue, unsigned i) const
{
    if (!m_autoRepeatLineSetListLength || i < m_insertionPoint) {
        appendLines(lineNamesValue, i, NamedLinesType::NamedLines);
        return;
    }

    if (i >= m_insertionPoint + m_autoRepeatTotalLineSets) {
        appendLines(lineNamesValue, i - m_autoRepeatTotalLineSets, NamedLinesType::NamedLines);
        return;
    }

    unsigned autoRepeatIndexInFirstRepetition = (i - m_insertionPoint) % m_autoRepeatLineSetListLength;
    appendLines(lineNamesValue, autoRepeatIndexInFirstRepetition, NamedLinesType::AutoRepeatNamedLines);
}

} // namespace Style
} // namespace WebCore
