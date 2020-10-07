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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutIntegrationInlineContent.h"
#include "LayoutIntegrationRunIteratorModernPath.h"

namespace WebCore {

namespace LayoutIntegration {

class ModernLinePath {
public:
    ModernLinePath(const InlineContent& inlineContent, size_t lineIndex)
        : m_inlineContent(&inlineContent)
        , m_lineIndex(lineIndex)
    {
        ASSERT(lineIndex <= lines().size());
    }
    ModernLinePath(ModernLinePath&&) = default;
    ModernLinePath(const ModernLinePath&) = default;
    ModernLinePath& operator=(const ModernLinePath&) = default;
    ModernLinePath& operator=(ModernLinePath&&) = default;

    FloatRect rect() const { return verticallyRoundedRect(line().rect()); }

    void traverseNext()
    {
        ASSERT(!atEnd());

        ++m_lineIndex;
    }

    void traversePrevious()
    {
        ASSERT(!atEnd());
        ASSERT(m_lineIndex);

        --m_lineIndex;
    }

    bool operator==(const ModernLinePath& other) const { return m_inlineContent == other.m_inlineContent && m_lineIndex == other.m_lineIndex; }

    bool atEnd() const { return m_lineIndex == lines().size(); }
    void setAtEnd() { m_lineIndex = lines().size(); }

private:
    const InlineContent::Lines& lines() const { return m_inlineContent->lines; }
    const Line& line() const { return lines()[m_lineIndex]; }

    RefPtr<const InlineContent> m_inlineContent;
    size_t m_lineIndex { 0 };
};

}
}

#endif

