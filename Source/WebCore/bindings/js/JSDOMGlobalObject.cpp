/*
 * Copyright (C) 2008-2018 Apple Inc. All Rights Reserved.
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
#include "JSRemoteDOMWindow.h"
#include "JSWorkerGlobalScope.h"
#include "JSWorkletGlobalScope.h"
#include "RejectedPromiseTracker.h"
#include "RuntimeEnabledFeatures.h"
#include "StructuredClone.h"
#include "WebCoreJSClientData.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/BuiltinNames.h>
#include <JavaScriptCore/CodeBlock.h>
#include <JavaScriptCore/JSInternalPromise.h>
#include <JavaScriptCore/StructureInlines.h>

namespace WebCore {
using namespace JSC;

EncodedJSValue JSC_HOST_CALL makeThisTypeErrorForBuiltins(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL makeGetterTypeErrorForBuiltins(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL isReadableByteStreamAPIEnabled(JSGlobalObject*, CallFrame*);

const ClassInfo JSDOMGlobalObject::s_info = { "DOMGlobalObject", &JSGlobalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDOMGlobalObject) };

JSDOMGlobalObject::JSDOMGlobalObject(VM& vm, Structure* structure, Ref<DOMWrapperWorld>&& world, const GlobalObjectMethodTable* globalObjectMethodTable)
    : JSGlobalObject(vm, structure, globalObjectMethodTable)
    , m_world(WTFMove(world))
    , m_worldIsNormal(m_world->isNormal())
    , m_builtinInternalFunctions(vm)
{
}

JSDOMGlobalObject::~JSDOMGlobalObject() = default;

void JSDOMGlobalObject::destroy(JSCell* cell)
{
    static_cast<JSDOMGlobalObject*>(cell)->JSDOMGlobalObject::~JSDOMGlobalObject();
}

EncodedJSValue JSC_HOST_CALL makeThisTypeErrorForBuiltins(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount() == 2);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto interfaceName = callFrame->uncheckedArgument(0).getString(globalObject);
    scope.assertNoException();
    auto functionName = callFrame->uncheckedArgument(1).getString(globalObject);
    scope.assertNoException();
    return JSValue::encode(createTypeError(globalObject, makeThisTypeErrorMessage(interfaceName.utf8().data(), functionName.utf8().data())));
}

EncodedJSValue JSC_HOST_CALL makeGetterTypeErrorForBuiltins(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount() == 2);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto interfaceName = callFrame->uncheckedArgument(0).getString(globalObject);
    scope.assertNoException();
    auto attributeName = callFrame->uncheckedArgument(1).getString(globalObject);
    scope.assertNoException();

    auto error = static_cast<ErrorInstance*>(createTypeError(globalObject, makeGetterTypeErrorMessage(interfaceName.utf8().data(), attributeName.utf8().data())));
    error->setNativeGetterTypeError();
    return JSValue::encode(error);
}

EncodedJSValue JSC_HOST_CALL isReadableByteStreamAPIEnabled(JSGlobalObject*, CallFrame*)
{
    return JSValue::encode(jsBoolean(RuntimeEnabledFeatures::sharedFeatures().readableByteStreamAPIEnabled()));
}

void JSDOMGlobalObject::addBuiltinGlobals(VM& vm)
{
    m_builtinInternalFunctions.initialize(*this);

    JSVMClientData& clientData = *static_cast<JSVMClientData*>(vm.clientData);
    JSDOMGlobalObject::GlobalPropertyInfo staticGlobals[] = {
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().makeThisTypeErrorPrivateName(),
            JSFunction::create(vm, this, 2, String(), makeThisTypeErrorForBuiltins), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().makeGetterTypeErrorPrivateName(),
            JSFunction::create(vm, this, 2, String(), makeGetterTypeErrorForBuiltins), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().cloneArrayBufferPrivateName(),
            JSFunction::create(vm, this, 3, String(), cloneArrayBuffer), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().structuredCloneArrayBufferPrivateName(),
            JSFunction::create(vm, this, 1, String(), structuredCloneArrayBuffer), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().structuredCloneArrayBufferViewPrivateName(),
            JSFunction::create(vm, this, 1, String(), structuredCloneArrayBufferView), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(vm.propertyNames->builtinNames().ArrayBufferPrivateName(), arrayBufferConstructor(), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamClosedPrivateName(), jsNumber(1), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamClosingPrivateName(), jsNumber(2), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamErroredPrivateName(), jsNumber(3), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamReadablePrivateName(), jsNumber(4), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamWaitingPrivateName(), jsNumber(5), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamWritablePrivateName(), jsNumber(6), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().readableByteStreamAPIEnabledPrivateName(), JSFunction::create(vm, this, 0, String(), isReadableByteStreamAPIEnabled), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
    };
    addStaticGlobals(staticGlobals, WTF_ARRAY_LENGTH(staticGlobals));
}

void JSDOMGlobalObject::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    addBuiltinGlobals(vm);

    RELEASE_ASSERT(classInfo(vm));
}

void JSDOMGlobalObject::finishCreation(VM& vm, JSObject* thisValue)
{
    Base::finishCreation(vm, thisValue);
    ASSERT(inherits(vm, info()));

    addBuiltinGlobals(vm);

    RELEASE_ASSERT(classInfo(vm));
}

ScriptExecutionContext* JSDOMGlobalObject::scriptExecutionContext() const
{
    if (inherits<JSDOMWindowBase>(vm()))
        return jsCast<const JSDOMWindowBase*>(this)->scriptExecutionContext();
    if (inherits<JSRemoteDOMWindowBase>(vm()))
        return nullptr;
    if (inherits<JSWorkerGlobalScopeBase>(vm()))
        return jsCast<const JSWorkerGlobalScopeBase*>(this)->scriptExecutionContext();
#if ENABLE(CSS_PAINTING_API)
    if (inherits<JSWorkletGlobalScopeBase>(vm()))
        return jsCast<const JSWorkletGlobalScopeBase*>(this)->scriptExecutionContext();
#endif
    dataLog("Unexpected global object: ", JSValue(this), "\n");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
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

void JSDOMGlobalObject::promiseRejectionTracker(JSGlobalObject* jsGlobalObject, JSPromise* promise, JSPromiseRejectionOperation operation)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#the-hostpromiserejectiontracker-implementation

    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(jsGlobalObject);
    auto* context = globalObject.scriptExecutionContext();
    if (!context)
        return;

    // FIXME: If script has muted errors (cross origin), terminate these steps.
    // <https://webkit.org/b/171415> Implement the `muted-errors` property of Scripts to avoid onerror/onunhandledrejection for cross-origin scripts

    switch (operation) {
    case JSPromiseRejectionOperation::Reject:
        context->ensureRejectedPromiseTracker().promiseRejected(globalObject, *promise);
        break;
    case JSPromiseRejectionOperation::Handle:
        context->ensureRejectedPromiseTracker().promiseHandled(globalObject, *promise);
        break;
    }
}

JSDOMGlobalObject& callerGlobalObject(JSGlobalObject& lexicalGlobalObject, CallFrame& callFrame)
{
    class GetCallerGlobalObjectFunctor {
    public:
        GetCallerGlobalObjectFunctor() = default;

        StackVisitor::Status operator()(StackVisitor& visitor) const
        {
            if (!m_hasSkippedFirstFrame) {
                m_hasSkippedFirstFrame = true;
                return StackVisitor::Continue;
            }

            if (auto* codeBlock = visitor->codeBlock())
                m_globalObject = codeBlock->globalObject();
            else {
                ASSERT(visitor->callee().rawPtr());
                // FIXME: Callee is not an object if the caller is Web Assembly.
                // Figure out what to do here. We can probably get the global object
                // from the top-most Wasm Instance. https://bugs.webkit.org/show_bug.cgi?id=165721
                if (visitor->callee().isCell() && visitor->callee().asCell()->isObject())
                    m_globalObject = jsCast<JSObject*>(visitor->callee().asCell())->globalObject();
            }
            return StackVisitor::Done;
        }

        JSGlobalObject* globalObject() const { return m_globalObject; }

    private:
        mutable bool m_hasSkippedFirstFrame { false };
        mutable JSGlobalObject* m_globalObject { nullptr };
    };

    GetCallerGlobalObjectFunctor iter;
    callFrame.iterate(lexicalGlobalObject.vm(), iter);
    if (iter.globalObject())
        return *jsCast<JSDOMGlobalObject*>(iter.globalObject());

    // If we cannot find JSGlobalObject in caller frames, we just return the current lexicalGlobalObject.
    return *jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject);
}

JSDOMGlobalObject* toJSDOMGlobalObject(ScriptExecutionContext& context, DOMWrapperWorld& world)
{
    if (is<Document>(context))
        return toJSDOMWindow(downcast<Document>(context).frame(), world);

    if (is<WorkerGlobalScope>(context))
        return downcast<WorkerGlobalScope>(context).script()->workerGlobalScopeWrapper();

    ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace WebCore
