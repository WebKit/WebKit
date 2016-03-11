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
#include "IDBFactoryImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMRequestState.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "IDBBindingUtilities.h"
#include "IDBDatabaseIdentifier.h"
#include "IDBOpenDBRequestImpl.h"
#include "Logging.h"
#include "Page.h"
#include "SchemeRegistry.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"

namespace WebCore {
namespace IDBClient {

static bool shouldThrowSecurityException(ScriptExecutionContext& context)
{
    ASSERT(is<Document>(context) || context.isWorkerGlobalScope());
    if (is<Document>(context)) {
        Document& document = downcast<Document>(context);
        if (!document.frame())
            return true;
        if (!document.page())
            return true;
    }

    if (!context.securityOrigin()->canAccessDatabase(context.topOrigin()))
        return true;

    return false;
}

Ref<IDBFactory> IDBFactory::create(IDBConnectionToServer& connection)
{
    return adoptRef(*new IDBFactory(connection));
}

IDBFactory::IDBFactory(IDBConnectionToServer& connection)
    : m_connectionToServer(connection)
{
}

RefPtr<WebCore::IDBRequest> IDBFactory::getDatabaseNames(ScriptExecutionContext&, ExceptionCode&)
{
    return nullptr;
}

RefPtr<WebCore::IDBOpenDBRequest> IDBFactory::open(ScriptExecutionContext& context, const String& name, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBFactory::open");
    
    return openInternal(context, name, 0, ec).release();
}

RefPtr<WebCore::IDBOpenDBRequest> IDBFactory::open(ScriptExecutionContext& context, const String& name, unsigned long long version, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBFactory::open");
    
    if (!version) {
        ec = TypeError;
        return nullptr;
    }

    return openInternal(context, name, version, ec).release();
}

RefPtr<IDBOpenDBRequest> IDBFactory::openInternal(ScriptExecutionContext& context, const String& name, unsigned long long version, ExceptionCode& ec)
{
    if (name.isNull()) {
        ec = TypeError;
        return nullptr;
    }

    if (shouldThrowSecurityException(context)) {
        ec = SECURITY_ERR;
        return nullptr;
    }

    ASSERT(context.securityOrigin());
    ASSERT(context.topOrigin());
    IDBDatabaseIdentifier databaseIdentifier(name, *context.securityOrigin(), *context.topOrigin());
    if (!databaseIdentifier.isValid()) {
        ec = TypeError;
        return nullptr;
    }

    auto request = IDBOpenDBRequest::createOpenRequest(m_connectionToServer.get(), context, databaseIdentifier, version);
    m_connectionToServer->openDatabase(request.get());

    return adoptRef(&request.leakRef());
}

RefPtr<WebCore::IDBOpenDBRequest> IDBFactory::deleteDatabase(ScriptExecutionContext& context, const String& name, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBFactory::deleteDatabase - %s", name.utf8().data());

    if (name.isNull()) {
        ec = TypeError;
        return nullptr;
    }
    
    if (shouldThrowSecurityException(context)) {
        ec = SECURITY_ERR;
        return nullptr;
    }

    ASSERT(context.securityOrigin());
    ASSERT(context.topOrigin());
    IDBDatabaseIdentifier databaseIdentifier(name, *context.securityOrigin(), *context.topOrigin());
    if (!databaseIdentifier.isValid()) {
        ec = TypeError;
        return nullptr;
    }

    auto request = IDBOpenDBRequest::createDeleteRequest(m_connectionToServer.get(), context, databaseIdentifier);
    m_connectionToServer->deleteDatabase(request.get());

    return adoptRef(&request.leakRef());
}

short IDBFactory::cmp(ScriptExecutionContext& context, const Deprecated::ScriptValue& firstValue, const Deprecated::ScriptValue& secondValue, ExceptionCodeWithMessage& ec)
{
    DOMRequestState requestState(&context);
    RefPtr<IDBKey> first = scriptValueToIDBKey(&requestState, firstValue);
    RefPtr<IDBKey> second = scriptValueToIDBKey(&requestState, secondValue);

    ASSERT(first);
    ASSERT(second);

    if (!first->isValid() || !second->isValid()) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to execute 'cmp' on 'IDBFactory': The parameter is not a valid key.");
        return 0;
    }

    return static_cast<short>(first->compare(second.get()));
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
