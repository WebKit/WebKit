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
#include "JSAsyncGenerator.h"
#include "JSGenerator.h"
#include "JSGlobalObject.h"
#include "JSMapIterator.h"
#include "JSModuleLoader.h"
#include "JSPromise.h"
#include "JSSetIterator.h"
#include "JSStringIterator.h"
#include "LinkTimeConstant.h"
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
    m_promiseRejectionReject.set(m_vm, jsNumber(static_cast<unsigned>(JSPromiseRejectionOperation::Reject)));
    m_promiseRejectionHandle.set(m_vm, jsNumber(static_cast<unsigned>(JSPromiseRejectionOperation::Handle)));
    m_promiseStatePending.set(m_vm, jsNumber(static_cast<unsigned>(JSPromise::Status::Pending)));
    m_promiseStateFulfilled.set(m_vm, jsNumber(static_cast<unsigned>(JSPromise::Status::Fulfilled)));
    m_promiseStateRejected.set(m_vm, jsNumber(static_cast<unsigned>(JSPromise::Status::Rejected)));
    m_promiseStateMask.set(m_vm, jsNumber(JSPromise::stateMask));
    m_promiseFlagsIsHandled.set(m_vm, jsNumber(JSPromise::isHandledFlag));
    m_promiseFlagsIsFirstResolvingFunctionCalled.set(m_vm, jsNumber(JSPromise::isFirstResolvingFunctionCalledFlag));
    // FIXME: Clean up JSInternalObjectImpl field registry.
    // https://bugs.webkit.org/show_bug.cgi?id=201894
    m_promiseFieldFlags.set(m_vm, jsNumber(static_cast<unsigned>(JSPromise::Field::Flags)));
    m_promiseFieldReactionsOrResult.set(m_vm, jsNumber(static_cast<unsigned>(JSPromise::Field::ReactionsOrResult)));
    m_generatorFieldState.set(m_vm, jsNumber(static_cast<unsigned>(JSGenerator::Field::State)));
    m_generatorFieldNext.set(m_vm, jsNumber(static_cast<unsigned>(JSGenerator::Field::Next)));
    m_generatorFieldThis.set(m_vm, jsNumber(static_cast<unsigned>(JSGenerator::Field::This)));
    m_generatorFieldFrame.set(m_vm, jsNumber(static_cast<unsigned>(JSGenerator::Field::Frame)));
    m_generatorFieldContext.set(m_vm, jsNumber(static_cast<unsigned>(JSGenerator::Field::Context)));
    m_GeneratorResumeModeNormal.set(m_vm, jsNumber(static_cast<int32_t>(JSGenerator::ResumeMode::NormalMode)));
    m_GeneratorResumeModeThrow.set(m_vm, jsNumber(static_cast<int32_t>(JSGenerator::ResumeMode::ThrowMode)));
    m_GeneratorResumeModeReturn.set(m_vm, jsNumber(static_cast<int32_t>(JSGenerator::ResumeMode::ReturnMode)));
    m_GeneratorStateCompleted.set(m_vm, jsNumber(static_cast<int32_t>(JSGenerator::State::Completed)));
    m_GeneratorStateExecuting.set(m_vm, jsNumber(static_cast<int32_t>(JSGenerator::State::Executing)));
    m_arrayIteratorFieldIteratedObject.set(m_vm, jsNumber(static_cast<int32_t>(JSArrayIterator::Field::IteratedObject)));
    m_arrayIteratorFieldIndex.set(m_vm, jsNumber(static_cast<int32_t>(JSArrayIterator::Field::Index)));
    m_arrayIteratorFieldKind.set(m_vm, jsNumber(static_cast<int32_t>(JSArrayIterator::Field::Kind)));
    m_mapIteratorFieldMapBucket.set(m_vm, jsNumber(static_cast<int32_t>(JSMapIterator::Field::MapBucket)));
    m_mapIteratorFieldKind.set(m_vm, jsNumber(static_cast<int32_t>(JSMapIterator::Field::Kind)));
    m_setIteratorFieldSetBucket.set(m_vm, jsNumber(static_cast<int32_t>(JSSetIterator::Field::SetBucket)));
    m_setIteratorFieldKind.set(m_vm, jsNumber(static_cast<int32_t>(JSSetIterator::Field::Kind)));
    m_stringIteratorFieldIndex.set(m_vm, jsNumber(static_cast<int32_t>(JSStringIterator::Field::Index)));
    m_stringIteratorFieldIteratedString.set(m_vm, jsNumber(static_cast<int32_t>(JSStringIterator::Field::IteratedString)));
    m_asyncGeneratorFieldSuspendReason.set(m_vm, jsNumber(static_cast<unsigned>(JSAsyncGenerator::Field::SuspendReason)));
    m_asyncGeneratorFieldQueueFirst.set(m_vm, jsNumber(static_cast<unsigned>(JSAsyncGenerator::Field::QueueFirst)));
    m_asyncGeneratorFieldQueueLast.set(m_vm, jsNumber(static_cast<unsigned>(JSAsyncGenerator::Field::QueueLast)));
    m_AsyncGeneratorStateCompleted.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed)));
    m_AsyncGeneratorStateExecuting.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Executing)));
    m_AsyncGeneratorStateSuspendedStart.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::SuspendedStart)));
    m_AsyncGeneratorStateSuspendedYield.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::SuspendedYield)));
    m_AsyncGeneratorStateAwaitingReturn.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::AwaitingReturn)));
    m_AsyncGeneratorSuspendReasonYield.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::Yield)));
    m_AsyncGeneratorSuspendReasonAwait.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::Await)));
    m_AsyncGeneratorSuspendReasonNone.set(m_vm, jsNumber(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::None)));
    m_abstractModuleRecordFieldState.set(m_vm, jsNumber(static_cast<int32_t>(AbstractModuleRecord::Field::State)));
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

JSValue BytecodeIntrinsicRegistry::sentinelMapBucketValue(BytecodeGenerator& generator)
{
    return generator.vm().sentinelMapBucket();
}

JSValue BytecodeIntrinsicRegistry::sentinelSetBucketValue(BytecodeGenerator& generator)
{
    return generator.vm().sentinelSetBucket();
}

} // namespace JSC

