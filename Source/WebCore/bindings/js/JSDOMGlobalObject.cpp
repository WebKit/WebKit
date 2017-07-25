/*
 * Copyright (C) 2008-2017 Apple Inc. All Rights Reserved.
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
 *
 */

#include "config.h"
#include "JSDOMGlobalObject.h"

#include "DOMWindow.h"
#include "Document.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDOMWindow.h"
#include "JSEventListener.h"
#include "JSMediaStream.h"
#include "JSMediaStreamTrack.h"
#include "JSRTCIceCandidate.h"
#include "JSRTCSessionDescription.h"
#include "JSReadableStream.h"
#include "JSReadableStreamPrivateConstructors.h"
#include "JSWorkerGlobalScope.h"
#include "RuntimeEnabledFeatures.h"
#include "StructuredClone.h"
#include "WebCoreJSClientData.h"
#include "WorkerGlobalScope.h"
#include <builtins/BuiltinNames.h>

using namespace JSC;

namespace WebCore {

EncodedJSValue JSC_HOST_CALL makeThisTypeErrorForBuiltins(ExecState*);
EncodedJSValue JSC_HOST_CALL makeGetterTypeErrorForBuiltins(ExecState*);
EncodedJSValue JSC_HOST_CALL isWebRTCLegacyAPIEnabled(ExecState*);
EncodedJSValue JSC_HOST_CALL isReadableByteStreamAPIEnabled(ExecState*);

const ClassInfo JSDOMGlobalObject::s_info = { "DOMGlobalObject", &JSGlobalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDOMGlobalObject) };

JSDOMGlobalObject::JSDOMGlobalObject(VM& vm, Structure* structure, Ref<DOMWrapperWorld>&& world, const GlobalObjectMethodTable* globalObjectMethodTable)
    : JSGlobalObject(vm, structure, globalObjectMethodTable)
    , m_currentEvent(0)
    , m_world(WTFMove(world))
    , m_worldIsNormal(m_world->isNormal())
    , m_builtinInternalFunctions(vm)
{
}

JSDOMGlobalObject::~JSDOMGlobalObject()
{
}

void JSDOMGlobalObject::destroy(JSCell* cell)
{
    static_cast<JSDOMGlobalObject*>(cell)->JSDOMGlobalObject::~JSDOMGlobalObject();
}

EncodedJSValue JSC_HOST_CALL makeThisTypeErrorForBuiltins(ExecState* execState)
{
    ASSERT(execState);
    ASSERT(execState->argumentCount() == 2);
    VM& vm = execState->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto interfaceName = execState->uncheckedArgument(0).getString(execState);
    scope.assertNoException();
    auto functionName = execState->uncheckedArgument(1).getString(execState);
    scope.assertNoException();
    return JSValue::encode(createTypeError(execState, makeThisTypeErrorMessage(interfaceName.utf8().data(), functionName.utf8().data())));
}

EncodedJSValue JSC_HOST_CALL makeGetterTypeErrorForBuiltins(ExecState* execState)
{
    ASSERT(execState);
    ASSERT(execState->argumentCount() == 2);
    VM& vm = execState->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto interfaceName = execState->uncheckedArgument(0).getString(execState);
    scope.assertNoException();
    auto attributeName = execState->uncheckedArgument(1).getString(execState);
    scope.assertNoException();
    return JSValue::encode(createTypeError(execState, makeGetterTypeErrorMessage(interfaceName.utf8().data(), attributeName.utf8().data())));
}

#if ENABLE(WEB_RTC)
EncodedJSValue JSC_HOST_CALL isWebRTCLegacyAPIEnabled(ExecState*)
{
    return JSValue::encode(jsBoolean(RuntimeEnabledFeatures::sharedFeatures().webRTCLegacyAPIEnabled()));
}
#endif

#if ENABLE(STREAMS_API)
EncodedJSValue JSC_HOST_CALL isReadableByteStreamAPIEnabled(ExecState*)
{
    return JSValue::encode(jsBoolean(RuntimeEnabledFeatures::sharedFeatures().readableByteStreamAPIEnabled()));
}
#endif

