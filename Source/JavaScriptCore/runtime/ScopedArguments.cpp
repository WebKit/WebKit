/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include "ScopedArguments.h"

#include "GenericArgumentsImplInlines.h"
#include "JSArray.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ScopedArguments);

const ClassInfo ScopedArguments::s_info = { "Arguments"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ScopedArguments) };

ScopedArguments::ScopedArguments(VM& vm, Structure* structure, WriteBarrier<Unknown>* storage, unsigned totalLength, JSFunction* callee, ScopedArgumentsTable* table, JSLexicalEnvironment* scope)
    : GenericArgumentsImpl(vm, structure)
    , m_totalLength(totalLength)
    , m_callee(callee, WriteBarrierEarlyInit)
    , m_table(table, WriteBarrierEarlyInit)
    , m_scope(scope, WriteBarrierEarlyInit)
    , m_storage(storage, WriteBarrierEarlyInit)
{
}

ScopedArguments* ScopedArguments::createUninitialized(VM& vm, Structure* structure, JSFunction* callee, ScopedArgumentsTable* table, JSLexicalEnvironment* scope, unsigned totalLength)
{
    WriteBarrier<Unknown>* storage = nullptr;
    if (totalLength > table->length()) {
        Checked<unsigned> overflowLength = totalLength - table->length();
        storage = static_cast<WriteBarrier<Unknown>*>(vm.auxiliarySpace().allocate(vm, overflowLength * sizeof(WriteBarrier<Unknown>), nullptr, AllocationFailureMode::Assert));
    }

    ScopedArguments* result = new (
        NotNull,
        allocateCell<ScopedArguments>(vm))
        ScopedArguments(vm, structure, storage, totalLength, callee, table, scope);
    result->finishCreation(vm);
    return result;
}

ScopedArguments* ScopedArguments::create(VM& vm, Structure* structure, JSFunction* callee, ScopedArgumentsTable* table, JSLexicalEnvironment* scope, unsigned totalLength)
{
    ScopedArguments* result =
        createUninitialized(vm, structure, callee, table, scope, totalLength);

    unsigned namedLength = table->length();
    for (unsigned i = namedLength; i < totalLength; ++i)
        result->storage()[i - namedLength].clear();
    
    return result;
}

ScopedArguments* ScopedArguments::createByCopying(JSGlobalObject* globalObject, CallFrame* callFrame, ScopedArgumentsTable* table, JSLexicalEnvironment* scope)
{
    return createByCopyingFrom(
        globalObject->vm(), globalObject->scopedArgumentsStructure(),
        callFrame->registers() + CallFrame::argumentOffset(0), callFrame->argumentCount(),
        jsCast<JSFunction*>(callFrame->jsCallee()), table, scope);
}

ScopedArguments* ScopedArguments::createByCopyingFrom(VM& vm, Structure* structure, Register* argumentsStart, unsigned totalLength, JSFunction* callee, ScopedArgumentsTable* table, JSLexicalEnvironment* scope)
{
    ScopedArguments* result =
        createUninitialized(vm, structure, callee, table, scope, totalLength);
    
    unsigned namedLength = table->length();
    for (unsigned i = namedLength; i < totalLength; ++i)
        result->storage()[i - namedLength].set(vm, result, argumentsStart[i].jsValue());
    
    return result;
}

template<typename Visitor>
void ScopedArguments::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    ScopedArguments* thisObject = static_cast<ScopedArguments*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    GenericArgumentsImpl::visitChildren(thisObject, visitor); // Including Base::visitChildren.

    visitor.append(thisObject->m_callee);
    visitor.append(thisObject->m_table);
    visitor.append(thisObject->m_scope);
    
    if (WriteBarrier<Unknown>* storage = thisObject->m_storage.get()) {
        visitor.markAuxiliary(storage);
        if (thisObject->m_totalLength > thisObject->m_table->length())
            visitor.appendValues(storage, thisObject->m_totalLength - thisObject->m_table->length());
    }
}

DEFINE_VISIT_CHILDREN(ScopedArguments);

Structure* ScopedArguments::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ScopedArgumentsType, StructureFlags), info());
}

