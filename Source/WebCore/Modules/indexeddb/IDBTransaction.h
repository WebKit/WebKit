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

#ifndef IDBTransaction_h
#define IDBTransaction_h

#if ENABLE(INDEXED_DATABASE)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "IndexedDB.h"
#include "ScriptWrappable.h"
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class DOMError;
class IDBCursor;
class IDBDatabase;
class IDBDatabaseError;
class IDBObjectStore;
class IDBOpenDBRequest;

struct ExceptionCodeWithMessage;

class IDBTransaction : public RefCounted<IDBTransaction>, public EventTargetWithInlineData, public ActiveDOMObject {
public:
    virtual ~IDBTransaction() { }

    static const AtomicString& modeReadOnly();
    static const AtomicString& modeReadWrite();
    static const AtomicString& modeVersionChange();
    static const AtomicString& modeReadOnlyLegacy();
    static const AtomicString& modeReadWriteLegacy();

    static IndexedDB::TransactionMode stringToMode(const String&, ExceptionCode&);
    static const AtomicString& modeToString(IndexedDB::TransactionMode);

    // Implement the IDBTransaction IDL
    virtual const String& mode() const = 0;
    virtual IDBDatabase* db() = 0;
    virtual RefPtr<DOMError> error() const = 0;
    virtual RefPtr<IDBObjectStore> objectStore(const String& name, ExceptionCodeWithMessage&) = 0;
    virtual void abort(ExceptionCodeWithMessage&) = 0;

    using RefCounted<IDBTransaction>::ref;
    using RefCounted<IDBTransaction>::deref;

protected:
    IDBTransaction(ScriptExecutionContext*);
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBTransaction_h
