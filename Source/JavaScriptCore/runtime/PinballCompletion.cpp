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

#include "ConservativeRoots.h"
#include "FunctionPrototype.h"
#include "JSPromise.h"
#include "JSWebAssemblyException.h"
#include "JSWebAssemblyInstance.h"
#include "StackAlignment.h"

namespace JSC {

const ClassInfo PinballCompletion::s_info = { "PinballCompletion"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(PinballCompletion) };

Structure* PinballCompletion::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
{
    return Structure::create(vm, globalObject, proto, TypeInfo(ObjectType, StructureFlags), info());
}

PinballCompletion* PinballCompletion::create(VM& vm, Vector<std::unique_ptr<EvacuatedStackSlice>>&& slices, CPURegister* calleeSaves)
{
    Structure* structure = vm.pinballCompletionStructure.get();
    auto* instance = new (NotNull, allocateCell<PinballCompletion>(vm)) PinballCompletion(vm, structure, WTFMove(slices), calleeSaves);
    for (auto& slice : instance->m_slices)
        vm.addEvacuatedStackSlice(slice.get());

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    vm.addEvacuatedCalleeSaves(std::span(reinterpret_cast<Register*>(instance->m_calleeSaves), NUMBER_OF_CALLEE_SAVES_REGISTERS));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    instance->finishCreation(vm);
    return instance;
}


PinballCompletion::PinballCompletion(VM& vm, Structure* structure, Vector<std::unique_ptr<EvacuatedStackSlice>>&& slices, CPURegister* calleeSaves)
    : Base(vm, structure)
    , m_slices(WTFMove(slices))
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    memcpySpan(std::span(m_calleeSaves), std::span(calleeSaves, NUMBER_OF_CALLEE_SAVES_REGISTERS));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}


void PinballCompletion::destroy(JSCell* cell)
{
    PinballCompletion* thisObject = static_cast<PinballCompletion*>(cell);
    thisObject->PinballCompletion::~PinballCompletion();
}

void PinballCompletion::setResultPromise(VM& vm, JSPromise* promise)
{
    m_resultPromise.set(vm, this, promise);
}

void PinballCompletion::assimilate(VM& vm, PinballCompletion* other)
{
    other->m_slices.appendVector(WTFMove(m_slices));
    m_slices = WTFMove(other->m_slices);
    setResultPromise(vm, other->resultPromise());
}

template<typename Visitor>
void PinballCompletion::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<PinballCompletion*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    if (thisObject->m_resultPromise)
        visitor.append(thisObject->m_resultPromise);
    // Evacuated stack slices are registered with the VM and are added to conservative roots.
}

DEFINE_VISIT_CHILDREN(PinballCompletion);

extern "C" JSC_DECLARE_HOST_FUNCTION(pinballHandlerFulfillFunction); // defined in InPlaceInterpreter.asm
static JSC_DECLARE_HOST_FUNCTION(pinballHandlerRejectFunction);

const ClassInfo PinballHandler::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(PinballHandler) };

PinballHandler* PinballHandler::createFulfiller(VM& vm, JSGlobalObject* globalObject, PinballCompletion* pinballCompletion)
{
    return create(vm, globalObject, pinballHandlerFulfillFunction, pinballCompletion);
}

PinballHandler* PinballHandler::createRejecter(VM& vm, JSGlobalObject* globalObject, PinballCompletion* pinballCompletion)
{
    return create(vm, globalObject, pinballHandlerRejectFunction, pinballCompletion);
}

PinballHandler* PinballHandler::create(VM& vm, JSGlobalObject* globalObject, NativeFunction handlerImpl, PinballCompletion* pinballCompletion)
{
    const String name = "<pinball handler>"_s;
    NativeExecutable* executable = vm.getHostFunction(handlerImpl, ImplementationVisibility::Public, NoIntrinsic, callHostFunctionAsConstructor, nullptr, name);
    Structure* structure = globalObject->pinballHandlerStructure();
    PinballHandler* function = new (NotNull, allocateCell<PinballHandler>(vm)) PinballHandler(vm, executable, globalObject, structure, pinballCompletion);
    function->finishCreation(vm, executable, name);
    return function;
}

Structure* PinballHandler::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
}

void PinballHandler::finishCreation(VM& vm, NativeExecutable* executable, const String& name)
{
    Base::finishCreation(vm, executable, 1, name);
    ASSERT(inherits(info()));
}

template<typename Visitor>
void PinballHandler::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    PinballHandler* self = jsCast<PinballHandler*>(cell);
    ASSERT_GC_OBJECT_INHERITS(self, info());
    Base::visitChildren(self, visitor);
    visitor.append(self->m_pinballCompletion);
}

