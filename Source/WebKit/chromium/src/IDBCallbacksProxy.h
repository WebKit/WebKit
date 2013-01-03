/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef IDBCallbacksProxy_h
#define IDBCallbacksProxy_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBCallbacks.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebKit {

class WebIDBCallbacks;
class IDBDatabaseCallbacksProxy;

class IDBCallbacksProxy : public WebCore::IDBCallbacks {
public:
    static PassRefPtr<IDBCallbacksProxy> create(PassOwnPtr<WebIDBCallbacks>);
    virtual ~IDBCallbacksProxy();

    virtual void onError(PassRefPtr<WebCore::IDBDatabaseError>);
    virtual void onSuccess(PassRefPtr<WebCore::DOMStringList>);
    virtual void onSuccess(PassRefPtr<WebCore::IDBCursorBackendInterface>, PassRefPtr<WebCore::IDBKey>, PassRefPtr<WebCore::IDBKey> primaryKey, PassRefPtr<WebCore::SerializedScriptValue>);
    virtual void onSuccess(PassRefPtr<WebCore::IDBDatabaseBackendInterface>);
    virtual void onSuccess(PassRefPtr<WebCore::IDBKey>);
    virtual void onSuccess(PassRefPtr<WebCore::SerializedScriptValue>);
    virtual void onSuccess(PassRefPtr<WebCore::SerializedScriptValue>, PassRefPtr<WebCore::IDBKey>, const WebCore::IDBKeyPath&);
    virtual void onSuccess(int64_t);
    virtual void onSuccess();
    virtual void onSuccess(PassRefPtr<WebCore::IDBKey>, PassRefPtr<WebCore::IDBKey> primaryKey, PassRefPtr<WebCore::SerializedScriptValue>);
    virtual void onSuccessWithPrefetch(const Vector<RefPtr<WebCore::IDBKey> >& keys, const Vector<RefPtr<WebCore::IDBKey> >& primaryKeys, const Vector<RefPtr<WebCore::SerializedScriptValue> >& values);
    virtual void onBlocked();
    virtual void onBlocked(int64_t existingVersion);
    virtual void onUpgradeNeeded(int64_t oldVersion, PassRefPtr<WebCore::IDBTransactionBackendInterface>, PassRefPtr<WebCore::IDBDatabaseBackendInterface>);

    void setDatabaseCallbacks(PassRefPtr<IDBDatabaseCallbacksProxy>);

private:
    IDBCallbacksProxy(PassOwnPtr<WebIDBCallbacks>);

    OwnPtr<WebIDBCallbacks> m_callbacks;
    RefPtr<IDBDatabaseCallbacksProxy> m_databaseCallbacks;
    bool m_didComplete;
    bool m_didCreateProxy;
};

} // namespace WebKit

#endif

#endif // IDBCallbacksProxy_h
