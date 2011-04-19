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
#include "IDBLevelDBBackingStore.h"

#if ENABLE(INDEXED_DATABASE)
#if ENABLE(LEVELDB)

#include "Assertions.h"
#include "FileSystem.h"
#include "IDBFactoryBackendImpl.h"
#include "IDBKeyRange.h"
#include "LevelDBComparator.h"
#include "LevelDBDatabase.h"
#include "LevelDBIterator.h"
#include "LevelDBSlice.h"
#include "SecurityOrigin.h"

#ifndef INT64_MAX
// FIXME: We shouldn't need to rely on these macros.
#define INT64_MAX 0x7fffffffffffffffLL
#endif
#ifndef INT32_MAX
#define INT32_MAX 0x7fffffffL
#endif

// LevelDB stores key/value pairs. Keys and values are strings of bytes, normally of type Vector<char>.
//
// The keys in the backing store are variable-length tuples with different types
// of fields. Each key in the backing store starts with a ternary prefix: (database id, object store id, index id). For each, 0 is reserved for meta-data.
// The prefix makes sure that data for a specific database, object store, and index are grouped together. The locality is important for performance: common
// operations should only need a minimal number of seek operations. For example, all the meta-data for a database is grouped together so that reading that
// meta-data only requires one seek.
//
// Each key type has a class (in square brackets below) which knows how to encode, decode, and compare that key type.
//
// Global meta-data have keys with prefix (0,0,0), followed by a type byte:
//
//     <0, 0, 0, 0>                                           => IndexedDB/LevelDB schema version (0 for now) [SchemaVersionKey]
//     <0, 0, 0, 1>                                           => The maximum database id ever allocated [MaxDatabaseIdKey]
//     <0, 0, 0, 100, database id>                            => Existence implies the database id is in the free list [DatabaseFreeListKey]
//     <0, 0, 0, 201, utf16 origin name, utf16 database name> => Database id [DatabaseNameKey]
//
//
// Database meta-data:
//
//     Again, the prefix is followed by a type byte.
//
//     <database id, 0, 0, 0> => utf16 origin name [DatabaseMetaDataKey]
//     <database id, 0, 0, 1> => utf16 database name [DatabaseMetaDataKey]
//     <database id, 0, 0, 2> => utf16 user version data [DatabaseMetaDataKey]
//     <database id, 0, 0, 3> => maximum object store id ever allocated [DatabaseMetaDataKey]
//
//
// Object store meta-data:
//
//     The prefix is followed by a type byte, then a variable-length integer, and then another variable-length integer (FIXME: this should be a byte).
//
//     <database id, 0, 0, 50, object store id, 0> => utf16 object store name [ObjectStoreMetaDataKey]
//     <database id, 0, 0, 50, object store id, 1> => utf16 key path [ObjectStoreMetaDataKey]
//     <database id, 0, 0, 50, object store id, 2> => has auto increment [ObjectStoreMetaDataKey]
//     <database id, 0, 0, 50, object store id, 3> => is evictable [ObjectStoreMetaDataKey]
//     <database id, 0, 0, 50, object store id, 4> => last "version" number [ObjectStoreMetaDataKey]
//     <database id, 0, 0, 50, object store id, 5> => maximum index id ever allocated [ObjectStoreMetaDataKey]
//
//
// Index meta-data:
//
//     The prefix is followed by a type byte, then two variable-length integers, and then another type byte.
//
//     <database id, 0, 0, 100, object store id, index id, 0> => utf16 index name [IndexMetaDataKey]
//     <database id, 0, 0, 100, object store id, index id, 1> => are index keys unique [IndexMetaDataKey]
//     <database id, 0, 0, 100, object store id, index id, 2> => utf16 key path [IndexMetaDataKey]
//
//
// Other object store and index meta-data:
//
//     The prefix is followed by a type byte. The object store and index id are variable length integers, the utf16 strings are variable length strings.
//
//     <database id, 0, 0, 150, object store id>                   => existence implies the object store id is in the free list [ObjectStoreFreeListKey]
//     <database id, 0, 0, 151, object store id, index id>         => existence implies the index id is in the free list [IndexFreeListKey]
//     <database id, 0, 0, 200, utf16 object store name>           => object store id [ObjectStoreNamesKey]
//     <database id, 0, 0, 201, object store id, utf16 index name> => index id [IndexNamesKey]
//
//
// Object store data:
//
//     The prefix is followed by a type byte. The user key is an encoded IDBKey.
//
//     <database id, object store id, 1, user key> => "version", serialized script value [ObjectStoreDataKey]
//
//
// "Exists" entry:
//
//     The prefix is followed by a type byte. The user key is an encoded IDBKey.
//
//     <database id, object store id, 2, user key> => "version" [ExistsEntryKey]
//
//
// Index data:
//
//     The prefix is followed by a type byte. The user key is an encoded IDBKey. The sequence number is a variable length integer.
//
//     <database id, object store id, index id, user key, sequence number> => "version", user key [IndexDataKey]
//
//     (The sequence number is used to allow two entries with the same user key
//     in non-unique indexes. The "version" field is used to weed out stale
//     index data. Whenever new object store data is inserted, it gets a new
//     "version" number, and new index data is written with this number. When
//     the index is used for look-ups, entries are validated against the
//     "exists" entries, and records with old "version" numbers are deleted
//     when they are encountered in getPrimaryKeyViaIndex,
//     IndexCursorImpl::loadCurrentRow, and IndexKeyCursorImpl::loadCurrentRow).

static const unsigned char kIDBKeyNullTypeByte = 0;
static const unsigned char kIDBKeyStringTypeByte = 1;
static const unsigned char kIDBKeyDateTypeByte = 2;
static const unsigned char kIDBKeyNumberTypeByte = 3;
static const unsigned char kIDBKeyMinKeyTypeByte = 4;

static const unsigned char kMinimumIndexId = 30;
static const unsigned char kObjectStoreDataIndexId = 1;
static const unsigned char kExistsEntryIndexId = 2;

static const unsigned char kSchemaVersionTypeByte = 0;
static const unsigned char kMaxDatabaseIdTypeByte = 1;
static const unsigned char kDatabaseFreeListTypeByte = 100;
static const unsigned char kDatabaseNameTypeByte = 201;

static const unsigned char kObjectStoreMetaDataTypeByte = 50;
static const unsigned char kIndexMetaDataTypeByte = 100;
static const unsigned char kObjectStoreFreeListTypeByte = 150;
static const unsigned char kIndexFreeListTypeByte = 151;
static const unsigned char kObjectStoreNamesTypeByte = 200;
static const unsigned char kIndexNamesKeyTypeByte = 201;

