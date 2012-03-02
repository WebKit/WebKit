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
#include "IDBLevelDBCoding.h"

#if ENABLE(INDEXED_DATABASE)
#if USE(LEVELDB)

#include "IDBKey.h"
#include "LevelDBSlice.h"
#include <wtf/text/StringBuilder.h>

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
//     <database id, 0, 0, 50, object store id, 6> => has key path (vs. null) [ObjectStoreMetaDataKey]
//
//
// Index meta-data:
//
//     The prefix is followed by a type byte, then two variable-length integers, and then another type byte.
//
//     <database id, 0, 0, 100, object store id, index id, 0> => utf16 index name [IndexMetaDataKey]
//     <database id, 0, 0, 100, object store id, index id, 1> => are index keys unique [IndexMetaDataKey]
//     <database id, 0, 0, 100, object store id, index id, 2> => utf16 key path [IndexMetaDataKey]
//     <database id, 0, 0, 100, object store id, index id, 3> => is index multi-entry [IndexMetaDataKey]
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
//     The prefix is followed by a type byte. The index key is an encoded IDBKey. The sequence number is a variable length integer.
//     The primary key is an encoded IDBKey.
//
//     <database id, object store id, index id, index key, sequence number, primary key> => "version", primary key [IndexDataKey]
//
//     (The sequence number is obsolete; it was used to allow two entries with
//     the same user (index) key in non-unique indexes prior to the inclusion of
//     the primary key in the data. The "version" field is used to weed out stale
//     index data. Whenever new object store data is inserted, it gets a new
//     "version" number, and new index data is written with this number. When
//     the index is used for look-ups, entries are validated against the
//     "exists" entries, and records with old "version" numbers are deleted
//     when they are encountered in getPrimaryKeyViaIndex,
//     IndexCursorImpl::loadCurrentRow, and IndexKeyCursorImpl::loadCurrentRow).

