/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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

#include "ExceptionOr.h"
#include "JSDOMPromiseDeferred.h"
#include <wtf/Function.h>
#include <wtf/Forward.h>
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC {
class CallFrame;
class JSGlobalObject;
class JSValue;
}

namespace WebCore {

class IDBOpenDBRequest;
class ScriptExecutionContext;
class SecurityOrigin;

namespace IDBClient {
class IDBConnectionProxy;
}

class IDBFactory : public ThreadSafeRefCounted<IDBFactory> {
    WTF_MAKE_ISO_ALLOCATED(IDBFactory);
public:
    static Ref<IDBFactory> create(IDBClient::IDBConnectionProxy&);
    ~IDBFactory();

    struct DatabaseInfo {
        String name;
        uint64_t version;
    };

    ExceptionOr<Ref<IDBOpenDBRequest>> open(ScriptExecutionContext&, const String& name, std::optional<uint64_t> version);
    ExceptionOr<Ref<IDBOpenDBRequest>> deleteDatabase(ScriptExecutionContext&, const String& name);

    ExceptionOr<short> cmp(JSC::JSGlobalObject&, JSC::JSValue first, JSC::JSValue second);

    using IDBDatabasesResponsePromise = DOMPromiseDeferred<IDLSequence<IDLDictionary<IDBFactory::DatabaseInfo>>>;

    void databases(ScriptExecutionContext&, IDBDatabasesResponsePromise&&);

    WEBCORE_EXPORT void getAllDatabaseNames(ScriptExecutionContext&, Function<void(const Vector<String>&)>&&);

private:
    explicit IDBFactory(IDBClient::IDBConnectionProxy&);

    ExceptionOr<Ref<IDBOpenDBRequest>> openInternal(ScriptExecutionContext&, const String& name, uint64_t version);

    Ref<IDBClient::IDBConnectionProxy> m_connectionProxy;
};

} // namespace WebCore