namespace WebCore {

static Vector<char> encodeByte(unsigned char c)
{
    Vector<char> v;
    v.append(c);
    return v;
}

static Vector<char> maxIDBKey()
{
    return encodeByte(kIDBKeyNullTypeByte);
}

static Vector<char> minIDBKey()
{
    return encodeByte(kIDBKeyMinKeyTypeByte);
}

static Vector<char> encodeInt(int64_t n)
{
    ASSERT(n >= 0);
    Vector<char> ret; // FIXME: Size this at creation.

    do {
        unsigned char c = n;
        ret.append(c);
        n >>= 8;
    } while (n);

    return ret;
}

static int64_t decodeInt(const char* begin, const char* end)
{
    ASSERT(begin <= end);
    int64_t ret = 0;

    while (begin < end) {
        unsigned char c = *begin++;
        ret = (ret << 8) | c;
    }

    return ret;
}

static Vector<char> encodeVarInt(int64_t n)
{
    Vector<char> ret; // FIXME: Size this at creation.

    do {
        unsigned char c = n & 0x7f;
        n >>= 7;
        if (n)
            c |= 128;
        ret.append(c);
    } while (n);

    return ret;
}

static const char* decodeVarInt(const char *p, const char* limit, int64_t& foundInt)
{
    ASSERT(limit >= p);
    foundInt = 0;

    do {
        if (p >= limit)
            return 0;

        foundInt = (foundInt << 7) | (*p & 0x7f);
    } while (*p++ & 128);
    return p;
}

static Vector<char> encodeString(const String& s)
{
    Vector<char> ret; // FIXME: Size this at creation.

    for (unsigned i = 0; i < s.length(); ++i) {
        UChar u = s[i];
        unsigned char hi = u >> 8;
        unsigned char lo = u;
        ret.append(hi);
        ret.append(lo);
    }

    return ret;
}

static String decodeString(const char* p, const char* end)
{
    ASSERT(end >= p);
    ASSERT(!((end - p) % 2));

    size_t len = (end - p) / 2;
    Vector<UChar> vector(len);

    for (size_t i = 0; i < len; ++i) {
        unsigned char hi = *p++;
        unsigned char lo = *p++;

        vector[i] = (hi << 8) | lo;
    }

    return String::adopt(vector);
}

static Vector<char> encodeStringWithLength(const String& s)
{
    Vector<char> ret = encodeVarInt(s.length());
    ret.append(encodeString(s));
    return ret;
}

static const char* decodeStringWithLength(const char* p, const char* limit, String& foundString)
{
    ASSERT(limit >= p);
    int64_t len;
    p = decodeVarInt(p, limit, len);
    if (!p)
        return 0;
    if (p + len * 2 > limit)
        return 0;

    foundString = decodeString(p, p + len * 2);
    p += len * 2;
    return p;
}

static Vector<char> encodeDouble(double x)
{
    // FIXME: It would be nice if we could be byte order independent.
    const char* p = reinterpret_cast<char*>(&x);
    Vector<char> v;
    v.append(p, sizeof(x));
    ASSERT(v.size() == sizeof(x));
    return v;
}

static const char* decodeDouble(const char* p, const char* limit, double* d)
{
    if (p + sizeof(*d) > limit)
        return 0;

    char* x = reinterpret_cast<char*>(d);
    for (size_t i = 0; i < sizeof(*d); ++i)
        *x++ = *p++;
    return p;
}

static Vector<char> encodeIDBKey(const IDBKey& key)
{
    Vector<char> ret;

    switch (key.type()) {
    case IDBKey::NullType:
        return encodeByte(kIDBKeyNullTypeByte);
    case IDBKey::StringType:
        ret = encodeByte(kIDBKeyStringTypeByte);
        ret.append(encodeStringWithLength(key.string()));
        return ret;
    case IDBKey::DateType:
        ret = encodeByte(kIDBKeyDateTypeByte);
        ret.append(encodeDouble(key.date()));
        ASSERT(ret.size() == 9);
        return ret;
    case IDBKey::NumberType:
        ret = encodeByte(kIDBKeyNumberTypeByte);
        ret.append(encodeDouble(key.number()));
        ASSERT(ret.size() == 9);
        return ret;
    }

    ASSERT_NOT_REACHED();
    return Vector<char>();
}

static const char* decodeIDBKey(const char* p, const char* limit, RefPtr<IDBKey>& foundKey)
{
    ASSERT(limit >= p);
    if (p >= limit)
        return 0;

    unsigned char type = *p++;
    String s;
    double d;

    switch (type) {
    case kIDBKeyNullTypeByte:
        // Null.
        foundKey = IDBKey::createNull();
        return p;
    case kIDBKeyStringTypeByte:
        // String.
        p = decodeStringWithLength(p, limit, s);
        if (!p)
            return 0;
        foundKey = IDBKey::createString(s);
        return p;
    case kIDBKeyDateTypeByte:
        // Date.
        p = decodeDouble(p, limit, &d);
        if (!p)
            return 0;
        foundKey = IDBKey::createDate(d);
        return p;
    case kIDBKeyNumberTypeByte:
        // Number.
        p = decodeDouble(p, limit, &d);
        if (!p)
            return 0;
        foundKey = IDBKey::createNumber(d);
        return p;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static const char* extractEncodedIDBKey(const char* start, const char* limit, Vector<char>* result)
{
    const char* p = start;
    if (p >= limit)
        return 0;

    unsigned char type = *p++;

    int64_t stringLen;

    switch (type) {
    case kIDBKeyNullTypeByte:
    case kIDBKeyMinKeyTypeByte:
        *result = encodeByte(type);
        return p;
    case kIDBKeyStringTypeByte:
        // String.
        p = decodeVarInt(p, limit, stringLen);
        if (!p)
            return 0;
        if (p + stringLen * 2 > limit)
            return 0;
        result->clear();
        result->append(start, p - start + stringLen * 2);
        return p + stringLen * 2;
    case kIDBKeyDateTypeByte:
    case kIDBKeyNumberTypeByte:
        // Date or number.
        if (p + sizeof(double) > limit)
            return 0;
        result->clear();
        result->append(start, 1 + sizeof(double));
        return p + sizeof(double);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static int compareEncodedIDBKeys(const Vector<char>& keyA, const Vector<char>& keyB)
{
    ASSERT(keyA.size() >= 1);
    ASSERT(keyB.size() >= 1);

    const char* p = keyA.data();
    const char* limitA = p + keyA.size();
    const char* q = keyB.data();
    const char* limitB = q + keyB.size();

    unsigned char typeA = *p++;
    unsigned char typeB = *q++;

    String s, t;
    double d, e;

    if (int x = typeB - typeA) // FIXME: Note the subtleness!
        return x;

    switch (typeA) {
    case kIDBKeyNullTypeByte:
    case kIDBKeyMinKeyTypeByte:
        // Null type or max type; no payload to compare.
        return 0;
    case kIDBKeyStringTypeByte:
        // String type.
        p = decodeStringWithLength(p, limitA, s); // FIXME: Compare without actually decoding the String!
        ASSERT(p);
        q = decodeStringWithLength(q, limitB, t);
        ASSERT(q);
        return codePointCompare(s, t);
    case kIDBKeyDateTypeByte:
    case kIDBKeyNumberTypeByte:
        // Date or number.
        p = decodeDouble(p, limitA, &d);
        ASSERT(p);
        q = decodeDouble(q, limitB, &e);
        ASSERT(q);
        if (d < e)
            return -1;
        if (d > e)
            return 1;
        return 0;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static bool getInt(LevelDBDatabase* db, const Vector<char>& key, int64_t& foundInt)
{
    Vector<char> result;
    if (!db->get(key, result))
        return false;

    foundInt = decodeInt(result.begin(), result.end());
    return true;
}

static bool putInt(LevelDBDatabase* db, const Vector<char>& key, int64_t value)
{
    return db->put(key, encodeInt(value));
}

static bool getString(LevelDBDatabase* db, const Vector<char>& key, String& foundString)
{
    Vector<char> result;
    if (!db->get(key, result))
        return false;

    foundString = decodeString(result.begin(), result.end());
    return true;
}

static bool putString(LevelDBDatabase* db, const Vector<char> key, const String& value)
{
    if (!db->put(key, encodeString(value)))
        return false;
    return true;
}

namespace {
class KeyPrefix {
public:
    KeyPrefix()
        : m_databaseId(kInvalidType)
        , m_objectStoreId(kInvalidType)
        , m_indexId(kInvalidType)
    {
    }

    KeyPrefix(int64_t databaseId, int64_t objectStoreId, int64_t indexId)
        : m_databaseId(databaseId)
        , m_objectStoreId(objectStoreId)
        , m_indexId(indexId)
    {
    }

    static const char* decode(const char* start, const char* limit, KeyPrefix* result)
    {
        if (start == limit)
            return 0;

        unsigned char firstByte = *start++;

        int databaseIdBytes = ((firstByte >> 5) & 0x7) + 1;
        int objectStoreIdBytes = ((firstByte >> 2) & 0x7) + 1;
        int indexIdBytes = (firstByte & 0x3) + 1;

        if (start + databaseIdBytes + objectStoreIdBytes + indexIdBytes > limit)
            return 0;

        result->m_databaseId = decodeInt(start, start + databaseIdBytes);
        start += databaseIdBytes;
        result->m_objectStoreId = decodeInt(start, start + objectStoreIdBytes);
        start += objectStoreIdBytes;
        result->m_indexId = decodeInt(start, start + indexIdBytes);
        start += indexIdBytes;

        return start;
    }

    Vector<char> encode() const
    {
        ASSERT(m_databaseId != kInvalidId);
        ASSERT(m_objectStoreId != kInvalidId);
        ASSERT(m_indexId != kInvalidId);

        Vector<char> databaseIdString = encodeInt(m_databaseId);
        Vector<char> objectStoreIdString = encodeInt(m_objectStoreId);
        Vector<char> indexIdString = encodeInt(m_indexId);

        ASSERT(databaseIdString.size() <= 8);
        ASSERT(objectStoreIdString.size() <= 8);
        ASSERT(indexIdString.size() <= 4);


        unsigned char firstByte = (databaseIdString.size() - 1) << 5 | (objectStoreIdString.size() - 1) << 2 | (indexIdString.size() - 1);
        Vector<char> ret;
        ret.append(firstByte);
        ret.append(databaseIdString);
        ret.append(objectStoreIdString);
        ret.append(indexIdString);

        return ret;
    }

    int compare(const KeyPrefix& other) const
    {
        ASSERT(m_databaseId != kInvalidId);
        ASSERT(m_objectStoreId != kInvalidId);
        ASSERT(m_indexId != kInvalidId);

        if (m_databaseId != other.m_databaseId)
            return m_databaseId - other.m_databaseId;
        if (m_objectStoreId != other.m_objectStoreId)
            return m_objectStoreId - other.m_objectStoreId;
        if (m_indexId != other.m_indexId)
            return m_indexId - other.m_indexId;
        return 0;
    }

    enum Type {
        kGlobalMetaData,
        kDatabaseMetaData,
        kObjectStoreData,
        kExistsEntry,
        kIndexData,
        kInvalidType
    };

    Type type() const
    {
        ASSERT(m_databaseId != kInvalidId);
        ASSERT(m_objectStoreId != kInvalidId);
        ASSERT(m_indexId != kInvalidId);

        if (!m_databaseId)
            return kGlobalMetaData;
        if (!m_objectStoreId)
            return kDatabaseMetaData;
        if (m_indexId == kObjectStoreDataIndexId)
            return kObjectStoreData;
        if (m_indexId == kExistsEntryIndexId)
            return kExistsEntry;
        if (m_indexId >= kMinimumIndexId)
            return kIndexData;

        ASSERT_NOT_REACHED();
        return kInvalidType;
    }

    int64_t m_databaseId;
    int64_t m_objectStoreId;
    int64_t m_indexId;

    static const int64_t kInvalidId = -1;
};

class SchemaVersionKey {
public:
    static Vector<char> encode()
    {
        KeyPrefix prefix(0, 0, 0);
        Vector<char> ret = prefix.encode();
        ret.append(encodeByte(kSchemaVersionTypeByte));
        return ret;
    }
};

class MaxDatabaseIdKey {
public:
    static Vector<char> encode()
    {
        KeyPrefix prefix(0, 0, 0);
        Vector<char> ret = prefix.encode();
        ret.append(encodeByte(kMaxDatabaseIdTypeByte));
        return ret;
    }
};

class DatabaseFreeListKey {
public:
    DatabaseFreeListKey()
        : m_databaseId(-1)
    {
    }

    static const char* decode(const char* start, const char* limit, DatabaseFreeListKey* result)
    {
        KeyPrefix prefix;
        const char *p = KeyPrefix::decode(start, limit, &prefix);
        if (!p)
            return 0;
        ASSERT(!prefix.m_databaseId);
        ASSERT(!prefix.m_objectStoreId);
        ASSERT(!prefix.m_indexId);
        if (p == limit)
            return 0;
        unsigned char typeByte = *p++;
        ASSERT_UNUSED(typeByte, typeByte == kDatabaseFreeListTypeByte);
        if (p == limit)
            return 0;
        return decodeVarInt(p, limit, result->m_databaseId);
    }

    static Vector<char> encode(int64_t databaseId)
    {
        KeyPrefix prefix(0, 0, 0);
        Vector<char> ret = prefix.encode();
        ret.append(encodeByte(kDatabaseFreeListTypeByte));
        ret.append(encodeVarInt(databaseId));
        return ret;
    }

    int64_t databaseId() const
    {
        ASSERT(m_databaseId >= 0);
        return m_databaseId;
    }

    int compare(const DatabaseFreeListKey& other) const
    {
        ASSERT(m_databaseId >= 0);
        return m_databaseId - other.m_databaseId;
    }

private:
    int64_t m_databaseId;
};

class DatabaseNameKey {
public:
    static const char* decode(const char* start, const char* limit, DatabaseNameKey* result)
    {
        KeyPrefix prefix;
        const char* p = KeyPrefix::decode(start, limit, &prefix);
        if (!p)
            return p;
        ASSERT(!prefix.m_databaseId);
        ASSERT(!prefix.m_objectStoreId);
        ASSERT(!prefix.m_indexId);
        if (p == limit)
            return 0;
        unsigned char typeByte = *p++;
        ASSERT_UNUSED(typeByte, typeByte == kDatabaseNameTypeByte);
        if (p == limit)
            return 0;
        p = decodeStringWithLength(p, limit, result->m_origin);
        if (!p)
            return 0;
        return decodeStringWithLength(p, limit, result->m_databaseName);
    }

    static Vector<char> encode(const String& origin, const String& databaseName)
    {
        KeyPrefix prefix(0, 0, 0);
        Vector<char> ret = prefix.encode();
        ret.append(encodeByte(kDatabaseNameTypeByte));
        ret.append(encodeStringWithLength(origin));
        ret.append(encodeStringWithLength(databaseName));
        return ret;
    }

    String origin() const { return m_origin; }
    String databaseName() const { return m_databaseName; }

    int compare(const DatabaseNameKey& other)
    {
        if (int x = codePointCompare(m_origin, other.m_origin))
            return x;
        return codePointCompare(m_databaseName, other.m_databaseName);
    }

private:
    String m_origin; // FIXME: Store encoded strings, or just pointers.
    String m_databaseName;
};

class DatabaseMetaDataKey {
public:
    enum MetaDataType {
        kOriginName = 0,
        kDatabaseName = 1,
        kUserVersion = 2,
        kMaxObjectStoreId = 3
    };

    static Vector<char> encode(int64_t databaseId, MetaDataType metaDataType)
    {
        KeyPrefix prefix(databaseId, 0, 0);
        Vector<char> ret = prefix.encode();
        ret.append(encodeByte(metaDataType));
        return ret;
    }
};

class ObjectStoreMetaDataKey {
public:
    ObjectStoreMetaDataKey()
        : m_objectStoreId(-1)
        , m_metaDataType(-1)
    {
    }

    static const char* decode(const char* start, const char* limit, ObjectStoreMetaDataKey* result)
    {
        KeyPrefix prefix;
        const char* p = KeyPrefix::decode(start, limit, &prefix);
        if (!p)
            return 0;
        ASSERT(prefix.m_databaseId);
        ASSERT(!prefix.m_objectStoreId);
        ASSERT(!prefix.m_indexId);
        if (p == limit)
            return 0;
        unsigned char typeByte = *p++;
        ASSERT_UNUSED(typeByte, typeByte == kObjectStoreMetaDataTypeByte);
        if (p == limit)
            return 0;
        p = decodeVarInt(p, limit, result->m_objectStoreId);
        if (!p)
            return 0;
        ASSERT(result->m_objectStoreId);
        if (p == limit)
            return 0;
        return decodeVarInt(p, limit, result->m_metaDataType);
    }

    static Vector<char> encode(int64_t databaseId, int64_t objectStoreId, int64_t metaDataType)
    {
        KeyPrefix prefix(databaseId, 0, 0);
        Vector<char> ret = prefix.encode();
        ret.append(encodeByte(kObjectStoreMetaDataTypeByte));
        ret.append(encodeVarInt(objectStoreId));
        ret.append(encodeVarInt(metaDataType));
        return ret;
    }

    int64_t objectStoreId() const
    {
        ASSERT(m_objectStoreId >= 0);
        return m_objectStoreId;
    }
    int64_t metaDataType() const
    {
        ASSERT(m_metaDataType >= 0);
        return m_metaDataType;
    }

    int compare(const ObjectStoreMetaDataKey& other)
    {
        ASSERT(m_objectStoreId >= 0);
        ASSERT(m_metaDataType >= 0);
        if (int x = m_objectStoreId - other.m_objectStoreId)
            return x; // FIXME: Is this cast safe? I.e., will it preserve the sign?
        return m_metaDataType - other.m_metaDataType;
    }

private:
    int64_t m_objectStoreId;
    int64_t m_metaDataType; // FIXME: Make this a byte.
};

class IndexMetaDataKey {
public:
    IndexMetaDataKey()
        : m_objectStoreId(-1)
        , m_indexId(-1)
        , m_metaDataType(0)
    {
    }

    static const char* decode(const char* start, const char* limit, IndexMetaDataKey* result)
    {
        KeyPrefix prefix;
        const char* p = KeyPrefix::decode(start, limit, &prefix);
        if (!p)
            return 0;
        ASSERT(prefix.m_databaseId);
        ASSERT(!prefix.m_objectStoreId);
        ASSERT(!prefix.m_indexId);
        if (p == limit)
            return 0;
        unsigned char typeByte = *p++;
        ASSERT_UNUSED(typeByte, typeByte == kIndexMetaDataTypeByte);
        if (p == limit)
            return 0;
        p = decodeVarInt(p, limit, result->m_objectStoreId);
        if (!p)
            return 0;
        p = decodeVarInt(p, limit, result->m_indexId);
        if (!p)
            return 0;
        if (p == limit)
            return 0;
        result->m_metaDataType = *p++;
        return p;
    }

    static Vector<char> encode(int64_t databaseId, int64_t objectStoreId, int64_t indexId, unsigned char metaDataType)
    {
        KeyPrefix prefix(databaseId, 0, 0);
        Vector<char> ret = prefix.encode();
        ret.append(encodeByte(kIndexMetaDataTypeByte));
        ret.append(encodeVarInt(objectStoreId));
        ret.append(encodeVarInt(indexId));
        ret.append(encodeByte(metaDataType));
        return ret;
    }

    int compare(const IndexMetaDataKey& other)
    {
        ASSERT(m_objectStoreId >= 0);
        ASSERT(m_indexId >= 0);

        if (int x = m_objectStoreId - other.m_objectStoreId)
            return x;
        if (int x = m_indexId - other.m_indexId)
            return x;
        return m_metaDataType - other.m_metaDataType;
    }

    int64_t indexId() const
    {
        ASSERT(m_indexId >= 0);
        return m_indexId;
    }

    unsigned char metaDataType() const { return m_metaDataType; }

private:
    int64_t m_objectStoreId;
    int64_t m_indexId;
    unsigned char m_metaDataType;
};

class ObjectStoreFreeListKey {
public:
    ObjectStoreFreeListKey()
        : m_objectStoreId(-1)
    {
    }

    static const char* decode(const char* start, const char* limit, ObjectStoreFreeListKey* result)
    {
        KeyPrefix prefix;
        const char *p = KeyPrefix::decode(start, limit, &prefix);
        if (!p)
            return 0;
        ASSERT(prefix.m_databaseId);
        ASSERT(!prefix.m_objectStoreId);
        ASSERT(!prefix.m_indexId);
        if (p == limit)
            return 0;
        unsigned char typeByte = *p++;
        ASSERT_UNUSED(typeByte, typeByte == kObjectStoreFreeListTypeByte);
        if (p == limit)
            return 0;
        return decodeVarInt(p, limit, result->m_objectStoreId);
    }

    static Vector<char> encode(int64_t databaseId, int64_t objectStoreId)
    {
        KeyPrefix prefix(databaseId, 0, 0);
        Vector<char> ret = prefix.encode();
        ret.append(encodeByte(kObjectStoreFreeListTypeByte));
        ret.append(encodeVarInt(objectStoreId));
        return ret;
    }

    int64_t objectStoreId() const
    {
        ASSERT(m_objectStoreId >= 0);
        return m_objectStoreId;
    }

    int compare(const ObjectStoreFreeListKey& other)
    {
        // FIXME: It may seem strange that we're not comparing database id's,
        // but that comparison will have been made earlier.
        // We should probably make this more clear, though...
        ASSERT(m_objectStoreId >= 0);
        return m_objectStoreId - other.m_objectStoreId;
    }

private:
    int64_t m_objectStoreId;
};

class IndexFreeListKey {
public:
    IndexFreeListKey()
        : m_objectStoreId(-1)
        , m_indexId(-1)
    {
    }

    static const char* decode(const char* start, const char* limit, IndexFreeListKey* result)
    {
        KeyPrefix prefix;
        const char* p = KeyPrefix::decode(start, limit, &prefix);
        if (!p)
            return 0;
        ASSERT(prefix.m_databaseId);
        ASSERT(!prefix.m_objectStoreId);
        ASSERT(!prefix.m_indexId);
        if (p == limit)
            return 0;
        unsigned char typeByte = *p++;
        ASSERT_UNUSED(typeByte, typeByte == kIndexFreeListTypeByte);
        if (p == limit)
            return 0;
        p = decodeVarInt(p, limit, result->m_objectStoreId);
        if (!p)
            return 0;
        return decodeVarInt(p, limit, result->m_indexId);
    }

    static Vector<char> encode(int64_t databaseId, int64_t objectStoreId, int64_t indexId)
    {
        KeyPrefix prefix(databaseId, 0, 0);
        Vector<char> ret = prefix.encode();
        ret.append(encodeByte(kIndexFreeListTypeByte));
        ret.append(encodeVarInt(objectStoreId));
        ret.append(encodeVarInt(indexId));
        return ret;
    }

    int compare(const IndexFreeListKey& other)
    {
        ASSERT(m_objectStoreId >= 0);
        ASSERT(m_indexId >= 0);
        if (int x = m_objectStoreId - other.m_objectStoreId)
            return x;
        return m_indexId - other.m_indexId;
    }

    int64_t objectStoreId() const
    {
        ASSERT(m_objectStoreId >= 0);
        return m_objectStoreId;
    }

    int64_t indexId() const
    {
        ASSERT(m_indexId >= 0);
        return m_indexId;
    }

private:
    int64_t m_objectStoreId;
    int64_t m_indexId;
};

class ObjectStoreNamesKey {
public:
    // FIXME: We never use this to look up object store ids, because a mapping
    // is kept in the IDBDatabaseBackendImpl. Can the mapping become unreliable?
    // Can we remove this?
    static const char* decode(const char* start, const char* limit, ObjectStoreNamesKey* result)
    {
        KeyPrefix prefix;
        const char* p = KeyPrefix::decode(start, limit, &prefix);
        if (!p)
            return 0;
        ASSERT(prefix.m_databaseId);
        ASSERT(!prefix.m_objectStoreId);
        ASSERT(!prefix.m_indexId);
        if (p == limit)
            return 0;
        unsigned char typeByte = *p++;
        ASSERT_UNUSED(typeByte, typeByte == kObjectStoreNamesTypeByte);
        return decodeStringWithLength(p, limit, result->m_objectStoreName);
    }

    static Vector<char> encode(int64_t databaseId, const String& objectStoreName)
    {
        KeyPrefix prefix(databaseId, 0, 0);
        Vector<char> ret = prefix.encode();
        ret.append(encodeByte(kSchemaVersionTypeByte));
        ret.append(encodeByte(kObjectStoreNamesTypeByte));
        ret.append(encodeStringWithLength(objectStoreName));
        return ret;
    }

    int compare(const ObjectStoreNamesKey& other)
    {
        return codePointCompare(m_objectStoreName, other.m_objectStoreName);
    }

    String objectStoreName() const { return m_objectStoreName; }

private:
    String m_objectStoreName; // FIXME: Store the encoded string, or just pointers to it.
};

class IndexNamesKey {
public:
    IndexNamesKey()
        : m_objectStoreId(-1)
    {
    }

    // FIXME: We never use this to look up index ids, because a mapping
    // is kept at a higher level.
    static const char* decode(const char* start, const char* limit, IndexNamesKey* result)
    {
        KeyPrefix prefix;
        const char* p = KeyPrefix::decode(start, limit, &prefix);
        if (!p)
            return 0;
        ASSERT(prefix.m_databaseId);
        ASSERT(!prefix.m_objectStoreId);
        ASSERT(!prefix.m_indexId);
        if (p == limit)
            return 0;
        unsigned char typeByte = *p++;
        ASSERT_UNUSED(typeByte, typeByte == kIndexNamesKeyTypeByte);
        if (p == limit)
            return 0;
        p = decodeVarInt(p, limit, result->m_objectStoreId);
        if (!p)
            return 0;
        return decodeStringWithLength(p, limit, result->m_indexName);
    }

    static Vector<char> encode(int64_t databaseId, int64_t objectStoreId, const String& indexName)
    {
        KeyPrefix prefix(databaseId, 0, 0);
        Vector<char> ret = prefix.encode();
        ret.append(encodeByte(kIndexNamesKeyTypeByte));
        ret.append(encodeVarInt(objectStoreId));
        ret.append(encodeStringWithLength(indexName));
        return ret;
    }

    int compare(const IndexNamesKey& other)
    {
        ASSERT(m_objectStoreId >= 0);
        if (int x = m_objectStoreId - other.m_objectStoreId)
            return x;
        return codePointCompare(m_indexName, other.m_indexName);
    }

    String indexName() const { return m_indexName; }

private:
    int64_t m_objectStoreId;
    String m_indexName;
};

class ObjectStoreDataKey {
public:
    static const char* decode(const char* start, const char* end, ObjectStoreDataKey* result)
    {
        KeyPrefix prefix;
        const char* p = KeyPrefix::decode(start, end, &prefix);
        if (!p)
            return 0;
        ASSERT(prefix.m_databaseId);
        ASSERT(prefix.m_objectStoreId);
        ASSERT(prefix.m_indexId == kSpecialIndexNumber);
        if (p == end)
            return 0;
        return extractEncodedIDBKey(p, end, &result->m_encodedUserKey);
    }

    static Vector<char> encode(int64_t databaseId, int64_t objectStoreId, const Vector<char> encodedUserKey)
    {
        KeyPrefix prefix(databaseId, objectStoreId, kSpecialIndexNumber);
        Vector<char> ret = prefix.encode();
        ret.append(encodedUserKey);

        return ret;
    }

    static Vector<char> encode(int64_t databaseId, int64_t objectStoreId, const IDBKey& userKey)
    {
        return encode(databaseId, objectStoreId, encodeIDBKey(userKey));
    }

    int compare(const ObjectStoreDataKey& other)
    {
        return compareEncodedIDBKeys(m_encodedUserKey, other.m_encodedUserKey);
    }

    PassRefPtr<IDBKey> userKey() const
    {
        RefPtr<IDBKey> key;
        decodeIDBKey(m_encodedUserKey.begin(), m_encodedUserKey.end(), key);
        return key;
    }

    static const int64_t kSpecialIndexNumber = kObjectStoreDataIndexId;

private:
    Vector<char> m_encodedUserKey;
};

class ExistsEntryKey {
public:
    static const char* decode(const char* start, const char* end, ExistsEntryKey* result)
    {
        KeyPrefix prefix;
        const char* p = KeyPrefix::decode(start, end, &prefix);
        if (!p)
            return 0;
        ASSERT(prefix.m_databaseId);
        ASSERT(prefix.m_objectStoreId);
        ASSERT(prefix.m_indexId == kSpecialIndexNumber);
        if (p == end)
            return 0;
        return extractEncodedIDBKey(p, end, &result->m_encodedUserKey);
    }

    static Vector<char> encode(int64_t databaseId, int64_t objectStoreId, const Vector<char>& encodedKey)
    {
        KeyPrefix prefix(databaseId, objectStoreId, kSpecialIndexNumber);
        Vector<char> ret = prefix.encode();
        ret.append(encodedKey);
        return ret;
    }

    static Vector<char> encode(int64_t databaseId, int64_t objectStoreId, const IDBKey& userKey)
    {
        return encode(databaseId, objectStoreId, encodeIDBKey(userKey));
    }

    int compare(const ExistsEntryKey& other)
    {
        return compareEncodedIDBKeys(m_encodedUserKey, other.m_encodedUserKey);
    }

    PassRefPtr<IDBKey> userKey() const
    {
        RefPtr<IDBKey> key;
        decodeIDBKey(m_encodedUserKey.begin(), m_encodedUserKey.end(), key);
        return key;
    }

    static const int64_t kSpecialIndexNumber = kExistsEntryIndexId;

private:
    Vector<char> m_encodedUserKey;
};

class IndexDataKey {
public:
    IndexDataKey()
        : m_databaseId(-1)
        , m_objectStoreId(-1)
        , m_indexId(-1)
        , m_sequenceNumber(-1)
    {
    }

    static const char* decode(const char* start, const char* limit, IndexDataKey* result)
    {
        KeyPrefix prefix;
        const char* p = KeyPrefix::decode(start, limit, &prefix);
        if (!p)
            return 0;
        ASSERT(prefix.m_databaseId);
        ASSERT(prefix.m_objectStoreId);
        ASSERT(prefix.m_indexId >= kMinimumIndexId);
        result->m_databaseId = prefix.m_databaseId;
        result->m_objectStoreId = prefix.m_objectStoreId;
        result->m_indexId = prefix.m_indexId;
        p = extractEncodedIDBKey(p, limit, &result->m_encodedUserKey);
        if (!p)
            return 0;
        if (p == limit) {
            result->m_sequenceNumber = -1; // FIXME: We should change it so that all keys have a sequence number. Shouldn't need to handle this case.
            return p;
        }
        return decodeVarInt(p, limit, result->m_sequenceNumber);
    }

    static Vector<char> encode(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const Vector<char>& encodedUserKey, int64_t sequenceNumber)
    {
        KeyPrefix prefix(databaseId, objectStoreId, indexId);
        Vector<char> ret = prefix.encode();
        ret.append(encodedUserKey);
        ret.append(encodeVarInt(sequenceNumber));
        return ret;
    }

    static Vector<char> encode(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& userKey, int64_t sequenceNumber)
    {
        return encode(databaseId, objectStoreId, indexId, encodeIDBKey(userKey), sequenceNumber);
    }

    static Vector<char> encodeMaxKey(int64_t databaseId, int64_t objectStoreId)
    {
        return encode(databaseId, objectStoreId, INT32_MAX, maxIDBKey(), INT64_MAX);
    }

    int compare(const IndexDataKey& other, bool ignoreSequenceNumber)
    {
        ASSERT(m_databaseId >= 0);
        ASSERT(m_objectStoreId >= 0);
        ASSERT(m_indexId >= 0);
        if (int x = compareEncodedIDBKeys(m_encodedUserKey, other.m_encodedUserKey))
            return x;
        if (ignoreSequenceNumber)
            return 0;
        return m_sequenceNumber - other.m_sequenceNumber;
    }

    int64_t databaseId() const
    {
        ASSERT(m_databaseId >= 0);
        return m_databaseId;
    }

    int64_t objectStoreId() const
    {
        ASSERT(m_objectStoreId >= 0);
        return m_objectStoreId;
    }

    int64_t indexId() const
    {
        ASSERT(m_indexId >= 0);
        return m_indexId;
    }

    PassRefPtr<IDBKey> userKey() const
    {
        RefPtr<IDBKey> key;
        decodeIDBKey(m_encodedUserKey.begin(), m_encodedUserKey.end(), key);
        return key;
    }

private:
    int64_t m_databaseId;
    int64_t m_objectStoreId;
    int64_t m_indexId;
    Vector<char> m_encodedUserKey;
    int64_t m_sequenceNumber;
};

namespace {
template<typename KeyType>
int decodeAndCompare(const LevelDBSlice& a, const LevelDBSlice& b)
{
    KeyType keyA;
    KeyType keyB;

    const char* ptrA = KeyType::decode(a.begin(), a.end(), &keyA);
    ASSERT_UNUSED(ptrA, ptrA);
    const char* ptrB = KeyType::decode(b.begin(), b.end(), &keyB);
    ASSERT_UNUSED(ptrB, ptrB);

    return keyA.compare(keyB);
}
}

static int realCompare(const LevelDBSlice& a, const LevelDBSlice& b, bool indexKeys = false)
{
    const char* ptrA = a.begin();
    const char* ptrB = b.begin();
    const char* endA = a.end();
    const char* endB = b.end();

    KeyPrefix prefixA;
    KeyPrefix prefixB;

    ptrA = KeyPrefix::decode(ptrA, endA, &prefixA);
    ptrB = KeyPrefix::decode(ptrB, endB, &prefixB);
    ASSERT(ptrA);
    ASSERT(ptrB);

    if (int x = prefixA.compare(prefixB))
        return x;

    if (prefixA.type() == KeyPrefix::kGlobalMetaData) {
        ASSERT(ptrA != endA);
        ASSERT(ptrB != endB);

        unsigned char typeByteA = *ptrA++;
        unsigned char typeByteB = *ptrB++;

        if (int x = typeByteA - typeByteB)
            return x;

        if (typeByteA <= 1)
            return 0;
        if (typeByteA == kDatabaseFreeListTypeByte)
            return decodeAndCompare<DatabaseFreeListKey>(a, b);
        if (typeByteA == kDatabaseNameTypeByte)
            return decodeAndCompare<DatabaseNameKey>(a, b);
    }

    if (prefixA.type() == KeyPrefix::kDatabaseMetaData) {
        ASSERT(ptrA != endA);
        ASSERT(ptrB != endB);

        unsigned char typeByteA = *ptrA++;
        unsigned char typeByteB = *ptrB++;

        if (int x = typeByteA - typeByteB)
            return x;

        if (typeByteA <= 3)
            return 0;

        if (typeByteA == kObjectStoreMetaDataTypeByte)
            return decodeAndCompare<ObjectStoreMetaDataKey>(a, b);
        if (typeByteA == kIndexMetaDataTypeByte)
            return decodeAndCompare<IndexMetaDataKey>(a, b);
        if (typeByteA == kObjectStoreFreeListTypeByte)
            return decodeAndCompare<ObjectStoreFreeListKey>(a, b);
        if (typeByteA == kIndexFreeListTypeByte)
            return decodeAndCompare<IndexFreeListKey>(a, b);
        if (typeByteA == kObjectStoreNamesTypeByte)
            return decodeAndCompare<ObjectStoreNamesKey>(a, b);
        if (typeByteA == kIndexNamesKeyTypeByte)
            return decodeAndCompare<IndexNamesKey>(a, b);

        return 0;
        ASSERT_NOT_REACHED();
    }

    if (prefixA.type() == KeyPrefix::kObjectStoreData) {
        if (ptrA == endA && ptrB == endB)
            return 0;
        if (ptrA == endA)
            return -1;
        if (ptrB == endB)
            return 1; // FIXME: This case of non-existing user keys should not have to be handled this way.

        return decodeAndCompare<ObjectStoreDataKey>(a, b);
    }
    if (prefixA.type() == KeyPrefix::kExistsEntry) {
        if (ptrA == endA && ptrB == endB)
            return 0;
        if (ptrA == endA)
            return -1;
        if (ptrB == endB)
            return 1; // FIXME: This case of non-existing user keys should not have to be handled this way.

        return decodeAndCompare<ExistsEntryKey>(a, b);
    }
    if (prefixA.type() == KeyPrefix::kIndexData) {
        if (ptrA == endA && ptrB == endB)
            return 0;
        if (ptrA == endA)
            return -1;
        if (ptrB == endB)
            return 1; // FIXME: This case of non-existing user keys should not have to be handled this way.

        IndexDataKey indexDataKeyA;
        IndexDataKey indexDataKeyB;

        ptrA = IndexDataKey::decode(a.begin(), endA, &indexDataKeyA);
        ptrB = IndexDataKey::decode(b.begin(), endB, &indexDataKeyB);
        ASSERT(ptrA);
        ASSERT(ptrB);

        bool ignoreSequenceNumber = indexKeys;
        return indexDataKeyA.compare(indexDataKeyB, ignoreSequenceNumber);
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static int compareKeys(const LevelDBSlice& a, const LevelDBSlice& b)
{
    return realCompare(a, b);
}

static int compareIndexKeys(const LevelDBSlice& a, const LevelDBSlice& b)
{
    return realCompare(a, b, true);
}

class Comparator : public LevelDBComparator {
public:
    virtual int compare(const LevelDBSlice& a, const LevelDBSlice& b) const { return realCompare(a, b); }
    virtual const char* name() const { return "idb_cmp1"; }
};
}

static bool setUpMetadata(LevelDBDatabase* db)
{
    const Vector<char> metaDataKey = SchemaVersionKey::encode();

    int64_t schemaVersion;
    if (!getInt(db, metaDataKey, schemaVersion)) {
        schemaVersion = 0;
        if (!putInt(db, metaDataKey, schemaVersion))
            return false;
    }

    // FIXME: Eventually, we'll need to be able to transition between schemas.
    if (schemaVersion)
        return false; // Don't know what to do with this version.

    return true;
}

IDBLevelDBBackingStore::IDBLevelDBBackingStore(String identifier, IDBFactoryBackendImpl* factory, LevelDBDatabase* db)
    : m_identifier(identifier)
    , m_factory(factory)
    , m_db(db)
{
    m_factory->addIDBBackingStore(identifier, this);
}

IDBLevelDBBackingStore::~IDBLevelDBBackingStore()
{
    m_factory->removeIDBBackingStore(m_identifier);
}

PassRefPtr<IDBBackingStore> IDBLevelDBBackingStore::open(SecurityOrigin* securityOrigin, const String& pathBaseArg, int64_t maximumSize, const String& fileIdentifier, IDBFactoryBackendImpl* factory)
{
    String pathBase = pathBaseArg;

    if (pathBase.isEmpty()) {
        ASSERT_NOT_REACHED(); // FIXME: We need to handle this case for incognito and DumpRenderTree.
        return 0;
    }

    if (!makeAllDirectories(pathBase)) {
        LOG_ERROR("Unable to create IndexedDB database path %s", pathBase.utf8().data());
        return 0;
    }
    // FIXME: We should eventually use the same LevelDB database for all origins.
    String path = pathByAppendingComponent(pathBase, securityOrigin->databaseIdentifier() + ".indexeddb.leveldb");

    OwnPtr<LevelDBComparator> comparator(new Comparator());
    LevelDBDatabase* db = LevelDBDatabase::open(path, comparator.get());
    if (!db)
        return 0;

    // FIXME: Handle comparator name changes.

    RefPtr<IDBLevelDBBackingStore> backingStore(adoptRef(new IDBLevelDBBackingStore(fileIdentifier, factory, db)));
    backingStore->m_comparator = comparator.release();

    if (!setUpMetadata(backingStore->m_db.get()))
        return 0;

    return backingStore.release();
}

bool IDBLevelDBBackingStore::extractIDBDatabaseMetaData(const String& name, String& foundVersion, int64_t& foundId)
{
    const Vector<char> key = DatabaseNameKey::encode(m_identifier, name);

    bool ok = getInt(m_db.get(), key, foundId);
    if (!ok)
        return false;

    ok = getString(m_db.get(), DatabaseMetaDataKey::encode(foundId, DatabaseMetaDataKey::kUserVersion), foundVersion);
    if (!ok)
        return false;

    return true;
}

static int64_t getNewDatabaseId(LevelDBDatabase* db)
{
    const Vector<char> freeListStartKey = DatabaseFreeListKey::encode(0);
    const Vector<char> freeListStopKey = DatabaseFreeListKey::encode(INT64_MAX);

    OwnPtr<LevelDBIterator> it(db->newIterator());
    for (it->seek(freeListStartKey); it->isValid() && compareKeys(it->key(), freeListStopKey) < 0; it->next()) {
        const char *p = it->key().begin();
        const char *limit = it->key().end();

        DatabaseFreeListKey freeListKey;
        p = DatabaseFreeListKey::decode(p, limit, &freeListKey);
        ASSERT(p);

        bool ok = db->remove(it->key());
        ASSERT_UNUSED(ok, ok);

        return freeListKey.databaseId();
    }

    // If we got here, there was no free-list.
    int64_t maxDatabaseId = -1;
    if (!getInt(db, MaxDatabaseIdKey::encode(), maxDatabaseId))
        maxDatabaseId = 0;

    ASSERT(maxDatabaseId >= 0);

    int64_t databaseId = maxDatabaseId + 1;
    bool ok = putInt(db, MaxDatabaseIdKey::encode(), databaseId);
    ASSERT_UNUSED(ok, ok);

    return databaseId;

}

bool IDBLevelDBBackingStore::setIDBDatabaseMetaData(const String& name, const String& version, int64_t& rowId, bool invalidRowId)
{
    if (invalidRowId) {
        rowId = getNewDatabaseId(m_db.get());

        const Vector<char> key = DatabaseNameKey::encode(m_identifier, name);
        if (!putInt(m_db.get(), key, rowId))
            return false;
    }

    if (!putString(m_db.get(), DatabaseMetaDataKey::encode(rowId, DatabaseMetaDataKey::kUserVersion), version))
        return false;

    return true;
}

void IDBLevelDBBackingStore::getObjectStores(int64_t databaseId, Vector<int64_t>& foundIds, Vector<String>& foundNames, Vector<String>& foundKeyPaths, Vector<bool>& foundAutoIncrementFlags)
{
    const Vector<char> startKey = ObjectStoreMetaDataKey::encode(databaseId, 1, 0);
    const Vector<char> stopKey = ObjectStoreMetaDataKey::encode(databaseId, INT64_MAX, 0);

    OwnPtr<LevelDBIterator> it(m_db->newIterator());
    for (it->seek(startKey); it->isValid() && compareKeys(it->key(), stopKey) < 0; it->next()) {
        const char *p = it->key().begin();
        const char *limit = it->key().end();

        ObjectStoreMetaDataKey metaDataKey;
        p = ObjectStoreMetaDataKey::decode(p, limit, &metaDataKey);
        ASSERT(p);

        int64_t objectStoreId = metaDataKey.objectStoreId();

        String objectStoreName = decodeString(it->value().begin(), it->value().end());

        it->next();
        if (!it->isValid()) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }
        String keyPath = decodeString(it->value().begin(), it->value().end());

        it->next();
        if (!it->isValid()) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }
        bool autoIncrement = *it->value().begin();

        it->next(); // Is evicatble.
        if (!it->isValid()) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }

        it->next(); // Last version.
        if (!it->isValid()) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }

        it->next(); // Maxium index id allocated.
        if (!it->isValid()) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }

        foundIds.append(objectStoreId);
        foundNames.append(objectStoreName);
        foundKeyPaths.append(keyPath);
        foundAutoIncrementFlags.append(autoIncrement);
    }
}

static int64_t getNewObjectStoreId(LevelDBDatabase* db, int64_t databaseId)
{
    const Vector<char> freeListStartKey = ObjectStoreFreeListKey::encode(databaseId, 0);
    const Vector<char> freeListStopKey = ObjectStoreFreeListKey::encode(databaseId, INT64_MAX);

    OwnPtr<LevelDBIterator> it(db->newIterator());
    for (it->seek(freeListStartKey); it->isValid() && compareKeys(it->key(), freeListStopKey) < 0; it->next()) {
        const char* p = it->key().begin();
        const char* limit = it->key().end();

        ObjectStoreFreeListKey freeListKey;
        p = ObjectStoreFreeListKey::decode(p, limit, &freeListKey);
        ASSERT(p);

        bool ok = db->remove(it->key());
        ASSERT_UNUSED(ok, ok);

        return freeListKey.objectStoreId();
    }

    int64_t maxObjectStoreId;
    const Vector<char> maxObjectStoreIdKey = DatabaseMetaDataKey::encode(databaseId, DatabaseMetaDataKey::kMaxObjectStoreId);
    if (!getInt(db, maxObjectStoreIdKey, maxObjectStoreId))
        maxObjectStoreId = 0;

    int64_t objectStoreId = maxObjectStoreId + 1;
    bool ok = putInt(db, maxObjectStoreIdKey, objectStoreId);
    ASSERT_UNUSED(ok, ok);

    return objectStoreId;
}

bool IDBLevelDBBackingStore::createObjectStore(int64_t databaseId, const String& name, const String& keyPath, bool autoIncrement, int64_t& assignedObjectStoreId)
{
    int64_t objectStoreId = getNewObjectStoreId(m_db.get(), databaseId);

    const Vector<char> nameKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 0);
    const Vector<char> keyPathKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 1);
    const Vector<char> autoIncrementKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 2);
    const Vector<char> evictableKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 3);
    const Vector<char> lastVersionKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 4);
    const Vector<char> maxIndexIdKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 5);
    const Vector<char> namesKey = ObjectStoreNamesKey::encode(databaseId, name);

    bool ok = putString(m_db.get(), nameKey, name);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putString(m_db.get(), keyPathKey, keyPath);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putInt(m_db.get(), autoIncrementKey, autoIncrement);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putInt(m_db.get(), evictableKey, false);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putInt(m_db.get(), lastVersionKey, 1);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putInt(m_db.get(), maxIndexIdKey, kMinimumIndexId);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putInt(m_db.get(), namesKey, objectStoreId);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    assignedObjectStoreId = objectStoreId;

    return true;
}

