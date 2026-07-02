/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "BytecodeIntrinsicRegistry.h"

#include "AbstractModuleRecord.h"
#include "BuiltinNames.h"
#include "BytecodeGenerator.h"
#include "GlobalObjectMethodTable.h"
#include "IdentifierInlines.h"
#include "IterationKind.h"
#include "JSArrayIterator.h"
#include "JSAsyncDisposableStack.h"
#include "JSAsyncFromSyncIterator.h"
#include "JSAsyncGenerator.h"
#include "JSDisposableStack.h"
#include "JSGenerator.h"
#include "JSGlobalObject.h"
#include "JSIteratorHelper.h"
#include "JSMapIterator.h"
#include "JSModuleLoader.h"
#include "JSPromise.h"
#include "JSSetIterator.h"
#include "JSStringIterator.h"
#include "JSWrapForValidIterator.h"
#include "LinkTimeConstant.h"
#include "Microtask.h"
#include "Nodes.h"
#include "StrongInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(BytecodeIntrinsicRegistry);

#define INITIALIZE_BYTECODE_INTRINSIC_NAMES_TO_SET(name) m_bytecodeIntrinsicMap.add(vm.propertyNames->builtinNames().name##PrivateName().impl(), Entry(&BytecodeIntrinsicNode::emit_intrinsic_##name));
#define INITIALIZE_BYTECODE_INTRINSIC_NAMES_TO_SET_FOR_LINK_TIME_CONSTANT(name, code) m_bytecodeIntrinsicMap.add(vm.propertyNames->builtinNames().name##PrivateName().impl(), JSC::LinkTimeConstant::name);

