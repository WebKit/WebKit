#include "config.h"
#include "IDBTransactionBackendImpl.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

PassRefPtr<IDBTransactionBackendInterface> IDBTransactionBackendImpl::create(DOMStringList* objectStores, unsigned short mode, unsigned long timeout, int id)
{
    return adoptRef(new IDBTransactionBackendImpl(objectStores, mode, timeout, id));
}

IDBTransactionBackendImpl::IDBTransactionBackendImpl(DOMStringList* objectStores, unsigned short mode, unsigned long timeout, int id)
    : m_objectStoreNames(objectStores)
    , m_mode(mode)
    , m_timeout(timeout)
    , m_id(id)
    , m_aborted(false)
{
}

PassRefPtr<IDBObjectStoreBackendInterface> IDBTransactionBackendImpl::objectStore(const String& name)
{
    // FIXME: implement.
    ASSERT_NOT_REACHED();
    return 0;
}

void IDBTransactionBackendImpl::scheduleTask(PassOwnPtr<ScriptExecutionContext::Task>)
{
    // FIXME: implement.
    ASSERT_NOT_REACHED();
}

void IDBTransactionBackendImpl::abort()
{
    m_aborted = true;
    m_callbacks->onAbort();
}

};

#endif // ENABLE(INDEXED_DATABASE)
