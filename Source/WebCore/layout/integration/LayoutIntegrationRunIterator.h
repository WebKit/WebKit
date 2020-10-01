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

class Run {
public:
    using PathVariant = Variant<
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
        ModernPath,
#endif
        LegacyPath
    >;

    Run(PathVariant&&);

    bool isText() const;

    FloatRect rect() const;

    float baseline() const;

    bool isLeftToRightDirection() const;
    bool isHorizontal() const;
    bool dirOverride() const;
    bool isLineBreak() const;
    bool useLineBreakBoxRenderTreeDumpQuirk() const;

    unsigned minimumCaretOffset() const;
    unsigned maximumCaretOffset() const;

    // For intermediate porting steps only.
    InlineBox* legacyInlineBox() const;

protected:
    friend class RunIterator;
    friend class LineRunIterator;
    friend class TextRunIterator;

    // To help with debugging.
    ModernPath& modernPath();
    LegacyPath& legacyPath();

    PathVariant m_pathVariant;
};

class TextRun : public Run {
public:
    TextRun(PathVariant&&);

    bool hasHyphen() const;
    StringView text() const;

    // These offsets are relative to the text renderer (not flow).
    unsigned localStartOffset() const;
    unsigned localEndOffset() const;
    unsigned length() const;

    bool isLastTextRunOnLine() const;
    bool isLastTextRun() const;

    InlineTextBox* legacyInlineBox() const { return downcast<InlineTextBox>(Run::legacyInlineBox()); }
};

class RunIterator {
public:
    RunIterator() : m_run(LegacyPath { nullptr, { } }) { };
    RunIterator(Run::PathVariant&&);

    explicit operator bool() const { return !atEnd(); }

    bool operator==(const RunIterator&) const;
    bool operator!=(const RunIterator& other) const { return !(*this == other); }

    bool operator==(EndIterator) const { return atEnd(); }
    bool operator!=(EndIterator) const { return !atEnd(); }

    const Run& operator*() const { return m_run; }
    const Run* operator->() const { return &m_run; }

    bool atEnd() const;

protected:
    Run m_run;
};

class TextRunIterator : public RunIterator {
public:
    TextRunIterator() { }
    TextRunIterator(Run::PathVariant&&);

    TextRunIterator& operator++() { return traverseNextTextRunInVisualOrder(); }

    const TextRun& operator*() const { return get(); }
    const TextRun* operator->() const { return &get(); }

    TextRunIterator& traverseNextTextRunInVisualOrder();
    TextRunIterator& traverseNextTextRunInTextOrder();

private:
    const TextRun& get() const { return downcast<TextRun>(m_run); }
};

class LineRunIterator : public RunIterator {
public:
    LineRunIterator() { }
    LineRunIterator(const RunIterator&);
    LineRunIterator(Run::PathVariant&&);

    LineRunIterator& operator++() { return traverseNextOnLine(); }

    LineRunIterator& traverseNextOnLine();
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
LineRunIterator lineRun(const RunIterator&);

// -----------------------------------------------

inline Run::Run(PathVariant&& path)
    : m_pathVariant(WTFMove(path))
{
}

inline bool Run::isText() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isText();
    });
}

inline FloatRect Run::rect() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.rect();
    });
}

inline float Run::baseline() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.baseline();
    });
}

inline bool Run::isLeftToRightDirection() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isLeftToRightDirection();
    });
}

inline bool Run::isHorizontal() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isHorizontal();
    });
}

inline bool Run::dirOverride() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.dirOverride();
    });
}

inline bool Run::isLineBreak() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isLineBreak();
    });
}

inline bool Run::useLineBreakBoxRenderTreeDumpQuirk() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.useLineBreakBoxRenderTreeDumpQuirk();
    });
}

inline unsigned Run::minimumCaretOffset() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.minimumCaretOffset();
    });
}

inline unsigned Run::maximumCaretOffset() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.maximumCaretOffset();
    });
}

inline InlineBox* Run::legacyInlineBox() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.legacyInlineBox();
    });
}

inline bool TextRun::hasHyphen() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.hasHyphen();
    });
}

inline TextRun::TextRun(PathVariant&& path)
    : Run(WTFMove(path))
{
}

inline StringView TextRun::text() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.text();
    });
}

inline unsigned TextRun::localStartOffset() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.localStartOffset();
    });
}

inline unsigned TextRun::localEndOffset() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.localEndOffset();
    });
}

inline unsigned TextRun::length() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.length();
    });
}

inline bool TextRun::isLastTextRunOnLine() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isLastTextRunOnLine();
    });
}

inline bool TextRun::isLastTextRun() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isLastTextRun();
    });
}

}
}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::LayoutIntegration::TextRun)
static bool isType(const WebCore::LayoutIntegration::Run& run) { return run.isText(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::LayoutIntegration::TextRunIterator)
static bool isType(const WebCore::LayoutIntegration::RunIterator& runIterator) { return !runIterator || runIterator->isText(); }
SPECIALIZE_TYPE_TRAITS_END()
