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

#ifndef IDBConnectionToServerDelegate_h
#define IDBConnectionToServerDelegate_h

#if ENABLE(INDEXED_DATABASE)

#include <wtf/text/WTFString.h>

namespace WebCore {

class IDBCursorInfo;
class IDBIndexInfo;
class IDBKey;
class IDBKeyData;
class IDBObjectStoreInfo;
class IDBRequestData;
class IDBResourceIdentifier;
class IDBTransactionInfo;
class SerializedScriptValue;

namespace IndexedDB {
enum class ObjectStoreOverwriteMode;
}

struct IDBKeyRangeData;

namespace IDBClient {

class IDBConnectionToServerDelegate {
public:
    virtual ~IDBConnectionToServerDelegate() { }

    virtual uint64_t identifier() const = 0;
    virtual void deleteDatabase(IDBRequestData&) = 0;
    virtual void openDatabase(IDBRequestData&) = 0;
    virtual void abortTransaction(IDBResourceIdentifier&) = 0;
    virtual void commitTransaction(IDBResourceIdentifier&) = 0;
    virtual void didFinishHandlingVersionChangeTransaction(IDBResourceIdentifier&) = 0;
    virtual void createObjectStore(const IDBRequestData&, const IDBObjectStoreInfo&) = 0;
    virtual void deleteObjectStore(const IDBRequestData&, const String& objectStoreName) = 0;
    virtual void clearObjectStore(const IDBRequestData&, uint64_t objectStoreIdentifier) = 0;
    virtual void createIndex(const IDBRequestData&, const IDBIndexInfo&) = 0;
    virtual void deleteIndex(const IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName) = 0;
    virtual void putOrAdd(const IDBRequestData&, IDBKey*, SerializedScriptValue&, const IndexedDB::ObjectStoreOverwriteMode) = 0;
    virtual void getRecord(const IDBRequestData&, const IDBKeyRangeData&) = 0;
    virtual void getCount(const IDBRequestData&, const IDBKeyRangeData&) = 0;
    virtual void deleteRecord(const IDBRequestData&, const IDBKeyRangeData&) = 0;
    virtual void openCursor(const IDBRequestData&, const IDBCursorInfo&) = 0;
    virtual void iterateCursor(const IDBRequestData&, const IDBKeyData&, unsigned long count) = 0;

    virtual void establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo&) = 0;

    virtual void databaseConnectionClosed(uint64_t databaseConnectionIdentifier) = 0;

    virtual void ref() = 0;
    virtual void deref() = 0;
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBConnectionToServerDelegate_h