static bool deleteRange(LevelDBDatabase* db, const Vector<char>& begin, const Vector<char>& end)
{
    // FIXME: LevelDB may be able to provide a bulk operation that we can do first.
    OwnPtr<LevelDBIterator> it(db->newIterator());
    for (it->seek(begin); it->isValid() && compareKeys(it->key(), end) < 0; it->next()) {
        if (!db->remove(it->key()))
            return false;
    }

    return true;
}

void IDBLevelDBBackingStore::deleteObjectStore(int64_t databaseId, int64_t objectStoreId)
{
    String objectStoreName;
    getString(m_db.get(), ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 0), objectStoreName);

    if (!deleteRange(m_db.get(), ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 0), ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 6)))
        return; // FIXME: Report error.

    putString(m_db.get(), ObjectStoreFreeListKey::encode(databaseId, objectStoreId), "");
    m_db->remove(ObjectStoreNamesKey::encode(databaseId, objectStoreName));

    if (!deleteRange(m_db.get(), IndexFreeListKey::encode(databaseId, objectStoreId, 0), IndexFreeListKey::encode(databaseId, objectStoreId, INT64_MAX)))
        return; // FIXME: Report error.
    if (!deleteRange(m_db.get(), IndexMetaDataKey::encode(databaseId, objectStoreId, 0, 0), IndexMetaDataKey::encode(databaseId, objectStoreId, INT64_MAX, 0)))
        return; // FIXME: Report error.

    clearObjectStore(databaseId, objectStoreId);
}

