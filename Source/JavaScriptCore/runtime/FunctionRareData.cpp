/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#include "FunctionRareData.h"

#include "JSCInlines.h"
#include "ObjectAllocationProfileInlines.h"

namespace JSC {

const ClassInfo FunctionRareData::s_info = { "FunctionRareData", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(FunctionRareData) };

FunctionRareData* FunctionRareData::create(VM& vm, ExecutableBase* executable)
{
    FunctionRareData* rareData = new (NotNull, allocateCell<FunctionRareData>(vm.heap)) FunctionRareData(vm, executable);
    rareData->finishCreation(vm);
    return rareData;
}

void FunctionRareData::destroy(JSCell* cell)
{
    FunctionRareData* rareData = static_cast<FunctionRareData*>(cell);
    rareData->FunctionRareData::~FunctionRareData();
}

Structure* FunctionRareData::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
}

void FunctionRareData::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    FunctionRareData* rareData = jsCast<FunctionRareData*>(cell);
    ASSERT_GC_OBJECT_INHERITS(cell, info());
    Base::visitChildren(cell, visitor);

    rareData->m_objectAllocationProfile.visitAggregate(visitor);
    rareData->m_internalFunctionAllocationProfile.visitAggregate(visitor);
    visitor.append(rareData->m_boundFunctionStructure);
    visitor.append(rareData->m_executable);
}

FunctionRareData::FunctionRareData(VM& vm, ExecutableBase* executable)
    : Base(vm, vm.functionRareDataStructure.get())
    , m_objectAllocationProfile()
    // We initialize blind so that changes to the prototype after function creation but before
    // the first allocation don't disable optimizations. This isn't super important, since the
    // function is unlikely to allocate a rare data until the first allocation anyway.
    , m_allocationProfileWatchpointSet(ClearWatchpoint)
    , m_executable(vm, this, executable)
    , m_hasReifiedLength(false)
    , m_hasReifiedName(false)
    , m_hasModifiedLength(false)
    , m_hasModifiedName(false)
{
}

FunctionRareData::~FunctionRareData()
{
}

void FunctionRareData::initializeObjectAllocationProfile(VM& vm, JSGlobalObject* globalObject, JSObject* prototype, size_t inlineCapacity, JSFunction* constructor)
{
    initializeAllocationProfileWatchpointSet();
    m_objectAllocationProfile.initializeProfile(vm, globalObject, this, prototype, inlineCapacity, constructor, this);
}

void FunctionRareData::clear(const char* reason)
{
    m_objectAllocationProfile.clear();
    m_internalFunctionAllocationProfile.clear();
    m_allocationProfileWatchpointSet.fireAll(vm(), reason);
}

void FunctionRareData::AllocationProfileClearingWatchpoint::fireInternal(VM&, const FireDetail&)
{
    m_rareData->clear("AllocationProfileClearingWatchpoint fired.");
}

}