BytecodeIntrinsicRegistry::BytecodeIntrinsicRegistry(VM& vm)
    : m_vm(vm)
    , m_bytecodeIntrinsicMap()
{
    JSC_COMMON_BYTECODE_INTRINSIC_FUNCTIONS_EACH_NAME(INITIALIZE_BYTECODE_INTRINSIC_NAMES_TO_SET)
    JSC_COMMON_BYTECODE_INTRINSIC_CONSTANTS_EACH_NAME(INITIALIZE_BYTECODE_INTRINSIC_NAMES_TO_SET)
    JSC_FOREACH_LINK_TIME_CONSTANTS(INITIALIZE_BYTECODE_INTRINSIC_NAMES_TO_SET_FOR_LINK_TIME_CONSTANT)

    m_undefined.set(m_vm, jsUndefined());
    m_Infinity.set(m_vm, jsDoubleNumber(std::numeric_limits<double>::infinity()));
    m_iterationKindKey.set(m_vm, jsNumber(static_cast<unsigned>(IterationKind::Keys)));
    m_iterationKindValue.set(m_vm, jsNumber(static_cast<unsigned>(IterationKind::Values)));
    m_iterationKindEntries.set(m_vm, jsNumber(static_cast<unsigned>(IterationKind::Entries)));
    m_MAX_ARRAY_INDEX.set(m_vm, jsNumber(MAX_ARRAY_INDEX));
    m_MAX_STRING_LENGTH.set(m_vm, jsNumber(JSString::MaxLength));
    m_MAX_SAFE_INTEGER.set(m_vm, jsDoubleNumber(maxSafeInteger()));
    m_ModuleFetch.set(m_vm, jsNumber(static_cast<unsigned>(JSModuleLoader::Status::Fetch)));
    m_ModuleInstantiate.set(m_vm, jsNumber(static_cast<unsigned>(JSModuleLoader::Status::Instantiate)));
    m_ModuleSatisfy.set(m_vm, jsNumber(static_cast<unsigned>(JSModuleLoader::Status::Satisfy)));
    m_ModuleLink.set(m_vm, jsNumber(static_cast<unsigned>(JSModuleLoader::Status::Link)));
    m_ModuleReady.set(m_vm, jsNumber(static_cast<unsigned>(JSModuleLoader::Status::Ready)));
    m_generatorFieldState.set(m_vm, jsNumber(static_cast<unsigned>(JSGenerator::Field::State)));
    m_generatorFieldNext.set(m_vm, jsNumber(static_cast<unsigned>(JSGenerator::Field::Next)));
    m_generatorFieldThis.set(m_vm, jsNumber(static_cast<unsigned>(JSGenerator::Field::This)));
    m_generatorFieldFrame.set(m_vm, jsNumber(static_cast<unsigned>(JSGenerator::Field::Frame)));
    m_GeneratorResumeModeNormal.set(m_vm, jsNumber(static_cast<int32_t>(JSGenerator::ResumeMode::NormalMode)));
    m_GeneratorResumeModeThrow.set(m_vm, jsNumber(static_cast<int32_t>(JSGenerator::ResumeMode::ThrowMode)));
    m_GeneratorResumeModeReturn.set(m_vm, jsNumber(static_cast<int32_t>(JSGenerator::ResumeMode::ReturnMode)));
    m_GeneratorStateCompleted.set(m_vm, jsNumber(static_cast<int32_t>(JSGenerator::State::Completed)));
    m_GeneratorStateExecuting.set(m_vm, jsNumber(static_cast<int32_t>(JSGenerator::State::Executing)));
    m_GeneratorStateInit.set(m_vm, jsNumber(static_cast<int32_t>(JSGenerator::State::Init)));
    m_arrayIteratorFieldIteratedObject.set(m_vm, jsNumber(static_cast<int32_t>(JSArrayIterator::Field::IteratedObject)));
    m_arrayIteratorFieldIndex.set(m_vm, jsNumber(static_cast<int32_t>(JSArrayIterator::Field::Index)));
    m_arrayIteratorFieldKind.set(m_vm, jsNumber(static_cast<int32_t>(JSArrayIterator::Field::Kind)));

    m_asyncGeneratorFieldQueue.set(m_vm, jsNumber(static_cast<unsigned>(JSAsyncGenerator::Field::Queue)));
    m_asyncGeneratorFieldResumeValue.set(m_vm, jsNumber(static_cast<unsigned>(JSAsyncGenerator::Field::ResumeValue)));
    m_asyncGeneratorFieldResumeMode.set(m_vm, jsNumber(static_cast<unsigned>(JSAsyncGenerator::Field::ResumeMode)));
    m_asyncGeneratorFieldResumePromise.set(m_vm, jsNumber(static_cast<unsigned>(JSAsyncGenerator::Field::ResumePromise)));
    m_AsyncGeneratorResumeModeEmpty.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorResumeMode::Empty)));
    m_AsyncGeneratorStateCompleted.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed)));
    m_AsyncGeneratorStateExecuting.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Executing)));
    m_AsyncGeneratorStateInit.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Init)));
    m_AsyncGeneratorStateDrainingQueue.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::DrainingQueue)));
    m_AsyncGeneratorSuspendReasonShift.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncGenerator::reasonShift)));
    m_asyncFromSyncIteratorFieldSyncIterator.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncFromSyncIterator::Field::SyncIterator)));
    m_asyncFromSyncIteratorFieldNextMethod.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncFromSyncIterator::Field::NextMethod)));
    m_abstractModuleRecordFieldState.set(m_vm, jsNumber(static_cast<int32_t>(AbstractModuleRecord::Field::State)));
    m_wrapForValidIteratorFieldIteratedIterator.set(m_vm, jsNumber(static_cast<int32_t>(JSWrapForValidIterator::Field::IteratedIterator)));
    m_wrapForValidIteratorFieldIteratedNextMethod.set(m_vm, jsNumber(static_cast<int32_t>(JSWrapForValidIterator::Field::IteratedNextMethod)));
    m_iteratorHelperFieldGenerator.set(m_vm, jsNumber(static_cast<int32_t>(JSIteratorHelper::Field::Generator)));
    m_iteratorHelperFieldUnderlyingIterator.set(m_vm, jsNumber(static_cast<int32_t>(JSIteratorHelper::Field::UnderlyingIterator)));
    m_disposableStackFieldState.set(m_vm, jsNumber(static_cast<int32_t>(JSDisposableStack::Field::State)));
    m_disposableStackFieldCapability.set(m_vm, jsNumber(static_cast<int32_t>(JSDisposableStack::Field::Capability)));
    m_DisposableStackStatePending.set(m_vm, jsNumber(static_cast<int32_t>(JSDisposableStack::State::Pending)));
    m_DisposableStackStateDisposed.set(m_vm, jsNumber(static_cast<int32_t>(JSDisposableStack::State::Disposed)));
    m_asyncDisposableStackFieldState.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncDisposableStack::Field::State)));
    m_asyncDisposableStackFieldCapability.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncDisposableStack::Field::Capability)));
    m_AsyncDisposableStackStatePending.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncDisposableStack::State::Pending)));
    m_AsyncDisposableStackStateDisposed.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncDisposableStack::State::Disposed)));
    m_InternalMicrotaskAsyncFromSyncIteratorContinue.set(m_vm, jsNumber(static_cast<int32_t>(InternalMicrotask::AsyncFromSyncIteratorContinue)));
    m_InternalMicrotaskAsyncFromSyncIteratorDone.set(m_vm, jsNumber(static_cast<int32_t>(InternalMicrotask::AsyncFromSyncIteratorDone)));
}

std::optional<BytecodeIntrinsicRegistry::Entry> BytecodeIntrinsicRegistry::lookup(const Identifier& ident) const
{
    if (!ident.isPrivateName())
        return std::nullopt;
    auto iterator = m_bytecodeIntrinsicMap.find(ident.impl());
    if (iterator == m_bytecodeIntrinsicMap.end())
        return std::nullopt;
    return iterator->value;
}

#define JSC_DECLARE_BYTECODE_INTRINSIC_CONSTANT_GENERATORS(name) \
    JSValue BytecodeIntrinsicRegistry::name##Value(BytecodeGenerator&) \
    { \
        return m_##name.get(); \
    }
    JSC_COMMON_BYTECODE_INTRINSIC_CONSTANTS_SIMPLE_EACH_NAME(JSC_DECLARE_BYTECODE_INTRINSIC_CONSTANT_GENERATORS)
#undef JSC_DECLARE_BYTECODE_INTRINSIC_CONSTANT_GENERATORS

JSValue BytecodeIntrinsicRegistry::orderedHashTableSentinelValue(BytecodeGenerator& generator)
{
    return generator.vm().orderedHashTableSentinel();
}

} // namespace JSC

