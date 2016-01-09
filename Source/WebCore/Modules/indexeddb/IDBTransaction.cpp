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
#include "IDBTransaction.h"

#if ENABLE(INDEXED_DATABASE)

#include "ExceptionCode.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

IDBTransaction::IDBTransaction(ScriptExecutionContext* context)
    : ActiveDOMObject(context)
{
}

const AtomicString& IDBTransaction::modeReadOnly()
{
    static NeverDestroyed<AtomicString> readonly("readonly", AtomicString::ConstructFromLiteral);
    return readonly;
}

const AtomicString& IDBTransaction::modeReadWrite()
{
    static NeverDestroyed<AtomicString> readwrite("readwrite", AtomicString::ConstructFromLiteral);
    return readwrite;
}

const AtomicString& IDBTransaction::modeVersionChange()
{
    static NeverDestroyed<AtomicString> versionchange("versionchange", AtomicString::ConstructFromLiteral);
    return versionchange;
}

const AtomicString& IDBTransaction::modeReadOnlyLegacy()
{
    static NeverDestroyed<AtomicString> readonly("0", AtomicString::ConstructFromLiteral);
    return readonly;
}

const AtomicString& IDBTransaction::modeReadWriteLegacy()
{
    static NeverDestroyed<AtomicString> readwrite("1", AtomicString::ConstructFromLiteral);
    return readwrite;
}

IndexedDB::TransactionMode IDBTransaction::stringToMode(const String& modeString, ExceptionCode& ec)
{
    if (modeString.isNull()
        || modeString == IDBTransaction::modeReadOnly())
        return IndexedDB::TransactionMode::ReadOnly;
    if (modeString == IDBTransaction::modeReadWrite())
        return IndexedDB::TransactionMode::ReadWrite;

    ec = TypeError;
    return IndexedDB::TransactionMode::ReadOnly;
}

const AtomicString& IDBTransaction::modeToString(IndexedDB::TransactionMode mode)
{
    switch (mode) {
    case IndexedDB::TransactionMode::ReadOnly:
        return IDBTransaction::modeReadOnly();

    case IndexedDB::TransactionMode::ReadWrite:
        return IDBTransaction::modeReadWrite();

    case IndexedDB::TransactionMode::VersionChange:
        return IDBTransaction::modeVersionChange();
    }

    ASSERT_NOT_REACHED();
    return IDBTransaction::modeReadOnly();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
