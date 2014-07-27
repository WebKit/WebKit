/*
 * Copyright (C) 2012, 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SymbolTable.h"

#include "JSDestructibleObject.h"
#include "JSCInlines.h"
#include "SlotVisitorInlines.h"
#include "VariableWatchpointSetInlines.h"

namespace JSC {

const ClassInfo SymbolTable::s_info = { "SymbolTable", 0, 0, 0, CREATE_METHOD_TABLE(SymbolTable) };

SymbolTableEntry& SymbolTableEntry::copySlow(const SymbolTableEntry& other)
{
    ASSERT(other.isFat());
    FatEntry* newFatEntry = new FatEntry(*other.fatEntry());
    freeFatEntry();
    m_bits = bitwise_cast<intptr_t>(newFatEntry);
    return *this;
}

void SymbolTable::destroy(JSCell* cell)
{
    SymbolTable* thisObject = jsCast<SymbolTable*>(cell);
    thisObject->SymbolTable::~SymbolTable();
}

void SymbolTableEntry::freeFatEntrySlow()
{
    ASSERT(isFat());
    delete fatEntry();
}

JSValue SymbolTableEntry::inferredValue()
{
    if (!isFat())
        return JSValue();
    return fatEntry()->m_watchpoints->inferredValue();
}

void SymbolTableEntry::prepareToWatch(SymbolTable* symbolTable)
{
    FatEntry* entry = inflate();
    if (entry->m_watchpoints)
        return;
    entry->m_watchpoints = adoptRef(new VariableWatchpointSet(*symbolTable));
}

void SymbolTableEntry::addWatchpoint(Watchpoint* watchpoint)
{
    fatEntry()->m_watchpoints->add(watchpoint);
}

void SymbolTableEntry::notifyWriteSlow(VM& vm, JSValue value)
{
    VariableWatchpointSet* watchpoints = fatEntry()->m_watchpoints.get();
    if (!watchpoints)
        return;
    
    watchpoints->notifyWrite(vm, value);
}

SymbolTableEntry::FatEntry* SymbolTableEntry::inflateSlow()
{
    FatEntry* entry = new FatEntry(m_bits);
    m_bits = bitwise_cast<intptr_t>(entry);
    return entry;
}

SymbolTable::SymbolTable(VM& vm)
    : JSCell(vm, vm.symbolTableStructure.get())
    , m_parameterCountIncludingThis(0)
    , m_usesNonStrictEval(false)
    , m_captureStart(0)
    , m_captureEnd(0)
    , m_functionEnteredOnce(ClearWatchpoint)
{
    if (vm.isProfilingTypesWithHighFidelity()) {
        m_uniqueIDMap = std::make_unique<UniqueIDMap>();
        m_registerToVariableMap = std::make_unique<RegisterToVariableMap>();
        m_uniqueTypeSetMap = std::make_unique<UniqueTypeSetMap>();
    }
}

SymbolTable::~SymbolTable() { }

void SymbolTable::visitChildren(JSCell* thisCell, SlotVisitor& visitor)
{
    SymbolTable* thisSymbolTable = jsCast<SymbolTable*>(thisCell);
    if (!thisSymbolTable->m_watchpointCleanup) {
        thisSymbolTable->m_watchpointCleanup =
            std::make_unique<WatchpointCleanup>(thisSymbolTable);
    }
    
    visitor.addUnconditionalFinalizer(thisSymbolTable->m_watchpointCleanup.get());
}

SymbolTable::WatchpointCleanup::WatchpointCleanup(SymbolTable* symbolTable)
    : m_symbolTable(symbolTable)
{
}

SymbolTable::WatchpointCleanup::~WatchpointCleanup() { }

void SymbolTable::WatchpointCleanup::finalizeUnconditionally()
{
    Map::iterator iter = m_symbolTable->m_map.begin();
    Map::iterator end = m_symbolTable->m_map.end();
    for (; iter != end; ++iter) {
        if (VariableWatchpointSet* set = iter->value.watchpointSet())
            set->finalizeUnconditionally();
    }
}

SymbolTable* SymbolTable::cloneCapturedNames(VM& vm)
{
    SymbolTable* result = SymbolTable::create(vm);
    
    result->m_parameterCountIncludingThis = m_parameterCountIncludingThis;
    result->m_usesNonStrictEval = m_usesNonStrictEval;
    result->m_captureStart = m_captureStart;
    result->m_captureEnd = m_captureEnd;

    for (auto iter = m_map.begin(), end = m_map.end(); iter != end; ++iter) {
        if (!isCaptured(iter->value.getIndex()))
            continue;
        result->m_map.add(
            iter->key,
            SymbolTableEntry(iter->value.getIndex(), iter->value.getAttributes()));
    }
    
    if (m_slowArguments) {
        result->m_slowArguments = std::make_unique<SlowArgument[]>(parameterCount());
        for (unsigned i = parameterCount(); i--;)
            result->m_slowArguments[i] = m_slowArguments[i];
    }

    if (m_uniqueIDMap && result->m_uniqueIDMap) {

        {
            auto iter = m_uniqueIDMap->begin();
            auto end = m_uniqueIDMap->end();
            for (; iter != end; ++iter)
                result->m_uniqueIDMap->set(iter->key, iter->value);
        }

        {
            auto iter = m_registerToVariableMap->begin();
            auto end = m_registerToVariableMap->end();
            for (; iter != end; ++iter)
                result->m_registerToVariableMap->set(iter->key, iter->value);
        }

        {
            auto iter = m_uniqueTypeSetMap->begin();
            auto end = m_uniqueTypeSetMap->end();
            for (; iter != end; ++iter)
                result->m_uniqueTypeSetMap->set(iter->key, iter->value);
        }
    }

    
    return result;
}

int64_t SymbolTable::uniqueIDForVariable(const ConcurrentJITLocker&, StringImpl* key, VM& vm)
{
    auto iter = m_uniqueIDMap->find(key);
    auto end = m_uniqueIDMap->end();
    ASSERT_UNUSED(end, iter != end);

    int64_t& id = iter->value;
    if (id == HighFidelityNeedsUniqueIDGeneration) {
        id = vm.getNextUniqueVariableID();
        m_uniqueTypeSetMap->set(key, TypeSet::create()); //make a new global typeset for the ID
    }
         
    return id;
}

int64_t SymbolTable::uniqueIDForRegister(const ConcurrentJITLocker& locker, int registerIndex, VM& vm)
{
    auto iter = m_registerToVariableMap->find(registerIndex);
    auto end = m_registerToVariableMap->end();
    ASSERT_UNUSED(end, iter != end);
    return uniqueIDForVariable(locker, iter->value.get(), vm);
}

RefPtr<TypeSet> SymbolTable::globalTypeSetForRegister(const ConcurrentJITLocker& locker, int registerIndex, VM& vm)
{
    uniqueIDForRegister(locker, registerIndex, vm); //ensure it's created
    auto iter = m_registerToVariableMap->find(registerIndex);
    auto end = m_registerToVariableMap->end();
    ASSERT_UNUSED(end, iter != end);
    return globalTypeSetForVariable(locker, iter->value.get(), vm);
}

RefPtr<TypeSet> SymbolTable::globalTypeSetForVariable(const ConcurrentJITLocker& locker, StringImpl* key, VM& vm)
{
    uniqueIDForVariable(locker, key, vm);
    return m_uniqueTypeSetMap->find(key)->value;
}

} // namespace JSC

