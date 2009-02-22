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

    class Register {
    public:
        Register();
        Register(JSValuePtr);

        JSValuePtr jsValue(CallFrame*) const;
        JSValuePtr getJSValue() const;

        bool marked() const;
        void mark();
        
    private:
        friend class ExecState;
        friend class Interpreter;
        friend class JITStubs;

        // Only CallFrame, Interpreter, and JITStubs should use these functions.

        Register(intptr_t);

        Register(JSActivation*);
        Register(Arguments*);
        Register(CallFrame*);
        Register(CodeBlock*);
        Register(JSFunction*);
        Register(JSPropertyNameIterator*);
        Register(ScopeChainNode*);
        Register(Instruction*);

        intptr_t i() const;
        void* v() const;

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
            JSValueEncodedAsPointer* value;

            JSActivation* activation;
            Arguments* arguments;
            CallFrame* callFrame;
            CodeBlock* codeBlock;
            JSFunction* function;
            JSPropertyNameIterator* propertyNameIterator;
            ScopeChainNode* scopeChain;
            Instruction* vPC;
        } u;

#ifndef NDEBUG
        enum {
            EmptyType,

            IntType,
            ValueType,

            ActivationType,
            ArgumentsType,
            CallFrameType,
            CodeBlockType,
            FunctionType,
            InstructionType,
            PropertyNameIteratorType,
            RegisterType,
            ScopeChainNodeType
        } m_type;
#endif
    };

#ifndef NDEBUG
    #define SET_TYPE(type) m_type = (type)
    // FIXME: The CTI code to put value into registers doesn't set m_type.
    // Once it does, we can turn this assertion back on.
    #define ASSERT_TYPE(type)
#else
    #define SET_TYPE(type)
    #define ASSERT_TYPE(type)
#endif

    ALWAYS_INLINE Register::Register()
    {
#ifndef NDEBUG
        SET_TYPE(EmptyType);
        u.value = JSValuePtr::encode(noValue());
#endif
    }

    ALWAYS_INLINE Register::Register(JSValuePtr v)
    {
        SET_TYPE(ValueType);
        u.value = JSValuePtr::encode(v);
    }

    // This function is scaffolding for legacy clients. It will eventually go away.
    ALWAYS_INLINE JSValuePtr Register::jsValue(CallFrame*) const
    {
        // Once registers hold doubles, this function will allocate a JSValue*
        // if the register doesn't hold one already. 
        ASSERT_TYPE(ValueType);
        return JSValuePtr::decode(u.value);
    }
    
    ALWAYS_INLINE JSValuePtr Register::getJSValue() const
    {
        ASSERT_TYPE(JSValueType);
        return JSValuePtr::decode(u.value);
    }
    
    ALWAYS_INLINE bool Register::marked() const
    {
        return getJSValue().marked();
    }

    ALWAYS_INLINE void Register::mark()
    {
        getJSValue().mark();
    }
    
    // Interpreter functions

    ALWAYS_INLINE Register::Register(Arguments* arguments)
    {
        SET_TYPE(ArgumentsType);
        u.arguments = arguments;
    }

    ALWAYS_INLINE Register::Register(JSActivation* activation)
    {
        SET_TYPE(ActivationType);
        u.activation = activation;
    }

    ALWAYS_INLINE Register::Register(CallFrame* callFrame)
    {
        SET_TYPE(CallFrameType);
        u.callFrame = callFrame;
    }

    ALWAYS_INLINE Register::Register(CodeBlock* codeBlock)
    {
        SET_TYPE(CodeBlockType);
        u.codeBlock = codeBlock;
    }

    ALWAYS_INLINE Register::Register(JSFunction* function)
    {
        SET_TYPE(FunctionType);
        u.function = function;
    }

    ALWAYS_INLINE Register::Register(Instruction* vPC)
    {
        SET_TYPE(InstructionType);
        u.vPC = vPC;
    }

    ALWAYS_INLINE Register::Register(ScopeChainNode* scopeChain)
    {
        SET_TYPE(ScopeChainNodeType);
        u.scopeChain = scopeChain;
    }

    ALWAYS_INLINE Register::Register(JSPropertyNameIterator* propertyNameIterator)
    {
        SET_TYPE(PropertyNameIteratorType);
        u.propertyNameIterator = propertyNameIterator;
    }

    ALWAYS_INLINE Register::Register(intptr_t i)
    {
        SET_TYPE(IntType);
        u.i = i;
    }

    ALWAYS_INLINE intptr_t Register::i() const
    {
        ASSERT_TYPE(IntType);
        return u.i;
    }
    
    ALWAYS_INLINE void* Register::v() const
    {
        return u.v;
    }

    ALWAYS_INLINE JSActivation* Register::activation() const
    {
        ASSERT_TYPE(ActivationType);
        return u.activation;
    }
    
    ALWAYS_INLINE Arguments* Register::arguments() const
    {
        ASSERT_TYPE(ArgumentsType);
        return u.arguments;
    }
    
    ALWAYS_INLINE CallFrame* Register::callFrame() const
    {
        ASSERT_TYPE(CallFrameType);
        return u.callFrame;
    }
    
    ALWAYS_INLINE CodeBlock* Register::codeBlock() const
    {
        ASSERT_TYPE(CodeBlockType);
        return u.codeBlock;
    }
    
    ALWAYS_INLINE JSFunction* Register::function() const
    {
        ASSERT_TYPE(FunctionType);
        return u.function;
    }
    
    ALWAYS_INLINE JSPropertyNameIterator* Register::propertyNameIterator() const
    {
        ASSERT_TYPE(PropertyNameIteratorType);
        return u.propertyNameIterator;
    }
    
    ALWAYS_INLINE ScopeChainNode* Register::scopeChain() const
    {
        ASSERT_TYPE(ScopeChainNodeType);
        return u.scopeChain;
    }
    
    ALWAYS_INLINE Instruction* Register::vPC() const
    {
        ASSERT_TYPE(InstructionType);
        return u.vPC;
    }

    #undef SET_TYPE
    #undef ASSERT_TYPE

} // namespace JSC

namespace WTF {

    template<> struct VectorTraits<JSC::Register> : VectorTraitsBase<true, JSC::Register> { };

} // namespace WTF

#endif // Register_h
