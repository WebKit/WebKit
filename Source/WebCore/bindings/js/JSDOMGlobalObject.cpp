/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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

#include "Document.h"
#include "JSDOMWindow.h"
#include "JSEventListener.h"
#include "JSMediaStreamTrack.h"
#include "JSRTCIceCandidate.h"
#include "JSRTCSessionDescription.h"
#include "JSReadableStream.h"
#include "JSReadableStreamPrivateConstructors.h"
#include "JSWorkerGlobalScope.h"
#include "RuntimeEnabledFeatures.h"
#include "WebCoreJSClientData.h"
#include "WorkerGlobalScope.h"

using namespace JSC;

namespace WebCore {

const ClassInfo JSDOMGlobalObject::s_info = { "DOMGlobalObject", &JSGlobalObject::s_info, 0, CREATE_METHOD_TABLE(JSDOMGlobalObject) };

JSDOMGlobalObject::JSDOMGlobalObject(VM& vm, Structure* structure, PassRefPtr<DOMWrapperWorld> world, const GlobalObjectMethodTable* globalObjectMethodTable)
    : JSGlobalObject(vm, structure, globalObjectMethodTable)
    , m_currentEvent(0)
    , m_world(world)
    , m_worldIsNormal(m_world->isNormal())
    , m_builtinInternalFunctions(vm)
{
    ASSERT(m_world);
}

void JSDOMGlobalObject::destroy(JSCell* cell)
{
    static_cast<JSDOMGlobalObject*>(cell)->JSDOMGlobalObject::~JSDOMGlobalObject();
}

void JSDOMGlobalObject::addBuiltinGlobals(VM& vm)
{
    m_builtinInternalFunctions.initialize(*this, vm);

#if ENABLE(STREAMS_API)
    JSObject* privateReadableStreamControllerConstructor = createReadableStreamControllerPrivateConstructor(vm, *this);
    JSObject* privateReadableStreamReaderConstructor = createReadableStreamReaderPrivateConstructor(vm, *this);

    ASSERT(!constructors().get(privateReadableStreamControllerConstructor->info()).get());
    ASSERT(!constructors().get(privateReadableStreamReaderConstructor->info()).get());
    JSC::WriteBarrier<JSC::JSObject> temp;
    constructors().add(privateReadableStreamControllerConstructor->info(), temp).iterator->value.set(vm, this, privateReadableStreamControllerConstructor);
    constructors().add(privateReadableStreamReaderConstructor->info(), temp).iterator->value.set(vm, this, privateReadableStreamReaderConstructor);

    JSVMClientData& clientData = *static_cast<JSVMClientData*>(vm.clientData);
    JSDOMGlobalObject::GlobalPropertyInfo staticGlobals[] = {
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamClosedPrivateName(), jsNumber(1), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamClosingPrivateName(), jsNumber(2), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamErroredPrivateName(), jsNumber(3), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamReadablePrivateName(), jsNumber(4), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamWaitingPrivateName(), jsNumber(5), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamWritablePrivateName(), jsNumber(6), DontDelete | ReadOnly),
#if ENABLE(MEDIA_STREAM)
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().MediaStreamTrackPrivateName(), JSMediaStreamTrack::getConstructor(vm, this), DontDelete | ReadOnly),
#endif
#if ENABLE(STREAMS_API)
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().ReadableStreamPrivateName(), JSReadableStream::getConstructor(vm, this), DontDelete | ReadOnly),
#endif
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().ReadableStreamControllerPrivateName(), privateReadableStreamControllerConstructor, DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().ReadableStreamReaderPrivateName(), privateReadableStreamReaderConstructor, DontDelete | ReadOnly),
#if ENABLE(WEB_RTC)
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().RTCIceCandidatePrivateName(), JSRTCIceCandidate::getConstructor(vm, this), DontDelete | ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().RTCSessionDescriptionPrivateName(), JSRTCSessionDescription::getConstructor(vm, this), DontDelete | ReadOnly),
#endif
    };
    addStaticGlobals(staticGlobals, WTF_ARRAY_LENGTH(staticGlobals));
#endif
}

void JSDOMGlobalObject::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    addBuiltinGlobals(vm);
}

void JSDOMGlobalObject::finishCreation(VM& vm, JSObject* thisValue)
{
    Base::finishCreation(vm, thisValue);
    ASSERT(inherits(info()));
    
    addBuiltinGlobals(vm);
}

ScriptExecutionContext* JSDOMGlobalObject::scriptExecutionContext() const
{
    if (inherits(JSDOMWindowBase::info()))
        return jsCast<const JSDOMWindowBase*>(this)->scriptExecutionContext();
    if (inherits(JSWorkerGlobalScopeBase::info()))
        return jsCast<const JSWorkerGlobalScopeBase*>(this)->scriptExecutionContext();
    ASSERT_NOT_REACHED();
    return 0;
}

void JSDOMGlobalObject::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSDOMGlobalObject* thisObject = jsCast<JSDOMGlobalObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    for (auto& structure : thisObject->structures().values())
        visitor.append(&structure);

    for (auto& constructor : thisObject->constructors().values())
        visitor.append(&constructor);

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
