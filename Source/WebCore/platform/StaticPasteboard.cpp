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

#include "SharedBuffer.h"

namespace WebCore {

StaticPasteboard::StaticPasteboard() = default;
StaticPasteboard::~StaticPasteboard() = default;

bool StaticPasteboard::hasData()
{
    return m_customData.hasData();
}

Vector<String> StaticPasteboard::typesSafeForBindings(const String&)
{
    return m_customData.orderedTypes();
}

Vector<String> StaticPasteboard::typesForLegacyUnsafeBindings()
{
    return m_customData.orderedTypes();
}

String StaticPasteboard::readString(const String& type)
{
    return m_customData.readString(type);
}

String StaticPasteboard::readStringInCustomData(const String& type)
{
    return m_customData.readStringInCustomData(type);
}

void StaticPasteboard::writeString(const String& type, const String& value)
{
    m_customData.writeString(type, value);
}

void StaticPasteboard::writeData(const String& type, Ref<SharedBuffer>&& data)
{
    m_customData.writeData(type, WTFMove(data));
}

void StaticPasteboard::writeStringInCustomData(const String& type, const String& value)
{
    m_customData.writeStringInCustomData(type, value);
}

void StaticPasteboard::clear()
{
    m_customData.clear();
}

void StaticPasteboard::clear(const String& type)
{
    m_customData.clear(type);
}

PasteboardCustomData StaticPasteboard::takeCustomData()
{
    return std::exchange(m_customData, { });
}

}
