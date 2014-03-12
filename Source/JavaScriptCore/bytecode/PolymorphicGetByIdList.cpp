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
#include "PolymorphicGetByIdList.h"

#if ENABLE(JIT)

#include "CodeBlock.h"
#include "Heap.h"
#include "JSCInlines.h"
#include "StructureStubInfo.h"

namespace JSC {

GetByIdAccess::GetByIdAccess(
    VM& vm, JSCell* owner, AccessType type, PassRefPtr<JITStubRoutine> stubRoutine,
    Structure* structure, StructureChain* chain, unsigned chainCount)
    : m_type(type)
    , m_chainCount(chainCount)
    , m_structure(vm, owner, structure)
    , m_stubRoutine(stubRoutine)
{
    if (chain)
        m_chain.set(vm, owner, chain);
}

GetByIdAccess::~GetByIdAccess()
{
}

GetByIdAccess GetByIdAccess::fromStructureStubInfo(StructureStubInfo& stubInfo)
{
    MacroAssemblerCodePtr initialSlowPath =
        stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToSlowCase);
    
    GetByIdAccess result;
    
    switch (stubInfo.accessType) {
    case access_get_by_id_self:
        result.m_type = SimpleInline;
        result.m_structure.copyFrom(stubInfo.u.getByIdSelf.baseObjectStructure);
        result.m_stubRoutine = JITStubRoutine::createSelfManagedRoutine(initialSlowPath);
        break;
        
    case access_get_by_id_chain:
        result.m_structure.copyFrom(stubInfo.u.getByIdChain.baseObjectStructure);
        result.m_chain.copyFrom(stubInfo.u.getByIdChain.chain);
        result.m_chainCount = stubInfo.u.getByIdChain.count;
        result.m_stubRoutine = stubInfo.stubRoutine;
        if (stubInfo.u.getByIdChain.isDirect)
            result.m_type = SimpleStub;
        else
            result.m_type = Getter;
        break;
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    return result;
}

bool GetByIdAccess::visitWeak() const
{
    if (m_structure && !Heap::isMarked(m_structure.get()))
        return false;
    if (m_chain && !Heap::isMarked(m_chain.get()))
        return false;
    return true;
}

PolymorphicGetByIdList::PolymorphicGetByIdList(StructureStubInfo& stubInfo)
{
    if (stubInfo.accessType == access_unset)
        return;
    
    m_list.append(GetByIdAccess::fromStructureStubInfo(stubInfo));
}

PolymorphicGetByIdList* PolymorphicGetByIdList::from(StructureStubInfo& stubInfo)
{
    if (stubInfo.accessType == access_get_by_id_list)
        return stubInfo.u.getByIdList.list;
    
    ASSERT(
        stubInfo.accessType == access_get_by_id_self
        || stubInfo.accessType == access_get_by_id_chain
        || stubInfo.accessType == access_unset);
    
    PolymorphicGetByIdList* result = new PolymorphicGetByIdList(stubInfo);
    
    stubInfo.initGetByIdList(result);
    
    return result;
}

PolymorphicGetByIdList::~PolymorphicGetByIdList() { }

MacroAssemblerCodePtr PolymorphicGetByIdList::currentSlowPathTarget(
    StructureStubInfo& stubInfo) const
{
    if (isEmpty())
        return stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToSlowCase);
    return m_list.last().stubRoutine()->code().code();
}

void PolymorphicGetByIdList::addAccess(const GetByIdAccess& access)
{
    ASSERT(!isFull());
    // Make sure that the resizing optimizes for space, not time.
    m_list.resize(m_list.size() + 1);
    m_list.last() = access;
}

bool PolymorphicGetByIdList::isFull() const
{
    ASSERT(size() <= POLYMORPHIC_LIST_CACHE_SIZE);
    return size() == POLYMORPHIC_LIST_CACHE_SIZE;
}

bool PolymorphicGetByIdList::isAlmostFull() const
{
    ASSERT(size() <= POLYMORPHIC_LIST_CACHE_SIZE);
    return size() >= POLYMORPHIC_LIST_CACHE_SIZE - 1;
}

bool PolymorphicGetByIdList::didSelfPatching() const
{
    for (unsigned i = size(); i--;) {
        if (at(i).type() == GetByIdAccess::SimpleInline)
            return true;
    }
    return false;
}

bool PolymorphicGetByIdList::visitWeak() const
{
    for (unsigned i = size(); i--;) {
        if (!at(i).visitWeak())
            return false;
    }
    return true;
}

} // namespace JSC

#endif // ENABLE(JIT)


