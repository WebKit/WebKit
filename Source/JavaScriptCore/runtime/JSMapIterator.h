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

#ifndef JSMapIterator_h
#define JSMapIterator_h

#include "JSMap.h"
#include "JSObject.h"
#include "MapData.h"

namespace JSC {
enum MapIterationKind : uint32_t {
    MapIterateKey,
    MapIterateValue,
    MapIterateKeyValue,
};

class JSMapIterator : public JSNonFinalObject {
public:
    typedef JSNonFinalObject Base;

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static JSMapIterator* create(VM& vm, Structure* structure, JSMap* iteratedObject, MapIterationKind kind)
    {
        JSMapIterator* instance = new (NotNull, allocateCell<JSMapIterator>(vm.heap)) JSMapIterator(vm, structure, iteratedObject, kind);
        instance->finishCreation(vm, iteratedObject);
        return instance;
    }

    bool next(CallFrame* callFrame, JSValue& value)
    {
        WTF::KeyValuePair<JSValue, JSValue> pair;
        if (!m_iterator.next(pair))
            return false;

        if (m_kind == MapIterateValue)
            value = pair.value;
        else if (m_kind == MapIterateKey)
            value = pair.key;
        else
            value = createPair(callFrame, pair.key, pair.value);
        return true;
    }

    bool nextKeyValue(JSValue& key, JSValue& value)
    {
        WTF::KeyValuePair<JSValue, JSValue> pair;
        if (!m_iterator.next(pair))
            return false;

        key = pair.key;
        value = pair.value;
        return true;
    }

    void finish()
    {
        m_iterator.finish();
    }

    MapIterationKind kind() const { return m_kind; }
    JSValue iteratedValue() const { return m_map.get(); }
    JSMapIterator* clone(ExecState*);

    JSMap::MapData::IteratorData* iteratorData()
    {
        return &m_iterator;
    }

private:
    JSMapIterator(VM& vm, Structure* structure, JSMap* iteratedObject, MapIterationKind kind)
        : Base(vm, structure)
        , m_iterator(iteratedObject->m_mapData.createIteratorData(this))
        , m_kind(kind)
    {
    }

    JS_EXPORT_PRIVATE void finishCreation(VM&, JSMap*);
    JSValue createPair(CallFrame*, JSValue, JSValue);
    static void visitChildren(JSCell*, SlotVisitor&);

    WriteBarrier<JSMap> m_map;
    JSMap::MapData::IteratorData m_iterator;
    MapIterationKind m_kind;
};
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSMapIterator);

}

#endif // !defined(JSMapIterator_h)
