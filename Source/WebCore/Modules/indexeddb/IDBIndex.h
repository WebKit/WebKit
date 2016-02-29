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

#ifndef IDBIndex_h
#define IDBIndex_h

#include "IDBCursor.h"
#include "IDBDatabase.h"
#include "IDBKeyPath.h"
#include "IDBKeyRange.h"
#include "IDBObjectStore.h"
#include "IDBRequest.h"
#include "ScriptWrappable.h"
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBObjectStore;

class IDBIndex {
public:
    virtual ~IDBIndex() { }

    // Implement the IDL
    virtual const String& name() const = 0;
    virtual RefPtr<IDBObjectStore> objectStore() = 0;
    virtual RefPtr<IDBAny> keyPathAny() const = 0;
    virtual const IDBKeyPath& keyPath() const = 0;
    virtual bool unique() const = 0;
    virtual bool multiEntry() const = 0;

    virtual RefPtr<IDBRequest> openCursor(ScriptExecutionContext*, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBRequest> openCursor(ScriptExecutionContext*, IDBKeyRange*, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBRequest> openCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBRequest> openCursor(ScriptExecutionContext*, IDBKeyRange*, const String& direction, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBRequest> openCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, const String& direction, ExceptionCodeWithMessage&) = 0;

    virtual RefPtr<IDBRequest> count(ScriptExecutionContext*, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBRequest> count(ScriptExecutionContext*, IDBKeyRange*, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBRequest> count(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&) = 0;

    virtual RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext*, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext*, IDBKeyRange*, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext*, IDBKeyRange*, const String& direction, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, const String& direction, ExceptionCodeWithMessage&) = 0;

    virtual RefPtr<IDBRequest> get(ScriptExecutionContext*, IDBKeyRange*, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBRequest> get(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&) = 0;

    virtual RefPtr<IDBRequest> getKey(ScriptExecutionContext*, IDBKeyRange*, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBRequest> getKey(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&) = 0;

    virtual bool isModern() const { return false; }

    // We use our own ref/deref function because Legacy IDB and Modern IDB have very different
    // lifetime management of their indexes.
    // This will go away once Legacy IDB is dropped.
    virtual void ref() = 0;
    virtual void deref() = 0;

protected:
    IDBIndex();
};

} // namespace WebCore

#endif

#endif // IDBIndex_h