void ScopedArguments::overrideThings(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();

    RELEASE_ASSERT(!m_overrodeThings);
    
    putDirect(vm, vm.propertyNames->length, jsNumber(m_table->length()), static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirect(vm, vm.propertyNames->callee, m_callee.get(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirect(vm, vm.propertyNames->iteratorSymbol, globalObject->arrayProtoValuesFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    
    m_overrodeThings = true;
}

void ScopedArguments::overrideThingsIfNecessary(JSGlobalObject* globalObject)
{
    if (!m_overrodeThings)
        overrideThings(globalObject);
}

void ScopedArguments::unmapArgument(JSGlobalObject* globalObject, uint32_t i)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT_WITH_SECURITY_IMPLICATION(i < m_totalLength);
    m_hasUnmappedArgument = true;
    unsigned namedLength = m_table->length();
    if (i < namedLength) {
        auto* maybeCloned = m_table->trySet(vm, i, ScopeOffset());
        if (!maybeCloned) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return;
        }
        m_table.set(vm, this, maybeCloned);
        m_table->clearWatchpointSet(i);
    } else
        storage()[i - namedLength].clear();
}

void ScopedArguments::copyToArguments(JSGlobalObject* globalObject, JSValue* firstElementDest, unsigned offset, unsigned length)
{
    GenericArgumentsImpl::copyToArguments(globalObject, firstElementDest, offset, length);
}

bool ScopedArguments::isIteratorProtocolFastAndNonObservable()
{
    Structure* structure = this->structure();
    JSGlobalObject* globalObject = structure->globalObject();
    if (!globalObject->isArgumentsPrototypeIteratorProtocolFastAndNonObservable())
        return false;

    if (m_overrodeThings) [[unlikely]]
        return false;

    if (m_hasUnmappedArgument) [[unlikely]]
        return false;

    if (structure->didTransition())
        return false;

    return true;
}

JSArray* ScopedArguments::fastSlice(JSGlobalObject* globalObject, ScopedArguments* arguments, uint64_t startIndex, uint64_t count)
{
    VM& vm = globalObject->vm();

    if (count >= MIN_SPARSE_ARRAY_INDEX)
        return nullptr;

    if (arguments->m_overrodeThings) [[unlikely]]
        return nullptr;

    if (arguments->m_hasUnmappedArgument) [[unlikely]]
        return nullptr;

    if (startIndex + count > arguments->m_totalLength)
        return nullptr;

    uint32_t resultLength = static_cast<uint32_t>(count);

    if (!resultLength)
        return constructEmptyArray(globalObject, nullptr);

    // Determine the optimal indexing type based on the values being sliced
    IndexingType indexingType = IsArray;
    for (uint32_t i = 0; i < resultLength; ++i) {
        JSValue value = arguments->getIndexQuickly(startIndex + i);
        indexingType = leastUpperBoundOfIndexingTypeAndValue(indexingType, value);
    }

    Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingType);
    IndexingType resultIndexingType = resultStructure->indexingType();

    if (hasAnyArrayStorage(resultIndexingType)) [[unlikely]]
        return nullptr;

    ASSERT(!globalObject->isHavingABadTime());

    unsigned vectorLength = Butterfly::optimalContiguousVectorLength(resultStructure, resultLength);
    void* memory = vm.auxiliarySpace().allocate(
        vm,
        Butterfly::totalSize(0, 0, true, vectorLength * sizeof(EncodedJSValue)),
        nullptr, AllocationFailureMode::ReturnNull);
    if (!memory) [[unlikely]]
        return nullptr;

    DeferGC deferGC(vm);
    auto* resultButterfly = Butterfly::fromBase(memory, 0, 0);
    resultButterfly->setVectorLength(vectorLength);
    resultButterfly->setPublicLength(resultLength);

    if (hasDouble(resultIndexingType)) {
        for (uint32_t i = 0; i < resultLength; ++i) {
            JSValue value = arguments->getIndexQuickly(startIndex + i);
            ASSERT(value.isNumber());
            resultButterfly->contiguousDouble().atUnsafe(i) = value.asNumber();
        }
    } else if (hasInt32(resultIndexingType) || hasContiguous(resultIndexingType)) {
        for (uint32_t i = 0; i < resultLength; ++i) {
            JSValue value = arguments->getIndexQuickly(startIndex + i);
            resultButterfly->contiguous().atUnsafe(i).setWithoutWriteBarrier(value);
        }
    } else
        RELEASE_ASSERT_NOT_REACHED();

    Butterfly::clearRange(resultIndexingType, resultButterfly, resultLength, vectorLength);
    return JSArray::createWithButterfly(vm, nullptr, resultStructure, resultButterfly);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