String IDBLevelDBBackingStore::getObjectStoreRecord(int64_t databaseId, int64_t objectStoreId, const IDBKey& key)
{
    const Vector<char> leveldbKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, key);
    Vector<char> data;

    if (!m_db->get(leveldbKey, data))
        return String();

    int64_t version;
    const char* p = decodeVarInt(data.begin(), data.end(), version);
    if (!p)
        return String();
    (void) version;

    return decodeString(p, data.end());
}

namespace {
class LevelDBRecordIdentifier : public IDBBackingStore::ObjectStoreRecordIdentifier {
public:
    static PassRefPtr<LevelDBRecordIdentifier> create(const Vector<char>& primaryKey, int64_t version) { return adoptRef(new LevelDBRecordIdentifier(primaryKey, version)); }
    static PassRefPtr<LevelDBRecordIdentifier> create() { return adoptRef(new LevelDBRecordIdentifier()); }

    virtual bool isValid() const { return m_primaryKey.isEmpty(); }
    Vector<char> primaryKey() const { return m_primaryKey; }
    void setPrimaryKey(const Vector<char>& primaryKey) { m_primaryKey = primaryKey; }
    int64_t version() const { return m_version; }
    void setVersion(int64_t version) { m_version = version; }

private:
    LevelDBRecordIdentifier(const Vector<char>& primaryKey, int64_t version) : m_primaryKey(primaryKey), m_version(version) { ASSERT(!primaryKey.isEmpty()); }
    LevelDBRecordIdentifier() : m_primaryKey(), m_version(-1) {}

