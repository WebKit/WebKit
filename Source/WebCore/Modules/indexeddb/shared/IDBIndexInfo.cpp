/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "IDBIndexInfo.h"

#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

IDBIndexInfo::IDBIndexInfo()
{
}

IDBIndexInfo::IDBIndexInfo(uint64_t identifier, uint64_t objectStoreIdentifier, const String& name, IDBKeyPath&& keyPath, bool unique, bool multiEntry)
    : m_identifier(identifier)
    , m_objectStoreIdentifier(objectStoreIdentifier)
    , m_name(name)
    , m_keyPath(WTFMove(keyPath))
    , m_unique(unique)
    , m_multiEntry(multiEntry)
{
}

IDBIndexInfo IDBIndexInfo::isolatedCopy() const
{
    return { m_identifier, m_objectStoreIdentifier, m_name.isolatedCopy(), WebCore::isolatedCopy(m_keyPath), m_unique, m_multiEntry };
}

#if !LOG_DISABLED

String IDBIndexInfo::loggingString(int indent) const
{
    String indentString;
    for (int i = 0; i < indent; ++i)
        indentString.append(" ");
    return makeString(indentString, "Index: ", m_name, " (", m_identifier, ") keyPath: ", WebCore::loggingString(m_keyPath), '\n');
}

String IDBIndexInfo::condensedLoggingString() const
{
    return makeString("<Idx: ", m_name, " (", m_identifier, "), OS (", m_objectStoreIdentifier, ")>");
}

#endif

} // namespace WebCore
