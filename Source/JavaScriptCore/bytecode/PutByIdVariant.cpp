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
#include "PutByIdVariant.h"

#include <wtf/ListDump.h>

namespace JSC {

Structure* PutByIdVariant::oldStructureForTransition() const
{
    ASSERT(kind() == Transition);
    ASSERT(m_oldStructure.size() <= 2);
    for (unsigned i = m_oldStructure.size(); i--;) {
        Structure* structure = m_oldStructure[i];
        if (structure != m_newStructure)
            return structure;
    }
    RELEASE_ASSERT_NOT_REACHED();

    return nullptr;
}

bool PutByIdVariant::writesStructures() const
{
    return kind() == Transition;
}

bool PutByIdVariant::reallocatesStorage() const
{
    if (kind() != Transition)
        return false;
    
    if (oldStructureForTransition()->outOfLineCapacity() == newStructure()->outOfLineCapacity())
        return false;
    
    return true;
}

bool PutByIdVariant::attemptToMerge(const PutByIdVariant& other)
{
    if (m_offset != other.m_offset)
        return false;
    
    switch (m_kind) {
    case Replace:
        switch (other.m_kind) {
        case Replace: {
            ASSERT(m_constantChecks.isEmpty());
            ASSERT(other.m_constantChecks.isEmpty());
            
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
        
    case Transition:
        switch (other.m_kind) {
        case Replace:
            return attemptToMergeTransitionWithReplace(other);
            
        default:
            return false;
        }
        
    default:
        return false;
    }
}

bool PutByIdVariant::attemptToMergeTransitionWithReplace(const PutByIdVariant& replace)
{
    ASSERT(m_kind == Transition);
    ASSERT(replace.m_kind == Replace);
    ASSERT(m_offset == replace.m_offset);
    ASSERT(!replace.writesStructures());
    ASSERT(!replace.reallocatesStorage());
    
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
            "<Replace: ", inContext(structure(), context), ", ", offset(), ">");
        return;
        
    case Transition:
        out.print(
            "<Transition: ", inContext(oldStructure(), context), " -> ",
            pointerDumpInContext(newStructure(), context), ", [",
            listDumpInContext(constantChecks(), context), "], ", offset(), ">");
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace JSC

