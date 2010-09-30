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

#ifndef IDBIndex_h
#define IDBIndex_h

#include "IDBCursor.h"
#include "IDBIndexBackendInterface.h"
#include "PlatformString.h"
#include <wtf/Forward.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBIndex : public RefCounted<IDBIndex> {
public:
    static PassRefPtr<IDBIndex> create(PassRefPtr<IDBIndexBackendInterface> backend, IDBTransactionBackendInterface* transaction)
    {
        return adoptRef(new IDBIndex(backend, transaction));
    }
    ~IDBIndex();

    // Implement the IDL
    String name() const { return m_backend->name(); }
    String storeName() const { return m_backend->storeName(); }
    String keyPath() const { return m_backend->keyPath(); }
    bool unique() const { return m_backend->unique(); }

    PassRefPtr<IDBRequest> openObjectCursor(ScriptExecutionContext*, PassRefPtr<IDBKeyRange> = 0, unsigned short direction = IDBCursor::NEXT);
    PassRefPtr<IDBRequest> openCursor(ScriptExecutionContext*, PassRefPtr<IDBKeyRange> = 0, unsigned short direction = IDBCursor::NEXT);
    PassRefPtr<IDBRequest> getObject(ScriptExecutionContext*, PassRefPtr<IDBKey>);
    PassRefPtr<IDBRequest> get(ScriptExecutionContext*, PassRefPtr<IDBKey>);

private:
    IDBIndex(PassRefPtr<IDBIndexBackendInterface>, IDBTransactionBackendInterface* transaction);

    RefPtr<IDBIndexBackendInterface> m_backend;
    RefPtr<IDBTransactionBackendInterface> m_transaction;
};

} // namespace WebCore

#endif

#endif // IDBIndex_h
