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

#ifndef IDBCursor_h
#define IDBCursor_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBKey.h"
#include "IDBTransaction.h"
#include "IndexedDB.h"
#include "ScriptWrappable.h"
#include <bindings/ScriptValue.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class DOMRequestState;
class IDBAny;
class IDBCallbacks;
class IDBRequest;
class ScriptExecutionContext;

struct ExceptionCodeWithMessage;

class IDBCursor : public ScriptWrappable, public RefCounted<IDBCursor> {
public:
    static const AtomicString& directionNext();
    static const AtomicString& directionNextUnique();
    static const AtomicString& directionPrev();
    static const AtomicString& directionPrevUnique();

    static IndexedDB::CursorDirection stringToDirection(const String& modeString, ExceptionCode&);
    static const AtomicString& directionToString(IndexedDB::CursorDirection mode);

    virtual ~IDBCursor() { }

    // Implement the IDL
    virtual const String& direction() const = 0;
    virtual const Deprecated::ScriptValue& key() const = 0;
    virtual const Deprecated::ScriptValue& primaryKey() const = 0;
    virtual const Deprecated::ScriptValue& value() const = 0;
    virtual IDBAny* source() = 0;

    virtual RefPtr<IDBRequest> update(JSC::ExecState&, Deprecated::ScriptValue&, ExceptionCodeWithMessage&) = 0;
    virtual void advance(unsigned long, ExceptionCodeWithMessage&) = 0;

    // FIXME: We should not need that method (taking a ScriptExecutionContext pointer and not a reference)
    // but InspectorIndexedDBAgent wants to call it with a null context. 
    virtual void continueFunction(ScriptExecutionContext*, ExceptionCodeWithMessage&) = 0;
    // FIXME: Try to modify the code generator so this overload is unneeded.
    void continueFunction(ScriptExecutionContext& context, ExceptionCodeWithMessage& ec) { continueFunction(&context, ec); }
    virtual void continueFunction(ScriptExecutionContext&, const Deprecated::ScriptValue& key, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBRequest> deleteFunction(ScriptExecutionContext&, ExceptionCodeWithMessage&) = 0;

    virtual bool isKeyCursor() const = 0;

    virtual bool isModernCursor() const { return false; }

    virtual bool hasPendingActivity() const { return false; }

protected:
    IDBCursor();
};

} // namespace WebCore

#endif

#endif // IDBCursor_h
