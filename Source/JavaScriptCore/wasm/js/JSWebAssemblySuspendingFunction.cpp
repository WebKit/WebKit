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
#include "JSWebAssemblySuspendingFunction.h"

#if ENABLE(WEBASSEMBLY)

#include "ArgList.h"
#include "Debugger.h"
#include "EvacuatedStack.h"
#include "ExceptionHelpers.h"
#include "FrameTracers.h"
#include "FunctionPrototype.h"
#include "InterpreterInlines.h"
#include "JSObjectInlines.h"
#include "JSPromise.h"
#include "JSWebAssemblySuspendError.h"
#include "PinballCompletion.h"

namespace JSC {

extern "C" {
    void captureABICalleeSaves(void* buffer);
    void teleportForJSPI(CPURegister* calleeSaves, CallFrame* returnOutOfFrame, EncodedJSValue result);
}

extern "C" JSC_DECLARE_HOST_FUNCTION(enterWebAssemblySuspendingFunction);
JSC_DECLARE_HOST_FUNCTION(runWebAssemblySuspendingFunction);

template<typename T>
struct SlicerDriver {
    static bool slice(VM& vm, CallFrame* callFrame, String& errorMessage, Vector<std::unique_ptr<EvacuatedStackSlice>>& slices, CallFrame*& teleportFrame)
    {
        T slicer;
        StackSlicerFunctor<T> functor(vm, slicer);
        StackVisitor::visit(callFrame, vm, functor);

        if (!slicer.succeeded()) {
            errorMessage = slicer.errorMessage();
            return false;
        }
        slices = slicer.reverseAndTakeSlices();
        teleportFrame = slicer.teleportFrame();
        return true;
    }
};

extern "C" EncodedJSValue SYSV_ABI runWebAssemblySuspendingFunction(JSGlobalObject* globalObject, CallFrame* callFrame, CPURegister* hereCalleeSaves);

EncodedJSValue runWebAssemblySuspendingFunction(JSGlobalObject* globalObject, CallFrame* callFrame, CPURegister* hereCalleeSaves)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    CPURegister* vmEntryFrameCalleeSaves = vmEntryRecord(vm.topEntryFrame)->calleeSaveRegistersBuffer;
    memcpySpan(std::span<CPURegister>(vmEntryFrameCalleeSaves, NUMBER_OF_CALLEE_SAVES_REGISTERS), std::span(hereCalleeSaves, NUMBER_OF_CALLEE_SAVES_REGISTERS));

    JSObject* callee = callFrame->jsCallee();
    JSWebAssemblySuspendingFunction* self = jsCast<JSWebAssemblySuspendingFunction*>(callee);
    JSFunction* wrappedFunction = self->wrappedFunction();

    MarkedArgumentBuffer args;
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i)
        args.append(callFrame->uncheckedArgument(i));
    if (args.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    auto callData = JSC::getCallData(wrappedFunction);
    if (callData.type == CallData::Type::None) [[unlikely]] {
        throwTypeError(globalObject, scope, "Object is not callable"_s);
        return { };
    }

    JSValue result = call(globalObject, wrappedFunction, callData, jsUndefined(), args);
    RETURN_IF_EXCEPTION(scope, { });

    JSPromise* promise = jsDynamicCast<JSPromise*>(result);
    if (!promise)
        return JSValue::encode(result);

    if (!vm.topJSPIContext) {
        auto errorMessage = makeString("JSPI stack scan failed: "_s);
        throwException(globalObject, scope, createJSWebAssemblySuspendError(globalObject, vm, "Suspending() wrapper called outside of a promising() context"_s));
        return { };
    }

    String errorMessage;
    Vector<std::unique_ptr<EvacuatedStackSlice>> slices;
    CallFrame* returnOutOfFrame = nullptr;
    bool success = false;

    // There are multiple ways of slicing the wasm stack here. What is the optimal way is
    // something we will have to research on real workloads. It will likely be some
    // adaptive scheme, perhaps with profile data associated with the promising wrapper.
    // For now we offer 3 strategies, for testing more than for performance tuning. The
    // default is 'slab', which saves the whole stack as a single slice if no option is
    // supplied. 'mixed' is an attempt at a quick and easy adaptive scheme, so that on
    // the first suspension we use 'slab', betting on a straight return, but on a resuspension
    // use 'frag', betting that more resuspensions will follow.
    const char* strategy = Options::useJSPISlicing();
    if (!strategy || equal(unsafeSpan(strategy), "slab"_s)) {
        success = SlicerDriver<SlabSlicer>::slice(vm, callFrame, errorMessage, slices, returnOutOfFrame);
    } else if (equal(unsafeSpan(strategy), "frag"_s)) {
        success = SlicerDriver<FragSlicer>::slice(vm, callFrame, errorMessage, slices, returnOutOfFrame);
    } else if (equal(unsafeSpan(strategy), "mixed"_s)) {
        if (vm.topJSPIContext->purpose == JSPIContext::Purpose::Promising)
            success = SlicerDriver<SlabSlicer>::slice(vm, callFrame, errorMessage, slices, returnOutOfFrame);
        else
            success = SlicerDriver<FragSlicer>::slice(vm, callFrame, errorMessage, slices, returnOutOfFrame);
    } else
        FATAL("unrecognized JSPI strategy");

    if (!success) {
        auto errorString = makeString("JSPI stack scan failed: "_s, errorMessage);
        throwVMError(globalObject, scope, errorString);
        return { };
    }

    auto* pinball = PinballCompletion::create(vm, WTFMove(slices), hereCalleeSaves);
    vm.topJSPIContext->completion = pinball;

    auto* fulfiller = PinballHandler::createFulfiller(vm, globalObject, pinball);
    auto* rejecter = PinballHandler::createRejecter(vm, globalObject, pinball);
    promise->performPromiseThen(vm, globalObject, fulfiller, rejecter, jsUndefined());

    teleportForJSPI(vmEntryFrameCalleeSaves, returnOutOfFrame, JSValue::encode(jsUndefined())); // returns in to promising or pinball handler frame
    RELEASE_ASSERT_NOT_REACHED();
    return JSValue::encode(jsNull());
}

const ClassInfo JSWebAssemblySuspendingFunction::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblySuspendingFunction) };

JSWebAssemblySuspendingFunction* JSWebAssemblySuspendingFunction::create(VM& vm, JSGlobalObject* globalObject, JSFunction* wrappedFunction)
{
    const String name = "WebAssembly.Suspending"_s;
    NativeExecutable* executable = vm.getHostFunction(enterWebAssemblySuspendingFunction, ImplementationVisibility::Public, NoIntrinsic, callHostFunctionAsConstructor, nullptr, name);
    Structure* structure = globalObject->webAssemblySuspendingStructure();
    auto* function = new (NotNull, allocateCell<JSWebAssemblySuspendingFunction>(vm)) JSWebAssemblySuspendingFunction(vm, executable, globalObject, structure, wrappedFunction);
    function->finishCreation(vm, executable, name);
    return function;
}

Structure* JSWebAssemblySuspendingFunction::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
}

void JSWebAssemblySuspendingFunction::finishCreation(VM& vm, NativeExecutable* executable, const String& name)
{
    constexpr unsigned length = 0;
    Base::finishCreation(vm, executable, length, name);
    ASSERT(inherits(info()));
}

template<typename Visitor>
void JSWebAssemblySuspendingFunction::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSWebAssemblySuspendingFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_wrappedFunction);
}

DEFINE_VISIT_CHILDREN(JSWebAssemblySuspendingFunction);

}

#endif // ENABLE(WEBASSEMBLY)
