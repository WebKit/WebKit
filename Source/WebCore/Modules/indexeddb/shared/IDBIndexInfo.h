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

#pragma once

#include "IDBKeyPath.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class IDBIndexInfo {
public:
    WEBCORE_EXPORT IDBIndexInfo();
    WEBCORE_EXPORT IDBIndexInfo(uint64_t identifier, uint64_t objectStoreIdentifier, const String& name, IDBKeyPath&&, bool unique, bool multiEntry);

    WEBCORE_EXPORT IDBIndexInfo isolatedCopy() const &;
    WEBCORE_EXPORT IDBIndexInfo isolatedCopy() &&;

    uint64_t identifier() const { return m_identifier; }
    uint64_t objectStoreIdentifier() const { return m_objectStoreIdentifier; }
    const String& name() const { return m_name; }
    const IDBKeyPath& keyPath() const { return m_keyPath; }
    bool unique() const { return m_unique; }
    bool multiEntry() const { return m_multiEntry; }

    void rename(const String& newName) { m_name = newName; }

#if !LOG_DISABLED
    String loggingString(int indent = 0) const;
    String condensedLoggingString() const;
#endif

    // FIXME: Remove the need for this.
    static const int64_t InvalidId = -1;

    void setIdentifier(uint64_t identifier) { m_identifier = identifier; }
private:
    uint64_t m_identifier { 0 };
    uint64_t m_objectStoreIdentifier { 0 };
    String m_name;
    IDBKeyPath m_keyPath;
    bool m_unique { true };
    bool m_multiEntry { false };
};

} // namespace WebCore
