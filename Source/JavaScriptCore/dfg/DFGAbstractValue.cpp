/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#include "JSCJSValueInlines.h"
#include "TrackedReferences.h"

namespace JSC { namespace DFG {

void AbstractValue::observeTransitions(const TransitionVector& vector)
{
    if (m_type & SpecCell) {
        m_structure.observeTransitions(vector);
        ArrayModes newModes = 0;
        for (unsigned i = vector.size(); i--;) {
            if (m_arrayModes & arrayModesFromStructure(vector[i].previous.get()))
                newModes |= arrayModesFromStructure(vector[i].next.get());
        }
        m_arrayModes |= newModes;
    }
    checkConsistency();
}

void AbstractValue::set(Graph& graph, const FrozenValue& value, StructureClobberState clobberState)
{
    if (!!value && value.value().isCell()) {
        Structure* structure = value.structure();
        StructureRegistrationResult result;
        RegisteredStructure registeredStructure = graph.registerStructure(structure, result);
        if (result == StructureRegisteredAndWatched) {
            m_structure = registeredStructure;
            if (clobberState == StructuresAreClobbered) {
                m_arrayModes = ALL_ARRAY_MODES;
                m_structure.clobber();
            } else
                m_arrayModes = arrayModesFromStructure(structure);
        } else {
            m_structure.makeTop();
            m_arrayModes = ALL_ARRAY_MODES;
        }
    } else {
        m_structure.clear();
        m_arrayModes = 0;
    }
    
    m_type = speculationFromValue(value.value());
    m_value = value.value();
    
    checkConsistency();
    assertIsRegistered(graph);
}

void AbstractValue::set(Graph& graph, Structure* structure)
{
    set(graph, graph.registerStructure(structure));
}

void AbstractValue::set(Graph& graph, RegisteredStructure structure)
{
    RELEASE_ASSERT(structure);
    
    m_structure = structure;
    m_arrayModes = arrayModesFromStructure(structure.get());
    m_type = speculationFromStructure(structure.get());
    m_value = JSValue();
    
    checkConsistency();
    assertIsRegistered(graph);
}

void AbstractValue::set(Graph& graph, const RegisteredStructureSet& set)
{
    m_structure = set;
    m_arrayModes = set.arrayModesFromStructures();
    m_type = set.speculationFromStructures();
    m_value = JSValue();
    
    checkConsistency();
    assertIsRegistered(graph);
}

void AbstractValue::setType(Graph& graph, SpeculatedType type)
{
    SpeculatedType cellType = type & SpecCell;
    if (cellType) {
        if (!(cellType & ~SpecString))
            m_structure = graph.stringStructure;
        else if (isSymbolSpeculation(cellType))
            m_structure = graph.symbolStructure;
        else
            m_structure.makeTop();
        m_arrayModes = ALL_ARRAY_MODES;
    } else {
        m_structure.clear();
        m_arrayModes = 0;
    }
    m_type = type;
    m_value = JSValue();
    checkConsistency();
}

void AbstractValue::fixTypeForRepresentation(Graph& graph, NodeFlags representation, Node* node)
{
    if (representation == NodeResultDouble) {
        if (m_value) {
            DFG_ASSERT(graph, node, m_value.isNumber());
            if (m_value.isInt32())
                m_value = jsDoubleNumber(m_value.asNumber());
        }
        if (m_type & SpecIntAnyFormat) {
            m_type &= ~SpecIntAnyFormat;
            m_type |= SpecAnyIntAsDouble;
        }
        if (m_type & ~SpecFullDouble)
            DFG_CRASH(graph, node, toCString("Abstract value ", *this, " for double node has type outside SpecFullDouble.\n").data());
    } else if (representation == NodeResultInt52) {
        if (m_type & SpecAnyIntAsDouble) {
            // AnyIntAsDouble can produce i32 or i52. SpecAnyIntAsDouble doesn't bound the magnitude of the value.
            m_type &= ~SpecAnyIntAsDouble;
            m_type |= SpecInt52Any;
        }

        if (m_type & SpecInt32Only) {
            m_type &= ~SpecInt32Only;
            m_type |= SpecInt32AsInt52;
        }

        if (m_type & ~SpecInt52Any)
            DFG_CRASH(graph, node, toCString("Abstract value ", *this, " for int52 node has type outside SpecInt52Any.\n").data());

        if (m_value) {
            DFG_ASSERT(graph, node, m_value.isAnyInt());
            m_type = int52AwareSpeculationFromValue(m_value);
        }
    } else {
        if (m_type & SpecInt32AsInt52) {
            m_type &= ~SpecInt32AsInt52;
            m_type |= SpecInt32Only;
        }
        if (m_type & SpecNonInt32AsInt52) {
            m_type &= ~SpecNonInt32AsInt52;
            m_type |= SpecAnyIntAsDouble;
        }
        if (m_type & ~SpecBytecodeTop)
            DFG_CRASH(graph, node, toCString("Abstract value ", *this, " for value node has type outside SpecBytecodeTop.\n").data());
    }
    
    checkConsistency();
}

void AbstractValue::fixTypeForRepresentation(Graph& graph, Node* node)
{
    fixTypeForRepresentation(graph, node->result(), node);
}

bool AbstractValue::mergeOSREntryValue(Graph& graph, JSValue value, VariableAccessData* variable, Node* node)
{
    FlushFormat flushFormat = variable->flushFormat();

    {
        if (flushFormat == FlushedDouble && value.isNumber())
            value = jsDoubleNumber(value.asNumber());
        SpeculatedType incomingType = resultFor(flushFormat) == NodeResultInt52 ? int52AwareSpeculationFromValue(value) : speculationFromValue(value);
        SpeculatedType requiredType = typeFilterFor(flushFormat);
        if (incomingType & ~requiredType)
            return false;
    }

    AbstractValue oldMe = *this;
    
    if (isClear()) {
        FrozenValue* frozenValue = graph.freeze(value);
        if (frozenValue->pointsToHeap()) {
            m_structure = graph.registerStructure(frozenValue->structure());
            m_arrayModes = arrayModesFromStructure(frozenValue->structure());
        } else {
            m_structure.clear();
            m_arrayModes = 0;
        }
        
        m_type = speculationFromValue(value);
        m_value = value;
    } else {
        mergeSpeculation(m_type, speculationFromValue(value));
        if (!!value && value.isCell()) {
            RegisteredStructure structure = graph.registerStructure(value.asCell()->structure(graph.m_vm));
            mergeArrayModes(m_arrayModes, arrayModesFromStructure(structure.get()));
            m_structure.merge(RegisteredStructureSet(structure));
        }
        if (m_value != value)
            m_value = JSValue();
    }
    
    assertIsRegistered(graph);

    fixTypeForRepresentation(graph, resultFor(flushFormat), node);

    checkConsistency();
    
    return oldMe != *this;
}

FiltrationResult AbstractValue::filter(
    Graph& graph, const RegisteredStructureSet& other, SpeculatedType admittedTypes)
{
    ASSERT(!(admittedTypes & SpecCell));
    
    if (isClear())
        return FiltrationOK;
    
    // FIXME: This could be optimized for the common case of m_type not
    // having structures, array modes, or a specific value.
    // https://bugs.webkit.org/show_bug.cgi?id=109663
    
    m_type &= other.speculationFromStructures() | admittedTypes;
    m_arrayModes &= other.arrayModesFromStructures();
    m_structure.filter(other);
    
    // It's possible that prior to the above two statements we had (Foo, TOP), where
    // Foo is a SpeculatedType that is disjoint with the passed RegisteredStructureSet. In that
    // case, we will now have (None, [someStructure]). In general, we need to make
    // sure that new information gleaned from the SpeculatedType needs to be fed back
    // into the information gleaned from the RegisteredStructureSet.
    m_structure.filter(m_type);
    
    filterArrayModesByType();
    filterValueByType();
    return normalizeClarity(graph);
}

FiltrationResult AbstractValue::changeStructure(Graph& graph, const RegisteredStructureSet& other)
{
    m_type &= other.speculationFromStructures();
    m_arrayModes = other.arrayModesFromStructures();
    m_structure = other;
    
    filterValueByType();
    
    return normalizeClarity(graph);
}

FiltrationResult AbstractValue::filterArrayModes(ArrayModes arrayModes, SpeculatedType admittedTypes)
{
    ASSERT(arrayModes);
    ASSERT(!(admittedTypes & SpecCell));
    
    if (isClear())
        return FiltrationOK;
    
    m_type &= SpecCell | admittedTypes;
    m_arrayModes &= arrayModes;
    return normalizeClarity();
}

FiltrationResult AbstractValue::filterClassInfo(Graph& graph, const ClassInfo* classInfo)
{
    // FIXME: AI should track ClassInfo to leverage hierarchical class information.
    // https://bugs.webkit.org/show_bug.cgi?id=162989
    if (isClear())
        return FiltrationOK;

    m_type &= speculationFromClassInfoInheritance(classInfo);
    m_structure.filterClassInfo(classInfo);

    m_structure.filter(m_type);

    filterArrayModesByType();
    filterValueByType();
    return normalizeClarity(graph);
}

FiltrationResult AbstractValue::filterSlow(SpeculatedType type)
{
    m_type &= type;
    
    // It's possible that prior to this filter() call we had, say, (Final, TOP), and
    // the passed type is Array. At this point we'll have (None, TOP). The best way
    // to ensure that the structure filtering does the right thing is to filter on
    // the new type (None) rather than the one passed (Array).
    m_structure.filter(m_type);
    filterArrayModesByType();
    filterValueByType();
    return normalizeClarity();
}

FiltrationResult AbstractValue::fastForwardToAndFilterSlow(AbstractValueClobberEpoch newEpoch, SpeculatedType type)
{
    if (newEpoch != m_effectEpoch)
        fastForwardToSlow(newEpoch);
    
    return filterSlow(type);
}

FiltrationResult AbstractValue::filterByValue(const FrozenValue& value)
{
    FiltrationResult result = filter(speculationFromValue(value.value()));
    if (m_type) {
        m_value = value.value();
        // It is possible that SpeculatedType from value is broader than original m_type.
        // The filter operation can only keep m_type as is or make it narrower.
        // As a result, the SpeculatedType from m_value can become broader than m_type. This breaks an invariant.
        // When setting m_value after filtering, we should filter m_value with m_type.
        filterValueByType();
    }
    checkConsistency();
    return result;
}

bool AbstractValue::contains(RegisteredStructure structure) const
{
    return couldBeType(speculationFromStructure(structure.get()))
        && (m_arrayModes & arrayModesFromStructure(structure.get()))
        && m_structure.contains(structure);
}

FiltrationResult AbstractValue::filter(const AbstractValue& other)
{
    m_type &= other.m_type;
    m_structure.filter(other.m_structure);
    m_arrayModes &= other.m_arrayModes;

    m_structure.filter(m_type);
    filterArrayModesByType();
    filterValueByType();
    
    if (normalizeClarity() == Contradiction)
        return Contradiction;
    
    if (m_value == other.m_value)
        return FiltrationOK;
    
    // Neither of us are BOTTOM, so an empty value means TOP.
    if (!m_value) {
        // We previously didn't prove a value but now we have done so.
        m_value = other.m_value; 
        // It is possible that SpeculatedType from other.m_value is broader than original m_type.
        // The filter operation can only keep m_type as is or make it narrower.
        // As a result, the SpeculatedType from m_value can become broader than m_type. This breaks an invariant.
        // When setting m_value after filtering, we should filter m_value with m_type.
        filterValueByType();
        return FiltrationOK;
    }
    
    if (!other.m_value) {
        // We had proved a value but the other guy hadn't, so keep our proof.
        return FiltrationOK;
    }
    
    // We both proved there to be a specific value but they are different.
    clear();
    return Contradiction;
}

void AbstractValue::filterValueByType()
{
    // We could go further, and ensure that if the futurePossibleStructure contravenes
    // the value, then we could clear both of those things. But that's unlikely to help
    // in any realistic scenario, so we don't do it. Simpler is better.

    if (!m_value)
        return;

    if (validateTypeAcceptingBoxedInt52(m_value))
        return;

    // We assume that the constant value can produce a narrower type at
    // some point. For example, rope JSString produces SpecString, but
    // it produces SpecStringIdent once it is resolved to AtomStringImpl.
    // We do not make this AbstractValue cleared, but clear the constant
    // value if validation fails currently.
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
        && (!m_arrayModes || m_structure.isClear()))
        return true;
    
