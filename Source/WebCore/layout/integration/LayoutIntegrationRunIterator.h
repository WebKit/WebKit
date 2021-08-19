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

#include "LayoutIntegrationRunIteratorLegacyPath.h"
#include "LayoutIntegrationRunIteratorModernPath.h"
#include "LegacyInlineElementBox.h"
#include <wtf/Variant.h>

namespace WebCore {

class RenderLineBreak;
class RenderObject;
class RenderStyle;
class RenderText;

namespace LayoutIntegration {

class LineIterator;
class TextRunIterator;

struct EndIterator { };

class PathRun {
public:
    using PathVariant = Variant<
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
        RunIteratorModernPath,
#endif
        RunIteratorLegacyPath
    >;

    PathRun(PathVariant&&);

    bool isText() const;

    FloatRect rect() const;

    float logicalTop() const { return isHorizontal() ? rect().y() : rect().x(); }
    float logicalBottom() const { return isHorizontal() ? rect().maxY() : rect().maxX(); }
    float logicalLeft() const { return isHorizontal() ? rect().x() : rect().y(); }
    float logicalRight() const { return isHorizontal() ? rect().maxX() : rect().maxY(); }
    float logicalWidth() const { return isHorizontal() ? rect().width() : rect().height(); }
    float logicalHeight() const { return isHorizontal() ? rect().height() : rect().width(); }

    bool isHorizontal() const;
    bool dirOverride() const;
    bool isLineBreak() const;

    unsigned minimumCaretOffset() const;
    unsigned maximumCaretOffset() const;
    unsigned leftmostCaretOffset() const { return isLeftToRightDirection() ? minimumCaretOffset() : maximumCaretOffset(); }
    unsigned rightmostCaretOffset() const { return isLeftToRightDirection() ? maximumCaretOffset() : minimumCaretOffset(); }

    unsigned char bidiLevel() const;
    TextDirection direction() const { return bidiLevel() % 2 ? TextDirection::RTL : TextDirection::LTR; }
    bool isLeftToRightDirection() const { return direction() == TextDirection::LTR; }

    const RenderObject& renderer() const;
    const RenderStyle& style() const;

    // For intermediate porting steps only.
    LegacyInlineBox* legacyInlineBox() const;

protected:
    friend class RunIterator;
    friend class TextRunIterator;

    LineIterator line() const;

    // To help with debugging.
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    const RunIteratorModernPath& modernPath() const;
#endif
    const RunIteratorLegacyPath& legacyPath() const;

    PathVariant m_pathVariant;
};

class PathTextRun : public PathRun {
public:
    PathTextRun(PathVariant&&);

    bool hasHyphen() const;
    StringView text() const;

    unsigned start() const;
    unsigned end() const;
    unsigned length() const;

    unsigned offsetForPosition(float x) const;
    float positionForOffset(unsigned) const;

    bool isSelectable(unsigned start, unsigned end) const;
    LayoutRect selectionRect(unsigned start, unsigned end) const;

    const RenderText& renderer() const { return downcast<RenderText>(PathRun::renderer()); }
    LegacyInlineTextBox* legacyInlineBox() const { return downcast<LegacyInlineTextBox>(PathRun::legacyInlineBox()); }
};

class RunIterator {
public:
    RunIterator() : m_run(RunIteratorLegacyPath { nullptr, { } }) { };
    RunIterator(PathRun::PathVariant&&);

    explicit operator bool() const { return !atEnd(); }

    bool operator==(const RunIterator&) const;
    bool operator!=(const RunIterator& other) const { return !(*this == other); }

    bool operator==(EndIterator) const { return atEnd(); }
    bool operator!=(EndIterator) const { return !atEnd(); }

    const PathRun& operator*() const { return m_run; }
    const PathRun* operator->() const { return &m_run; }

    bool atEnd() const;

    RunIterator nextOnLine() const;
    RunIterator previousOnLine() const;
    RunIterator nextOnLineIgnoringLineBreak() const;
    RunIterator previousOnLineIgnoringLineBreak() const;

    RunIterator& traverseNextOnLine();
    RunIterator& traversePreviousOnLine();
    RunIterator& traverseNextOnLineIgnoringLineBreak();
    RunIterator& traversePreviousOnLineIgnoringLineBreak();
    RunIterator& traverseNextOnLineInLogicalOrder();
    RunIterator& traversePreviousOnLineInLogicalOrder();

