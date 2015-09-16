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

#include "ExceptionCode.h"
#include "IDBDatabaseIdentifier.h"
#include "IDBOpenDBRequestImpl.h"
#include "Logging.h"
#include "Page.h"
#include "SchemeRegistry.h"
#include "SecurityOrigin.h"

namespace WebCore {
namespace IDBClient {

static bool shouldThrowSecurityException(ScriptExecutionContext* context)
{
    ASSERT(is<Document>(*context) || context->isWorkerGlobalScope());
    if (is<Document>(*context)) {
        Document& document = downcast<Document>(*context);
        if (!document.frame())
            return true;
        if (!document.page())
            return true;
        if (document.page()->usesEphemeralSession() && !SchemeRegistry::allowsDatabaseAccessInPrivateBrowsing(document.securityOrigin()->protocol()))
            return true;
    }

    if (!context->securityOrigin()->canAccessDatabase(context->topOrigin()))
        return true;

    return false;
}

Ref<IDBFactory> IDBFactory::create()
{
    return adoptRef(*new IDBFactory);
}

IDBFactory::IDBFactory()
{

}

PassRefPtr<WebCore::IDBRequest> IDBFactory::getDatabaseNames(ScriptExecutionContext*, ExceptionCode&)
{
    return nullptr;
}

PassRefPtr<WebCore::IDBOpenDBRequest> IDBFactory::open(ScriptExecutionContext*, const String&, ExceptionCode&)
{
    return nullptr;
}

PassRefPtr<WebCore::IDBOpenDBRequest> IDBFactory::open(ScriptExecutionContext*, const String&, unsigned long long, ExceptionCode&)
{
    return nullptr;
}

PassRefPtr<WebCore::IDBOpenDBRequest> IDBFactory::deleteDatabase(ScriptExecutionContext* context, const String& name, ExceptionCode& ec)
{
    LOG(IndexedDB, "IDBFactory::deleteDatabase");

    if (name.isNull()) {
        ec = TypeError;
        return nullptr;
    }
    
    if (shouldThrowSecurityException(context)) {
        ec = SECURITY_ERR;
        return nullptr;
    }

    ASSERT(context->securityOrigin());
    ASSERT(context->topOrigin());
    IDBDatabaseIdentifier databaseIdentifier(name, *context->securityOrigin(), *context->topOrigin());
    if (!databaseIdentifier.isValid()) {
        ec = TypeError;
        return nullptr;
    }

    auto request = IDBOpenDBRequest::create(context);
    return adoptRef(&request.leakRef());
}

short IDBFactory::cmp(ScriptExecutionContext*, const Deprecated::ScriptValue&, const Deprecated::ScriptValue&, ExceptionCode&)
{
    return 0;
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
