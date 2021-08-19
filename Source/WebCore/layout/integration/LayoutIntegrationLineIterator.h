/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "FontBaseline.h"
#include "LayoutIntegrationLineIteratorLegacyPath.h"
#include "LayoutIntegrationLineIteratorModernPath.h"
#include <wtf/Variant.h>

namespace WebCore {

namespace LayoutIntegration {

class LineIterator;
class PathIterator;
class RunIterator;

struct EndLineIterator { };

class PathLine {
public:
    using PathVariant = Variant<
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
        LineIteratorModernPath,
#endif
        LineIteratorLegacyPath
    >;

    PathLine(PathVariant&&);

    LayoutUnit top() const;
    LayoutUnit bottom() const;
    LayoutUnit selectionTop() const;
    LayoutUnit selectionTopForHitTesting() const;
    LayoutUnit selectionBottom() const;
    LayoutUnit selectionHeight() const;
    LayoutUnit lineBoxTop() const;
    LayoutUnit lineBoxBottom() const;

    LayoutRect selectionRect() const;

    float y() const;
    float contentLogicalLeft() const;
    float contentLogicalRight() const;
    float logicalHeight() const;

    int blockDirectionPointInLine() const;

    bool isHorizontal() const;

    FontBaseline baselineType() const;

    const RenderBlockFlow& containingBlock() const;
    const LegacyRootInlineBox* legacyRootInlineBox() const;

protected:
    friend class LineIterator;

    PathVariant m_pathVariant;
};

class LineIterator {
public:
    LineIterator() : m_line(LineIteratorLegacyPath { nullptr }) { };
    LineIterator(const LegacyRootInlineBox* rootInlineBox) : m_line(LineIteratorLegacyPath { rootInlineBox }) { };
    LineIterator(PathLine::PathVariant&&);

    LineIterator& operator++() { return traverseNext(); }
    LineIterator& traverseNext();
    LineIterator& traversePrevious();

    LineIterator next() const;
    LineIterator previous() const;

    bool isFirst() const { return !previous(); }

    explicit operator bool() const { return !atEnd(); }

    bool operator==(const LineIterator&) const;
    bool operator!=(const LineIterator& other) const { return !(*this == other); }

    bool operator==(EndLineIterator) const { return atEnd(); }
    bool operator!=(EndLineIterator) const { return !atEnd(); }

    const PathLine& operator*() const { return m_line; }
    const PathLine* operator->() const { return &m_line; }

    bool atEnd() const;

    RunIterator firstRun() const;
    RunIterator lastRun() const;

    RunIterator closestRunForPoint(const IntPoint& pointInContents, bool editableOnly);
    RunIterator closestRunForLogicalLeftPosition(int position, bool editableOnly = false);

    RunIterator logicalStartRun() const;
    RunIterator logicalEndRun() const;
    RunIterator logicalStartRunWithNode() const;
    RunIterator logicalEndRunWithNode() const;

private:
    PathLine m_line;
};

LineIterator firstLineFor(const RenderBlockFlow&);
LineIterator lastLineFor(const RenderBlockFlow&);

// -----------------------------------------------

inline PathLine::PathLine(PathVariant&& path)
    : m_pathVariant(WTFMove(path))
{
}

inline LayoutUnit PathLine::top() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.top();
    });
}

inline LayoutUnit PathLine::bottom() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.bottom();
    });
}

inline LayoutUnit PathLine::selectionTop() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.selectionTop();
    });
}

inline LayoutUnit PathLine::selectionTopForHitTesting() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.selectionTopForHitTesting();
    });
}

inline LayoutUnit PathLine::selectionBottom() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.selectionBottom();
    });
}

inline LayoutUnit PathLine::selectionHeight() const
{
    return selectionBottom() - selectionTop();
}

inline LayoutUnit PathLine::lineBoxTop() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.lineBoxTop();
    });
}

inline LayoutUnit PathLine::lineBoxBottom() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.lineBoxBottom();
    });
}

inline LayoutRect PathLine::selectionRect() const
{
    return { LayoutPoint { contentLogicalLeft(), selectionTop() }, LayoutPoint { contentLogicalRight(), selectionBottom() } };
}

inline float PathLine::y() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.y();
    });
}

inline float PathLine::contentLogicalLeft() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.contentLogicalLeft();
    });
}

inline float PathLine::contentLogicalRight() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.contentLogicalRight();
    });
}

inline float PathLine::logicalHeight() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.logicalHeight();
    });
}

inline bool PathLine::isHorizontal() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.isHorizontal();
    });
}

inline FontBaseline PathLine::baselineType() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.baselineType();
    });
}

inline const RenderBlockFlow& PathLine::containingBlock() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) -> const RenderBlockFlow& {
        return path.containingBlock();
    });
}

inline const LegacyRootInlineBox* PathLine::legacyRootInlineBox() const
{
    return WTF::switchOn(m_pathVariant, [](const auto& path) {
        return path.legacyRootInlineBox();
    });
}

}
}

