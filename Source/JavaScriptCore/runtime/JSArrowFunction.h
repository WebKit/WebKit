/*
 * Copyright (C) 2015 Aleksandr Skachkov <gskachkov@gmail.com>.
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

#ifndef JSArrowFunction_h
#define JSArrowFunction_h

#include "JSFunction.h"
#include "JSGlobalObject.h"

namespace JSC {
    
class JSGlobalObject;
class LLIntOffsetsExtractor;
class LLIntDesiredOffsets;
    
class JSArrowFunction : public JSFunction {
    friend class JIT;
#if ENABLE(DFG_JIT)
    friend class DFG::SpeculativeJIT;
    friend class DFG::JITCompiler;
#endif
    friend class VM;
public:
    typedef JSFunction Base;

    static JSArrowFunction* create(VM&, FunctionExecutable*, JSScope*, JSValue);
    static JSArrowFunction* createWithInvalidatedReallocationWatchpoint(VM&, FunctionExecutable*, JSScope*, JSValue);

    static void destroy(JSCell*);
    
    static size_t allocationSize(size_t inlineCapacity)
    {
        ASSERT_UNUSED(inlineCapacity, !inlineCapacity);
        return sizeof(JSArrowFunction);
    }
    
    static JSArrowFunction* createImpl(VM& vm, FunctionExecutable* executable, JSScope* scope, JSValue boundThis)
    {
        JSArrowFunction* function = new (NotNull, allocateCell<JSArrowFunction>(vm.heap)) JSArrowFunction(vm, executable, scope, boundThis);
        ASSERT(function->structure()->globalObject());
        function->finishCreation(vm);
        return function;
    }
    
    static ConstructType getConstructData(JSCell*, ConstructData&);

    JSValue boundThis() { return m_boundThis.get(); }
    
    DECLARE_EXPORT_INFO;
    
    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        ASSERT(globalObject);
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
    }
    
    static inline ptrdiff_t offsetOfThisValue()
    {
        return OBJECT_OFFSETOF(JSArrowFunction, m_boundThis);
    }
    
    const static unsigned StructureFlags = Base::StructureFlags;

protected:
    static void visitChildren(JSCell*, SlotVisitor&);
        
private:
    JSArrowFunction(VM&, FunctionExecutable*, JSScope*, JSValue);
    
    friend class LLIntOffsetsExtractor;
    
    WriteBarrier<Unknown> m_boundThis;
};
    
} // namespace JSC

#endif // JSArrowFunction_h
