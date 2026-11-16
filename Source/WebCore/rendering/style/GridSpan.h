/**
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

namespace WebCore {

// A span in a single direction (either rows or columns). Note that |startLine|
// and |endLine| are grid lines' indexes.
// Despite line numbers in the spec start in "1", the indexes here start in "0".
class GridSpan {
public:

    NODELETE static GridSpan untranslatedDefiniteGridSpan(int startLine, int endLine);
    NODELETE static GridSpan translatedDefiniteGridSpan(unsigned startLine, unsigned endLine);
    NODELETE static GridSpan indefiniteGridSpan();

    NODELETE static GridSpan stackingAxisTranslatedDefiniteGridSpan();

    friend bool operator==(const GridSpan&, const GridSpan&) = default;

    NODELETE unsigned integerSpan() const;

    NODELETE int untranslatedStartLine() const;
    NODELETE int untranslatedEndLine() const;

    NODELETE unsigned startLine() const;
    NODELETE unsigned endLine() const;

    struct GridSpanIterator {
        GridSpanIterator(unsigned value)
            : value(value)
        {
        }

        operator unsigned&() { return value; }
        unsigned operator*() const { return value; }

        unsigned value;
    };

    NODELETE GridSpanIterator begin() const;

    NODELETE GridSpanIterator end() const;

    NODELETE bool isTranslatedDefinite() const;
    NODELETE bool isIndefinite() const;

    NODELETE void translate(unsigned offset);

    // Moves this span to be in the same coordinate space as |parent|.
    // If reverse is specified, then swaps the direction to handle RTL/LTR changes.
    NODELETE void translateTo(const GridSpan& parent, bool reverse);

    NODELETE void clamp(int max);

    NODELETE bool clamp(int min, int max);

private:

    enum GridSpanType { UntranslatedDefinite, TranslatedDefinite, Indefinite };

    NODELETE GridSpan(int startLine, int endLine, GridSpanType);

    int m_startLine;
    int m_endLine;
    GridSpanType m_type;
};

}
