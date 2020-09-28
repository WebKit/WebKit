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

class ElementRunIterator;
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

    FloatRect rect() const;

    float baseline() const;

    bool isLeftToRightDirection() const;
    bool isHorizontal() const;
    bool dirOverride() const;
    bool isLineBreak() const;
    bool useLineBreakBoxRenderTreeDumpQuirk() const;

protected:
    friend class ElementRunIterator;
    friend class TextRunIterator;

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

    bool isLastOnLine() const;
    bool isLast() const;
};

class TextRunIterator {
public:
    TextRunIterator() : m_textRun(LegacyPath { nullptr, { } }) { };
    TextRunIterator(Run::PathVariant&&);

    TextRunIterator& operator++() { return traverseNextInVisualOrder(); }
    TextRunIterator& traverseNextInVisualOrder();
    TextRunIterator& traverseNextInTextOrder();

    explicit operator bool() const { return !atEnd(); }

    bool operator==(const TextRunIterator&) const;
    bool operator!=(const TextRunIterator& other) const { return !(*this == other); }

    bool operator==(EndIterator) const { return atEnd(); }
    bool operator!=(EndIterator) const { return !atEnd(); }

    const TextRun& operator*() const { return m_textRun; }
    const TextRun* operator->() const { return &m_textRun; }

    bool atEnd() const;

private:
    TextRun m_textRun;
};

class ElementRunIterator {
public:
    ElementRunIterator() : m_run(LegacyPath { nullptr, { } }) { };
    ElementRunIterator(Run::PathVariant&&);

    explicit operator bool() const { return !atEnd(); }

    const Run& operator*() const { return m_run; }
    const Run* operator->() const { return &m_run; }

    bool atEnd() const;

private:
    Run m_run;
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
ElementRunIterator elementRunFor(const RenderLineBreak&);

// -----------------------------------------------

inline Run::Run(PathVariant&& path)
    : m_pathVariant(WTFMove(path))
{
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

inline bool TextRun::isLastOnLine() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isLastOnLine();
    });
}

inline bool TextRun::isLast() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.isLast();
    });
}


}
}