namespace WebCore {
namespace IDBLevelDBCoding {

static const unsigned char kIDBKeyNullTypeByte = 0;
static const unsigned char kIDBKeyStringTypeByte = 1;
static const unsigned char kIDBKeyDateTypeByte = 2;
static const unsigned char kIDBKeyNumberTypeByte = 3;
static const unsigned char kIDBKeyArrayTypeByte = 4;
static const unsigned char kIDBKeyMinKeyTypeByte = 5;

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

#ifndef INT64_MAX
#define INT64_MAX 0x7fffffffffffffffLL
#endif
#ifndef INT32_MAX
#define INT32_MAX 0x7fffffffL
#endif


Vector<char> encodeByte(unsigned char c)
{
    Vector<char> v;
    v.append(c);
    return v;
}

Vector<char> maxIDBKey()
{
    return encodeByte(kIDBKeyNullTypeByte);
}

Vector<char> minIDBKey()
{
    return encodeByte(kIDBKeyMinKeyTypeByte);
}

Vector<char> encodeInt(int64_t n)
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

int64_t decodeInt(const char* begin, const char* end)
{
    ASSERT(begin <= end);
    int64_t ret = 0;

    int shift = 0;
    while (begin < end) {
        unsigned char c = *begin++;
        ret |= static_cast<int64_t>(c) << shift;
        shift += 8;
    }

    return ret;
}

static int compareInts(int64_t a, int64_t b)
{
    ASSERT(a >= 0);
    ASSERT(b >= 0);

    int64_t diff = a - b;
    if (diff < 0)
        return -1;
    if (diff > 0)
        return 1;
    return 0;
}

Vector<char> encodeVarInt(int64_t n)
{
    Vector<char> ret; // FIXME: Size this at creation.

    do {
        unsigned char c = n & 0x7f;
        n >>= 7;
        if (n)
            c |= 0x80;
        ret.append(c);
    } while (n);

    return ret;
}

const char* decodeVarInt(const char *p, const char* limit, int64_t& foundInt)
{
    ASSERT(limit >= p);
    foundInt = 0;
    int shift = 0;

    do {
        if (p >= limit)
            return 0;

        unsigned char c = *p;
        foundInt |= static_cast<int64_t>(c & 0x7f) << shift;
        shift += 7;
    } while (*p++ & 0x80);
    return p;
}

Vector<char> encodeString(const String& s)
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

String decodeString(const char* p, const char* end)
{
    ASSERT(end >= p);
    ASSERT(!((end - p) % 2));

    size_t len = (end - p) / 2;
    StringBuilder result;
    result.reserveCapacity(len);

    for (size_t i = 0; i < len; ++i) {
        unsigned char hi = *p++;
        unsigned char lo = *p++;

        result.append(static_cast<UChar>((hi << 8) | lo));
    }

    return result.toString();
}

Vector<char> encodeStringWithLength(const String& s)
{
    Vector<char> ret = encodeVarInt(s.length());
    ret.append(encodeString(s));
    return ret;
}

const char* decodeStringWithLength(const char* p, const char* limit, String& foundString)
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

int compareEncodedStringsWithLength(const char*& p, const char* limitP, const char*& q, const char* limitQ)
{
    ASSERT(&p != &q);
    ASSERT(limitP >= p);
    ASSERT(limitQ >= q);
    int64_t lenP, lenQ;
    p = decodeVarInt(p, limitP, lenP);
    q = decodeVarInt(q, limitQ, lenQ);
    ASSERT(p && q);
    ASSERT(lenP >= 0);
    ASSERT(lenQ >= 0);
    ASSERT(p + lenP * 2 <= limitP);
    ASSERT(q + lenQ * 2 <= limitQ);

    const char* startP = p;
    const char* startQ = q;
    p += lenP * 2;
    q += lenQ * 2;

    if (p > limitP || q > limitQ)
        return 0;

    const size_t lmin = static_cast<size_t>(lenP < lenQ ? lenP : lenQ);
    if (int x = memcmp(startP, startQ, lmin * 2))
        return x;

    if (lenP == lenQ)
        return 0;

    return (lenP > lenQ) ? 1 : -1;
}

Vector<char> encodeDouble(double x)
{
    // FIXME: It would be nice if we could be byte order independent.
    const char* p = reinterpret_cast<char*>(&x);
    Vector<char> v;
    v.append(p, sizeof(x));
    ASSERT(v.size() == sizeof(x));
    return v;
}

const char* decodeDouble(const char* p, const char* limit, double* d)
{
    if (p + sizeof(*d) > limit)
        return 0;

    char* x = reinterpret_cast<char*>(d);
    for (size_t i = 0; i < sizeof(*d); ++i)
        *x++ = *p++;
    return p;
}

Vector<char> encodeIDBKey(const IDBKey& key)
{
    Vector<char> ret;
    encodeIDBKey(key, ret);
    return ret;
}

void encodeIDBKey(const IDBKey& key, Vector<char>& into)
{
    size_t previousSize = into.size();
    switch (key.type()) {
    case IDBKey::InvalidType:
    case IDBKey::MinType:
        ASSERT_NOT_REACHED();
        into.append(encodeByte(kIDBKeyNullTypeByte));
        return;
    case IDBKey::ArrayType:
        {
            into.append(encodeByte(kIDBKeyArrayTypeByte));
            size_t length = key.array().size();
            into.append(encodeVarInt(length));
            for (size_t i = 0; i < length; ++i)
                encodeIDBKey(*key.array()[i], into);
            ASSERT_UNUSED(previousSize, into.size() > previousSize);
            return;
        }
    case IDBKey::StringType:
        into.append(encodeByte(kIDBKeyStringTypeByte));
        into.append(encodeStringWithLength(key.string()));
        ASSERT_UNUSED(previousSize, into.size() > previousSize);
        return;
    case IDBKey::DateType:
        into.append(encodeByte(kIDBKeyDateTypeByte));
        into.append(encodeDouble(key.date()));
        ASSERT_UNUSED(previousSize, into.size() - previousSize == 9);
        return;
    case IDBKey::NumberType:
        into.append(encodeByte(kIDBKeyNumberTypeByte));
        into.append(encodeDouble(key.number()));
        ASSERT_UNUSED(previousSize, into.size() - previousSize == 9);
        return;
    }

    ASSERT_NOT_REACHED();
}


const char* decodeIDBKey(const char* p, const char* limit, RefPtr<IDBKey>& foundKey)
{
    ASSERT(limit >= p);
    if (p >= limit)
        return 0;

    unsigned char type = *p++;

    switch (type) {
    case kIDBKeyNullTypeByte:
        foundKey = IDBKey::createInvalid();
        return p;

    case kIDBKeyArrayTypeByte:
        {
            int64_t length;
            p = decodeVarInt(p, limit, length);
            if (!p)
                return 0;
            if (length < 0)
                return 0;
            IDBKey::KeyArray array;
            while (length--) {
                RefPtr<IDBKey> key;
                p = decodeIDBKey(p, limit, key);
                if (!p)
                    return 0;
                array.append(key);
            }
            foundKey = IDBKey::createArray(array);
            return p;
        }
    case kIDBKeyStringTypeByte:
        {
            String s;
            p = decodeStringWithLength(p, limit, s);
            if (!p)
                return 0;
            foundKey = IDBKey::createString(s);
            return p;
        }
    case kIDBKeyDateTypeByte:
        {
            double d;
            p = decodeDouble(p, limit, &d);
            if (!p)
                return 0;
            foundKey = IDBKey::createDate(d);
            return p;
        }
    case kIDBKeyNumberTypeByte:
        {
            double d;
            p = decodeDouble(p, limit, &d);
            if (!p)
                return 0;
            foundKey = IDBKey::createNumber(d);
            return p;
        }
    }

    ASSERT_NOT_REACHED();
    return 0;
}

const char* extractEncodedIDBKey(const char* start, const char* limit, Vector<char>* result)
{
    ASSERT(result);
    const char* p = start;
    if (p >= limit)
        return 0;

    unsigned char type = *p++;

    switch (type) {
    case kIDBKeyNullTypeByte:
    case kIDBKeyMinKeyTypeByte:
        *result = encodeByte(type);
        return p;
    case kIDBKeyArrayTypeByte:
        {
            int64_t length;
            p = decodeVarInt(p, limit, length);
            if (!p)
                return 0;
            if (length < 0)
                return 0;
            result->clear();
            result->append(start, p - start);
            while (length--) {
                Vector<char> subkey;
                p = extractEncodedIDBKey(p, limit, &subkey);
                if (!p)
                    return 0;
                result->append(subkey);
            }
            return p;
        }
    case kIDBKeyStringTypeByte:
        {
            int64_t length;
            p = decodeVarInt(p, limit, length);
            if (!p)
                return 0;
            if (p + length * 2 > limit)
                return 0;
            result->clear();
            result->append(start, p - start + length * 2);
            return p + length * 2;
        }
    case kIDBKeyDateTypeByte:
    case kIDBKeyNumberTypeByte:
        if (p + sizeof(double) > limit)
            return 0;
        result->clear();
        result->append(start, 1 + sizeof(double));
        return p + sizeof(double);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static IDBKey::Type keyTypeByteToKeyType(unsigned char type)
{
    switch (type) {
    case kIDBKeyNullTypeByte:
        return IDBKey::InvalidType;
    case kIDBKeyArrayTypeByte:
        return IDBKey::ArrayType;
    case kIDBKeyStringTypeByte:
        return IDBKey::StringType;
    case kIDBKeyDateTypeByte:
        return IDBKey::DateType;
    case kIDBKeyNumberTypeByte:
        return IDBKey::NumberType;
    case kIDBKeyMinKeyTypeByte:
        return IDBKey::MinType;
    }

    ASSERT_NOT_REACHED();
    return IDBKey::InvalidType;
}

int compareEncodedIDBKeys(const char*& p, const char* limitA, const char*& q, const char* limitB)
{
    ASSERT(&p != &q);
    ASSERT(p < limitA);
    ASSERT(q < limitB);
    unsigned char typeA = *p++;
    unsigned char typeB = *q++;

    if (int x = IDBKey::compareTypes(keyTypeByteToKeyType(typeA), keyTypeByteToKeyType(typeB)))
        return x;

    switch (typeA) {
    case kIDBKeyNullTypeByte:
    case kIDBKeyMinKeyTypeByte:
        // Null type or max type; no payload to compare.
        return 0;
    case kIDBKeyArrayTypeByte:
        {
            int64_t lengthA, lengthB;
            p = decodeVarInt(p, limitA, lengthA);
            if (!p)
                return 0;
            q = decodeVarInt(q, limitB, lengthB);
            if (!q)
                return 0;
            if (lengthA < 0 || lengthB < 0)
                return 0;
            for (int64_t i = 0; i < lengthA && i < lengthB; ++i) {
                if (int cmp = compareEncodedIDBKeys(p, limitA, q, limitB))
                    return cmp;
            }
            if (lengthA < lengthB)
                return -1;
            if (lengthA > lengthB)
                return 1;
            return 0;
        }
    case kIDBKeyStringTypeByte:
        return compareEncodedStringsWithLength(p, limitA, q, limitB);
    case kIDBKeyDateTypeByte:
    case kIDBKeyNumberTypeByte:
        {
            double d, e;
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
    }

    ASSERT_NOT_REACHED();
    return 0;
}

int compareEncodedIDBKeys(const Vector<char>& keyA, const Vector<char>& keyB)
{
    ASSERT(keyA.size() >= 1);
    ASSERT(keyB.size() >= 1);

    const char* p = keyA.data();
    const char* limitA = p + keyA.size();
    const char* q = keyB.data();
    const char* limitB = q + keyB.size();

    return compareEncodedIDBKeys(p, limitA, q, limitB);
}

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

int compare(const LevelDBSlice& a, const LevelDBSlice& b, bool indexKeys)
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

        bool ignoreDuplicates = indexKeys;
        return indexDataKeyA.compare(indexDataKeyB, ignoreDuplicates);
    }

    ASSERT_NOT_REACHED();
    return 0;
}


KeyPrefix::KeyPrefix()
    : m_databaseId(kInvalidType)
    , m_objectStoreId(kInvalidType)
    , m_indexId(kInvalidType)
{
}

KeyPrefix::KeyPrefix(int64_t databaseId, int64_t objectStoreId, int64_t indexId)
    : m_databaseId(databaseId)
    , m_objectStoreId(objectStoreId)
    , m_indexId(indexId)
{
}

const char* KeyPrefix::decode(const char* start, const char* limit, KeyPrefix* result)
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

Vector<char> KeyPrefix::encode() const
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

int KeyPrefix::compare(const KeyPrefix& other) const
{
    ASSERT(m_databaseId != kInvalidId);
    ASSERT(m_objectStoreId != kInvalidId);
    ASSERT(m_indexId != kInvalidId);

    if (m_databaseId != other.m_databaseId)
        return compareInts(m_databaseId, other.m_databaseId);
    if (m_objectStoreId != other.m_objectStoreId)
        return compareInts(m_objectStoreId, other.m_objectStoreId);
    if (m_indexId != other.m_indexId)
        return compareInts(m_indexId, other.m_indexId);
    return 0;
}

KeyPrefix::Type KeyPrefix::type() const
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

Vector<char> SchemaVersionKey::encode()
{
    KeyPrefix prefix(0, 0, 0);
    Vector<char> ret = prefix.encode();
    ret.append(encodeByte(kSchemaVersionTypeByte));
    return ret;
}

Vector<char> MaxDatabaseIdKey::encode()
{
    KeyPrefix prefix(0, 0, 0);
    Vector<char> ret = prefix.encode();
    ret.append(encodeByte(kMaxDatabaseIdTypeByte));
    return ret;
}

DatabaseFreeListKey::DatabaseFreeListKey()
    : m_databaseId(-1)
{
}

const char* DatabaseFreeListKey::decode(const char* start, const char* limit, DatabaseFreeListKey* result)
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

Vector<char> DatabaseFreeListKey::encode(int64_t databaseId)
{
    KeyPrefix prefix(0, 0, 0);
    Vector<char> ret = prefix.encode();
    ret.append(encodeByte(kDatabaseFreeListTypeByte));
    ret.append(encodeVarInt(databaseId));
    return ret;
}

Vector<char> DatabaseFreeListKey::encodeMaxKey()
{
    return encode(INT64_MAX);
}

int64_t DatabaseFreeListKey::databaseId() const
{
    ASSERT(m_databaseId >= 0);
    return m_databaseId;
}

int DatabaseFreeListKey::compare(const DatabaseFreeListKey& other) const
{
    ASSERT(m_databaseId >= 0);
    return compareInts(m_databaseId, other.m_databaseId);
}

const char* DatabaseNameKey::decode(const char* start, const char* limit, DatabaseNameKey* result)
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

Vector<char> DatabaseNameKey::encode(const String& origin, const String& databaseName)
{
    KeyPrefix prefix(0, 0, 0);
    Vector<char> ret = prefix.encode();
    ret.append(encodeByte(kDatabaseNameTypeByte));
    ret.append(encodeStringWithLength(origin));
    ret.append(encodeStringWithLength(databaseName));
    return ret;
}

Vector<char> DatabaseNameKey::encodeMinKeyForOrigin(const String& origin)
{
    return encode(origin, "");
}

Vector<char> DatabaseNameKey::encodeStopKeyForOrigin(const String& origin)
{
    // just after origin in collation order
    return encodeMinKeyForOrigin(origin + "\x01");
}

int DatabaseNameKey::compare(const DatabaseNameKey& other)
{
    if (int x = codePointCompare(m_origin, other.m_origin))
        return x;
    return codePointCompare(m_databaseName, other.m_databaseName);
}

Vector<char> DatabaseMetaDataKey::encode(int64_t databaseId, MetaDataType metaDataType)
{
    KeyPrefix prefix(databaseId, 0, 0);
    Vector<char> ret = prefix.encode();
    ret.append(encodeByte(metaDataType));
    return ret;
}

ObjectStoreMetaDataKey::ObjectStoreMetaDataKey()
    : m_objectStoreId(-1)
    , m_metaDataType(-1)
{
}

const char* ObjectStoreMetaDataKey::decode(const char* start, const char* limit, ObjectStoreMetaDataKey* result)
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

Vector<char> ObjectStoreMetaDataKey::encode(int64_t databaseId, int64_t objectStoreId, int64_t metaDataType)
{
    KeyPrefix prefix(databaseId, 0, 0);
    Vector<char> ret = prefix.encode();
    ret.append(encodeByte(kObjectStoreMetaDataTypeByte));
    ret.append(encodeVarInt(objectStoreId));
    ret.append(encodeVarInt(metaDataType));
    return ret;
}

Vector<char> ObjectStoreMetaDataKey::encodeMaxKey(int64_t databaseId)
{
    return encode(databaseId, INT64_MAX, INT64_MAX);
}

Vector<char> ObjectStoreMetaDataKey::encodeMaxKey(int64_t databaseId, int64_t objectStoreId)
{
    return encode(databaseId, objectStoreId, INT64_MAX);
}

int64_t ObjectStoreMetaDataKey::objectStoreId() const
{
    ASSERT(m_objectStoreId >= 0);
    return m_objectStoreId;
}
int64_t ObjectStoreMetaDataKey::metaDataType() const
{
    ASSERT(m_metaDataType >= 0);
    return m_metaDataType;
}

int ObjectStoreMetaDataKey::compare(const ObjectStoreMetaDataKey& other)
{
    ASSERT(m_objectStoreId >= 0);
    ASSERT(m_metaDataType >= 0);
    if (int x = compareInts(m_objectStoreId, other.m_objectStoreId))
        return x;
    return m_metaDataType - other.m_metaDataType;
}

IndexMetaDataKey::IndexMetaDataKey()
    : m_objectStoreId(-1)
    , m_indexId(-1)
    , m_metaDataType(0)
{
}

const char* IndexMetaDataKey::decode(const char* start, const char* limit, IndexMetaDataKey* result)
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

Vector<char> IndexMetaDataKey::encode(int64_t databaseId, int64_t objectStoreId, int64_t indexId, unsigned char metaDataType)
{
    KeyPrefix prefix(databaseId, 0, 0);
    Vector<char> ret = prefix.encode();
    ret.append(encodeByte(kIndexMetaDataTypeByte));
    ret.append(encodeVarInt(objectStoreId));
    ret.append(encodeVarInt(indexId));
    ret.append(encodeByte(metaDataType));
    return ret;
}

Vector<char> IndexMetaDataKey::encodeMaxKey(int64_t databaseId, int64_t objectStoreId)
{
    return encode(databaseId, objectStoreId, INT64_MAX, 255);
}

int IndexMetaDataKey::compare(const IndexMetaDataKey& other)
{
    ASSERT(m_objectStoreId >= 0);
    ASSERT(m_indexId >= 0);

    if (int x = compareInts(m_objectStoreId, other.m_objectStoreId))
        return x;
    if (int x = compareInts(m_indexId, other.m_indexId))
        return x;
    return m_metaDataType - other.m_metaDataType;
}

int64_t IndexMetaDataKey::indexId() const
{
    ASSERT(m_indexId >= 0);
    return m_indexId;
}

ObjectStoreFreeListKey::ObjectStoreFreeListKey()
    : m_objectStoreId(-1)
{
}

const char* ObjectStoreFreeListKey::decode(const char* start, const char* limit, ObjectStoreFreeListKey* result)
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

Vector<char> ObjectStoreFreeListKey::encode(int64_t databaseId, int64_t objectStoreId)
{
    KeyPrefix prefix(databaseId, 0, 0);
    Vector<char> ret = prefix.encode();
    ret.append(encodeByte(kObjectStoreFreeListTypeByte));
    ret.append(encodeVarInt(objectStoreId));
    return ret;
}

Vector<char> ObjectStoreFreeListKey::encodeMaxKey(int64_t databaseId)
{
    return encode(databaseId, INT64_MAX);
}

int64_t ObjectStoreFreeListKey::objectStoreId() const
{
    ASSERT(m_objectStoreId >= 0);
    return m_objectStoreId;
}

int ObjectStoreFreeListKey::compare(const ObjectStoreFreeListKey& other)
{
    // FIXME: It may seem strange that we're not comparing database id's,
    // but that comparison will have been made earlier.
    // We should probably make this more clear, though...
    ASSERT(m_objectStoreId >= 0);
    return compareInts(m_objectStoreId, other.m_objectStoreId);
}

IndexFreeListKey::IndexFreeListKey()
    : m_objectStoreId(-1)
    , m_indexId(-1)
{
}

const char* IndexFreeListKey::decode(const char* start, const char* limit, IndexFreeListKey* result)
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

Vector<char> IndexFreeListKey::encode(int64_t databaseId, int64_t objectStoreId, int64_t indexId)
{
    KeyPrefix prefix(databaseId, 0, 0);
    Vector<char> ret = prefix.encode();
    ret.append(encodeByte(kIndexFreeListTypeByte));
    ret.append(encodeVarInt(objectStoreId));
    ret.append(encodeVarInt(indexId));
    return ret;
}

Vector<char> IndexFreeListKey::encodeMaxKey(int64_t databaseId, int64_t objectStoreId)
{
    return encode(databaseId, objectStoreId, INT64_MAX);
}

int IndexFreeListKey::compare(const IndexFreeListKey& other)
{
    ASSERT(m_objectStoreId >= 0);
    ASSERT(m_indexId >= 0);
    if (int x = compareInts(m_objectStoreId, other.m_objectStoreId))
        return x;
    return compareInts(m_indexId, other.m_indexId);
}

int64_t IndexFreeListKey::objectStoreId() const
{
    ASSERT(m_objectStoreId >= 0);
    return m_objectStoreId;
}

int64_t IndexFreeListKey::indexId() const
{
    ASSERT(m_indexId >= 0);
    return m_indexId;
}

// FIXME: We never use this to look up object store ids, because a mapping
// is kept in the IDBDatabaseBackendImpl. Can the mapping become unreliable?
// Can we remove this?
const char* ObjectStoreNamesKey::decode(const char* start, const char* limit, ObjectStoreNamesKey* result)
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

Vector<char> ObjectStoreNamesKey::encode(int64_t databaseId, const String& objectStoreName)
{
    KeyPrefix prefix(databaseId, 0, 0);
    Vector<char> ret = prefix.encode();
    ret.append(encodeByte(kObjectStoreNamesTypeByte));
    ret.append(encodeStringWithLength(objectStoreName));
    return ret;
}

int ObjectStoreNamesKey::compare(const ObjectStoreNamesKey& other)
{
    return codePointCompare(m_objectStoreName, other.m_objectStoreName);
}

IndexNamesKey::IndexNamesKey()
    : m_objectStoreId(-1)
{
}

// FIXME: We never use this to look up index ids, because a mapping
// is kept at a higher level.
const char* IndexNamesKey::decode(const char* start, const char* limit, IndexNamesKey* result)
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

Vector<char> IndexNamesKey::encode(int64_t databaseId, int64_t objectStoreId, const String& indexName)
{
    KeyPrefix prefix(databaseId, 0, 0);
    Vector<char> ret = prefix.encode();
    ret.append(encodeByte(kIndexNamesKeyTypeByte));
    ret.append(encodeVarInt(objectStoreId));
    ret.append(encodeStringWithLength(indexName));
    return ret;
}

int IndexNamesKey::compare(const IndexNamesKey& other)
{
    ASSERT(m_objectStoreId >= 0);
    if (int x = compareInts(m_objectStoreId, other.m_objectStoreId))
        return x;
    return codePointCompare(m_indexName, other.m_indexName);
}

const char* ObjectStoreDataKey::decode(const char* start, const char* end, ObjectStoreDataKey* result)
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

Vector<char> ObjectStoreDataKey::encode(int64_t databaseId, int64_t objectStoreId, const Vector<char> encodedUserKey)
{
    KeyPrefix prefix(databaseId, objectStoreId, kSpecialIndexNumber);
    Vector<char> ret = prefix.encode();
    ret.append(encodedUserKey);

    return ret;
}

Vector<char> ObjectStoreDataKey::encode(int64_t databaseId, int64_t objectStoreId, const IDBKey& userKey)
{
    return encode(databaseId, objectStoreId, encodeIDBKey(userKey));
}

int ObjectStoreDataKey::compare(const ObjectStoreDataKey& other)
{
    return compareEncodedIDBKeys(m_encodedUserKey, other.m_encodedUserKey);
}

PassRefPtr<IDBKey> ObjectStoreDataKey::userKey() const
{
    RefPtr<IDBKey> key;
    decodeIDBKey(m_encodedUserKey.begin(), m_encodedUserKey.end(), key);
    return key;
}

const int64_t ObjectStoreDataKey::kSpecialIndexNumber = kObjectStoreDataIndexId;

const char* ExistsEntryKey::decode(const char* start, const char* end, ExistsEntryKey* result)
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

Vector<char> ExistsEntryKey::encode(int64_t databaseId, int64_t objectStoreId, const Vector<char>& encodedKey)
{
    KeyPrefix prefix(databaseId, objectStoreId, kSpecialIndexNumber);
    Vector<char> ret = prefix.encode();
    ret.append(encodedKey);
    return ret;
}

Vector<char> ExistsEntryKey::encode(int64_t databaseId, int64_t objectStoreId, const IDBKey& userKey)
{
    return encode(databaseId, objectStoreId, encodeIDBKey(userKey));
}

int ExistsEntryKey::compare(const ExistsEntryKey& other)
{
    return compareEncodedIDBKeys(m_encodedUserKey, other.m_encodedUserKey);
}

PassRefPtr<IDBKey> ExistsEntryKey::userKey() const
{
    RefPtr<IDBKey> key;
    decodeIDBKey(m_encodedUserKey.begin(), m_encodedUserKey.end(), key);
    return key;
}

const int64_t ExistsEntryKey::kSpecialIndexNumber = kExistsEntryIndexId;

IndexDataKey::IndexDataKey()
    : m_databaseId(-1)
    , m_objectStoreId(-1)
    , m_indexId(-1)
    , m_sequenceNumber(-1)
{
}

const char* IndexDataKey::decode(const char* start, const char* limit, IndexDataKey* result)
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
    result->m_sequenceNumber = -1;
    result->m_encodedPrimaryKey = minIDBKey();

    p = extractEncodedIDBKey(p, limit, &result->m_encodedUserKey);
    if (!p)
        return 0;

    // [optional] sequence number
    if (p == limit)
        return p;
    p =  decodeVarInt(p, limit, result->m_sequenceNumber);
    if (!p)
        return 0;

    // [optional] primary key
    if (p == limit)
        return p;
    p = extractEncodedIDBKey(p, limit, &result->m_encodedPrimaryKey);
    if (!p)
        return 0;

    return p;
}

Vector<char> IndexDataKey::encode(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const Vector<char>& encodedUserKey, const Vector<char>& encodedPrimaryKey, int64_t sequenceNumber)
{
    KeyPrefix prefix(databaseId, objectStoreId, indexId);
    Vector<char> ret = prefix.encode();
    ret.append(encodedUserKey);
    ret.append(encodeVarInt(sequenceNumber));
    ret.append(encodedPrimaryKey);
    return ret;
}

Vector<char> IndexDataKey::encode(int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& userKey)
{
    return encode(databaseId, objectStoreId, indexId, encodeIDBKey(userKey), minIDBKey());
}

Vector<char> IndexDataKey::encodeMinKey(int64_t databaseId, int64_t objectStoreId, int64_t indexId)
{
    return encode(databaseId, objectStoreId, indexId, minIDBKey(), minIDBKey());
}

Vector<char> IndexDataKey::encodeMaxKey(int64_t databaseId, int64_t objectStoreId, int64_t indexId)
{
    return encode(databaseId, objectStoreId, indexId, maxIDBKey(), maxIDBKey(), INT64_MAX);
}

int IndexDataKey::compare(const IndexDataKey& other, bool ignoreDuplicates)
{
    ASSERT(m_databaseId >= 0);
    ASSERT(m_objectStoreId >= 0);
    ASSERT(m_indexId >= 0);
    if (int x = compareEncodedIDBKeys(m_encodedUserKey, other.m_encodedUserKey))
        return x;
    if (ignoreDuplicates)
        return 0;
    if (int x = compareEncodedIDBKeys(m_encodedPrimaryKey, other.m_encodedPrimaryKey))
        return x;
    return compareInts(m_sequenceNumber, other.m_sequenceNumber);
}

int64_t IndexDataKey::databaseId() const
{
    ASSERT(m_databaseId >= 0);
    return m_databaseId;
}

int64_t IndexDataKey::objectStoreId() const
{
    ASSERT(m_objectStoreId >= 0);
    return m_objectStoreId;
}

int64_t IndexDataKey::indexId() const
{
    ASSERT(m_indexId >= 0);
    return m_indexId;
}

PassRefPtr<IDBKey> IndexDataKey::userKey() const
{
    RefPtr<IDBKey> key;
    decodeIDBKey(m_encodedUserKey.begin(), m_encodedUserKey.end(), key);
    return key;
}

PassRefPtr<IDBKey> IndexDataKey::primaryKey() const
{
    RefPtr<IDBKey> key;
    decodeIDBKey(m_encodedPrimaryKey.begin(), m_encodedPrimaryKey.end(), key);
    return key;
}

} // namespace IDBLevelDBCoding
} // namespace WebCore

#endif // USE(LEVELDB)
#endif // ENABLE(INDEXED_DATABASE)
