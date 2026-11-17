/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ScriptBuffer.h"

#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

ScriptBuffer::ScriptBuffer(RefPtr<const FragmentedSharedBuffer>&& buffer)
{
    if (buffer)
        append(*buffer);
}

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
    protect(m_buffer.buffer())->forEachSegment([&](auto segment) {
        builder.append(byteCast<char8_t>(segment));
    });
    return builder.toString();
}

bool ScriptBuffer::containsSingleFileMappedSegment() const
{
    return m_buffer.hasOneSegment() && m_buffer.begin()->segment->containsMappedFileData();
}

void ScriptBuffer::append(const String& string)
{
    if (string.isEmpty())
        return;
    auto result = string.tryGetUTF8([&](std::span<const char8_t> span) -> bool {
        m_buffer.append(span);
        return true;
    });
    RELEASE_ASSERT(result);
}

void ScriptBuffer::append(const FragmentedSharedBuffer& buffer)
{
    m_buffer.append(buffer);
}

} // namespace WebCore
