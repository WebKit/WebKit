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
#include "PinballCompletion.h"

#if ENABLE(WEBASSEMBLY)

#include "CallFrame.h"
#include "EvacuatedStack.h"
#include "Exception.h"
#include "JSPIContextInlines.h"
#include "JSPromise.h"
#include "PinballHandlerContext.h"
#include "TopExceptionScope.h"

#include <wtf/StdLibExtras.h>

namespace JSC {

const ClassInfo PinballCompletion::s_info = { "PinballCompletion"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(PinballCompletion) };

Structure* PinballCompletion::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
{
    return Structure::create(vm, globalObject, proto, TypeInfo(ObjectType, StructureFlags), info());
}

PinballCompletion* PinballCompletion::create(VM& vm, Vector<std::unique_ptr<EvacuatedStackSlice>>&& slices, CPURegister* calleeSaves, JSPromise* resultPromise)
{
    Structure* structure = vm.pinballCompletionStructure.get();
    auto* instance = new (NotNull, allocateCell<PinballCompletion>(vm)) PinballCompletion(vm, structure, WTF::move(slices), calleeSaves, resultPromise);
    for (auto& slice : instance->m_slices)
        vm.addEvacuatedStackSlice(slice.get());

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    vm.addEvacuatedCalleeSaves(std::span(instance->m_calleeSaves, NUMBER_OF_CALLEE_SAVES_REGISTERS));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    instance->finishCreation(vm);
    return instance;
}


PinballCompletion::PinballCompletion(VM& vm, Structure* structure, Vector<std::unique_ptr<EvacuatedStackSlice>>&& slices, CPURegister* calleeSaves, JSPromise* resultPromise)
    : Base(vm, structure)
    , m_slices(WTF::move(slices))
    , m_resultPromise(resultPromise, WriteBarrierEarlyInit)
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    memcpySpan(std::span(m_calleeSaves), std::span(calleeSaves, NUMBER_OF_CALLEE_SAVES_REGISTERS));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

PinballCompletion::~PinballCompletion()
{
    VM& vm = this->vm();
    for (auto& slice : m_slices)
        vm.removeEvacuatedStackSlice(slice.get());
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    vm.removeEvacuatedCalleeSaves(std::span(m_calleeSaves, NUMBER_OF_CALLEE_SAVES_REGISTERS));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

void PinballCompletion::destroy(JSCell* cell)
{
    SUPPRESS_MEMORY_UNSAFE_CAST // jsCast() is not available here; validity is guaranteed by the destruction mechanism
    auto* thisObject = static_cast<PinballCompletion*>(cell);
    thisObject->~PinballCompletion();
}

void PinballCompletion::assimilate(PinballCompletion* other)
{
    other->m_slices.appendVector(WTF::move(m_slices));
    m_slices = WTF::move(other->m_slices);
}

template<typename Visitor>
void PinballCompletion::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<PinballCompletion*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_resultPromise);
    // Evacuated stack slices are registered with the VM and are added to conservative roots,
    // so no need to do anything about them here.
}

DEFINE_VISIT_CHILDREN(PinballCompletion);

extern "C" {
    // defined in InPlaceInterpreter.asm
    JSC_DECLARE_HOST_FUNCTION(pinballHandlerFulfillFunction);
    JSC_DECLARE_HOST_FUNCTION(pinballHandlerRejectFunction);
}

static JSFunctionWithFields* createHandler(VM& vm, JSGlobalObject* globalObject, PinballCompletion* pinballCompletion, NativeFunction function, const String name)
{
    NativeExecutable* executable = vm.getHostFunction(function, ImplementationVisibility::Public, NoIntrinsic, callHostFunctionAsConstructor, nullptr, name);
    constexpr unsigned length = 1;
    JSFunctionWithFields* handler = JSFunctionWithFields::create(vm, globalObject, executable, length, name);
    handler->setField(vm, JSFunctionWithFields::Field::PromiseHandlerPinballCompletion, pinballCompletion);
    return handler;
}

JSFunctionWithFields* createPinballCompletionFulfillHandler(VM& vm, JSGlobalObject* globalObject, PinballCompletion* pinballCompletion)
{
    return createHandler(vm, globalObject, pinballCompletion, pinballHandlerFulfillFunction, "<pinball fulfill handler>"_s);
}

