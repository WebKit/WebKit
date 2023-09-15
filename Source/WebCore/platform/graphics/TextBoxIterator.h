/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "TextRun.h"

namespace WebCore {

class TextBoxIterator {
public:
    TextBoxIterator() = default;

    TextBoxIterator(const TextRun* textRun, unsigned offset)
        : m_textRun(textRun)
        , m_offset(offset)
    {
    }

    unsigned offset() const { return m_offset; }
    void increment() { m_offset++; }
    bool atEnd() const { return !m_textRun || m_offset >= m_textRun->length(); }
    UChar current() const { return (*m_textRun)[m_offset]; }
    UCharDirection direction() const { return atEnd() ? U_OTHER_NEUTRAL : u_charDirection(current()); }

    friend bool operator==(const TextBoxIterator&, const TextBoxIterator&) = default;

private:
    const TextRun* m_textRun { nullptr };
    unsigned m_offset { 0 };
};

} // namespace WebCore
