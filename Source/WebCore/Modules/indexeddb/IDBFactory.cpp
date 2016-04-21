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
#include "IDBFactory.h"

#if ENABLE(INDEXED_DATABASE)

#include "Document.h"
#include "ExceptionCode.h"
#include "IDBBindingUtilities.h"
#include "IDBConnectionProxy.h"
#include "IDBConnectionToServer.h"
#include "IDBDatabaseIdentifier.h"
#include "IDBKey.h"
#include "IDBOpenDBRequest.h"
#include "Logging.h"
#include "Page.h"
#include "SchemeRegistry.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"

using namespace JSC;

namespace WebCore {

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

Ref<IDBFactory> IDBFactory::create(IDBClient::IDBConnectionProxy& connectionProxy)
{
    return adoptRef(*new IDBFactory(connectionProxy));
}

IDBFactory::IDBFactory(IDBClient::IDBConnectionProxy& connectionProxy)
    : m_connectionProxy(connectionProxy)
{
}

IDBFactory::~IDBFactory()
{
}

RefPtr<IDBOpenDBRequest> IDBFactory::open(ScriptExecutionContext& context, const String& name, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBFactory::open");
    
    return openInternal(context, name, 0, ec);
}

RefPtr<IDBOpenDBRequest> IDBFactory::open(ScriptExecutionContext& context, const String& name, unsigned long long version, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBFactory::open");
    
    if (!version) {
        ec.code = TypeError;
        ec.message = ASCIILiteral("IDBFactory.open() called with a version of 0");
        return nullptr;
    }

    return openInternal(context, name, version, ec);
}

RefPtr<IDBOpenDBRequest> IDBFactory::openInternal(ScriptExecutionContext& context, const String& name, unsigned long long version, ExceptionCodeWithMessage& ec)
{
    if (name.isNull()) {
        ec.code = TypeError;
        ec.message = ASCIILiteral("IDBFactory.open() called without a database name");
        return nullptr;
    }

    if (shouldThrowSecurityException(context)) {
        ec.code = SECURITY_ERR;
        ec.message = ASCIILiteral("IDBFactory.open() called in an invalid security context");
        return nullptr;
    }

    ASSERT(context.securityOrigin());
    ASSERT(context.topOrigin());
    IDBDatabaseIdentifier databaseIdentifier(name, *context.securityOrigin(), *context.topOrigin());
    if (!databaseIdentifier.isValid()) {
        ec.code = TypeError;
        ec.message = ASCIILiteral("IDBFactory.open() called with an invalid security origin");
        return nullptr;
    }

    return m_connectionProxy->openDatabase(context, databaseIdentifier, version);
}

RefPtr<IDBOpenDBRequest> IDBFactory::deleteDatabase(ScriptExecutionContext& context, const String& name, ExceptionCodeWithMessage& ec)
{
    LOG(IndexedDB, "IDBFactory::deleteDatabase - %s", name.utf8().data());

    if (name.isNull()) {
        ec.code = TypeError;
        ec.message = ASCIILiteral("IDBFactory.deleteDatabase() called without a database name");
    }
    
    if (shouldThrowSecurityException(context)) {
        ec.code = SECURITY_ERR;
        ec.message = ASCIILiteral("IDBFactory.deleteDatabase() called in an invalid security context");
        return nullptr;
    }

    ASSERT(context.securityOrigin());
    ASSERT(context.topOrigin());
    IDBDatabaseIdentifier databaseIdentifier(name, *context.securityOrigin(), *context.topOrigin());
    if (!databaseIdentifier.isValid()) {
        ec.code = TypeError;
        ec.message = ASCIILiteral("IDBFactory.deleteDatabase() called with an invalid security origin");
        return nullptr;
    }

    return m_connectionProxy->deleteDatabase(context, databaseIdentifier);
}

short IDBFactory::cmp(ScriptExecutionContext& context, JSValue firstValue, JSValue secondValue, ExceptionCodeWithMessage& ec)
{
    RefPtr<IDBKey> first = scriptValueToIDBKey(context, firstValue);
    RefPtr<IDBKey> second = scriptValueToIDBKey(context, secondValue);

    ASSERT(first);
    ASSERT(second);

    if (!first->isValid() || !second->isValid()) {
        ec.code = IDBDatabaseException::DataError;
        ec.message = ASCIILiteral("Failed to execute 'cmp' on 'IDBFactory': The parameter is not a valid key.");
        return 0;
    }

    return first->compare(second.get());
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
