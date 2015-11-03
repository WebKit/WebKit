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

#ifndef IDBIndexImpl_h
#define IDBIndexImpl_h

#include "IDBIndex.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBIndexInfo.h"

namespace WebCore {
namespace IDBClient {

class IDBIndex : public WebCore::IDBIndex {
public:
    virtual ~IDBIndex();

    // Implement the IDL
    virtual const String& name() const override final;
    virtual RefPtr<IDBObjectStore> objectStore() const override final;
    virtual RefPtr<IDBAny> keyPathAny() const override final;
    virtual const IDBKeyPath& keyPath() const override final;
    virtual bool unique() const override final;
    virtual bool multiEntry() const override final;

    virtual RefPtr<IDBRequest> openCursor(ScriptExecutionContext*, IDBKeyRange*, const String& direction, ExceptionCode&) override final;
    virtual RefPtr<IDBRequest> openCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, const String& direction, ExceptionCode&) override final;
    virtual RefPtr<IDBRequest> count(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&) override final;
    virtual RefPtr<IDBRequest> count(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCode&) override final;

    virtual RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext*, IDBKeyRange*, const String& direction, ExceptionCode&) override final;
    virtual RefPtr<IDBRequest> openKeyCursor(ScriptExecutionContext*, const Deprecated::ScriptValue& key, const String& direction, ExceptionCode&) override final;

    virtual RefPtr<IDBRequest> get(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&) override final;
    virtual RefPtr<IDBRequest> get(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCode&) override final;
    virtual RefPtr<IDBRequest> getKey(ScriptExecutionContext*, IDBKeyRange*, ExceptionCode&) override final;
    virtual RefPtr<IDBRequest> getKey(ScriptExecutionContext*, const Deprecated::ScriptValue& key, ExceptionCode&) override final;

private:
    IDBIndexInfo m_info;
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBIndexImpl_h
