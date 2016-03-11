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

#ifndef IDBFactoryImpl_h
#define IDBFactoryImpl_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToServer.h"
#include "IDBFactory.h"

namespace WebCore {
namespace IDBClient {

class IDBOpenDBRequest;

class IDBFactory : public WebCore::IDBFactory {
public:
    static Ref<IDBFactory> create(IDBConnectionToServer&);

    RefPtr<WebCore::IDBRequest> getDatabaseNames(ScriptExecutionContext&, ExceptionCode&) final;

    RefPtr<WebCore::IDBOpenDBRequest> open(ScriptExecutionContext&, const String& name, ExceptionCode&) final;
    RefPtr<WebCore::IDBOpenDBRequest> open(ScriptExecutionContext&, const String& name, unsigned long long version, ExceptionCode&) final;
    RefPtr<WebCore::IDBOpenDBRequest> deleteDatabase(ScriptExecutionContext&, const String& name, ExceptionCode&) final;

    short cmp(ScriptExecutionContext&, const Deprecated::ScriptValue& first, const Deprecated::ScriptValue& second, ExceptionCodeWithMessage&) final;

private:
    IDBFactory(IDBConnectionToServer&);
    
    RefPtr<IDBOpenDBRequest> openInternal(ScriptExecutionContext&, const String& name, unsigned long long version, ExceptionCode&);
    
    Ref<IDBConnectionToServer> m_connectionToServer;
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBFactoryImpl_h