void JSDOMGlobalObject::addBuiltinGlobals(VM& vm)
{
    m_builtinInternalFunctions.initialize(*this);

#if ENABLE(STREAMS_API)
    JSObject* privateReadableStreamDefaultControllerConstructor = createReadableStreamDefaultControllerPrivateConstructor(vm, *this);
    JSObject* privateReadableByteStreamControllerConstructor = createReadableByteStreamControllerPrivateConstructor(vm, *this);
    JSObject* privateReadableStreamBYOBRequestConstructor = createReadableStreamBYOBRequestPrivateConstructor(vm, *this);
    JSObject* privateReadableStreamDefaultReaderConstructor = createReadableStreamDefaultReaderPrivateConstructor(vm, *this);
    JSObject* privateReadableStreamBYOBReaderConstructor = createReadableStreamBYOBReaderPrivateConstructor(vm, *this);

    ASSERT(!constructors(NoLockingNecessary).get(privateReadableStreamDefaultControllerConstructor->info()).get());
    ASSERT(!constructors(NoLockingNecessary).get(privateReadableByteStreamControllerConstructor->info()).get());
    ASSERT(!constructors(NoLockingNecessary).get(privateReadableStreamBYOBRequestConstructor->info()).get());
    ASSERT(!constructors(NoLockingNecessary).get(privateReadableStreamDefaultReaderConstructor->info()).get());
    ASSERT(!constructors(NoLockingNecessary).get(privateReadableStreamBYOBReaderConstructor->info()).get());
    JSC::WriteBarrier<JSC::JSObject> temp;
    {
        auto locker = lockDuringMarking(vm.heap, m_gcLock);
        constructors(locker).add(privateReadableStreamDefaultControllerConstructor->info(), temp).iterator->value.set(vm, this, privateReadableStreamDefaultControllerConstructor);
        constructors(locker).add(privateReadableByteStreamControllerConstructor->info(), temp).iterator->value.set(vm, this, privateReadableByteStreamControllerConstructor);
        constructors(locker).add(privateReadableStreamBYOBRequestConstructor->info(), temp).iterator->value.set(vm, this, privateReadableStreamBYOBRequestConstructor);
        constructors(locker).add(privateReadableStreamDefaultReaderConstructor->info(), temp).iterator->value.set(vm, this, privateReadableStreamDefaultReaderConstructor);
        constructors(locker).add(privateReadableStreamBYOBReaderConstructor->info(), temp).iterator->value.set(vm, this, privateReadableStreamBYOBReaderConstructor);
    }
#endif
    JSVMClientData& clientData = *static_cast<JSVMClientData*>(vm.clientData);
    JSDOMGlobalObject::GlobalPropertyInfo staticGlobals[] = {
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().makeThisTypeErrorPrivateName(),
            JSFunction::create(vm, this, 2, String(), makeThisTypeErrorForBuiltins), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().makeGetterTypeErrorPrivateName(),
            JSFunction::create(vm, this, 2, String(), makeGetterTypeErrorForBuiltins), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().cloneArrayBufferPrivateName(),
            JSFunction::create(vm, this, 3, String(), cloneArrayBuffer), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().structuredCloneArrayBufferPrivateName(),
            JSFunction::create(vm, this, 1, String(), structuredCloneArrayBuffer), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().structuredCloneArrayBufferViewPrivateName(),
            JSFunction::create(vm, this, 1, String(), structuredCloneArrayBufferView), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(vm.propertyNames->builtinNames().ArrayBufferPrivateName(), getDirect(vm, vm.propertyNames->ArrayBuffer), DontDelete | ReadOnly),
#if ENABLE(STREAMS_API)
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamClosedPrivateName(), jsNumber(1), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamClosingPrivateName(), jsNumber(2), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamErroredPrivateName(), jsNumber(3), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamReadablePrivateName(), jsNumber(4), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamWaitingPrivateName(), jsNumber(5), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamWritablePrivateName(), jsNumber(6), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().ReadableStreamDefaultControllerPrivateName(), privateReadableStreamDefaultControllerConstructor, DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().ReadableByteStreamControllerPrivateName(), privateReadableByteStreamControllerConstructor, DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().ReadableStreamBYOBRequestPrivateName(), privateReadableStreamBYOBRequestConstructor, DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().ReadableStreamDefaultReaderPrivateName(), privateReadableStreamDefaultReaderConstructor, DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().ReadableStreamBYOBReaderPrivateName(), privateReadableStreamBYOBReaderConstructor, DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().readableByteStreamAPIEnabledPrivateName(), JSFunction::create(vm, this, 0, String(), isReadableByteStreamAPIEnabled), DontDelete | ReadOnly),
#endif
#if ENABLE(WEB_RTC)
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().webRTCLegacyAPIEnabledPrivateName(), JSFunction::create(vm, this, 0, String(), isWebRTCLegacyAPIEnabled), DontDelete | ReadOnly),
#endif
    };
    addStaticGlobals(staticGlobals, WTF_ARRAY_LENGTH(staticGlobals));
}

