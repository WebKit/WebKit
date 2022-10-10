/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ScriptBuffer.h"

#include <wtf/text/StringBuilder.h>

namespace WebCore {

ScriptBuffer::ScriptBuffer(const String& string)
{
    append(string);
}

ScriptBuffer ScriptBuffer::empty()
{
    return ScriptBuffer { SharedBuffer::create() };
}

String ScriptBuffer::toString() const
{
    if (!m_buffer)
        return String();

    StringBuilder builder;
    m_buffer.get()->forEachSegment([&](auto& segment) {
        builder.append(String::fromUTF8(segment.data(), segment.size()));
    });
    return builder.toString();
}

bool ScriptBuffer::containsSingleFileMappedSegment() const
{
    return m_buffer && m_buffer.get()->hasOneSegment() && m_buffer.get()->begin()->segment->containsMappedFileData();
}

void ScriptBuffer::append(const String& string)
{
    auto result = string.tryGetUTF8([&](Span<const char> span) -> bool {
        m_buffer.append(span.data(), span.size());
        return true;
    });
    RELEASE_ASSERT(result);
}

void ScriptBuffer::append(const FragmentedSharedBuffer& buffer)
{
    m_buffer.append(buffer);
}

bool operator==(const ScriptBuffer& a, const ScriptBuffer& b)
{
    if (a.buffer() == b.buffer())
        return true;
    if (!a.buffer() || !b.buffer())
        return false;
    return *a.buffer() == *b.buffer();
}

bool operator!=(const ScriptBuffer& a, const ScriptBuffer& b)
{
    return !(a == b);
}

} // namespace WebCore
