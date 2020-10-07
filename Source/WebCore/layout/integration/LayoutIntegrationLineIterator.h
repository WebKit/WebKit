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

#include "LayoutIntegrationLineIteratorLegacyPath.h"
#include "LayoutIntegrationLineIteratorModernPath.h"
#include <wtf/Variant.h>

namespace WebCore {

namespace LayoutIntegration {

class LineIterator;
class PathIterator;

struct EndLineIterator { };

class PathLine {
public:
    using PathVariant = Variant<
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
        ModernLinePath,
#endif
        LegacyLinePath
    >;

    PathLine(PathVariant&&);

    FloatRect rect() const;

    bool operator==(const PathLine&) const;
    bool operator!=(const PathLine& other) const { return !(*this == other); }

protected:
    friend class LineIterator;

    PathVariant m_pathVariant;
};

class LineIterator {
public:
    LineIterator() : m_line(LegacyLinePath { nullptr }) { };
    LineIterator(PathLine::PathVariant&&);

    LineIterator& operator++() { return traverseNext(); }
    LineIterator& traverseNext();
    LineIterator& traversePrevious();

    LineIterator next() const;
    LineIterator previous() const;

    explicit operator bool() const { return !atEnd(); }

    bool operator==(const LineIterator& other) const { return m_line == other.m_line; }
    bool operator!=(const LineIterator& other) const { return m_line != other.m_line; }

    bool operator==(EndLineIterator) const { return atEnd(); }
    bool operator!=(EndLineIterator) const { return !atEnd(); }

    const PathLine& operator*() const { return m_line; }
    const PathLine* operator->() const { return &m_line; }

    bool atEnd() const;

private:
    PathLine m_line;
};

LineIterator lineFor(const PathIterator&);

// -----------------------------------------------

inline PathLine::PathLine(PathVariant&& path)
    : m_pathVariant(WTFMove(path))
{
}

inline FloatRect PathLine::rect() const
{
    return WTF::switchOn(m_pathVariant, [](auto& path) {
        return path.rect();
    });
}

inline bool PathLine::operator==(const PathLine& other) const
{
    if (m_pathVariant.index() != other.m_pathVariant.index())
        return false;

    return WTF::switchOn(m_pathVariant, [&](const auto& path) {
        return path == WTF::get<std::decay_t<decltype(path)>>(other.m_pathVariant);
    });
}

}
}

