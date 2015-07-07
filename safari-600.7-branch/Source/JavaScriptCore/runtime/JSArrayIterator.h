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

#ifndef JSArrayIterator_h
#define JSArrayIterator_h

#include "JSObject.h"

namespace JSC {

enum ArrayIterationKind : uint32_t {
    ArrayIterateKey,
    ArrayIterateValue,
    ArrayIterateKeyValue,
    ArrayIterateSparseTag = 4,
    ArrayIterateSparseKey,
    ArrayIterateSparseValue,
    ArrayIterateSparseKeyValue
};

class JSArrayIterator : public JSNonFinalObject {
public:
    typedef JSNonFinalObject Base;

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static JSArrayIterator* create(ExecState* exec, Structure* structure, ArrayIterationKind kind, JSObject* iteratedObject)
    {
        VM& vm = exec->vm();
        JSArrayIterator* instance = new (NotNull, allocateCell<JSArrayIterator>(vm.heap)) JSArrayIterator(vm, structure);
        instance->finishCreation(vm, structure->globalObject(), kind, iteratedObject);
        return instance;
    }

    ArrayIterationKind iterationKind() const { return m_iterationKind; }
    JSObject* iteratedObject() const { return m_iteratedObject.get(); }
    size_t nextIndex() const { return m_nextIndex; }
    void setNextIndex(size_t nextIndex) { m_nextIndex = nextIndex; }
    void finish() { m_nextIndex = std::numeric_limits<uint32_t>::max(); }
    
    using JSNonFinalObject::arrayStorageOrNull;
    static ptrdiff_t offsetOfIterationKind() { return OBJECT_OFFSETOF(JSArrayIterator, m_iterationKind); }
    static ptrdiff_t offsetOfIteratedObject() { return OBJECT_OFFSETOF(JSArrayIterator, m_iteratedObject); }
    static ptrdiff_t offsetOfNextIndex() { return OBJECT_OFFSETOF(JSArrayIterator, m_nextIndex); }

private:

    static const unsigned StructureFlags = Base::StructureFlags | OverridesVisitChildren;

    JSArrayIterator(VM& vm, Structure* structure)
        : Base(vm, structure)
        , m_nextIndex(0)
    {
    }

    void finishCreation(VM&, JSGlobalObject*, ArrayIterationKind, JSObject* iteratedObject);
    static void visitChildren(JSCell*, SlotVisitor&);
    
    ArrayIterationKind m_iterationKind;
    WriteBarrier<JSObject> m_iteratedObject;
    uint32_t m_nextIndex;
};

}

#endif // !defined(JSArrayIterator_h)
