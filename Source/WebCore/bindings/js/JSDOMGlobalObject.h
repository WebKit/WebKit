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

#pragma once

#include "WebCoreJSBuiltinInternals.h"
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSObjectInlines.h>
#include <JavaScriptCore/LockDuringMarking.h>

namespace WebCore {

class DOMGuardedObject;
class Event;
class DOMWrapperWorld;
class ScriptExecutionContext;

using JSDOMStructureMap = HashMap<const JSC::ClassInfo*, JSC::WriteBarrier<JSC::Structure>>;
using JSDOMConstructorMap = HashMap<const JSC::ClassInfo*, JSC::WriteBarrier<JSC::JSObject>>;
using DOMGuardedObjectSet = HashSet<DOMGuardedObject*>;

class WEBCORE_EXPORT JSDOMGlobalObject : public JSC::JSGlobalObject {
public:
    struct JSDOMGlobalObjectData;

    using Base = JSC::JSGlobalObject;

    static const JSC::ClassInfo s_info;

    template<typename, JSC::SubspaceAccess>
    static void subspaceFor(JSC::VM&) { RELEASE_ASSERT_NOT_REACHED(); }

    static void destroy(JSC::JSCell*);
public:
    Lock& gcLock() { return m_gcLock; }

    JSDOMStructureMap& structures(const AbstractLocker&) { return m_structures; }
    JSDOMConstructorMap& constructors(const AbstractLocker&) { return m_constructors; }

    DOMGuardedObjectSet& guardedObjects(const AbstractLocker&) { return m_guardedObjects; }

    ScriptExecutionContext* scriptExecutionContext() const;

    // Make binding code generation easier.
    JSDOMGlobalObject* globalObject() { return this; }

    void setCurrentEvent(Event*);
    Event* currentEvent() const;

    static void visitChildren(JSC::JSCell*, JSC::SlotVisitor&);

    DOMWrapperWorld& world() { return m_world.get(); }
    bool worldIsNormal() const { return m_worldIsNormal; }
    static ptrdiff_t offsetOfWorldIsNormal() { return OBJECT_OFFSETOF(JSDOMGlobalObject, m_worldIsNormal); }

    JSBuiltinInternalFunctions& builtinInternalFunctions() { return m_builtinInternalFunctions; }

    static void reportUncaughtExceptionAtEventLoop(JSGlobalObject*, JSC::Exception*);

public:
    ~JSDOMGlobalObject();

    static constexpr const JSC::ClassInfo* info() { return &s_info; }

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, 0, prototype, JSC::TypeInfo(JSC::GlobalObjectType, StructureFlags), info());
    }

protected:
    JSDOMGlobalObject(JSC::VM&, JSC::Structure*, Ref<DOMWrapperWorld>&&, const JSC::GlobalObjectMethodTable* = nullptr);
    void finishCreation(JSC::VM&);
    void finishCreation(JSC::VM&, JSC::JSObject*);

    static void promiseRejectionTracker(JSC::JSGlobalObject*, JSC::JSPromise*, JSC::JSPromiseRejectionOperation);

    JSDOMStructureMap m_structures;
    JSDOMConstructorMap m_constructors;
    DOMGuardedObjectSet m_guardedObjects;

    Ref<DOMWrapperWorld> m_world;
    uint8_t m_worldIsNormal;
    Lock m_gcLock;

private:
    void addBuiltinGlobals(JSC::VM&);
    friend void JSBuiltinInternalFunctions::initialize(JSDOMGlobalObject&);

    Event* m_currentEvent { nullptr };

    JSBuiltinInternalFunctions m_builtinInternalFunctions;
};

template<class ConstructorClass>
inline JSC::JSObject* getDOMConstructor(JSC::VM& vm, const JSDOMGlobalObject& globalObject)
{
    if (JSC::JSObject* constructor = const_cast<JSDOMGlobalObject&>(globalObject).constructors(NoLockingNecessary).get(ConstructorClass::info()).get())
        return constructor;
    JSC::JSObject* constructor = ConstructorClass::create(vm, ConstructorClass::createStructure(vm, const_cast<JSDOMGlobalObject&>(globalObject), ConstructorClass::prototypeForStructure(vm, globalObject)), const_cast<JSDOMGlobalObject&>(globalObject));
    ASSERT(!const_cast<JSDOMGlobalObject&>(globalObject).constructors(NoLockingNecessary).contains(ConstructorClass::info()));
    JSC::WriteBarrier<JSC::JSObject> temp;
    JSDOMGlobalObject& mutableGlobalObject = const_cast<JSDOMGlobalObject&>(globalObject);
    auto locker = JSC::lockDuringMarking(vm.heap, mutableGlobalObject.gcLock());
    mutableGlobalObject.constructors(locker).add(ConstructorClass::info(), temp).iterator->value.set(vm, &globalObject, constructor);
    return constructor;
}

WEBCORE_EXPORT JSDOMGlobalObject& callerGlobalObject(JSC::JSGlobalObject&, JSC::CallFrame&);

JSDOMGlobalObject* toJSDOMGlobalObject(ScriptExecutionContext&, DOMWrapperWorld&);

} // namespace WebCore