void JSDOMGlobalObject::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    addBuiltinGlobals(vm);

    RELEASE_ASSERT(classInfo());
}

void JSDOMGlobalObject::finishCreation(VM& vm, JSObject* thisValue)
{
    Base::finishCreation(vm, thisValue);
    ASSERT(inherits(vm, info()));

    addBuiltinGlobals(vm);

    RELEASE_ASSERT(classInfo());
}

ScriptExecutionContext* JSDOMGlobalObject::scriptExecutionContext() const
{
    if (inherits(vm(), JSDOMWindowBase::info()))
        return jsCast<const JSDOMWindowBase*>(this)->scriptExecutionContext();
    if (inherits(vm(), JSWorkerGlobalScopeBase::info()))
        return jsCast<const JSWorkerGlobalScopeBase*>(this)->scriptExecutionContext();
    dataLog("Unexpected global object: ", JSValue(this), "\n");
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

void JSDOMGlobalObject::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSDOMGlobalObject* thisObject = jsCast<JSDOMGlobalObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    {
        auto locker = holdLock(thisObject->m_gcLock);

        for (auto& structure : thisObject->structures(locker).values())
            visitor.append(structure);

        for (auto& constructor : thisObject->constructors(locker).values())
            visitor.append(constructor);

        for (auto& guarded : thisObject->guardedObjects(locker))
            guarded->visitAggregate(visitor);
    }

    thisObject->m_builtinInternalFunctions.visit(visitor);
}

void JSDOMGlobalObject::setCurrentEvent(Event* currentEvent)
{
    m_currentEvent = currentEvent;
}

Event* JSDOMGlobalObject::currentEvent() const
{
    return m_currentEvent;
}

JSDOMGlobalObject* toJSDOMGlobalObject(Document* document, JSC::ExecState* exec)
{
    return toJSDOMWindow(document->frame(), currentWorld(exec));
}

JSDOMGlobalObject* toJSDOMGlobalObject(ScriptExecutionContext* scriptExecutionContext, JSC::ExecState* exec)
{
    if (is<Document>(*scriptExecutionContext))
        return toJSDOMGlobalObject(downcast<Document>(scriptExecutionContext), exec);

    if (is<WorkerGlobalScope>(*scriptExecutionContext))
        return downcast<WorkerGlobalScope>(*scriptExecutionContext).script()->workerGlobalScopeWrapper();

    ASSERT_NOT_REACHED();
    return nullptr;
}

JSDOMGlobalObject* toJSDOMGlobalObject(Document* document, DOMWrapperWorld& world)
{
    return toJSDOMWindow(document->frame(), world);
}

JSDOMGlobalObject* toJSDOMGlobalObject(ScriptExecutionContext* scriptExecutionContext, DOMWrapperWorld& world)
{
    if (is<Document>(*scriptExecutionContext))
        return toJSDOMGlobalObject(downcast<Document>(scriptExecutionContext), world);

    if (is<WorkerGlobalScope>(*scriptExecutionContext))
        return downcast<WorkerGlobalScope>(*scriptExecutionContext).script()->workerGlobalScopeWrapper();

    ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace WebCore
