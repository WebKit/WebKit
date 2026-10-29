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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/ObjectIdentifier.h>
#include <wtf/Platform.h>

namespace JSC {

enum class MicrotaskIdentifierType { };
using MicrotaskIdentifier = ObjectIdentifier<MicrotaskIdentifierType>;

#if ENABLE(WEBASSEMBLY)
#define JSC_FOR_EACH_WEBASSEMBLY_INTERNAL_MICROTASK(macro) \
    macro(WebAssemblyCompileStreaming) \
    macro(WebAssemblyInstantiateStreaming) \

#else
#define JSC_FOR_EACH_WEBASSEMBLY_INTERNAL_MICROTASK(macro)
#endif

// runInternalMicrotask dispatches each of these to a function named run<name>. Running None or Opaque
// there is a bug: Opaque tasks are run entirely by their dispatcher.
#define JSC_FOR_EACH_INTERNAL_MICROTASK(macro) \
    macro(None) \
    macro(PromiseResolveThenableJobFast) \
    macro(PromiseResolveThenableJobWithInternalMicrotaskFast) \
    macro(PromiseResolveThenableJob) \
    macro(PromiseResolveThenableJobWithInternalMicrotask) \
    macro(PromiseResolveWithoutHandlerJob) \
    macro(PromiseFulfillWithoutHandlerJob) \
    macro(PromiseRaceResolveJob) \
    macro(PromiseAllResolveJob) \
    macro(PromiseAllSettledResolveJob) \
    macro(PromiseAnyResolveJob) \
    macro(PromiseFinallyReactionJob) \
    macro(PromiseFinallyAwaitJob) \
    macro(PromiseReactionJob) \
    macro(AsyncFunctionResume) \
    macro(AsyncFromSyncIteratorContinue) \
    macro(AsyncFromSyncIteratorDone) \
    macro(AsyncGeneratorYieldAwaited) \
    macro(AsyncGeneratorBodyCallNormal) \
    macro(AsyncGeneratorBodyCallReturn) \
    macro(AsyncGeneratorAwaitReturn) \
    macro(AsyncGeneratorDriverResume) \
    macro(InvokeFunctionJob) \
    macro(AsyncModuleExecutionResume) \
    macro(AsyncModuleExecutionDone) \
    macro(ModuleRegistryFetchSettled) \
    macro(ModuleRegistryModuleSettled) \
    macro(ModuleGraphLoadingError) \
    macro(ModuleLoadStep) \
    macro(ModuleLoadTopSettled) \
    macro(ModuleLoadTopRejected) \
    macro(ModuleLoadSpecifierTransform) \
    macro(ModuleLoadCombinedLoadSettled) \
    macro(ModuleLoadCombinedStateSettled) \
    macro(ModuleLoadLinkEvaluateSettled) \
    macro(ModuleLoadReturnRecord) \
    macro(ModuleLoadReturnModuleKey) \
    macro(ModuleLoadStoreError) \
    macro(DynamicImportLoadSettled) \
    macro(DynamicImportEvaluateSettled) \
    macro(DynamicImportDeferLoadSettled) \
    macro(DynamicImportDeferDependencySettled) \
    macro(ImportModuleNamespace) \
    JSC_FOR_EACH_WEBASSEMBLY_INTERNAL_MICROTASK(macro) \
    macro(Opaque) \

enum class InternalMicrotask : uint8_t {
#define JSC_DECLARE_INTERNAL_MICROTASK(name) name,
    JSC_FOR_EACH_INTERNAL_MICROTASK(JSC_DECLARE_INTERNAL_MICROTASK)
#undef JSC_DECLARE_INTERNAL_MICROTASK
};
static_assert(!static_cast<uint8_t>(InternalMicrotask::None));

constexpr unsigned maxMicrotaskArguments = 3;

// True for Promise.all/allSettled/any element jobs, whose reaction packs
// (globalContext cell, element index) instead of a single context cell.
constexpr bool promiseReactionPacksGlobalContextAndIndex(InternalMicrotask task)
{
    static_assert(static_cast<uint8_t>(InternalMicrotask::PromiseAllSettledResolveJob) == static_cast<uint8_t>(InternalMicrotask::PromiseAllResolveJob) + 1);
    static_assert(static_cast<uint8_t>(InternalMicrotask::PromiseAnyResolveJob) == static_cast<uint8_t>(InternalMicrotask::PromiseAllSettledResolveJob) + 1);
    return task >= InternalMicrotask::PromiseAllResolveJob && task <= InternalMicrotask::PromiseAnyResolveJob;
}

enum class QueuedTaskResult : uint8_t {
    Executed,
    Discard,
    Suspended,
};

} // namespace JSC
