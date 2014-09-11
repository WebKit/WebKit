/*
 * Copyright (C) 2014 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TypeProfiler_h
#define TypeProfiler_h

#include "CodeBlock.h"
#include "FunctionHasExecutedCache.h"
#include "TypeLocationCache.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace Inspector { namespace Protocol  { namespace Runtime {
class TypeDescription;
}}}

namespace JSC {

class TypeLocation;

struct QueryKey {
    QueryKey()
        : m_sourceID(0)
        , m_divot(0)
    { }

    QueryKey(intptr_t sourceID, unsigned divot)
        : m_sourceID(sourceID)
        , m_divot(divot)
    { }

    QueryKey(WTF::HashTableDeletedValueType)
        : m_sourceID(INTPTR_MAX)
        , m_divot(UINT_MAX)
    { }

    bool isHashTableDeletedValue() const { return m_sourceID == INTPTR_MAX && m_divot == UINT_MAX; }
    bool operator==(const QueryKey& other) const { return m_sourceID == other.m_sourceID && m_divot == other.m_divot; }
    unsigned hash() const { return m_sourceID + m_divot; }

    intptr_t m_sourceID;
    unsigned m_divot;
};

struct QueryKeyHash {
    static unsigned hash(const QueryKey& key) { return key.hash(); }
    static bool equal(const QueryKey& a, const QueryKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::QueryKey> {
    typedef JSC::QueryKeyHash Hash;
};

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::QueryKey> : SimpleClassHashTraits<JSC::QueryKey> { };

} // namespace WTF

namespace JSC {

enum TypeProfilerSearchDescriptor {
    TypeProfilerSearchDescriptorNormal = 1,
    TypeProfilerSearchDescriptorFunctionReturn = 2
};

class TypeProfiler {
public:
    void logTypesForTypeLocation(TypeLocation*);
    JS_EXPORT_PRIVATE String typeInformationForExpressionAtOffset(TypeProfilerSearchDescriptor, unsigned offset, intptr_t sourceID);
    void insertNewLocation(TypeLocation*);
    FunctionHasExecutedCache* functionHasExecutedCache() { return &m_functionHasExecutedCache; }
    TypeLocationCache* typeLocationCache() { return &m_typeLocationCache; }
    TypeLocation* findLocation(unsigned divot, intptr_t sourceID, TypeProfilerSearchDescriptor);
    
private:
    typedef HashMap<intptr_t, Vector<TypeLocation*>> SourceIDToLocationBucketMap;
    SourceIDToLocationBucketMap m_bucketMap;
    FunctionHasExecutedCache m_functionHasExecutedCache;
    TypeLocationCache m_typeLocationCache;
    typedef HashMap<QueryKey, TypeLocation*> TypeLocationQueryCache;
    TypeLocationQueryCache m_queryCache;
};

} // namespace JSC

#endif // TypeProfiler_h
