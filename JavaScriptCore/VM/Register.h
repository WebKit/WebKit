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

namespace KJS {

    class CodeBlock;
    class ExecState;
    class JSPropertyNameIterator;
    class JSValue;
    class ScopeChainNode;
    struct Instruction;
    
    static JSValue* const nullJSValue = 0;

    class Register {
    public:
        Register();
        Register(JSValue*);

        JSValue* jsValue(ExecState*) const;
        JSValue* getJSValue() const;

        bool marked() const;
        void mark();
        
    private:
        friend class Machine;

        // Only the Machine should use these functions.

        Register(CodeBlock*);
        Register(ScopeChainNode*);
        Register(intptr_t);
        Register(Register*);
        Register(Instruction*);
        Register(JSPropertyNameIterator*);
        explicit Register(void*);

        CodeBlock* codeBlock() const;
        ScopeChainNode* scopeChain() const;
        intptr_t i() const;
        Register* r() const;
        Instruction* vPC() const;
        JSPropertyNameIterator* jsPropertyNameIterator() const;
        void* v() const;

        union {
        private:
            friend class Register;

            CodeBlock* codeBlock;
            Instruction* vPC;
            JSValue* jsValue;
            ScopeChainNode* scopeChain;
            JSPropertyNameIterator* jsPropertyNameIterator;
            Register* r;
            void* v;
            intptr_t i;
        } u;

#ifndef NDEBUG
        enum {
            CodeBlockType = 0, 
            InstructionType, 
            JSValueType, 
            ScopeChainNodeType, 
            JSPropertyNameIteratorType, 
            RegisterType, 
            IntType
        } m_type;
#endif

// FIXME: The commented out ASSERTs below are valid; NDEBUG CTI should set these when up to date.
//        static inline ptrdiff_t offsetOf_type()
//        {
//            return OBJECT_OFFSET(Register, m_type);
//        }
    };

    ALWAYS_INLINE Register::Register()
    {
#ifndef NDEBUG
        *this = nullJSValue;
#endif
    }

    ALWAYS_INLINE Register::Register(JSValue* v)
    {
#ifndef NDEBUG
        m_type = JSValueType;
#endif
        u.jsValue = v;
    }
    
    // This function is scaffolding for legacy clients. It will eventually go away.
    ALWAYS_INLINE JSValue* Register::jsValue(ExecState*) const
    {
        // Once registers hold doubles, this function will allocate a JSValue*
        // if the register doesn't hold one already. 
//        ASSERT(m_type == JSValueType);
        return u.jsValue;
    }
    
    ALWAYS_INLINE JSValue* Register::getJSValue() const
    {
//        ASSERT(m_type == JSValueType);
        return u.jsValue;
    }
    
    ALWAYS_INLINE bool Register::marked() const
    {
        return getJSValue()->marked();
    }

    ALWAYS_INLINE void Register::mark()
    {
        getJSValue()->mark();
    }
    
    // Machine functions

    ALWAYS_INLINE Register::Register(CodeBlock* codeBlock)
    {
#ifndef NDEBUG
        m_type = CodeBlockType;
#endif
        u.codeBlock = codeBlock;
    }

    ALWAYS_INLINE Register::Register(Instruction* vPC)
    {
#ifndef NDEBUG
        m_type = InstructionType;
#endif
        u.vPC = vPC;
    }

    ALWAYS_INLINE Register::Register(ScopeChainNode* scopeChain)
    {
#ifndef NDEBUG
        m_type = ScopeChainNodeType;
#endif
        u.scopeChain = scopeChain;
    }

    ALWAYS_INLINE Register::Register(JSPropertyNameIterator* jsPropertyNameIterator)
    {
#ifndef NDEBUG
        m_type = JSPropertyNameIteratorType;
#endif
        u.jsPropertyNameIterator = jsPropertyNameIterator;
    }

    ALWAYS_INLINE Register::Register(Register* r)
    {
#ifndef NDEBUG
        m_type = RegisterType;
#endif
        u.r = r;
    }

    ALWAYS_INLINE Register::Register(intptr_t i)
    {
#ifndef NDEBUG
        m_type = IntType;
#endif
        u.i = i;
    }

    ALWAYS_INLINE Register::Register(void* v)
    {
        u.v = v;
    }

    ALWAYS_INLINE CodeBlock* Register::codeBlock() const
    {
//        ASSERT(m_type == CodeBlockType);
        return u.codeBlock;
    }
    
    ALWAYS_INLINE ScopeChainNode* Register::scopeChain() const
    {
//        ASSERT(m_type == ScopeChainNodeType);
        return u.scopeChain;
    }
    
    ALWAYS_INLINE intptr_t Register::i() const
    {
//        ASSERT(m_type == IntType);
        return u.i;
    }
    
    ALWAYS_INLINE Register* Register::r() const
    {
//        ASSERT(m_type == RegisterType);
        return u.r;
    }
    
    ALWAYS_INLINE Instruction* Register::vPC() const
    {
//        ASSERT(m_type == InstructionType);
        return u.vPC;
    }
    
    ALWAYS_INLINE JSPropertyNameIterator* Register::jsPropertyNameIterator() const
    {
//        ASSERT(m_type == JSPropertyNameIteratorType);
        return u.jsPropertyNameIterator;
    }
    
    ALWAYS_INLINE void* Register::v() const
    {
        return u.v;
    }

} // namespace KJS

namespace WTF {

    template<> struct VectorTraits<KJS::Register> : VectorTraitsBase<true, KJS::Register> { };

} // namespace WTF

#endif // Register_h
