/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IDBFactory.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMStringList.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "GroupSettings.h"
#include "IDBDatabase.h"
#include "IDBDatabaseException.h"
#include "IDBFactoryBackendInterface.h"
#include "IDBKey.h"
#include "IDBKeyRange.h"
#include "IDBOpenDBRequest.h"
#include "IDBVersionChangeRequest.h"
#include "Page.h"
#include "PageGroup.h"
#include "SecurityOrigin.h"
#include "WorkerContext.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"

namespace WebCore {

IDBFactory::IDBFactory(IDBFactoryBackendInterface* factory)
    : m_backend(factory)
{
    // We pass a reference to this object before it can be adopted.
    relaxAdoptionRequirement();
}

IDBFactory::~IDBFactory()
{
}

namespace {
static bool isContextValid(ScriptExecutionContext* context)
{
    ASSERT(context->isDocument() || context->isWorkerContext());
    if (context->isDocument()) {
        Document* document = static_cast<Document*>(context);
        return document->frame() && document->page();
    }
#if !ENABLE(WORKERS)
    if (context->isWorkerContext())
        return false;
#endif
    return true;
}

static String getIndexedDBDatabasePath(ScriptExecutionContext* context)
{
    ASSERT(isContextValid(context));
    if (context->isDocument()) {
        Document* document = static_cast<Document*>(context);
        return document->page()->group().groupSettings()->indexedDBDatabasePath();
    }
#if ENABLE(WORKERS)
    WorkerContext* workerContext = static_cast<WorkerContext*>(context);
    const GroupSettings* groupSettings = workerContext->groupSettings();
    if (groupSettings)
        return groupSettings->indexedDBDatabasePath();
#endif
    return String();
}
}

PassRefPtr<IDBRequest> IDBFactory::getDatabaseNames(ScriptExecutionContext* context)
{
    if (!isContextValid(context))
        return 0;

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), 0);
    m_backend->getDatabaseNames(request, context->securityOrigin(), context, getIndexedDBDatabasePath(context));
    return request;
}

PassRefPtr<IDBOpenDBRequest> IDBFactory::open(ScriptExecutionContext* context, const String& name, int64_t version, ExceptionCode& ec)
{
    if (name.isNull()) {
        ec = NATIVE_TYPE_ERR;
        return 0;
    }
    if (!isContextValid(context))
        return 0;

    RefPtr<IDBOpenDBRequest> request = IDBOpenDBRequest::create(context, IDBAny::create(this), version);
    m_backend->open(name, version, request, context->securityOrigin(), context, getIndexedDBDatabasePath(context));
    return request;
}

PassRefPtr<IDBOpenDBRequest> IDBFactory::open(ScriptExecutionContext* context, const String& name, ExceptionCode& ec)
{
    return open(context, name, IDBDatabaseMetadata::NoIntVersion, ec);
}

PassRefPtr<IDBVersionChangeRequest> IDBFactory::deleteDatabase(ScriptExecutionContext* context, const String& name, ExceptionCode& ec)
{
    if (name.isNull()) {
        ec = NATIVE_TYPE_ERR;
        return 0;
    }
    if (!isContextValid(context))
        return 0;

    RefPtr<IDBVersionChangeRequest> request = IDBVersionChangeRequest::create(context, IDBAny::createNull(), "");
    m_backend->deleteDatabase(name, request, context->securityOrigin(), context, getIndexedDBDatabasePath(context));
    return request;
}

short IDBFactory::cmp(PassRefPtr<IDBKey> first, PassRefPtr<IDBKey> second, ExceptionCode& ec)
{
    ASSERT(first);
    ASSERT(second);

    if (!first->isValid() || !second->isValid()) {
        ec = IDBDatabaseException::DATA_ERR;
        return 0;
    }

    return static_cast<short>(first->compare(second.get()));
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
