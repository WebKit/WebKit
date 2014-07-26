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

#ifndef HighFidelityTypeProfiler_h
#define HighFidelityTypeProfiler_h

#include "CodeBlock.h"
#include <unordered_map>
#include <wtf/HashMap.h>
#include <wtf/HashMethod.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class TypeLocation;

struct LocationKey {

public:
    LocationKey(intptr_t sourceID, unsigned line, unsigned column) 
        : m_sourceID(sourceID)
        , m_line(line)
        , m_column(column)

    {
    }

    unsigned hash() const
    {
        return m_line + m_sourceID;
    }

    // FIXME: For now, this is a hack. We do the following: Map:"ID:Line" => TypeSet. Obviously, this assumes all assignments are on discrete lines, which is an incorrect assumption.
    bool operator==(const LocationKey& other) const
    {
        return m_sourceID == other.m_sourceID
               && m_line == other.m_line;
    }

    intptr_t m_sourceID;
    unsigned m_line;
    unsigned m_column;
};

class HighFidelityTypeProfiler {

public:
    String getTypesForVariableInRange(unsigned startLine, unsigned startColumn, unsigned endLine, unsigned endColumn, const String& variableName, intptr_t sourceID);
    String getGlobalTypesForVariableInRange(unsigned startLine, unsigned startColumn, unsigned endLine, unsigned endColumn, const String& variableName, intptr_t sourceID);
    String getLocalTypesForVariableInRange(unsigned startLine, unsigned startColumn, unsigned endLine, unsigned endColumn, const String& variableName, intptr_t sourceID);
    void insertNewLocation(TypeLocation*);
    
private:
    static LocationKey getLocationBasedHash(intptr_t, unsigned);

    typedef std::unordered_map<LocationKey, RefPtr<TypeSet>, HashMethod<LocationKey>> GlobalLocationMap; 
    typedef std::unordered_map<int64_t, RefPtr<TypeSet>> GlobalIDMap; 
    typedef std::unordered_map<LocationKey, int64_t, HashMethod<LocationKey>> GlobalLocationToGlobalIDMap;

    GlobalIDMap m_globalIDMap;
    GlobalLocationMap m_globalLocationMap;
    GlobalLocationToGlobalIDMap m_globalLocationToGlobalIDMap;
};

} //namespace JSC

#endif //HighFidelityTypeProfiler_h
