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

#include "config.h"
#include "DFGObjectMaterializationData.h"

#if ENABLE(DFG_JIT)

#include <wtf/ListDump.h>

namespace JSC { namespace DFG {

void PhantomPropertyValue::dump(PrintStream& out) const
{
    out.print("id", m_identifierNumber);
}

void ObjectMaterializationData::dump(PrintStream& out) const
{
    out.print("[", listDump(m_properties), "]");
}

float ObjectMaterializationData::oneWaySimilarityScore(
    const ObjectMaterializationData& other) const
{
    unsigned numHits = 0;
    for (PhantomPropertyValue value : m_properties) {
        if (other.m_properties.contains(value))
            numHits++;
    }
    return static_cast<float>(numHits) / static_cast<float>(m_properties.size());
}

float ObjectMaterializationData::similarityScore(const ObjectMaterializationData& other) const
{
    return std::min(oneWaySimilarityScore(other), other.oneWaySimilarityScore(*this));
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
