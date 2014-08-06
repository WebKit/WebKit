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

String HighFidelityTypeProfiler::getTypesForVariableInAtOffset(unsigned divot, const String& variableName, intptr_t sourceID)
{
    String global = getGlobalTypesForVariableAtOffset(divot, variableName, sourceID);
    if (!global.isEmpty())
        return global;
    
    return getLocalTypesForVariableAtOffset(divot, variableName, sourceID);
}

String HighFidelityTypeProfiler::getGlobalTypesForVariableAtOffset(unsigned divot, const String& , intptr_t sourceID)
{
    TypeLocation* location = findLocation(divot, sourceID);
    if (!location)
        return  "";

    if (location->m_globalVariableID == HighFidelityNoGlobalIDExists)
        return "";

    return location->m_globalTypeSet->seenTypes();
}

String HighFidelityTypeProfiler::getLocalTypesForVariableAtOffset(unsigned divot, const String& , intptr_t sourceID)
{
    TypeLocation* location = findLocation(divot, sourceID);
    if (!location)
        return  "";

    return location->m_instructionTypeSet->seenTypes();
}

void HighFidelityTypeProfiler::insertNewLocation(TypeLocation* location)
{
    if (verbose)
        dataLogF("Registering location:: divotStart:%u, divotEnd:%u\n", location->m_divotStart, location->m_divotEnd);

    if (!m_bucketMap.contains(location->m_sourceID)) {
        Vector<TypeLocation*> bucket;
        m_bucketMap.set(location->m_sourceID, bucket);
    }

    Vector<TypeLocation*>& bucket = m_bucketMap.find(location->m_sourceID)->value;
    bucket.append(location);
}

TypeLocation* HighFidelityTypeProfiler::findLocation(unsigned divot, intptr_t sourceID)
{
    ASSERT(m_bucketMap.contains(sourceID)); 

    Vector<TypeLocation*>& bucket = m_bucketMap.find(sourceID)->value;
    unsigned distance = UINT_MAX; // Because assignments may be nested, make sure we find the closest enclosing assignment to this character offset.
    TypeLocation* bestMatch = nullptr;
    for (size_t i = 0, size = bucket.size(); i < size; i++) {
        TypeLocation* location = bucket.at(i);
        if (location->m_divotStart <= divot && divot <= location->m_divotEnd && location->m_divotEnd - location->m_divotStart <= distance) {
            distance = location->m_divotEnd - location->m_divotStart;
            bestMatch = location;
        }
    }

    // FIXME: BestMatch should never be null. This doesn't hold currently because we ignore some Eval/With/VarInjection variable assignments.
    return bestMatch;
}

} //namespace JSC
