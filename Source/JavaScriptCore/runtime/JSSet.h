/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef JSSet_h
#define JSSet_h

#include "JSDestructibleObject.h"
#include "JSObject.h"
#include "MapData.h"

namespace JSC {

class JSSetIterator;

class JSSet : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;

    friend class JSSetIterator;

    // Our marking functions expect Entry to maintain this layout, and have all
    // fields be WriteBarrier<Unknown>
    class Entry {
    private:
        WriteBarrier<Unknown> m_key;

    public:
        const WriteBarrier<Unknown>& key() const
        {
            return m_key;
        }

        const WriteBarrier<Unknown>& value() const
        {
            return m_key;
        }

        void visitChildren(SlotVisitor& visitor)
        {
            visitor.append(&m_key);
        }

        void setKey(VM& vm, const JSCell* owner, JSValue key)
        {
            m_key.set(vm, owner, key);
        }

        void setKeyWithoutWriteBarrier(JSValue key)
        {
            m_key.setWithoutWriteBarrier(key);
        }

        void setValue(VM&, const JSCell*, JSValue)
        {
        }

        void clear()
        {
            m_key.clear();
        }
    };

    typedef MapDataImpl<Entry, JSSetIterator> SetData;

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static JSSet* create(VM& vm, Structure* structure)
    {
        JSSet* instance = new (NotNull, allocateCell<JSSet>(vm.heap)) JSSet(vm, structure);
        instance->finishCreation(vm);
        return instance;
    }

    static JSSet* create(ExecState* exec, Structure* structure)
    {
        return create(exec->vm(), structure);
    }

    bool has(ExecState*, JSValue);
    size_t size(ExecState*);
    JS_EXPORT_PRIVATE void add(ExecState*, JSValue);
    void clear(ExecState*);
    bool remove(ExecState*, JSValue);

private:
    JSSet(VM& vm, Structure* structure)
        : Base(vm, structure)
        , m_setData(vm, this)
    {
    }

    static void destroy(JSCell*);
    static size_t estimatedSize(JSCell*);
    static void visitChildren(JSCell*, SlotVisitor&);
    static void copyBackingStore(JSCell*, CopyVisitor&, CopyToken);

    SetData m_setData;
};

}

#endif // !defined(JSSet_h)
