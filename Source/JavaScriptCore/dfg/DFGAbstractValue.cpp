/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "DFGAbstractValue.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "Operations.h"

namespace JSC { namespace DFG {

void AbstractValue::setMostSpecific(Graph& graph, JSValue value)
{
    if (!!value && value.isCell()) {
        Structure* structure = value.asCell()->structure();
        m_currentKnownStructure = structure;
        setFuturePossibleStructure(graph, structure);
        m_arrayModes = asArrayModes(structure->indexingType());
    } else {
        m_currentKnownStructure.clear();
        m_futurePossibleStructure.clear();
        m_arrayModes = 0;
    }
        
    m_type = speculationFromValue(value);
    m_value = value;
        
    checkConsistency();
}

void AbstractValue::set(Graph& graph, JSValue value)
{
    if (!!value && value.isCell()) {
        m_currentKnownStructure.makeTop();
        Structure* structure = value.asCell()->structure();
        setFuturePossibleStructure(graph, structure);
        m_arrayModes = asArrayModes(structure->indexingType());
        clobberArrayModes();
    } else {
        m_currentKnownStructure.clear();
        m_futurePossibleStructure.clear();
        m_arrayModes = 0;
    }
        
    m_type = speculationFromValue(value);
    m_value = value;
        
    checkConsistency();
}

void AbstractValue::set(Graph& graph, Structure* structure)
{
    m_currentKnownStructure = structure;
    setFuturePossibleStructure(graph, structure);
    m_arrayModes = asArrayModes(structure->indexingType());
    m_type = speculationFromStructure(structure);
    m_value = JSValue();
        
    checkConsistency();
}

void AbstractValue::filter(Graph& graph, const StructureSet& other)
{
    // FIXME: This could be optimized for the common case of m_type not
    // having structures, array modes, or a specific value.
    // https://bugs.webkit.org/show_bug.cgi?id=109663
    m_type &= other.speculationFromStructures();
    m_arrayModes &= other.arrayModesFromStructures();
    m_currentKnownStructure.filter(other);
    if (m_currentKnownStructure.isClear())
        m_futurePossibleStructure.clear();
    else if (m_currentKnownStructure.hasSingleton())
        filterFuturePossibleStructure(graph, m_currentKnownStructure.singleton());
        
    // It's possible that prior to the above two statements we had (Foo, TOP), where
    // Foo is a SpeculatedType that is disjoint with the passed StructureSet. In that
    // case, we will now have (None, [someStructure]). In general, we need to make
    // sure that new information gleaned from the SpeculatedType needs to be fed back
    // into the information gleaned from the StructureSet.
    m_currentKnownStructure.filter(m_type);
    m_futurePossibleStructure.filter(m_type);
        
    filterArrayModesByType();
    filterValueByType();
        
    checkConsistency();
}

void AbstractValue::setFuturePossibleStructure(Graph&, Structure* structure)
{
    if (structure->transitionWatchpointSetIsStillValid())
        m_futurePossibleStructure = structure;
    else
        m_futurePossibleStructure.makeTop();
}

void AbstractValue::filterFuturePossibleStructure(Graph&, Structure* structure)
{
    if (structure->transitionWatchpointSetIsStillValid())
        m_futurePossibleStructure.filter(StructureAbstractValue(structure));
}

void AbstractValue::dump(PrintStream& out) const
{
    out.print(
        "(", SpeculationDump(m_type), ", ", ArrayModesDump(m_arrayModes), ", ",
        m_currentKnownStructure, ", ", m_futurePossibleStructure);
    if (!!m_value)
        out.print(", ", m_value);
    out.print(")");
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

