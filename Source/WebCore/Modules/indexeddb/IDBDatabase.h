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

#ifndef IDBDatabase_h
#define IDBDatabase_h

#include "ActiveDOMObject.h"
#include "DOMStringList.h"
#include "Dictionary.h"
#include "Event.h"
#include "EventTarget.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "IndexedDB.h"
#include "ScriptWrappable.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class ScriptExecutionContext;

struct ExceptionCodeWithMessage;

class IDBDatabase : public RefCounted<IDBDatabase>, public EventTargetWithInlineData, public ActiveDOMObject {
public:
    virtual ~IDBDatabase() { }

    // Implement the IDL
    virtual const String name() const = 0;
    virtual uint64_t version() const = 0;
    virtual RefPtr<DOMStringList> objectStoreNames() const = 0;

    virtual RefPtr<IDBObjectStore> createObjectStore(const String& name, const Dictionary&, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBObjectStore> createObjectStore(const String& name, const IDBKeyPath&, bool autoIncrement, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBTransaction> transaction(ScriptExecutionContext*, const Vector<String>&, const String& mode, ExceptionCodeWithMessage&) = 0;
    virtual RefPtr<IDBTransaction> transaction(ScriptExecutionContext*, const String&, const String& mode, ExceptionCodeWithMessage&) = 0;
    virtual void deleteObjectStore(const String& name, ExceptionCodeWithMessage&) = 0;
    virtual void close() = 0;

    using RefCounted<IDBDatabase>::ref;
    using RefCounted<IDBDatabase>::deref;

protected:
    IDBDatabase(ScriptExecutionContext*);
};

} // namespace WebCore

#endif

#endif // IDBDatabase_h
