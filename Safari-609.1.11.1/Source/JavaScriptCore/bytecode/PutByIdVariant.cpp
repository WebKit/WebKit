/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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
#include "PutByIdVariant.h"

#include "CallLinkStatus.h"
#include "JSCInlines.h"
#include <wtf/ListDump.h>

namespace JSC {

PutByIdVariant::PutByIdVariant(const PutByIdVariant& other)
    : PutByIdVariant()
{
    *this = other;
}

PutByIdVariant& PutByIdVariant::operator=(const PutByIdVariant& other)
{
    m_kind = other.m_kind;
    m_oldStructure = other.m_oldStructure;
    m_newStructure = other.m_newStructure;
    m_conditionSet = other.m_conditionSet;
    m_offset = other.m_offset;
    if (other.m_callLinkStatus)
        m_callLinkStatus = makeUnique<CallLinkStatus>(*other.m_callLinkStatus);
    else
        m_callLinkStatus = nullptr;
    return *this;
}

PutByIdVariant PutByIdVariant::replace(
    const StructureSet& structure, PropertyOffset offset)
{
    PutByIdVariant result;
    result.m_kind = Replace;
    result.m_oldStructure = structure;
    result.m_offset = offset;
    return result;
}

PutByIdVariant PutByIdVariant::transition(
    const StructureSet& oldStructure, Structure* newStructure,
    const ObjectPropertyConditionSet& conditionSet, PropertyOffset offset)
{
    PutByIdVariant result;
    result.m_kind = Transition;
    result.m_oldStructure = oldStructure;
    result.m_newStructure = newStructure;
    result.m_conditionSet = conditionSet;
    result.m_offset = offset;
    return result;
}

PutByIdVariant PutByIdVariant::setter(
    const StructureSet& structure, PropertyOffset offset,
    const ObjectPropertyConditionSet& conditionSet,
    std::unique_ptr<CallLinkStatus> callLinkStatus)
{
    PutByIdVariant result;
    result.m_kind = Setter;
    result.m_oldStructure = structure;
    result.m_conditionSet = conditionSet;
    result.m_offset = offset;
    result.m_callLinkStatus = WTFMove(callLinkStatus);
    return result;
}

Structure* PutByIdVariant::oldStructureForTransition() const
{
    RELEASE_ASSERT(kind() == Transition);
    RELEASE_ASSERT(m_oldStructure.size() <= 2);
    for (unsigned i = m_oldStructure.size(); i--;) {
        Structure* structure = m_oldStructure[i];
        if (structure != m_newStructure)
            return structure;
    }
    RELEASE_ASSERT_NOT_REACHED();

    return nullptr;
}

void PutByIdVariant::fixTransitionToReplaceIfNecessary()
{
    if (kind() != Transition)
        return;
    
    RELEASE_ASSERT(m_oldStructure.size() <= 2);
    for (unsigned i = m_oldStructure.size(); i--;) {
        Structure* structure = m_oldStructure[i];
        if (structure != m_newStructure)
            return;
    }
    
    m_newStructure = nullptr;
    m_kind = Replace;
    m_conditionSet = ObjectPropertyConditionSet();
    RELEASE_ASSERT(!m_callLinkStatus);
}

bool PutByIdVariant::writesStructures() const
{
    switch (kind()) {
    case Transition:
    case Setter:
        return true;
    default:
        return false;
    }
}

bool PutByIdVariant::reallocatesStorage() const
{
    switch (kind()) {
    case Transition:
        return oldStructureForTransition()->outOfLineCapacity() != newStructure()->outOfLineCapacity();
    case Setter:
        return true;
    default:
        return false;
    }
}

bool PutByIdVariant::makesCalls() const
{
    return kind() == Setter;
}

bool PutByIdVariant::attemptToMerge(const PutByIdVariant& other)
{
    if (m_offset != other.m_offset)
        return false;

    switch (m_kind) {
    case NotSet:
        RELEASE_ASSERT_NOT_REACHED();
        return false;
        
    case Replace: {
        switch (other.m_kind) {
        case Replace: {
            ASSERT(m_conditionSet.isEmpty());
            ASSERT(other.m_conditionSet.isEmpty());
            
            m_oldStructure.merge(other.m_oldStructure);
            return true;
        }
            
        case Transition: {
            PutByIdVariant newVariant = other;
            if (newVariant.attemptToMergeTransitionWithReplace(*this)) {
                *this = newVariant;
                return true;
            }
            return false;
        }
            
        default:
            return false;
        }
    }
        
    case Transition:
        switch (other.m_kind) {
        case Replace:
            return attemptToMergeTransitionWithReplace(other);
            
        case Transition: {
            if (m_oldStructure != other.m_oldStructure)
                return false;
            
            if (m_newStructure != other.m_newStructure)
                return false;
            
            ObjectPropertyConditionSet mergedConditionSet;
            if (!m_conditionSet.isEmpty()) {
                mergedConditionSet = m_conditionSet.mergedWith(other.m_conditionSet);
                if (!mergedConditionSet.isValid())
                    return false;
            }
            m_conditionSet = mergedConditionSet;
            return true;
        }
            
        default:
            return false;
        }
        
    case Setter: {
        if (other.m_kind != Setter)
            return false;
        
        if (m_callLinkStatus || other.m_callLinkStatus) {
            if (!(m_callLinkStatus && other.m_callLinkStatus))
                return false;
        }
        
        if (m_conditionSet.isEmpty() != other.m_conditionSet.isEmpty())
            return false;
        
        ObjectPropertyConditionSet mergedConditionSet;
        if (!m_conditionSet.isEmpty()) {
            mergedConditionSet = m_conditionSet.mergedWith(other.m_conditionSet);
            if (!mergedConditionSet.isValid() || !mergedConditionSet.hasOneSlotBaseCondition())
                return false;
        }
        m_conditionSet = mergedConditionSet;
        
        if (m_callLinkStatus)
            m_callLinkStatus->merge(*other.m_callLinkStatus);
        
        m_oldStructure.merge(other.m_oldStructure);
        return true;
    } }
    
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

bool PutByIdVariant::attemptToMergeTransitionWithReplace(const PutByIdVariant& replace)
{
    ASSERT(m_kind == Transition);
    ASSERT(replace.m_kind == Replace);
    ASSERT(m_offset == replace.m_offset);
    ASSERT(!replace.writesStructures());
    ASSERT(!replace.reallocatesStorage());
    ASSERT(replace.conditionSet().isEmpty());
    
    // This sort of merging only works when we have one path along which we add a new field which
    // transitions to structure S while the other path was already on structure S. This doesn't
    // work if we need to reallocate anything or if the replace path is polymorphic.
    
    if (reallocatesStorage())
        return false;
    
    if (replace.m_oldStructure.onlyStructure() != m_newStructure)
        return false;
    
    m_oldStructure.merge(m_newStructure);
    return true;
}

void PutByIdVariant::markIfCheap(SlotVisitor& visitor)
{
    m_oldStructure.markIfCheap(visitor);
    if (m_newStructure)
        m_newStructure->markIfCheap(visitor);
}

bool PutByIdVariant::finalize(VM& vm)
{
    if (!m_oldStructure.isStillAlive(vm))
        return false;
    if (m_newStructure && !vm.heap.isMarked(m_newStructure))
        return false;
    if (!m_conditionSet.areStillLive(vm))
        return false;
    if (m_callLinkStatus && !m_callLinkStatus->finalize(vm))
        return false;
    return true;
}

void PutByIdVariant::dump(PrintStream& out) const
{
    dumpInContext(out, 0);
}

void PutByIdVariant::dumpInContext(PrintStream& out, DumpContext* context) const
{
    switch (kind()) {
    case NotSet:
        out.print("<empty>");
        return;
        
    case Replace:
        out.print(
            "<Replace: ", inContext(structure(), context), ", offset = ", offset(), ", ", ">");
        return;
        
    case Transition:
        out.print(
            "<Transition: ", inContext(oldStructure(), context), " to ",
            pointerDumpInContext(newStructure(), context), ", [",
            inContext(m_conditionSet, context), "], offset = ", offset(), ", ", ">");
        return;
        
    case Setter:
        out.print(
            "<Setter: ", inContext(structure(), context), ", [",
            inContext(m_conditionSet, context), "]");
        out.print(", offset = ", m_offset);
        out.print(", call = ", *m_callLinkStatus);
        out.print(">");
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace JSC

