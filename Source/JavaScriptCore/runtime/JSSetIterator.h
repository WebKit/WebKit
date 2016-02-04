/*
 * Copyright (C) 2013 Apple, Inc. All rights reserved.
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

#ifndef JSSetIterator_h
#define JSSetIterator_h

#include "JSObject.h"
#include "JSSet.h"

#include "MapData.h"

namespace JSC {
enum SetIterationKind : uint32_t {
    SetIterateKey,
    SetIterateValue,
    SetIterateKeyValue,
};

class JSSetIterator : public JSNonFinalObject {
public:
    typedef JSNonFinalObject Base;

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static JSSetIterator* create(VM& vm, Structure* structure, JSSet* iteratedObject, SetIterationKind kind)
    {
        JSSetIterator* instance = new (NotNull, allocateCell<JSSetIterator>(vm.heap)) JSSetIterator(vm, structure, iteratedObject, kind);
        instance->finishCreation(vm, iteratedObject);
        return instance;
    }

    bool next(CallFrame* callFrame, JSValue& value)
    {
        WTF::KeyValuePair<JSValue, JSValue> pair;
        if (!m_iterator.next(pair))
            return false;
        if (m_kind == SetIterateValue || m_kind == SetIterateKey)
            value = pair.key;
        else
            value = createPair(callFrame, pair.key, pair.key);
        return true;
    }

    void finish()
    {
        m_iterator.finish();
    }

    SetIterationKind kind() const { return m_kind; }
    JSValue iteratedValue() const { return m_set.get(); }
    JSSetIterator* clone(ExecState*);

    JSSet::SetData::IteratorData* iteratorData()
    {
        return &m_iterator;
    }

private:
    JSSetIterator(VM& vm, Structure* structure, JSSet* iteratedObject, SetIterationKind kind)
        : Base(vm, structure)
        , m_iterator(iteratedObject->m_setData.createIteratorData(this))
        , m_kind(kind)
    {
    }

    JS_EXPORT_PRIVATE void finishCreation(VM&, JSSet*);
    JS_EXPORT_PRIVATE JSValue createPair(CallFrame*, JSValue, JSValue);
    static void visitChildren(JSCell*, SlotVisitor&);

    WriteBarrier<JSSet> m_set;
    JSSet::SetData::IteratorData m_iterator;
    SetIterationKind m_kind;
};
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSSetIterator);

}

#endif // !defined(JSSetIterator_h)