    return false;
}

FiltrationResult AbstractValue::normalizeClarity()
{
    // It's useful to be able to quickly check if an abstract value is clear.
    // This normalizes everything to make that easy.
    
    FiltrationResult result;
    
    if (shouldBeClear()) {
        clear();
        result = Contradiction;
    } else
        result = FiltrationOK;

    checkConsistency();
    
    return result;
}

FiltrationResult AbstractValue::normalizeClarity(Graph& graph)
{
    FiltrationResult result = normalizeClarity();
    assertIsRegistered(graph);
    return result;
}

#if ASSERT_ENABLED
void AbstractValue::checkConsistency() const
{
    if (!(m_type & SpecCell)) {
        RELEASE_ASSERT(m_structure.isClear());
        RELEASE_ASSERT(!m_arrayModes);
    }
    
    if (isClear())
        RELEASE_ASSERT(!m_value);
    
    if (!!m_value)
        RELEASE_ASSERT(validateTypeAcceptingBoxedInt52(m_value));

    // Note that it's possible for a prediction like (Final, []). This really means that
    // the value is bottom and that any code that uses the value is unreachable. But
    // we don't want to get pedantic about this as it would only increase the computational
    // complexity of the code.
}

void AbstractValue::assertIsRegistered(Graph& graph) const
{
    m_structure.assertIsRegistered(graph);
}
#endif // ASSERT_ENABLED

ResultType AbstractValue::resultType() const
{
    ASSERT(isType(SpecBytecodeTop));
    if (isType(SpecBoolean))
        return ResultType::booleanType();
    if (isType(SpecInt32Only))
        return ResultType::numberTypeIsInt32();
    if (isType(SpecBytecodeNumber))
        return ResultType::numberType();
    if (isType(SpecString))
        return ResultType::stringType();
    if (isType(SpecString | SpecBytecodeNumber))
        return ResultType::stringOrNumberType();
    return ResultType::unknownType();
}

void AbstractValue::dump(PrintStream& out) const
{
    dumpInContext(out, nullptr);
}

void AbstractValue::dumpInContext(PrintStream& out, DumpContext* context) const
{
    out.print("(", SpeculationDump(m_type));
    if (m_type & SpecCell) {
        out.print(
            ", ", ArrayModesDump(m_arrayModes), ", ",
            inContext(m_structure, context));
    }
    if (!!m_value)
        out.print(", ", inContext(m_value, context));
    out.print(", ", m_effectEpoch);
    out.print(")");
}

void AbstractValue::validateReferences(const TrackedReferences& trackedReferences)
{
    trackedReferences.check(m_value);
    m_structure.validateReferences(trackedReferences);
}

#if USE(JSVALUE64) && !defined(NDEBUG)
void AbstractValue::ensureCanInitializeWithZeros()
{
    std::aligned_storage<sizeof(AbstractValue), alignof(AbstractValue)>::type zeroFilledStorage;
    memset(static_cast<void*>(&zeroFilledStorage), 0, sizeof(AbstractValue));
    ASSERT(*this == *static_cast<AbstractValue*>(static_cast<void*>(&zeroFilledStorage)));
}
#endif

void AbstractValue::fastForwardToSlow(AbstractValueClobberEpoch newEpoch)
{
    ASSERT(newEpoch != m_effectEpoch);
    
    if (newEpoch.clobberEpoch() != m_effectEpoch.clobberEpoch())
        clobberStructures();
    if (newEpoch.structureClobberState() == StructuresAreWatched)
        m_structure.observeInvalidationPoint();
    
    m_effectEpoch = newEpoch;
    
    checkConsistency();
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

