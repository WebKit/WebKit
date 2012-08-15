/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef ArrayProfile_h
#define ArrayProfile_h

#include "JSArray.h"
#include "Structure.h"
#include <wtf/HashMap.h>
#include <wtf/SegmentedVector.h>

namespace JSC {

class LLIntOffsetsExtractor;

typedef unsigned ArrayModes;

static const unsigned IsNotArray = 1;
static const unsigned IsJSArray  = 2;

inline ArrayModes arrayModeFromStructure(Structure* structure)
{
    if (structure->classInfo() == &JSArray::s_info)
        return IsJSArray;
    return IsNotArray;
}

class ArrayProfile {
public:
    ArrayProfile()
        : m_bytecodeOffset(std::numeric_limits<unsigned>::max())
        , m_lastSeenStructure(0)
        , m_expectedStructure(0)
        , m_structureIsPolymorphic(false)
        , m_observedArrayModes(0)
    {
    }
    
    ArrayProfile(unsigned bytecodeOffset)
        : m_bytecodeOffset(bytecodeOffset)
        , m_lastSeenStructure(0)
        , m_expectedStructure(0)
        , m_structureIsPolymorphic(false)
        , m_observedArrayModes(0)
    {
    }
    
    unsigned bytecodeOffset() const { return m_bytecodeOffset; }
    
    Structure** addressOfLastSeenStructure() { return &m_lastSeenStructure; }
    
    void observeStructure(Structure* structure)
    {
        m_lastSeenStructure = structure;
    }
    
    void computeUpdatedPrediction(OperationInProgress operation = NoOperation);
    
    Structure* expectedStructure() const { return m_expectedStructure; }
    bool structureIsPolymorphic() const { return m_structureIsPolymorphic; }
    bool hasDefiniteStructure() const
    {
        return !structureIsPolymorphic() && m_expectedStructure;
    }
    ArrayModes observedArrayModes() const { return m_observedArrayModes; }
    
private:
    friend class LLIntOffsetsExtractor;
    
    unsigned m_bytecodeOffset;
    Structure* m_lastSeenStructure;
    Structure* m_expectedStructure;
    bool m_structureIsPolymorphic;
    ArrayModes m_observedArrayModes;
};

typedef SegmentedVector<ArrayProfile, 4, 0> ArrayProfileVector;

} // namespace JSC

#endif // ArrayProfile_h

