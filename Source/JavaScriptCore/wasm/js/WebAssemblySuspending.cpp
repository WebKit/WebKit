/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "WebAssemblySuspending.h"

#if ENABLE(WEBASSEMBLY)

#include "ArgList.h"
#include "Debugger.h"
#include "EvacuatedStack.h"
#include "ExceptionHelpers.h"
#include "InterpreterInlines.h"
#include "JSFunctionWithFields.h"
#include "JSObjectInlines.h"
#include "JSPromise.h"
#include "JSWebAssemblySuspendError.h"
#include "PinballCompletion.h"

#include <wtf/HexNumber.h>

#if ASAN_ENABLED
#include <sanitizer/asan_interface.h>
#endif

namespace JSC {

extern "C" JSC_DECLARE_HOST_FUNCTION(enterWebAssemblySuspendingFunction);
JSC_DECLARE_HOST_FUNCTION(runWebAssemblySuspendingFunction);

enum class SlicingOutcome {
    None,
    Success,
    Overrun,
    Error
};

enum class SlicingStrategy {
    // The entire Wasm stack is captured as a single slice.
    Slab,
    // One Wasm frame per slice, with entry/exit frames merged.
    Frag,
    // Slab initially, Frag on resuspension.
    Mixed
};

constexpr SlicingStrategy slicingStrategy = SlicingStrategy::Slab; // currently only Slab is fully supported

template<typename T>
struct SlicerDriver {

    static SlicingOutcome slice(VM& vm, CallFrame* callFrame, String& errorMessage, Vector<std::unique_ptr<EvacuatedStackSlice>>& slices, CallFrame*& teleportFrame)
    {
        T slicer;
        StackSlicerFunctor<T> functor(vm, slicer);
        StackVisitor::visit(callFrame, vm, functor);

        if (!slicer.succeeded()) {
            errorMessage = slicer.errorMessage();
            return slicer.didOverrun() ? SlicingOutcome::Overrun : SlicingOutcome::Error;
        }
        slices = slicer.reverseAndTakeSlices();
        teleportFrame = slicer.teleportFrame();
        return SlicingOutcome::Success;
    }
};

extern "C" void* SYSV_ABI runWebAssemblySuspendingFunction(JSGlobalObject* globalObject, CallFrame* callFrame, CPURegister* originalCalleeSaves);

// Executes when a function produced by the expression 'new WebAssembly.Suspending(wrappedFunction)'
// is called. The initial entry point is the offlineasm function
// 'enterWebAssemblySuspendingFunction', which calls here. In the entry function we capture the
// original values of callee saves before they've been tampered with by this function's prologue.
// Those callee saves serve two purposes: 1) they are the initial state when walking the stack
// looking for the teleport target; 2) they are saved in the PinballCompletion and later used as the
// initial state of callee saves for running the suspended Wasm frames.
//
// Returns the FP the entry function should teleport to to skip the evacuated frames, or a nullptr
// for a normal return.

void* runWebAssemblySuspendingFunction(JSGlobalObject* globalObject, CallFrame* callFrame, CPURegister* originalCalleeSaves)
{
    VM& vm = globalObject->vm();

    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!vm.topJSPIContext) {
        throwException(globalObject, scope, createJSWebAssemblySuspendError(globalObject, vm, "Suspending() wrapper called outside of a promising() context"_s));
        return { };
    }

    CPURegister* vmEntryFrameCalleeSaves = vmEntryRecord(vm.topEntryFrame)->calleeSaveRegistersBuffer;
    memcpySpan(std::span<CPURegister>(vmEntryFrameCalleeSaves, NUMBER_OF_CALLEE_SAVES_REGISTERS), std::span(originalCalleeSaves, NUMBER_OF_CALLEE_SAVES_REGISTERS));

    JSObject* callee = callFrame->jsCallee();
    JSFunctionWithFields* self = jsCast<JSFunctionWithFields*>(callee);
    JSValue callable = self->getField(JSFunctionWithFields::Field::WebAssemblySuspendingWrappedCallable);