    Vector<char> m_primaryKey; // FIXME: Make it more clear that this is the *encoded* version of the key.
    int64_t m_version;
};
}

static int64_t getNewVersionNumber(LevelDBDatabase* db, int64_t databaseId, int64_t objectStoreId)
{
    const Vector<char> lastVersionKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 4);

    int64_t lastVersion = -1;
    if (!getInt(db, lastVersionKey, lastVersion))
        lastVersion = 0;

    ASSERT(lastVersion >= 0);

    int64_t version = lastVersion + 1;
    bool ok = putInt(db, lastVersionKey, version);
    ASSERT_UNUSED(ok, ok);

    ASSERT(version > lastVersion); // FIXME: Think about how we want to handle the overflow scenario.

    return version;
}

bool IDBLevelDBBackingStore::putObjectStoreRecord(int64_t databaseId, int64_t objectStoreId, const IDBKey& key, const String& value, ObjectStoreRecordIdentifier* recordIdentifier)
{
    int64_t version = getNewVersionNumber(m_db.get(), databaseId, objectStoreId);
    const Vector<char> objectStoredataKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, key);

    Vector<char> v;
    v.append(encodeVarInt(version));
    v.append(encodeString(value));

    if (!m_db->put(objectStoredataKey, v))
        return false;

    const Vector<char> existsEntryKey = ExistsEntryKey::encode(databaseId, objectStoreId, key);
    if (!m_db->put(existsEntryKey, encodeInt(version)))
        return false;

    LevelDBRecordIdentifier* levelDBRecordIdentifier = static_cast<LevelDBRecordIdentifier*>(recordIdentifier);
    levelDBRecordIdentifier->setPrimaryKey(encodeIDBKey(key));
    levelDBRecordIdentifier->setVersion(version);
    return true;
}

