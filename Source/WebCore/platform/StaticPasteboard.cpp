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

#include "Settings.h"
#include "SharedBuffer.h"

namespace WebCore {

StaticPasteboard::StaticPasteboard()
{
}

bool StaticPasteboard::hasData()
{
    return !m_platformData.isEmpty() || !m_customData.isEmpty();
}

String StaticPasteboard::readString(const String& type)
{
    return m_platformData.get(type);
}

String StaticPasteboard::readStringInCustomData(const String& type)
{
    return m_customData.get(type);
}

void StaticPasteboard::writeString(const String& type, const String& value)
{
    auto& pasteboardData = Pasteboard::isSafeTypeForDOMToReadAndWrite(type) ? m_platformData : m_customData;
    if (pasteboardData.set(type, value).isNewEntry)
        m_types.append(type);
    else {
        m_types.removeFirst(type);
        ASSERT(!m_types.contains(type));
        m_types.append(type);
    }
}

void StaticPasteboard::clear()
{
    m_customData.clear();
    m_platformData.clear();
    m_types.clear();
}

void StaticPasteboard::clear(const String& type)
{
    if (!m_platformData.remove(type) && !m_customData.remove(type))
        return;
    m_types.removeFirst(type);
    ASSERT(!m_types.contains(type));
}

void StaticPasteboard::commitToPasteboard(Pasteboard& pasteboard)
{
    if (m_platformData.isEmpty() && m_customData.isEmpty())
        return;

    if (Settings::customPasteboardDataEnabled()) {
        pasteboard.writeCustomData({ { }, WTFMove(m_types), WTFMove(m_platformData), WTFMove(m_customData) });
        return;
    }

    for (auto& entry : m_platformData)
        pasteboard.writeString(entry.key, entry.value);
    for (auto& entry : m_customData)
        pasteboard.writeString(entry.key, entry.value);
}

}
