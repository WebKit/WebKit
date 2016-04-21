/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToServer.h"

namespace WebCore {

class IDBDatabaseIdentifier;
class IDBOpenDBRequest;
class ScriptExecutionContext;

namespace IDBClient {

class IDBConnectionProxy {
    WTF_MAKE_NONCOPYABLE(IDBConnectionProxy);
public:
    IDBConnectionProxy(IDBConnectionToServer&);

    RefPtr<IDBOpenDBRequest> openDatabase(ScriptExecutionContext&, const IDBDatabaseIdentifier&, uint64_t version);
    RefPtr<IDBOpenDBRequest> deleteDatabase(ScriptExecutionContext&, const IDBDatabaseIdentifier&);

    uint64_t serverConnectionIdentifier() const { return m_serverConnectionIdentifier; }

    // FIXME: Temporarily required during bringup of IDB-in-Workers.
    // Once all IDB object reliance on the IDBConnectionToServer has been shifted to reliance on
    // IDBConnectionProxy, remove this.
    IDBConnectionToServer& connectionToServer();

private:
    Ref<IDBConnectionToServer> m_connectionToServer;
    uint64_t m_serverConnectionIdentifier;
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
