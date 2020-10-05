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

#include "InlineElementBox.h"
#include "LayoutIntegrationRunIteratorLegacyPath.h"
#include "LayoutIntegrationRunIteratorModernPath.h"
#include <wtf/Variant.h>

namespace WebCore {

class RenderLineBreak;
class RenderText;

namespace LayoutIntegration {

class LineRunIterator;
class TextRunIterator;

struct EndIterator { };

class PathRun {
public:
    using PathVariant = Variant<
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
        ModernPath,
#endif
        LegacyPath
    >;

    PathRun(PathVariant&&);

    bool isText() const;

    FloatRect rect() const;

    float baseline() const;

    bool isHorizontal() const;
    bool dirOverride() const;
    bool isLineBreak() const;
    bool useLineBreakBoxRenderTreeDumpQuirk() const;

    unsigned minimumCaretOffset() const;
    unsigned maximumCaretOffset() const;
    unsigned leftmostCaretOffset() const { return isLeftToRightDirection() ? minimumCaretOffset() : maximumCaretOffset(); }
    unsigned rightmostCaretOffset() const { return isLeftToRightDirection() ? maximumCaretOffset() : minimumCaretOffset(); }

    unsigned char bidiLevel() const;
    TextDirection direction() const { return bidiLevel() % 2 ? TextDirection::RTL : TextDirection::LTR; }
    bool isLeftToRightDirection() const { return direction() == TextDirection::LTR; }

    bool onSameLine(const PathRun&) const;

    // For intermediate porting steps only.
    InlineBox* legacyInlineBox() const;

protected:
    friend class RunIterator;
    friend class LineRunIterator;
    friend class TextRunIterator;

    // To help with debugging.
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    ModernPath& modernPath();
#endif
    LegacyPath& legacyPath();

    PathVariant m_pathVariant;
};

class PathTextRun : public PathRun {
public:
    PathTextRun(PathVariant&&);

    bool hasHyphen() const;
    StringView text() const;

    // These offsets are relative to the text renderer (not flow).
    unsigned localStartOffset() const;
    unsigned localEndOffset() const;
    unsigned length() const;

    bool isLastTextRunOnLine() const;
    bool isLastTextRun() const;

    InlineTextBox* legacyInlineBox() const { return downcast<InlineTextBox>(PathRun::legacyInlineBox()); }
};

class RunIterator {
public:
    RunIterator() : m_run(LegacyPath { nullptr, { } }) { };
    RunIterator(PathRun::PathVariant&&);

    explicit operator bool() const { return !atEnd(); }

    bool operator==(const RunIterator&) const;
    bool operator!=(const RunIterator& other) const { return !(*this == other); }

    bool operator==(EndIterator) const { return atEnd(); }
    bool operator!=(EndIterator) const { return !atEnd(); }

    const PathRun& operator*() const { return m_run; }
    const PathRun* operator->() const { return &m_run; }

    bool atEnd() const;

    LineRunIterator nextOnLine() const;
    LineRunIterator previousOnLine() const;
    LineRunIterator nextOnLineIgnoringLineBreak() const;
    LineRunIterator previousOnLineIgnoringLineBreak() const;

protected:
    void setAtEnd();

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
    const PathTextRun& get() const { return downcast<PathTextRun>(m_run); }
};

class LineRunIterator : public RunIterator {
public:
    LineRunIterator() { }
    LineRunIterator(const RunIterator&);
    LineRunIterator(PathRun::PathVariant&&);

    LineRunIterator& operator++() { return traverseNextOnLine(); }

    LineRunIterator& traverseNextOnLine();
    LineRunIterator& traversePreviousOnLine();
    LineRunIterator& traverseNextOnLineIgnoringLineBreak();
    LineRunIterator& traversePreviousOnLineIgnoringLineBreak();
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
TextRunRange textRunsFor(const RenderText&);
RunIterator runFor(const RenderLineBreak&);
RunIterator runFor(const RenderBox&);
LineRunIterator lineRun(const RunIterator&);

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

inline float PathRun::baseline() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.baseline();
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

inline bool PathRun::useLineBreakBoxRenderTreeDumpQuirk() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.useLineBreakBoxRenderTreeDumpQuirk();
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

inline bool PathRun::onSameLine(const PathRun& other) const
{
    if (m_pathVariant.index() != other.m_pathVariant.index())
        return false;

    return WTF::switchOn(m_pathVariant, [&](const auto& path) {
        return path.onSameLine(WTF::get<std::decay_t<decltype(path)>>(other.m_pathVariant));
    });
}

inline InlineBox* PathRun::legacyInlineBox() const
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

inline unsigned PathTextRun::localStartOffset() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.localStartOffset();
    });
}

inline unsigned PathTextRun::localEndOffset() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.localEndOffset();
    });
}

inline unsigned PathTextRun::length() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.length();
    });
}

inline bool PathTextRun::isLastTextRunOnLine() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isLastTextRunOnLine();
    });
}

inline bool PathTextRun::isLastTextRun() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isLastTextRun();
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
