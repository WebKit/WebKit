/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Register_h
#define Register_h

#include "JSValue.h"
#include <wtf/Assertions.h>
#include <wtf/FastAllocBase.h>
#include <wtf/VectorTraits.h>

namespace JSC {

    class Arguments;
    class CodeBlock;
    class ExecState;
    class JSActivation;
    class JSFunction;
    class JSPropertyNameIterator;
    class ScopeChainNode;

    struct Instruction;

    typedef ExecState CallFrame;

    class Register : public WTF::FastAllocBase {
    public:
        Register();
        Register(JSValue);
        Register(Arguments*);

        JSValue jsValue() const;

        bool marked() const;
        void mark();
        
        int32_t i() const;
        void* v() const;

    private:
        friend class ExecState;
        friend class Interpreter;

        // Only CallFrame, Interpreter, and JITStubs should use these functions.

        Register(intptr_t);

        Register(JSActivation*);
        Register(CallFrame*);
        Register(CodeBlock*);
        Register(JSFunction*);
        Register(JSPropertyNameIterator*);
        Register(ScopeChainNode*);
        Register(Instruction*);

        JSActivation* activation() const;
        Arguments* arguments() const;
        CallFrame* callFrame() const;
        CodeBlock* codeBlock() const;
        JSFunction* function() const;
        JSPropertyNameIterator* propertyNameIterator() const;
        ScopeChainNode* scopeChain() const;
        Instruction* vPC() const;

        union {
            intptr_t i;
            void* v;
            EncodedJSValue value;

            JSActivation* activation;
            Arguments* arguments;
            CallFrame* callFrame;
            CodeBlock* codeBlock;
            JSFunction* function;
            JSPropertyNameIterator* propertyNameIterator;
            ScopeChainNode* scopeChain;
            Instruction* vPC;
        } u;
    };

    ALWAYS_INLINE Register::Register()
    {
#ifndef NDEBUG
        u.value = JSValue::encode(JSValue());
#endif
    }

    ALWAYS_INLINE Register::Register(JSValue v)
    {
        u.value = JSValue::encode(v);
    }

    ALWAYS_INLINE JSValue Register::jsValue() const
    {
        return JSValue::decode(u.value);
    }
    
    ALWAYS_INLINE bool Register::marked() const
    {
        return jsValue().marked();
    }

    ALWAYS_INLINE void Register::mark()
    {
        jsValue().mark();
    }
    
    // Interpreter functions

    ALWAYS_INLINE Register::Register(Arguments* arguments)
    {
        u.arguments = arguments;
    }

    ALWAYS_INLINE Register::Register(JSActivation* activation)
    {
        u.activation = activation;
    }

    ALWAYS_INLINE Register::Register(CallFrame* callFrame)
    {
        u.callFrame = callFrame;
    }

    ALWAYS_INLINE Register::Register(CodeBlock* codeBlock)
    {
        u.codeBlock = codeBlock;
    }

    ALWAYS_INLINE Register::Register(JSFunction* function)
    {
        u.function = function;
    }

    ALWAYS_INLINE Register::Register(Instruction* vPC)
    {
        u.vPC = vPC;
    }

    ALWAYS_INLINE Register::Register(ScopeChainNode* scopeChain)
    {
        u.scopeChain = scopeChain;
    }

    ALWAYS_INLINE Register::Register(JSPropertyNameIterator* propertyNameIterator)
    {
        u.propertyNameIterator = propertyNameIterator;
    }

    ALWAYS_INLINE Register::Register(intptr_t i)
    {
        // See comment on 'i()' below.
        ASSERT(i == static_cast<int32_t>(i));
        u.i = i;
    }

    // Read 'i' as a 32-bit integer; we only use it to hold 32-bit values,
    // and we only write 32-bits when writing the arg count from JIT code.
    ALWAYS_INLINE int32_t Register::i() const
    {
        return static_cast<int32_t>(u.i);
    }
    
    ALWAYS_INLINE void* Register::v() const
    {
        return u.v;
    }

    ALWAYS_INLINE JSActivation* Register::activation() const
    {
        return u.activation;
    }
    
    ALWAYS_INLINE Arguments* Register::arguments() const
    {
        return u.arguments;
    }
    
    ALWAYS_INLINE CallFrame* Register::callFrame() const
    {
        return u.callFrame;
    }
    
    ALWAYS_INLINE CodeBlock* Register::codeBlock() const
    {
        return u.codeBlock;
    }
    
    ALWAYS_INLINE JSFunction* Register::function() const
    {
        return u.function;
    }
    
    ALWAYS_INLINE JSPropertyNameIterator* Register::propertyNameIterator() const
    {
        return u.propertyNameIterator;
    }
    
    ALWAYS_INLINE ScopeChainNode* Register::scopeChain() const
    {
        return u.scopeChain;
    }
    
    ALWAYS_INLINE Instruction* Register::vPC() const
    {
        return u.vPC;
    }

} // namespace JSC

namespace WTF {

    template<> struct VectorTraits<JSC::Register> : VectorTraitsBase<true, JSC::Register> { };

} // namespace WTF

#endif // Register_h
