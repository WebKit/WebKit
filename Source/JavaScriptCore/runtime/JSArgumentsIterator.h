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

#ifndef JSArgumentsIterator_h
#define JSArgumentsIterator_h

#include "Arguments.h"

namespace JSC {

class JSArgumentsIterator : public JSNonFinalObject {
public:
    typedef JSNonFinalObject Base;

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static JSArgumentsIterator* create(VM& vm, Structure* structure, Arguments* arguments)
    {
        JSArgumentsIterator* instance = new (NotNull, allocateCell<JSArgumentsIterator>(vm.heap)) JSArgumentsIterator(vm, structure);
        instance->finishCreation(vm, arguments);
        return instance;
    }

    bool next(CallFrame* callFrame, JSValue& value)
    {
        if (m_nextIndex >= m_arguments->length(callFrame))
            return false;
        value = m_arguments->tryGetArgument(m_nextIndex++);
        if (!value)
            value = jsUndefined();
        return true;
    }

private:

    static const unsigned StructureFlags = Base::StructureFlags;

    JSArgumentsIterator(VM& vm, Structure* structure)
        : Base(vm, structure)
        , m_nextIndex(0)
    {
    }

    void finishCreation(VM&, Arguments*);
    
    WriteBarrier<Arguments> m_arguments;
    size_t m_nextIndex;
};

}

#endif // !defined(JSArgumentsIterator_h)