DEFINE_VISIT_CHILDREN(PinballHandler);

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

/*
    The following three C++ functions implement the "normal" part of the logic of reviving and executing suspended Wasm stack
    when the suspension promise has been fulfilled. The magical stack and register manipulation is done by the
    core handler code implemented in offlineasm.
*/

extern "C" void SYSV_ABI pinballHandlerFulfillFunctionInit(JSGlobalObject*, CallFrame*, PinballFulfillContext*);
extern "C" UGPRPair SYSV_ABI pinballHandlerFulfillFunctionImplant(PinballFulfillContext*, Register*, CallFrame*, void*);
extern "C" bool SYSV_ABI pinballHandlerFulfillFunctionContinue(PinballFulfillContext*);

extern "C" EncodedJSValue SYSV_ABI pinballHandlerPropagateException(JSGlobalObject*, PinballCompletion*, JSWebAssemblyException*);
extern "C" void SYSV_ABI pinballHandlerUnwindInit(JSGlobalObject*, PinballCompletion*, JSWebAssemblyException*, PinballUnwindContext*);
extern "C" void SYSV_ABI pinballHandlerUnwindInitWasm(PinballUnwindContext*, JSWebAssemblyInstance*);
extern "C" UGPRPair SYSV_ABI pinballHandlerUnwindImplant(PinballUnwindContext*, Register *base, CallFrame* returnFP, void* returnPC);

void pinballHandlerFulfillFunctionInit(JSGlobalObject* globalObject, CallFrame* callFrame, PinballFulfillContext* context)
{
    PinballHandler* self = jsCast<PinballHandler*>(callFrame->jsCallee());
    ASSERT(self);

    ASSERT(callFrame->argumentCount() == 1);
    PinballCompletion* pinball = self->pinballCompletion();
    ASSERT(pinball->hasSlices());
    auto* slice = pinball->slices().last().get(); // keep the slice owned by pinball until implanted

#if ASSERT_ENABLED
    context->magic = 0xBA11FEED;
#endif
    context->globalObject = globalObject;
    context->handler = self;
    constexpr CallFrame* noHandlerFrame = nullptr; // set later by the assembly logic when the handler frame is created
    new (&context->jspiContext) JSPIContext(JSPIContext::Purpose::Completing, globalObject->vm(), noHandlerFrame);
    context->slice = slice;
    context->sliceByteSize = slice->size() * sizeof(Register);
    ASSERT(!(context->sliceByteSize % stackAlignmentBytes()), "invalid stack slice detected with an odd number of slots");
    context->evacuatedCalleeSaves = pinball->calleeSaves();
    context->arguments[0] = JSValue::encode(callFrame->argument(0));
}

SUPPRESS_ASAN
UGPRPair pinballHandlerFulfillFunctionImplant(PinballFulfillContext* context, Register *base, CallFrame* returnFP, void* returnPC)
{
    ASSERT(context->magic == 0xBA11FEED);
    VM& vm = context->globalObject->vm();
    PinballCompletion* pinball = context->handler->pinballCompletion();

    auto* slice = context->slice;
    CallFrame* entryFP = slice->implant(base, returnFP, returnPC);
    vm.removeEvacuatedStackSlice(slice); // the slice is now scanned as part of the stack
    const void* entryPC = slice->entryPC();
    pinball->takeTopSlice(); // and drop
    context->slice = nullptr;
    // At this point callee saves have been loaded into the registers and it is safe for the VM to forget them.
    vm.removeEvacuatedCalleeSaves(std::span(reinterpret_cast<Register*>(pinball->calleeSaves()), NUMBER_OF_CALLEE_SAVES_REGISTERS));

    return makeUGPRPair(reinterpret_cast<UCPURegister>(entryFP), reinterpret_cast<UCPURegister>(entryPC));
}

