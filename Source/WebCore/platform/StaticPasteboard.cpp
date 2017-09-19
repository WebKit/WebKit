/*
* Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "StaticPasteboard.h"

namespace WebCore {

StaticPasteboard::StaticPasteboard()
{
}

bool StaticPasteboard::hasData()
{
    return !m_stringContents.isEmpty();
}

String StaticPasteboard::readString(const String& type)
{
    if (!m_stringContents.contains(type))
        return { };
    return m_stringContents.get(type);
}

void StaticPasteboard::writeString(const String& type, const String& value)
{
    auto result = m_stringContents.set(type, value);
    if (result.isNewEntry)
        m_types.append(type);
    else {
        m_types.removeFirst(type);
        ASSERT(!m_types.contains(type));
        m_types.append(type);
    }
}

void StaticPasteboard::clear()
{
    m_stringContents.clear();
    m_types.clear();
}

void StaticPasteboard::clear(const String& type)
{
    if (!m_stringContents.remove(type))
        return;
    m_types.removeFirst(type);
    ASSERT(!m_types.contains(type));
}

// FIXME: Copy the entire StaticPasteboard to UIProcess instead of writing each string.
void StaticPasteboard::commitToPasteboard(Pasteboard& pasteboard)
{
    for (auto& type : m_types)
        pasteboard.writeString(type, m_stringContents.get(type));
}

}