void IDBLevelDBBackingStore::clearObjectStore(int64_t databaseId, int64_t objectStoreId)
{
    const Vector<char> startKey = KeyPrefix(databaseId, objectStoreId, 0).encode();
    const Vector<char> stopKey = KeyPrefix(databaseId, objectStoreId + 1, 0).encode();

    deleteRange(m_db.get(), startKey, stopKey);
}

PassRefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> IDBLevelDBBackingStore::createInvalidRecordIdentifier()
{
    return LevelDBRecordIdentifier::create();
}

void IDBLevelDBBackingStore::deleteObjectStoreRecord(int64_t databaseId, int64_t objectStoreId, const ObjectStoreRecordIdentifier* recordIdentifier)
{
    const LevelDBRecordIdentifier* levelDBRecordIdentifier = static_cast<const LevelDBRecordIdentifier*>(recordIdentifier);
    const Vector<char> key = ObjectStoreDataKey::encode(databaseId, objectStoreId, levelDBRecordIdentifier->primaryKey());
    m_db->remove(key);
}

double IDBLevelDBBackingStore::nextAutoIncrementNumber(int64_t databaseId, int64_t objectStoreId)
{
    const Vector<char> startKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, minIDBKey());
    const Vector<char> stopKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, maxIDBKey());

    OwnPtr<LevelDBIterator> it(m_db->newIterator());

    int maxNumericKey = 0;

    // FIXME: Be more efficient: seek to something after the object store data, then reverse.

    for (it->seek(startKey); it->isValid() && compareKeys(it->key(), stopKey) < 0; it->next()) {
        const char *p = it->key().begin();
        const char *limit = it->key().end();

        ObjectStoreDataKey dataKey;
        p = ObjectStoreDataKey::decode(p, limit, &dataKey);
        ASSERT(p);

        if (dataKey.userKey()->type() == IDBKey::NumberType) {
            int64_t n = static_cast<int64_t>(dataKey.userKey()->number());
            if (n > maxNumericKey)
                maxNumericKey = n;
        }
    }

    return maxNumericKey + 1;
}

bool IDBLevelDBBackingStore::keyExistsInObjectStore(int64_t databaseId, int64_t objectStoreId, const IDBKey& key, ObjectStoreRecordIdentifier* foundRecordIdentifier)
{
    const Vector<char> leveldbKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, key);
    Vector<char> data;

    if (!m_db->get(leveldbKey, data))
        return false;

    int64_t version;
    if (!decodeVarInt(data.begin(), data.end(), version))
        return false;

    LevelDBRecordIdentifier* levelDBRecordIdentifier = static_cast<LevelDBRecordIdentifier*>(foundRecordIdentifier);
    levelDBRecordIdentifier->setPrimaryKey(encodeIDBKey(key));
    levelDBRecordIdentifier->setVersion(version);
    return true;
}

bool IDBLevelDBBackingStore::forEachObjectStoreRecord(int64_t databaseId, int64_t objectStoreId, ObjectStoreRecordCallback& callback)
{
    const Vector<char> startKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, minIDBKey());
    const Vector<char> stopKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, maxIDBKey());

    OwnPtr<LevelDBIterator> it(m_db->newIterator());
    for (it->seek(startKey); it->isValid() && compareKeys(it->key(), stopKey) < 0; it->next()) {
        const char *p = it->key().begin();
        const char *limit = it->key().end();
        ObjectStoreDataKey dataKey;
        p = ObjectStoreDataKey::decode(p, limit, &dataKey);
        ASSERT(p);

        RefPtr<IDBKey> primaryKey = dataKey.userKey();

        int64_t version;
        const char* q = decodeVarInt(it->value().begin(), it->value().end(), version);
        if (!q)
            return false;

        RefPtr<LevelDBRecordIdentifier> ri = LevelDBRecordIdentifier::create(encodeIDBKey(*primaryKey), version);
        String idbValue = decodeString(q, it->value().end());

        callback.callback(ri.get(), idbValue);
    }

    return true;
}

void IDBLevelDBBackingStore::getIndexes(int64_t databaseId, int64_t objectStoreId, Vector<int64_t>& foundIds, Vector<String>& foundNames, Vector<String>& foundKeyPaths, Vector<bool>& foundUniqueFlags)
{
    const Vector<char> startKey = IndexMetaDataKey::encode(databaseId, objectStoreId, 0, 0);
    const Vector<char> stopKey = IndexMetaDataKey::encode(databaseId, objectStoreId + 1, 0, 0);

    OwnPtr<LevelDBIterator> it(m_db->newIterator());
    for (it->seek(startKey); it->isValid() && compareKeys(it->key(), stopKey) < 0; it->next()) {
        const char* p = it->key().begin();
        const char* limit = it->key().end();

        IndexMetaDataKey metaDataKey;
        p = IndexMetaDataKey::decode(p, limit, &metaDataKey);
        ASSERT(p);

        int64_t indexId = metaDataKey.indexId();
        ASSERT(!metaDataKey.metaDataType());

        String indexName = decodeString(it->value().begin(), it->value().end());
        it->next();
        if (!it->isValid()) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }

        bool indexUnique = *it->value().begin();
        it->next();
        if (!it->isValid()) {
            LOG_ERROR("Internal Indexed DB error.");
            return;
        }

        String keyPath = decodeString(it->value().begin(), it->value().end());

        foundIds.append(indexId);
        foundNames.append(indexName);
        foundKeyPaths.append(keyPath);
        foundUniqueFlags.append(indexUnique);
    }
}

static int64_t getNewIndexId(LevelDBDatabase* db, int64_t databaseId, int64_t objectStoreId)
{
    const Vector<char> startKey = IndexFreeListKey::encode(databaseId, objectStoreId, 0);
    const Vector<char> stopKey = IndexFreeListKey::encode(databaseId, objectStoreId, INT64_MAX);

    OwnPtr<LevelDBIterator> it(db->newIterator());
    for (it->seek(startKey); it->isValid() && compareKeys(it->key(), stopKey) < 0; it->next()) {
        const char* p = it->key().begin();
        const char* limit = it->key().end();

        IndexFreeListKey freeListKey;
        p = IndexFreeListKey::decode(p, limit, &freeListKey);
        ASSERT(p);

        bool ok = db->remove(it->key());
        ASSERT_UNUSED(ok, ok);

        ASSERT(freeListKey.indexId() >= kMinimumIndexId);
        return freeListKey.indexId();
    }

    int64_t maxIndexId;
    const Vector<char> maxIndexIdKey = ObjectStoreMetaDataKey::encode(databaseId, objectStoreId, 5);
    if (!getInt(db, maxIndexIdKey, maxIndexId))
        maxIndexId = kMinimumIndexId;

    int64_t indexId = maxIndexId + 1;
    bool ok = putInt(db, maxIndexIdKey, indexId);
    if (!ok)
        return false;

    return indexId;
}

