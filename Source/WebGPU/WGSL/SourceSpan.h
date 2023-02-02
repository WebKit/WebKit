/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

namespace WGSL {

struct SourcePosition {
    unsigned m_line;
    unsigned m_lineOffset;
    unsigned m_offset;
};

struct SourceSpan {
    // FIXME: we could possibly skip m_lineOffset and recompute it only when trying to show an error
    // This would shrink the AST size by 32 bits per AST node, at the cost of a bit of code complexity in the error toString function.
    unsigned m_line;
    unsigned m_lineOffset;
    unsigned m_offset;
    unsigned m_length;

    static constexpr SourceSpan empty() { return { 0, 0, 0, 0 }; }

    constexpr SourceSpan(unsigned line, unsigned lineOffset, unsigned offset, unsigned length)
        : m_line(line)
        , m_lineOffset(lineOffset)
        , m_offset(offset)
        , m_length(length)
    { }

    constexpr SourceSpan(SourcePosition start, SourcePosition end)
        : SourceSpan(start.m_line, start.m_lineOffset, start.m_offset, end.m_offset - start.m_offset)
    { }

    constexpr bool operator==(const SourceSpan& other) const
    {
        return (m_line == other.m_line
            && m_lineOffset == other.m_lineOffset
            && m_offset == other.m_offset
            && m_length == other.m_length);
    }

    constexpr bool operator!=(const SourceSpan& other) const
    {
        return !(*this == other);
    }
};

}
