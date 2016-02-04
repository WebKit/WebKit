/*
 * Copyright (C) 2016 Canon, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY CANON INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CANON INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JSKeyValueIterator_h
#define JSKeyValueIterator_h

#include "JSDOMBinding.h"
#include <runtime/JSDestructibleObject.h>

namespace WebCore {

// FIXME: Update binding generator to allow getting DOMWrapped from JSWrapper.
template<typename JSWrapper, typename DOMWrapped>
class JSKeyValueIteratorPrototype : public JSC::JSNonFinalObject {
public:
    typedef JSC::JSNonFinalObject Base;

    static JSKeyValueIteratorPrototype* create(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::Structure* structure)
    {
        JSKeyValueIteratorPrototype* prototype = new (NotNull, JSC::allocateCell<JSKeyValueIteratorPrototype>(vm.heap)) JSKeyValueIteratorPrototype(vm, structure);
        prototype->finishCreation(vm, globalObject);
        return prototype;
    }

    DECLARE_INFO;

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

private:
    JSKeyValueIteratorPrototype(JSC::VM& vm, JSC::Structure* structure) : Base(vm, structure) { }

    void finishCreation(JSC::VM&, JSC::JSGlobalObject*);
};

enum class IterationKind { Key, Value, KeyValue };

template<typename JSWrapper, typename DOMWrapped>
class JSKeyValueIterator: public JSDOMObject {
public:
    typedef JSDOMObject Base;

    DECLARE_EXPORT_INFO;

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

    static JSKeyValueIterator* create(JSC::VM& vm, JSC::Structure* structure, JSWrapper& iteratedObject, IterationKind kind)
    {
        JSKeyValueIterator* instance = new (NotNull, JSC::allocateCell<JSKeyValueIterator>(vm.heap)) JSKeyValueIterator(structure, iteratedObject, kind);
        instance->finishCreation(vm);
        return instance;
    }

    static JSKeyValueIteratorPrototype<JSWrapper, DOMWrapped>* createPrototype(JSC::VM& vm, JSC::JSGlobalObject* globalObject)
    {
        return JSKeyValueIteratorPrototype<JSWrapper, DOMWrapped>::create(vm, globalObject,
            JSKeyValueIteratorPrototype<JSWrapper, DOMWrapped>::createStructure(vm, globalObject, globalObject->objectPrototype()));
    }

    bool next(JSC::ExecState&, JSC::JSValue&);

private:
    JSKeyValueIterator(JSC::Structure* structure, JSWrapper& iteratedObject, IterationKind kind)
        : Base(structure, *iteratedObject.globalObject())
        , m_iterator(iteratedObject.wrapped().createIterator())
        , m_kind(kind)
    {
    }

    static void destroy(JSC::JSCell*);

    typename DOMWrapped::Iterator m_iterator;
    IterationKind m_kind;
};

template<typename JSWrapper, typename DOMWrapped>
JSKeyValueIterator<JSWrapper, DOMWrapped>* createIterator(JSDOMGlobalObject& globalObject, JSWrapper& wrapper, IterationKind kind)
{
    return JSKeyValueIterator<JSWrapper, DOMWrapped>::create(globalObject.vm(), getDOMStructure<JSKeyValueIterator<JSWrapper, DOMWrapped>>(globalObject.vm(), globalObject), wrapper, kind);
}

template<typename JSWrapper, typename DOMWrapped>
void JSKeyValueIterator<JSWrapper, DOMWrapped>::destroy(JSCell* cell)
{
    JSKeyValueIterator<JSWrapper, DOMWrapped>* thisObject = JSC::jsCast<JSKeyValueIterator<JSWrapper, DOMWrapped>*>(cell);
    thisObject->JSKeyValueIterator<JSWrapper, DOMWrapped>::~JSKeyValueIterator();
}

template<typename JSWrapper, typename DOMWrapped>
bool JSKeyValueIterator<JSWrapper, DOMWrapped>::next(JSC::ExecState& state, JSC::JSValue& value)
{
    typename DOMWrapped::Iterator::Key nextKey;
    typename DOMWrapped::Iterator::Value nextValue;
    if (m_iterator.next(nextKey, nextValue)) {
        value = JSC::jsUndefined();
        return true;
    }
    if (m_kind == IterationKind::Value)
        value = toJS(&state, globalObject(), nextValue);
    else if (m_kind == IterationKind::Key)
        value = toJS(&state, globalObject(), nextKey);
    else
        value = jsPair(state, globalObject(), nextKey, nextValue);
    return false;
}

template<typename JSWrapper, typename DOMWrapped>
JSC::EncodedJSValue JSC_HOST_CALL JSKeyValueIteratorPrototypeFunctionNext(JSC::ExecState* state)
{
    JSC::JSValue result;
    JSKeyValueIterator<JSWrapper, DOMWrapped>* iterator = JSC::jsDynamicCast<JSKeyValueIterator<JSWrapper, DOMWrapped>*>(state->thisValue());
    if (!iterator)
        return JSC::JSValue::encode(throwTypeError(state, ASCIILiteral("Cannot call next() on a non-Iterator object")));

    bool isDone = iterator->next(*state, result);
    return JSC::JSValue::encode(createIteratorResultObject(state, result, isDone));
}

using NextFunction = JSC::EncodedJSValue JSC_HOST_CALL (*)(JSC::CallFrame*);
template<typename JSWrapper, typename DOMWrapped>
void JSKeyValueIteratorPrototype<JSWrapper, DOMWrapped>::finishCreation(JSC::VM& vm, JSC::JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    NextFunction nextFunction = JSKeyValueIteratorPrototypeFunctionNext<JSWrapper, DOMWrapped>;
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->next, nextFunction, JSC::DontEnum, 0, JSC::NoIntrinsic);
}

}

#endif // !defined(JSKeyValueIterator_h)
