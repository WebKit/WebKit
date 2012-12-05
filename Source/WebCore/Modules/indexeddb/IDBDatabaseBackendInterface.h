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

#ifndef IDBDatabaseBackendInterface_h
#define IDBDatabaseBackendInterface_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBCallbacks;
class IDBDatabaseCallbacks;
class IDBKeyPath;
class IDBObjectStoreBackendInterface;
class IDBTransactionBackendInterface;
struct IDBDatabaseMetadata;

typedef int ExceptionCode;

// This class is shared by IDBDatabase (async) and IDBDatabaseSync (sync).
// This is implemented by IDBDatabaseBackendImpl and optionally others (in order to proxy
// calls across process barriers). All calls to these classes should be non-blocking and
// trigger work on a background thread if necessary.
class IDBDatabaseBackendInterface : public RefCounted<IDBDatabaseBackendInterface> {
public:
    virtual ~IDBDatabaseBackendInterface() { }

    virtual IDBDatabaseMetadata metadata() const = 0;

    virtual PassRefPtr<IDBObjectStoreBackendInterface> createObjectStore(int64_t, const String& name, const IDBKeyPath&, bool autoIncrement, IDBTransactionBackendInterface*, ExceptionCode&) = 0;
    virtual void deleteObjectStore(int64_t, IDBTransactionBackendInterface*, ExceptionCode&) = 0;
    virtual PassRefPtr<IDBTransactionBackendInterface> createTransaction(int64_t transactionId, const Vector<int64_t>& objectStoreIds, unsigned short mode) = 0;
    virtual void close(PassRefPtr<IDBDatabaseCallbacks>) = 0;
};

} // namespace WebCore

#endif

#endif // IDBDatabaseBackendInterface_h