// After the execution of a slice returns, determine how to proceed.
// True return means a new slice is now ready for another execution cycle, false means exit.
bool pinballHandlerFulfillFunctionContinue(PinballFulfillContext* context)
{
    ASSERT(context->magic == 0xBA11FEED);
    ASSERT(!context->slice);

    VM& vm = context->globalObject->vm();
    JSPIContext& jspiContext = context->jspiContext;
    PinballCompletion* pinball = context->handler->pinballCompletion();

    if (jspiContext.completion) {
        // Computation was suspended again; the remainder of this completion should be added to the new one.
        jspiContext.completion->assimilate(vm, pinball);
        jspiContext.deactivate(vm);
        context->arguments[0] = JSValue::encode(jsUndefined());
        context->~PinballFulfillContext(); // context is owned by the asm caller frame, destruct it from here just in case
        return false;
    }

    if (pinball->hasSlices()) {
        auto* slice = pinball->takeTopSlice().release();
        context->slice = slice;
        context->sliceByteSize = slice->size() * sizeof(Register);
        return true;
    }

    jspiContext.deactivate(vm);
    JSValue arg = JSValue::decode(context->arguments[0]);

    JSPromise* resultPromise = pinball->resultPromise();
    ASSERT(resultPromise);
    resultPromise->resolve(context->globalObject, arg);

    context->arguments[0] = JSValue::encode(jsUndefined());
    context->~PinballFulfillContext();
    return false;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

static void discardEvacuatedStackAndPropagateRejection(JSGlobalObject* globalObject, PinballCompletion* pinball, JSValue rejectionReason)
{
    VM& vm = globalObject->vm();

    JSPromise* resultPromise = pinball->resultPromise();
    ASSERT(resultPromise);
    while (pinball->hasSlices()) {
        auto slice = pinball->takeTopSlice();
        vm.removeEvacuatedStackSlice(slice.get());
    }
    resultPromise->reject(vm, globalObject, rejectionReason);
}

JSC_DEFINE_HOST_FUNCTION(pinballHandlerRejectFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue reason = callFrame->argument(0);

    PinballHandler* self = jsCast<PinballHandler*>(callFrame->jsCallee());
    PinballCompletion* pinball = self->pinballCompletion();

    auto* wasmException = jsDynamicCast<JSWebAssemblyException*>(reason);
    if (!wasmException) {
        discardEvacuatedStackAndPropagateRejection(globalObject, pinball, reason);
        return JSValue::encode(jsUndefined());
    }

    // If the reason is a Wasm exception, we should revive the Wasm stack and give it a chance to handle it.
    EncodedJSValue result = pinballHandlerPropagateException(globalObject, pinball, wasmException);
    JSPromise* resultPromise = pinball->resultPromise();
    Exception* exception = scope.exception();
    if (exception) {
        JSValue exceptionValue = exception->value();
        scope.clearException();
        resultPromise->reject(vm, globalObject, exceptionValue);
    } else
        resultPromise->resolve(globalObject, JSValue::decode(result));

    return JSValue::encode(jsUndefined());
}

void pinballHandlerUnwindInit(JSGlobalObject* globalObject, PinballCompletion* pinball, JSWebAssemblyException* exception, PinballUnwindContext* context)
{
    ASSERT(pinball->slices().size() == 1); // Right now we only support the slab slicing strategy
    auto* slice = pinball->takeTopSlice().release();

#if ASSERT_ENABLED
    context->magic = 0xBA11FEED;
#endif
    context->vm = &globalObject->vm();
    context->zombieFrameCallee = globalObject->zombieFrameCallee();
    context->slice = slice;
    context->sliceByteSize = slice->size() * sizeof(Register);
    context->exception = exception;
    ASSERT(!(context->sliceByteSize % stackAlignmentBytes()), "invalid stack slice detected with an odd number of slots");
    context->evacuatedCalleeSaves = pinball->calleeSaves();
}

// FIXME: is there a nice official way of doing this?
static bool lookupExceptionIndex(JSWebAssemblyInstance* instance, JSWebAssemblyException* exception, unsigned* result)
{
    const auto& moduleInfo = instance->moduleInformation();
    unsigned tagCount = moduleInfo.exceptionIndexSpaceSize();
    for (unsigned i = 0; i < tagCount; ++i) {
        if (&instance->tag(i) == &exception->tag()) {
            *result = i;
            return true;
        }
    }
    return false;
}

void pinballHandlerUnwindInitWasm(PinballUnwindContext* context, JSWebAssemblyInstance* instance)
{
    ASSERT(context->magic == 0xBA11FEED);
    bool found = lookupExceptionIndex(instance, context->exception, &context->exceptionIndex);
    RELEASE_ASSERT(found);
}

// TODO: this is virtually identical to the fulfill case and we should be able to have just one
UGPRPair pinballHandlerUnwindImplant(PinballUnwindContext* context, Register *base, CallFrame* returnFP, void* returnPC)
{
    ASSERT(context->magic == 0xBA11FEED);

    // dataLogLn("unwind implant base: ", RawPointer(base), " return FP: ", RawPointer(returnFP));

    auto* slice = context->slice;
    CallFrame* entryFP = slice->implant(base, returnFP, returnPC);
    context->vm->removeEvacuatedStackSlice(slice);
    const void* entryPC = slice->entryPC();
    slice->~EvacuatedStackSlice();
    EvacuatedStackSlice::freeAfterDestruction(slice);
    context->slice = nullptr;
    return makeUGPRPair(reinterpret_cast<UCPURegister>(entryFP), reinterpret_cast<UCPURegister>(entryPC));
}


} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