    MarkedArgumentBuffer args;
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i)
        args.append(callFrame->uncheckedArgument(i));
    if (args.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    auto callData = JSC::getCallData(callable);
    if (callData.type == CallData::Type::None) [[unlikely]] {
        throwTypeError(globalObject, scope, "Object is not callable"_s);
        return { };
    }

    JSValue result = call(globalObject, callable, callData, jsUndefined(), args);
    RETURN_IF_EXCEPTION(scope, { });

    JSPromise* promise = jsDynamicCast<JSPromise*>(result);
    if (!promise) {
        // The spec requires us to suspend even if the wrapped function returned a real value.
        promise = JSPromise::create(vm, globalObject->promiseStructure());
        promise->resolve(globalObject, vm, result);
        RETURN_IF_EXCEPTION(scope, { });
    }

    String errorMessage;
    Vector<std::unique_ptr<EvacuatedStackSlice>> slices;
    CallFrame* returnOutOfFrame = nullptr;
    SlicingOutcome outcome = SlicingOutcome::None;

    // There are multiple ways of slicing the wasm stack here. How to pick the best one is something
    // we will have to research on real workloads. It will likely be some adaptive scheme, perhaps
    // with profile data associated with the promising wrapper. For now the default strategy is
    // 'slab', and it's the only one that does the right thing for exceptions. The strategy choice
    // option is for experimentation only.
    switch (slicingStrategy) {
    case SlicingStrategy::Slab:
        outcome = SlicerDriver<SlabSlicer>::slice(vm, callFrame, errorMessage, slices, returnOutOfFrame);
        break;
    case SlicingStrategy::Frag:
        outcome = SlicerDriver<FragSlicer>::slice(vm, callFrame, errorMessage, slices, returnOutOfFrame);
        break;
    case SlicingStrategy::Mixed:
        if (vm.topJSPIContext->purpose == JSPIContext::Purpose::Promising)
            outcome = SlicerDriver<SlabSlicer>::slice(vm, callFrame, errorMessage, slices, returnOutOfFrame);
        else
            outcome = SlicerDriver<FragSlicer>::slice(vm, callFrame, errorMessage, slices, returnOutOfFrame);
        break;
    }

    if (outcome == SlicingOutcome::Overrun) {
        auto* error = createJSWebAssemblySuspendError(globalObject, vm, "JavaScript frames found between WebAssembly.Suspending and WebAssembly.promising"_s);
        throwException(globalObject, scope, error);
        return { };
    }
    if (outcome == SlicingOutcome::Error) {
        auto errorString = makeString("JSPI stack scan failed: "_s, errorMessage);
        throwVMError(globalObject, scope, errorString);
        return { };
    }
    ASSERT(outcome == SlicingOutcome::Success);

    auto* pinball = PinballCompletion::create(vm, WTF::move(slices), originalCalleeSaves, vm.topJSPIContext->resultPromise);
    vm.topJSPIContext->completion = pinball;

    auto* fulfiller = createPinballCompletionFulfillHandler(vm, globalObject, pinball);
    auto* rejecter = createPinballCompletionRejectHandler(vm, globalObject, pinball);
    promise->performPromiseThen(vm, globalObject, fulfiller, rejecter, jsUndefined());

    return returnOutOfFrame;
}

JSFunctionWithFields* createWebAssemblySuspendingFunction(VM& vm, JSGlobalObject* globalObject, JSValue callable)
{
    const String name = "WebAssembly.Suspending"_s;
    NativeExecutable* executable = vm.getHostFunction(enterWebAssemblySuspendingFunction, ImplementationVisibility::Public, NoIntrinsic, callHostFunctionAsConstructor, nullptr, name);
    constexpr unsigned length = 0;
    JSFunctionWithFields* function = JSFunctionWithFields::create(vm, globalObject, executable, length, name);
    function->setField(vm, JSFunctionWithFields::Field::WebAssemblySuspendingWrappedCallable, callable);
    return function;
}

}

#endif // ENABLE(WEBASSEMBLY)