bool IDBLevelDBBackingStore::createIndex(int64_t databaseId, int64_t objectStoreId, const String& name, const String& keyPath, bool isUnique, int64_t& indexId)
{
    indexId = getNewIndexId(m_db.get(), databaseId, objectStoreId);

    const Vector<char> nameKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, 0);
    const Vector<char> uniqueKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, 1);
    const Vector<char> keyPathKey = IndexMetaDataKey::encode(databaseId, objectStoreId, indexId, 2);

    bool ok = putString(m_db.get(), nameKey, name);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putInt(m_db.get(), uniqueKey, isUnique);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    ok = putString(m_db.get(), keyPathKey, keyPath);
    if (!ok) {
        LOG_ERROR("Internal Indexed DB error.");
        return false;
    }

    return true;
}

void IDBLevelDBBackingStore::deleteIndex(int64_t, int64_t, int64_t)
{
    ASSERT_NOT_REACHED(); // FIXME: Implement and add layout test.
    return;
}

bool IDBLevelDBBackingStore::putIndexDataForRecord(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& key, const ObjectStoreRecordIdentifier* recordIdentifier)
{
    ASSERT(indexId >= kMinimumIndexId);
    const LevelDBRecordIdentifier* levelDBRecordIdentifier = static_cast<const LevelDBRecordIdentifier*>(recordIdentifier);

    const int64_t globalSequenceNumber = getNewVersionNumber(m_db.get(), databaseId, objectStoreId);
    const Vector<char> indexDataKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, key, globalSequenceNumber);

    Vector<char> data;
    data.append(encodeVarInt(levelDBRecordIdentifier->version()));
    data.append(levelDBRecordIdentifier->primaryKey());

    return m_db->put(indexDataKey, data);
}

static bool findGreatestKeyLessThan(LevelDBDatabase* db, const Vector<char>& target, Vector<char>& foundKey)
{
    OwnPtr<LevelDBIterator> it(db->newIterator());
    it->seek(target);

    if (!it->isValid()) {
        it->seekToLast();
        if (!it->isValid())
            return false;
    }

    while (compareIndexKeys(it->key(), target) >= 0) {
        it->prev();
        if (!it->isValid())
            return false;
    }

    foundKey.clear();
    foundKey.append(it->key().begin(), it->key().end() - it->key().begin());
    return true;
}

bool IDBLevelDBBackingStore::deleteIndexDataForRecord(int64_t, int64_t, int64_t, const ObjectStoreRecordIdentifier*)
{
    // FIXME: This isn't needed since we invalidate index data via the version number mechanism.
    return true;
}

String IDBLevelDBBackingStore::getObjectViaIndex(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& key)
{
    RefPtr<IDBKey> primaryKey = getPrimaryKeyViaIndex(databaseId, objectStoreId, indexId, key);
    if (!primaryKey)
        return String();

    return getObjectStoreRecord(databaseId, objectStoreId, *primaryKey);
}

static bool versionExists(LevelDBDatabase* db, int64_t databaseId, int64_t objectStoreId, int64_t version, const Vector<char>& encodedPrimaryKey)
{
    const Vector<char> key = ExistsEntryKey::encode(databaseId, objectStoreId, encodedPrimaryKey);
    Vector<char> data;

    if (!db->get(key, data))
        return false;

    return decodeInt(data.begin(), data.end()) == version;
}

PassRefPtr<IDBKey> IDBLevelDBBackingStore::getPrimaryKeyViaIndex(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& key)
{
    const Vector<char> leveldbKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, key, 0);
    OwnPtr<LevelDBIterator> it(m_db->newIterator());
    it->seek(leveldbKey);

    for (;;) {
        if (!it->isValid())
            return 0;
        if (compareIndexKeys(it->key(), leveldbKey) > 0)
            return 0;

        int64_t version;
        const char* p = decodeVarInt(it->value().begin(), it->value().end(), version);
        if (!p)
            return 0;
        Vector<char> encodedPrimaryKey;
        encodedPrimaryKey.append(p, it->value().end() - p);

        if (!versionExists(m_db.get(), databaseId, objectStoreId, version, encodedPrimaryKey)) {
            // Delete stale index data entry and continue.
            m_db->remove(it->key());
            it->next();
            continue;
        }

        RefPtr<IDBKey> primaryKey;
        decodeIDBKey(encodedPrimaryKey.begin(), encodedPrimaryKey.end(), primaryKey);
        return primaryKey.release();
    }
}

bool IDBLevelDBBackingStore::keyExistsInIndex(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& key)
{
    const Vector<char> levelDBKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, key, 0);
    OwnPtr<LevelDBIterator> it(m_db->newIterator());

    bool found = false;

    it->seek(levelDBKey);
    if (it->isValid() && !compareIndexKeys(it->key(), levelDBKey))
        found = true;

    return found;
}

namespace {
class CursorImplCommon : public IDBBackingStore::Cursor {
public:
    // IDBBackingStore::Cursor
    virtual bool continueFunction(const IDBKey*);
    virtual PassRefPtr<IDBKey> key() { return m_currentKey; }
    virtual PassRefPtr<IDBKey> primaryKey() { return m_currentKey; }
    virtual String value() = 0;
    virtual PassRefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> objectStoreRecordIdentifier() = 0; // FIXME: I don't think this is actually used, so drop it.
    virtual int64_t indexDataId() = 0;

    virtual bool loadCurrentRow() = 0;
    bool firstSeek();

protected:
    CursorImplCommon(LevelDBDatabase* db, const Vector<char>& lowKey, bool lowOpen, const Vector<char>& highKey, bool highOpen, bool forward)
        : m_db(db)
        , m_lowKey(lowKey)
        , m_lowOpen(lowOpen)
        , m_highKey(highKey)
        , m_highOpen(highOpen)
        , m_forward(forward)
    {
    }
    virtual ~CursorImplCommon() {}

    LevelDBDatabase* m_db;
    OwnPtr<LevelDBIterator> m_iterator;
    Vector<char> m_lowKey;
    bool m_lowOpen;
    Vector<char> m_highKey;
    bool m_highOpen;
    bool m_forward;
    RefPtr<IDBKey> m_currentKey;
};

bool CursorImplCommon::firstSeek()
{
    m_iterator = m_db->newIterator();

    if (m_forward)
        m_iterator->seek(m_lowKey);
    else
        m_iterator->seek(m_highKey);

    for (;;) {
        if (!m_iterator->isValid())
            return false;

        if (m_forward && m_highOpen && compareIndexKeys(m_iterator->key(), m_highKey) >= 0)
            return false;
        if (m_forward && !m_highOpen && compareIndexKeys(m_iterator->key(), m_highKey) > 0)
            return false;
        if (!m_forward && m_lowOpen && compareIndexKeys(m_iterator->key(), m_lowKey) <= 0)
            return false;
        if (!m_forward && !m_lowOpen && compareIndexKeys(m_iterator->key(), m_lowKey) < 0)
            return false;

        if (m_forward && m_lowOpen) {
            // lowKey not included in the range.
            if (compareIndexKeys(m_iterator->key(), m_lowKey) <= 0) {
                m_iterator->next();
                continue;
            }
        }
        if (!m_forward && m_highOpen) {
            // highKey not included in the range.
            if (compareIndexKeys(m_iterator->key(), m_highKey) >= 0) {
                m_iterator->prev();
                continue;
            }
        }

        if (!loadCurrentRow()) {
            if (m_forward)
                m_iterator->next();
            else
                m_iterator->prev();
            continue;
        }

        return true;
    }
}

bool CursorImplCommon::continueFunction(const IDBKey* key)
{
    // FIXME: This shares a lot of code with firstSeek.

    for (;;) {
        if (m_forward)
            m_iterator->next();
        else
            m_iterator->prev();

        if (!m_iterator->isValid())
            return false;

        Vector<char> trash;
        if (!m_db->get(m_iterator->key(), trash))
             continue;

        if (m_forward && m_highOpen && compareIndexKeys(m_iterator->key(), m_highKey) >= 0) // high key not included in range
            return false;
        if (m_forward && !m_highOpen && compareIndexKeys(m_iterator->key(), m_highKey) > 0)
            return false;
        if (!m_forward && m_lowOpen && compareIndexKeys(m_iterator->key(), m_lowKey) <= 0) // low key not included in range
            return false;
        if (!m_forward && !m_lowOpen && compareIndexKeys(m_iterator->key(), m_lowKey) < 0)
            return false;

        if (!loadCurrentRow())
            continue;

        if (key) {
            if (m_forward) {
                if (m_currentKey->isLessThan(key))
                    continue;
            } else {
                if (key->isLessThan(m_currentKey.get()))
                    continue;
            }
        }

        // FIXME: Obey the uniqueness constraint (and test for it!)

        break;
    }

    return true;
}

class ObjectStoreCursorImpl : public CursorImplCommon {
public:
    static PassRefPtr<ObjectStoreCursorImpl> create(LevelDBDatabase* db, const Vector<char>& lowKey, bool lowOpen, const Vector<char>& highKey, bool highOpen, bool forward)
    {
        return adoptRef(new ObjectStoreCursorImpl(db, lowKey, lowOpen, highKey, highOpen, forward));
    }

    // CursorImplCommon
    virtual String value() { return m_currentValue; }
    virtual PassRefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> objectStoreRecordIdentifier() { ASSERT_NOT_REACHED(); return 0; }
    virtual int64_t indexDataId() { ASSERT_NOT_REACHED(); return 0; }
    virtual bool loadCurrentRow();

private:
    ObjectStoreCursorImpl(LevelDBDatabase* db, const Vector<char>& lowKey, bool lowOpen, const Vector<char>& highKey, bool highOpen, bool forward)
        : CursorImplCommon(db, lowKey, lowOpen, highKey, highOpen, forward)
    {
    }

