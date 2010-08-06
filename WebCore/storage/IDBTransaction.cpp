#include "config.h"
#include "IDBTransaction.h"

#if ENABLE(INDEXED_DATABASE)

#include "Event.h"
#include "EventException.h"
#include "IDBDatabase.h"
#include "IDBObjectStore.h"
#include "IDBObjectStoreBackendInterface.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

IDBTransaction::IDBTransaction(ScriptExecutionContext* context, PassRefPtr<IDBTransactionBackendInterface> backend, IDBDatabase* db)
    : ActiveDOMObject(context, this)
    , m_backend(backend)
    , m_database(db)
{
}

IDBTransaction::~IDBTransaction()
{
}

unsigned short IDBTransaction::mode() const
{
    return m_backend->mode();
}

IDBDatabase* IDBTransaction::db()
{
    return m_database.get();
}

PassRefPtr<IDBObjectStore> IDBTransaction::objectStore(const String& name, const ExceptionCode&)
{
    RefPtr<IDBObjectStoreBackendInterface> objectStoreBackend = m_backend->objectStore(name);
    RefPtr<IDBObjectStore> objectStore = IDBObjectStore::create(objectStoreBackend);
    return objectStore.release();
}

void IDBTransaction::abort()
{
    m_backend->abort();
}

ScriptExecutionContext* IDBTransaction::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

bool IDBTransaction::canSuspend() const
{
    // We may be in the middle of a transaction so we cannot suspend our object.
    // Instead, we simply don't allow the owner page to go into the back/forward cache.
    return false;
}

EventTargetData* IDBTransaction::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* IDBTransaction::ensureEventTargetData()
{
    return &m_eventTargetData;
}

}

#endif // ENABLE(INDEXED_DATABASE)
