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

#include "config.h"
#include "HighFidelityTypeProfiler.h"

#include "TypeLocation.h"

namespace JSC {

static const bool verbose = false;

String HighFidelityTypeProfiler::getTypesForVariableInRange(unsigned startLine, unsigned startColumn, unsigned endLine , unsigned endColumn, const String& variableName, intptr_t sourceID)
{
    String global = getGlobalTypesForVariableInRange(startLine, startColumn, endLine, endColumn, variableName, sourceID);
    if (!global.isEmpty())
        return global;
    
    return getLocalTypesForVariableInRange(startLine, startColumn, endLine, endColumn, variableName, sourceID);
}

WTF::String HighFidelityTypeProfiler::getGlobalTypesForVariableInRange(unsigned startLine, unsigned, unsigned, unsigned, const WTF::String&, intptr_t sourceID)
{
    auto iterLocationMap = m_globalLocationToGlobalIDMap.find(getLocationBasedHash(sourceID, startLine));
    if (iterLocationMap == m_globalLocationToGlobalIDMap.end())
        return "";

    auto iterIDMap = m_globalIDMap.find(iterLocationMap->second);
    if (iterIDMap == m_globalIDMap.end())
        return "";

    return iterIDMap->second->seenTypes();
}

WTF::String HighFidelityTypeProfiler::getLocalTypesForVariableInRange(unsigned startLine, unsigned , unsigned , unsigned , const WTF::String& , intptr_t sourceID)
{
    auto iter = m_globalLocationMap.find(getLocationBasedHash(sourceID, startLine));
    auto end = m_globalLocationMap.end();
    if (iter == end)
        return  "";

    return iter->second->seenTypes();
}

void HighFidelityTypeProfiler::insertNewLocation(TypeLocation* location)
{
    if (verbose)
        dataLogF("Registering location:: line:%u, column:%u\n", location->m_line, location->m_column);

    LocationKey key(getLocationBasedHash(location->m_sourceID, location->m_line));

    if (location->m_globalVariableID != HighFidelityNoGlobalIDExists) { 
        // Build the mapping relationships Map1:key=>globalId, Map2:globalID=>TypeSet 
        m_globalLocationToGlobalIDMap[key] = location->m_globalVariableID;
        m_globalIDMap[location->m_globalVariableID] = location->m_globalTypeSet;
    }

    m_globalLocationMap[key] = location->m_instructionTypeSet;
}

LocationKey HighFidelityTypeProfiler::getLocationBasedHash(intptr_t id, unsigned line)
{
    return LocationKey(id, line, 1);
}

} //namespace JSC
