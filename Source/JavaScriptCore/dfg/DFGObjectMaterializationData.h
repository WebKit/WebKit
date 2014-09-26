/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef DFGObjectMaterializationData_h
#define DFGObjectMaterializationData_h

#if ENABLE(DFG_JIT)

#include <limits.h>
#include <wtf/MathExtras.h>
#include <wtf/PrintStream.h>
#include <wtf/Vector.h>

namespace JSC { namespace DFG {

struct PhantomPropertyValue {
    PhantomPropertyValue()
        : m_identifierNumber(UINT_MAX)
    {
    }
    
    PhantomPropertyValue(unsigned identifierNumber)
        : m_identifierNumber(identifierNumber)
    {
    }
    
    unsigned m_identifierNumber;
    
    bool operator==(const PhantomPropertyValue& other) const
    {
        return m_identifierNumber == other.m_identifierNumber;
    }
    
    void dump(PrintStream&) const;
};

struct ObjectMaterializationData {
    // Determines the meaning of the passed nodes.
    Vector<PhantomPropertyValue> m_properties;
    
    void dump(PrintStream&) const;
    
    // The fraction of my properties that the other data has.
    float oneWaySimilarityScore(const ObjectMaterializationData&) const;
    
    // The minimum of the two possible one-way scores.
    float similarityScore(const ObjectMaterializationData&) const;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGObjectMaterializationData_h