    LineIterator line() const;

protected:
    PathRun m_run;
};

class TextRunIterator : public RunIterator {
public:
    TextRunIterator() { }
    TextRunIterator(PathRun::PathVariant&&);

    TextRunIterator& operator++() { return traverseNextTextRun(); }

    const PathTextRun& operator*() const { return get(); }
    const PathTextRun* operator->() const { return &get(); }

    TextRunIterator& traverseNextTextRun();
    TextRunIterator& traverseNextTextRunInTextOrder();

    TextRunIterator nextTextRun() const { return TextRunIterator(*this).traverseNextTextRun(); }
    TextRunIterator nextTextRunInTextOrder() const { return TextRunIterator(*this).traverseNextTextRunInTextOrder(); }

private:
    RunIterator& traverseNextOnLine() = delete;
    RunIterator& traversePreviousOnLine() = delete;
    RunIterator& traverseNextOnLineIgnoringLineBreak() = delete;
    RunIterator& traversePreviousOnLineIgnoringLineBreak() = delete;
    RunIterator& traverseNextOnLineInLogicalOrder() = delete;
    RunIterator& traversePreviousOnLineInLogicalOrder() = delete;

    const PathTextRun& get() const { return downcast<PathTextRun>(m_run); }
};

class TextRunRange {
public:
    TextRunRange(TextRunIterator begin)
        : m_begin(begin)
    {
    }

    TextRunIterator begin() const { return m_begin; }
    EndIterator end() const { return { }; }

private:
    TextRunIterator m_begin;
};

TextRunIterator firstTextRunFor(const RenderText&);
TextRunIterator firstTextRunInTextOrderFor(const RenderText&);
TextRunIterator textRunFor(const LegacyInlineTextBox*);
TextRunRange textRunsFor(const RenderText&);
RunIterator runFor(const RenderLineBreak&);
RunIterator runFor(const RenderBox&);

// -----------------------------------------------

inline PathRun::PathRun(PathVariant&& path)
    : m_pathVariant(WTFMove(path))
{
}

inline bool PathRun::isText() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isText();
    });
}

inline FloatRect PathRun::rect() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.rect();
    });
}

inline bool PathRun::isHorizontal() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isHorizontal();
    });
}

inline bool PathRun::dirOverride() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.dirOverride();
    });
}

inline bool PathRun::isLineBreak() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isLineBreak();
    });
}

inline unsigned PathRun::minimumCaretOffset() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.minimumCaretOffset();
    });
}

inline unsigned PathRun::maximumCaretOffset() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.maximumCaretOffset();
    });
}

inline unsigned char PathRun::bidiLevel() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.bidiLevel();
    });
}

inline const RenderObject& PathRun::renderer() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) -> const RenderObject& {
        return path.renderer();
    });
}

inline LegacyInlineBox* PathRun::legacyInlineBox() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.legacyInlineBox();
    });
}

inline bool PathTextRun::hasHyphen() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.hasHyphen();
    });
}

inline PathTextRun::PathTextRun(PathVariant&& path)
    : PathRun(WTFMove(path))
{
}

inline StringView PathTextRun::text() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.text();
    });
}

inline unsigned PathTextRun::start() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.start();
    });
}

inline unsigned PathTextRun::end() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.end();
    });
}

inline unsigned PathTextRun::length() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.length();
    });
}

inline unsigned PathTextRun::offsetForPosition(float x) const
{
    return WTF::switchOn(m_pathVariant, [&](auto& path) {
        return path.offsetForPosition(x);
    });
}

inline float PathTextRun::positionForOffset(unsigned offset) const
{
    return WTF::switchOn(m_pathVariant, [&](auto& path) {
        return path.positionForOffset(offset);
    });
}

inline bool PathTextRun::isSelectable(unsigned start, unsigned end) const
{
    return WTF::switchOn(m_pathVariant, [&](auto& path) {
        return path.isSelectable(start, end);
    });
}

inline LayoutRect PathTextRun::selectionRect(unsigned start, unsigned end) const
{
    return WTF::switchOn(m_pathVariant, [&](auto& path) {
        return path.selectionRect(start, end);
    });
}

}
}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::LayoutIntegration::PathTextRun)
static bool isType(const WebCore::LayoutIntegration::PathRun& run) { return run.isText(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::LayoutIntegration::TextRunIterator)
static bool isType(const WebCore::LayoutIntegration::RunIterator& runIterator) { return !runIterator || runIterator->isText(); }
SPECIALIZE_TYPE_TRAITS_END()