    String m_currentValue;
};

bool ObjectStoreCursorImpl::loadCurrentRow()
{
    const char* p = m_iterator->key().begin();
    const char* keyLimit = m_iterator->key().end();

    ObjectStoreDataKey objectStoreDataKey;
    p = ObjectStoreDataKey::decode(p, keyLimit, &objectStoreDataKey);
    ASSERT(p);
    if (!p)
        return false;

    m_currentKey = objectStoreDataKey.userKey();

    int64_t version;
    const char* q = decodeVarInt(m_iterator->value().begin(), m_iterator->value().end(), version);
    ASSERT(q);
    if (!q)
        return false;
    (void) version;

    m_currentValue = decodeString(q, m_iterator->value().end());

    return true;
}

class IndexKeyCursorImpl : public CursorImplCommon {
public:
    static PassRefPtr<IndexKeyCursorImpl> create(LevelDBDatabase* db, const Vector<char>& lowKey, bool lowOpen, const Vector<char>& highKey, bool highOpen, bool forward)
    {
        return adoptRef(new IndexKeyCursorImpl(db, lowKey, lowOpen, highKey, highOpen, forward));
    }

    // CursorImplCommon
    virtual String value() { ASSERT_NOT_REACHED(); return String(); }
    virtual PassRefPtr<IDBKey> primaryKey() { return m_primaryKey; }
    virtual PassRefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> objectStoreRecordIdentifier() { ASSERT_NOT_REACHED(); return 0; }
    virtual int64_t indexDataId() { ASSERT_NOT_REACHED(); return 0; }
    virtual bool loadCurrentRow();

private:
    IndexKeyCursorImpl(LevelDBDatabase* db, const Vector<char>& lowKey, bool lowOpen, const Vector<char>& highKey, bool highOpen, bool forward)
        : CursorImplCommon(db, lowKey, lowOpen, highKey, highOpen, forward)
    {
    }

    RefPtr<IDBKey> m_primaryKey;
};

bool IndexKeyCursorImpl::loadCurrentRow()
{
    const char* p = m_iterator->key().begin();
    const char* keyLimit = m_iterator->key().end();
    IndexDataKey indexDataKey;
    p = IndexDataKey::decode(p, keyLimit, &indexDataKey);

    m_currentKey = indexDataKey.userKey();

    int64_t indexDataVersion;
    const char* q = decodeVarInt(m_iterator->value().begin(), m_iterator->value().end(), indexDataVersion);
    ASSERT(q);
    if (!q)
        return false;

    q = decodeIDBKey(q, m_iterator->value().end(), m_primaryKey);
    ASSERT(q);
    if (!q)
        return false;

    Vector<char> primaryLevelDBKey = ObjectStoreDataKey::encode(indexDataKey.databaseId(), indexDataKey.objectStoreId(), *m_primaryKey);

    Vector<char> result;
    if (!m_db->get(primaryLevelDBKey, result))
        return false;

    int64_t objectStoreDataVersion;
    const char* t = decodeVarInt(result.begin(), result.end(), objectStoreDataVersion);
    ASSERT(t);
    if (!t)
        return false;

    if (objectStoreDataVersion != indexDataVersion) { // FIXME: This is probably not very well covered by the layout tests.
        m_db->remove(m_iterator->key());
        return false;
    }

    return true;
}

class IndexCursorImpl : public CursorImplCommon {
public:
    static PassRefPtr<IndexCursorImpl> create(LevelDBDatabase* db, const Vector<char>& lowKey, bool lowOpen, const Vector<char>& highKey, bool highOpen, bool forward)
    {
        return adoptRef(new IndexCursorImpl(db, lowKey, lowOpen, highKey, highOpen, forward));
    }

    // CursorImplCommon
    virtual String value() { return m_value; }
    virtual PassRefPtr<IDBKey> primaryKey() { return m_primaryKey; }
    virtual PassRefPtr<IDBBackingStore::ObjectStoreRecordIdentifier> objectStoreRecordIdentifier() { ASSERT_NOT_REACHED(); return 0; }
    virtual int64_t indexDataId() { ASSERT_NOT_REACHED(); return 0; }
    bool loadCurrentRow();

private:
    IndexCursorImpl(LevelDBDatabase* db, const Vector<char>& lowKey, bool lowOpen, const Vector<char>& highKey, bool highOpen, bool forward)
        : CursorImplCommon(db, lowKey, lowOpen, highKey, highOpen, forward)
    {
    }

    RefPtr<IDBKey> m_primaryKey;
    String m_value;
    Vector<char> m_primaryLevelDBKey;
};

bool IndexCursorImpl::loadCurrentRow()
{
    const char *p = m_iterator->key().begin();
    const char *limit = m_iterator->key().end();

    IndexDataKey indexDataKey;
    p = IndexDataKey::decode(p, limit, &indexDataKey);

    m_currentKey = indexDataKey.userKey();

    const char *q = m_iterator->value().begin();
    const char *valueLimit = m_iterator->value().end();

    int64_t indexDataVersion;
    q = decodeVarInt(q, valueLimit, indexDataVersion);
    ASSERT(q);
    if (!q)
        return false;
    q = decodeIDBKey(q, valueLimit, m_primaryKey);
    ASSERT(q);
    if (!q)
        return false;

    m_primaryLevelDBKey = ObjectStoreDataKey::encode(indexDataKey.databaseId(), indexDataKey.objectStoreId(), *m_primaryKey);

    Vector<char> result;
    if (!m_db->get(m_primaryLevelDBKey, result))
        return false;

    int64_t objectStoreDataVersion;
    const char* t = decodeVarInt(result.begin(), result.end(), objectStoreDataVersion);
    ASSERT(t);
    if (!t)
        return false;

    if (objectStoreDataVersion != indexDataVersion) {
        m_db->remove(m_iterator->key());
        return false;
    }

    m_value = decodeString(t, result.end());
    return true;
}

}

static bool findLastIndexKeyEqualTo(LevelDBDatabase* db, const Vector<char>& target, Vector<char>& foundKey)
{
    OwnPtr<LevelDBIterator> it(db->newIterator());
    it->seek(target);

    if (!it->isValid())
        return false;

    while (it->isValid() && !compareIndexKeys(it->key(), target)) {
        foundKey.clear();
        foundKey.append(it->key().begin(), it->key().end() - it->key().begin());
        it->next();
    }

    return true;
}

PassRefPtr<IDBBackingStore::Cursor> IDBLevelDBBackingStore::openObjectStoreCursor(int64_t databaseId, int64_t objectStoreId, const IDBKeyRange* range, IDBCursor::Direction direction)
{
    bool lowerBound = range && range->lower();
    bool upperBound = range && range->upper();
    bool forward = (direction == IDBCursor::NEXT_NO_DUPLICATE || direction == IDBCursor::NEXT);

    bool lowerOpen, upperOpen;
    Vector<char> startKey, stopKey;

    if (!lowerBound) {
        startKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, minIDBKey());
        lowerOpen = true; // Not included.
    } else {
        startKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, *range->lower());
        lowerOpen = range->lowerOpen();
    }

    if (!upperBound) {
        stopKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, maxIDBKey());
        upperOpen = true; // Not included.

        if (!forward) { // We need a key that exists.
            if (!findGreatestKeyLessThan(m_db.get(), stopKey, stopKey))
                return 0;
            upperOpen = false;
        }
    } else {
        stopKey = ObjectStoreDataKey::encode(databaseId, objectStoreId, *range->upper());
        upperOpen = range->upperOpen();
    }

    RefPtr<ObjectStoreCursorImpl> cursor = ObjectStoreCursorImpl::create(m_db.get(), startKey, lowerOpen, stopKey, upperOpen, forward);
    if (!cursor->firstSeek())
        return 0;

    return cursor.release();
}

PassRefPtr<IDBBackingStore::Cursor> IDBLevelDBBackingStore::openIndexKeyCursor(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange* range, IDBCursor::Direction direction)
{
    bool lowerBound = range && range->lower();
    bool upperBound = range && range->upper();
    bool forward = (direction == IDBCursor::NEXT_NO_DUPLICATE || direction == IDBCursor::NEXT);

    bool lowerOpen, upperOpen;
    Vector<char> startKey, stopKey;

    if (!lowerBound) {
        startKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, minIDBKey(), 0);
        lowerOpen = false; // Included.
    } else {
        startKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, *range->lower(), 0);
        lowerOpen = range->lowerOpen();
    }

    if (!upperBound) {
        stopKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, maxIDBKey(), 0);
        upperOpen = false; // Included.

        if (!forward) { // We need a key that exists.
            if (!findGreatestKeyLessThan(m_db.get(), stopKey, stopKey))
                return 0;
            upperOpen = false;
        }
    } else {
        stopKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, *range->upper(), 0);
        if (!findLastIndexKeyEqualTo(m_db.get(), stopKey, stopKey)) // Seek to the *last* key in the set of non-unique keys.
            return 0;
        upperOpen = range->upperOpen();
    }

    RefPtr<IndexKeyCursorImpl> cursor = IndexKeyCursorImpl::create(m_db.get(), startKey, lowerOpen, stopKey, upperOpen, forward);
    if (!cursor->firstSeek())
        return 0;

    return cursor.release();
}

PassRefPtr<IDBBackingStore::Cursor> IDBLevelDBBackingStore::openIndexCursor(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange* range, IDBCursor::Direction direction)
{
    bool lowerBound = range && range->lower();
    bool upperBound = range && range->upper();
    bool forward = (direction == IDBCursor::NEXT_NO_DUPLICATE || direction == IDBCursor::NEXT);

    bool lowerOpen, upperOpen;
    Vector<char> startKey, stopKey;

    if (!lowerBound) {
        startKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, minIDBKey(), 0);
        lowerOpen = false; // Included.
    } else {
        startKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, *range->lower(), 0);
        lowerOpen = range->lowerOpen();
    }

    if (!upperBound) {
        stopKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, maxIDBKey(), 0);
        upperOpen = false; // Included.

        if (!forward) { // We need a key that exists.
            if (!findGreatestKeyLessThan(m_db.get(), stopKey, stopKey))
                return 0;
            upperOpen = false;
        }
    } else {
        stopKey = IndexDataKey::encode(databaseId, objectStoreId, indexId, *range->upper(), 0);
        if (!findLastIndexKeyEqualTo(m_db.get(), stopKey, stopKey)) // Seek to the *last* key in the set of non-unique keys.
            return 0;
        upperOpen = range->upperOpen();
    }

    RefPtr<IndexCursorImpl> cursor = IndexCursorImpl::create(m_db.get(), startKey, lowerOpen, stopKey, upperOpen, forward);
    if (!cursor->firstSeek())
        return 0;

    return cursor.release();
}

namespace {
class DummyTransaction : public IDBBackingStore::Transaction {
public:
    virtual void begin() {}
    virtual void commit() {}
    virtual void rollback() {}
};
}

PassRefPtr<IDBBackingStore::Transaction> IDBLevelDBBackingStore::createTransaction()
{
    // FIXME: We need to implement a transaction abstraction that allows for roll-backs, and write tests for it.
    return adoptRef(new DummyTransaction());
}

// FIXME: deleteDatabase should be part of IDBBackingStore.

} // namespace WebCore

#endif // ENABLE(LEVELDB)
#endif // ENABLE(INDEXED_DATABASE)
