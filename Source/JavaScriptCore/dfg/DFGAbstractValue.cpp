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
    
    // It's possible that prior to the above two statements we had (Foo, TOP), where
    // Foo is a SpeculatedType that is disjoint with the passed StructureSet. In that
    // case, we will now have (None, [someStructure]). In general, we need to make
    // sure that new information gleaned from the SpeculatedType needs to be fed back
    // into the information gleaned from the StructureSet.
    m_currentKnownStructure.filter(m_type);
    
    if (m_currentKnownStructure.hasSingleton())
        setFuturePossibleStructure(graph, m_currentKnownStructure.singleton());
        
    filterArrayModesByType();
    filterValueByType();
    normalizeClarity();
    
    checkConsistency();
}

void AbstractValue::filterArrayModes(ArrayModes arrayModes)
{
    ASSERT(arrayModes);
    
    m_type &= SpecCell;
    m_arrayModes &= arrayModes;
    normalizeClarity();
    
    checkConsistency();
}

void AbstractValue::filter(SpeculatedType type)
{
    if (type == SpecTop)
        return;
    m_type &= type;
    
    // It's possible that prior to this filter() call we had, say, (Final, TOP), and
    // the passed type is Array. At this point we'll have (None, TOP). The best way
    // to ensure that the structure filtering does the right thing is to filter on
    // the new type (None) rather than the one passed (Array).
    m_currentKnownStructure.filter(m_type);
    m_futurePossibleStructure.filter(m_type);
    filterArrayModesByType();
    filterValueByType();
    normalizeClarity();
    
    checkConsistency();
}

void AbstractValue::setFuturePossibleStructure(Graph& graph, Structure* structure)
{
    if (graph.watchpoints().isStillValid(structure->transitionWatchpointSet()))
        m_futurePossibleStructure = structure;
    else
        m_futurePossibleStructure.makeTop();
}

void AbstractValue::filterValueByType()
{
    // We could go further, and ensure that if the futurePossibleStructure contravenes
    // the value, then we could clear both of those things. But that's unlikely to help
    // in any realistic scenario, so we don't do it. Simpler is better.

    if (!!m_type) {
        // The type is still non-empty. It may be that the new type renders
        // the value empty because it contravenes the constant value we had.
        if (m_value && !validateType(m_value))
            clear();
        return;
    }
    
    // The type has been rendered empty. That means that the value must now be invalid,
    // as well.
    ASSERT(!m_value || !validateType(m_value));
    m_value = JSValue();
}

void AbstractValue::filterArrayModesByType()
{
    if (!(m_type & SpecCell))
        m_arrayModes = 0;
    else if (!(m_type & ~SpecArray))
        m_arrayModes &= ALL_ARRAY_ARRAY_MODES;
    
    // NOTE: If m_type doesn't have SpecArray set, that doesn't mean that the
    // array modes have to be a subset of ALL_NON_ARRAY_ARRAY_MODES, since
    // in the speculated type type-system, RegExpMatchesArry and ArrayPrototype
    // are Otherobj (since they are not *exactly* JSArray) but in the ArrayModes
    // type system they are arrays (since they expose the magical length
    // property and are otherwise allocated using array allocation). Hence the
    // following would be wrong:
    //
    // if (!(m_type & SpecArray))
    //    m_arrayModes &= ALL_NON_ARRAY_ARRAY_MODES;
}

bool AbstractValue::shouldBeClear() const
{
    if (m_type == SpecNone)
        return true;
    
    if (!(m_type & ~SpecCell)
        && (!m_arrayModes
            || m_currentKnownStructure.isClear()))
        return true;
    
    return false;
}

void AbstractValue::normalizeClarity()
{
    // It's useful to be able to quickly check if an abstract value is clear.
    // This normalizes everything to make that easy.
    
    if (shouldBeClear())
        clear();
}

void AbstractValue::checkConsistency() const
{
    if (!(m_type & SpecCell)) {
        ASSERT(m_currentKnownStructure.isClear());
        ASSERT(m_futurePossibleStructure.isClear());
        ASSERT(!m_arrayModes);
    }
    
    if (isClear())
        ASSERT(!m_value);
    
    if (!!m_value)
        ASSERT(mergeSpeculations(m_type, speculationFromValue(m_value)) == m_type);
    
    // Note that it's possible for a prediction like (Final, []). This really means that
    // the value is bottom and that any code that uses the value is unreachable. But
    // we don't want to get pedantic about this as it would only increase the computational
    // complexity of the code.
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