JSFunctionWithFields* createPinballCompletionRejectHandler(VM& vm, JSGlobalObject* globalObject, PinballCompletion* pinballCompletion)
{
    return createHandler(vm, globalObject, pinballCompletion, pinballHandlerRejectFunction, "<pinball reject handler>"_s);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

/*
    The following C++ functions implement the "normal" part of the logic of reviving
    and executing a suspended Wasm stack when the suspension promise has been fulfilled. The
    magical stack and register manipulation is done by the core handler code implemented
    in offlineasm.
*/

extern "C" void SYSV_ABI pinballHandlerInitContextForFulfill(JSGlobalObject*, CallFrame*, PinballHandlerContext*);
extern "C" void SYSV_ABI pinballHandlerInitContextForReject(JSGlobalObject*, CallFrame*, PinballHandlerContext*);
extern "C" void SYSV_ABI pinballHandlerImplantSlice(PinballHandlerContext*, Register*, CallFrame*, CallerFrameAndPC*);
extern "C" UCPURegister SYSV_ABI pinballHandlerFulfillFunctionContinue(PinballHandlerContext*);
extern "C" void pinballHandlerFinishReject(PinballHandlerContext*);

static void pinballHandlerInitContext(JSGlobalObject* globalObject, CallFrame* callFrame, PinballHandlerContext* context)
{
    VM& vm = globalObject->vm();
    JSFunctionWithFields* self = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());

    ASSERT(callFrame->argumentCount() == 1);
    PinballCompletion* pinball = jsCast<PinballCompletion*>(self->getField(JSFunctionWithFields::Field::PromiseHandlerPinballCompletion));
    ASSERT(pinball->hasSlices());
    auto* slice = pinball->takeTopSlice().release();

#if ASSERT_ENABLED
    context->magic = 0xBA11FEED;
#endif
    context->globalObject = globalObject;
    context->vm = &vm;
    context->handler = self;
    new (&context->jspiContext) JSPIContext(JSPIContext::Purpose::Completing, vm, callFrame, pinball->resultPromise());
    context->slice = slice;
    context->sliceByteSize = slice->size() * sizeof(Register);
    ASSERT(!(context->sliceByteSize % stackAlignmentBytes())); // asm code assumes alignment is not needed
    context->evacuatedCalleeSaves = pinball->calleeSaves();
#if ASSERT_ENABLED
    zeroSpan(std::span(context->arguments));
#endif
}

void pinballHandlerInitContextForFulfill(JSGlobalObject* globalObject, CallFrame* callFrame, PinballHandlerContext* context)
{
    pinballHandlerInitContext(globalObject, callFrame, context);
    ASSERT(callFrame->argumentCount() == 1);
    context->arguments[0] = JSValue::encode(callFrame->argument(0));
}

void pinballHandlerInitContextForReject(JSGlobalObject* globalObject, CallFrame* callFrame, PinballHandlerContext* context)
{
#if ASSERT_ENABLED
    JSFunctionWithFields* self = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    PinballCompletion* pinball = jsCast<PinballCompletion*>(self->getField(JSFunctionWithFields::Field::PromiseHandlerPinballCompletion));
    ASSERT(pinball->slices().size() == 1); // exceptions are only supported with slab slicing, expecting 1 slice
#endif

    pinballHandlerInitContext(globalObject, callFrame, context);
    JSValue reason = callFrame->argument(0);

    context->zombieFrameCallee = globalObject->zombieFrameCallee();
    context->exception = Exception::create(globalObject->vm(), reason);
}

