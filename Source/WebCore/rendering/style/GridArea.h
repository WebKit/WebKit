/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2017 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "GridPosition.h"
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// A span in a single direction (either rows or columns). Note that |startLine|
// and |endLine| are grid lines' indexes.
// Despite line numbers in the spec start in "1", the indexes here start in "0".
class GridSpan {
public:

    static GridSpan untranslatedDefiniteGridSpan(int startLine, int endLine)
    {
        return GridSpan(startLine, endLine, UntranslatedDefinite);
    }

    static GridSpan translatedDefiniteGridSpan(unsigned startLine, unsigned endLine)
    {
        return GridSpan(startLine, endLine, TranslatedDefinite);
    }

    static GridSpan indefiniteGridSpan()
    {
        return GridSpan(0, 1, Indefinite);
    }

    bool operator==(const GridSpan& o) const
    {
        return m_type == o.m_type && m_startLine == o.m_startLine && m_endLine == o.m_endLine;
    }

    unsigned integerSpan() const
    {
        ASSERT(isTranslatedDefinite());
        return m_endLine - m_startLine;
    }

    int untranslatedStartLine() const
    {
        ASSERT(m_type == UntranslatedDefinite);
        return m_startLine;
    }

    int untranslatedEndLine() const
    {
        ASSERT(m_type == UntranslatedDefinite);
        return m_endLine;
    }

    unsigned startLine() const
    {
        ASSERT(isTranslatedDefinite());
        ASSERT(m_endLine >= 0);
        return m_startLine;
    }

    unsigned endLine() const
    {
        ASSERT(isTranslatedDefinite());
        ASSERT(m_endLine > 0);
        return m_endLine;
    }

    struct GridSpanIterator {
        GridSpanIterator(unsigned value)
            : value(value)
        {
        }

        operator unsigned&() { return value; }
        unsigned operator*() const { return value; }

        unsigned value;
    };

    GridSpanIterator begin() const
    {
        ASSERT(isTranslatedDefinite());
        return m_startLine;
    }

    GridSpanIterator end() const
    {
        ASSERT(isTranslatedDefinite());
        return m_endLine;
    }

    bool isTranslatedDefinite() const
    {
        return m_type == TranslatedDefinite;
    }

    bool isIndefinite() const
    {
        return m_type == Indefinite;
    }

    void translate(unsigned offset)
    {
        ASSERT(m_type == UntranslatedDefinite);

        m_type = TranslatedDefinite;
        m_startLine += offset;
        m_endLine += offset;

        ASSERT(m_startLine >= 0);
        ASSERT(m_endLine > 0);
    }

private:

    enum GridSpanType {UntranslatedDefinite, TranslatedDefinite, Indefinite};

    GridSpan(int startLine, int endLine, GridSpanType type)
        : m_type(type)
    {
#if ASSERT_ENABLED
        ASSERT(startLine < endLine);
        if (type == TranslatedDefinite) {
            ASSERT(startLine >= 0);
            ASSERT(endLine > 0);
        }
#endif

        m_startLine = std::max(GridPosition::min(), std::min(startLine, GridPosition::max() - 1));
        m_endLine = std::max(GridPosition::min() + 1, std::min(endLine, GridPosition::max()));
    }

    int m_startLine;
    int m_endLine;
    GridSpanType m_type;


};

// This represents a grid area that spans in both rows' and columns' direction.
class GridArea {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // HashMap requires a default constuctor.
    GridArea()
        : columns(GridSpan::indefiniteGridSpan())
        , rows(GridSpan::indefiniteGridSpan())
    {
    }

    GridArea(const GridSpan& r, const GridSpan& c)
        : columns(c)
        , rows(r)
    {
    }

    bool operator==(const GridArea& o) const
    {
        return columns == o.columns && rows == o.rows;
    }

    bool operator!=(const GridArea& o) const
    {
        return !(*this == o);
    }

    GridSpan columns;
    GridSpan rows;
};

typedef HashMap<String, GridArea> NamedGridAreaMap;

} // namespace WebCore