void pinballHandlerImplantSlice(PinballHandlerContext* context, Register *base, CallFrame* sentinelFrame, CallerFrameAndPC* returnFrame)
{
    ASSERT(context->magic == 0xBA11FEED);
    VM& vm = context->globalObject->vm();
    PinballCompletion* pinball = jsCast<PinballCompletion*>(context->handler->getField(JSFunctionWithFields::Field::PromiseHandlerPinballCompletion));

    auto* slice = context->slice;
    CallFrame* bottommostImplantedFrame = slice->implant(base, sentinelFrame);
    returnFrame->callerFrame = bottommostImplantedFrame;
    returnFrame->returnPC = relocateReturnPC(const_cast<void*>(slice->entryPC()), reinterpret_cast<const CallerFrameAndPC*>(slice->entryPCFrame()), returnFrame);

    vm.removeEvacuatedStackSlice(slice); // the slice data is now scanned as part of the stack
    delete slice;
    context->slice = nullptr;
    // At this point callee saves have been loaded into the registers and it is safe for the VM to forget them.
    // We end up doing it multiple times, which is okay. Repeat removals do nothing.
    vm.removeEvacuatedCalleeSaves(std::span(pinball->calleeSaves(), NUMBER_OF_CALLEE_SAVES_REGISTERS));
}

// After the execution of a slice returns, determine how to proceed.
// The return value is essentially a 'bool', but making it a UCPURegister allows for uniform treatment
// in offlineasm. Otherwise we'd need a special case for x86 where a bool is returned as an 8-bit AL register.
// True return indicates that the assembly driver should install and execute the next slice,
// false means execution completed, the result promise has been resolved, and the driver should exit.
UCPURegister pinballHandlerFulfillFunctionContinue(PinballHandlerContext* context)
{
    ASSERT(context->magic == 0xBA11FEED);
    ASSERT(!context->slice);

    VM& vm = *context->vm;
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    JSPIContext& jspiContext = context->jspiContext;
    PinballCompletion* pinball = jsCast<PinballCompletion*>(context->handler->getField(JSFunctionWithFields::Field::PromiseHandlerPinballCompletion));

    if (jspiContext.completion) {
        // Computation was suspended again; the remainder of this completion should be added to the new one.
        jspiContext.completion->assimilate(pinball);
        jspiContext.deactivate(vm);
        context->arguments[0] = JSValue::encode(jsUndefined());
        context->~PinballHandlerContext(); // context is in asm caller frame data, we destruct it from here
        return 0;
    }

    if (pinball->hasSlices()) {
        RELEASE_ASSERT(!scope.exception()); // multi-slice completion is not yet prepared to handle exceptions; we should never encounter one at this point
        auto* slice = pinball->takeTopSlice().release();
        context->slice = slice;
        context->sliceByteSize = slice->size() * sizeof(Register);
        return 1;
    }

    jspiContext.deactivate(vm);

    JSPromise* resultPromise = pinball->resultPromise();
    if (!scope.exception()) {
        JSValue arg = JSValue::decode(context->arguments[0]);
        resultPromise->resolve(context->globalObject, vm, arg);
    } else {
        resultPromise->reject(vm, context->globalObject, scope.exception());
        scope.clearException();
    }

    context->arguments[0] = JSValue::encode(jsNull());
    context->~PinballHandlerContext();
    return 0;
}

void pinballHandlerFinishReject(PinballHandlerContext* context)
{
    ASSERT(context->magic == 0xBA11FEED);
    ASSERT(!context->slice);

    VM& vm = *context->vm;
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    JSPIContext& jspiContext = context->jspiContext;
    PinballCompletion* pinball = jsCast<PinballCompletion*>(context->handler->getField(JSFunctionWithFields::Field::PromiseHandlerPinballCompletion));
    ASSERT(!pinball->hasSlices());

    if (jspiContext.completion) {
        // The exception was apparently caught, execution proceeded, and was resuspended again.

        jspiContext.completion->assimilate(pinball);
        jspiContext.deactivate(vm);
        context->arguments[0] = JSValue::encode(jsUndefined());
        context->~PinballHandlerContext(); // context is in asm caller frame data, we destruct it from here
        return;
    }

    jspiContext.deactivate(vm);

    JSPromise* resultPromise = pinball->resultPromise();
    ASSERT(resultPromise);


    if (!scope.exception()) {
        JSValue arg = JSValue::decode(context->arguments[0]);
        resultPromise->resolve(context->globalObject, vm, arg);
    } else {
        resultPromise->reject(vm, context->globalObject, scope.exception());
        scope.clearException();
    }

    context->arguments[0] = JSValue::encode(jsNull());
    context->~PinballHandlerContext();
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
