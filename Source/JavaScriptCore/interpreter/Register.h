/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#pragma once

#include <JavaScriptCore/EncodedValueDescriptor.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/VectorTraits.h>

namespace JSC {

    class CallFrame;
    class CodeBlock;
    class JSCell;
    class JSLexicalEnvironment;
    class JSObject;
    class JSScope;
    class JSValue;

    class Register {
        WTF_MAKE_TZONE_NON_HEAP_ALLOCATABLE(Register);
    public:
        inline Register(); // Defined in RegisterInlines.h

        inline Register(const JSValue&); // Defined in RegisterInlines.h
        inline JSValue jsValue() const; // Defined in RegisterInlines.h
        inline JSValue asanUnsafeJSValue() const; // Defined in RegisterInlines.h
        inline EncodedJSValue encodedJSValue() const; // Defined in RegisterInlines.h
        
        ALWAYS_INLINE Register& operator=(CallFrame*);
        ALWAYS_INLINE Register& operator=(CodeBlock*);
        ALWAYS_INLINE Register& operator=(JSScope*);
        ALWAYS_INLINE Register& operator=(JSCell*);
        ALWAYS_INLINE Register& operator=(EncodedJSValue);

        inline int32_t i() const; // Defined in RegisterInlines.h
        ALWAYS_INLINE CallFrame* callFrame() const;
        ALWAYS_INLINE CodeBlock* codeBlock() const;
        ALWAYS_INLINE CodeBlock* asanUnsafeCodeBlock() const;
        ALWAYS_INLINE JSObject* object() const;
        ALWAYS_INLINE JSScope* scope() const;
        int32_t unboxedInt32() const;
        uint32_t unboxedUInt32() const;
        int32_t asanUnsafeUnboxedInt32() const;
        inline int64_t unboxedInt52() const; // Defined in RegisterInlines.h
        inline int64_t asanUnsafeUnboxedInt52() const; // Defined in RegisterInlines.h
        int64_t unboxedStrictInt52() const;
        int64_t asanUnsafeUnboxedStrictInt52() const;
        int64_t unboxedInt64() const;
        int64_t asanUnsafeUnboxedInt64() const;
        bool unboxedBoolean() const;
#if ENABLE(WEBASSEMBLY) && USE(JSVALUE32_64)
        float unboxedFloat() const;
        float asanUnsafeUnboxedFloat() const;
#endif
        double unboxedDouble() const;
        double asanUnsafeUnboxedDouble() const;
        JSCell* unboxedCell() const;
        JSCell* asanUnsafeUnboxedCell() const;
        int32_t payload() const;
        int32_t tag() const;
        int32_t unsafePayload() const;
        int32_t unsafeTag() const;
        int32_t& payload();
        int32_t& tag();

        void* pointer() const;
        void* asanUnsafePointer() const;

        static inline Register withInt(int32_t); // Defined in RegisterInlines.h

    private:
        union {
            EncodedJSValue value;
            CallFrame* callFrame;
            CodeBlock* codeBlock;
            EncodedValueDescriptor encodedValue;
            double number;
            int64_t integer;
        } u;
    };

    // Register(), Register(const JSValue&), jsValue(), asanUnsafeJSValue(),
    // encodedJSValue(), i(), unboxedInt52(), asanUnsafeUnboxedInt52()
    // are defined in RegisterInlines.h

    ALWAYS_INLINE int32_t Register::unboxedInt32() const
    {
        return payload();
    }

    ALWAYS_INLINE uint32_t Register::unboxedUInt32() const
    {
        return static_cast<uint32_t>(unboxedInt32());
    }

    SUPPRESS_ASAN ALWAYS_INLINE int32_t Register::asanUnsafeUnboxedInt32() const
    {
        return unsafePayload();
    }

    ALWAYS_INLINE int64_t Register::unboxedStrictInt52() const
    {
        return u.integer;
    }

    SUPPRESS_ASAN ALWAYS_INLINE int64_t Register::asanUnsafeUnboxedStrictInt52() const
    {
        return u.integer;
    }

    ALWAYS_INLINE int64_t Register::unboxedInt64() const
    {
        return u.integer;
    }

    SUPPRESS_ASAN ALWAYS_INLINE int64_t Register::asanUnsafeUnboxedInt64() const
    {
        return u.integer;
    }

    ALWAYS_INLINE bool Register::unboxedBoolean() const
    {
        return !!payload();
    }

#if ENABLE(WEBASSEMBLY) && USE(JSVALUE32_64)
    ALWAYS_INLINE float Register::unboxedFloat() const
    {
        return std::bit_cast<float>(payload());
    }

    SUPPRESS_ASAN ALWAYS_INLINE float Register::asanUnsafeUnboxedFloat() const
    {
        return std::bit_cast<float>(payload());
    }
#endif

    ALWAYS_INLINE double Register::unboxedDouble() const
    {
        return u.number;
    }

    SUPPRESS_ASAN ALWAYS_INLINE double Register::asanUnsafeUnboxedDouble() const
    {
        return u.number;
    }

    ALWAYS_INLINE JSCell* Register::unboxedCell() const
    {
#if USE(JSVALUE64)
        return u.encodedValue.ptr;
#else
        return std::bit_cast<JSCell*>(payload());
#endif
    }

    SUPPRESS_ASAN ALWAYS_INLINE JSCell* Register::asanUnsafeUnboxedCell() const
    {
#if USE(JSVALUE64)
        return u.encodedValue.ptr;
#else
        return std::bit_cast<JSCell*>(payload());
#endif
    }

    ALWAYS_INLINE void* Register::pointer() const
    {
#if USE(JSVALUE64)
        return u.encodedValue.ptr;
#else
        return std::bit_cast<void*>(payload());
#endif
    }

    SUPPRESS_ASAN ALWAYS_INLINE void* Register::asanUnsafePointer() const
    {
#if USE(JSVALUE64)
        return u.encodedValue.ptr;
#else
        return std::bit_cast<void*>(unsafePayload());
#endif
    }

    ALWAYS_INLINE int32_t Register::payload() const
    {
        return u.encodedValue.asBits.payload;
    }

    ALWAYS_INLINE int32_t Register::tag() const
    {
        return u.encodedValue.asBits.tag;
    }

    SUPPRESS_ASAN ALWAYS_INLINE int32_t Register::unsafePayload() const
    {
        return u.encodedValue.asBits.payload;
    }

    SUPPRESS_ASAN ALWAYS_INLINE int32_t Register::unsafeTag() const
    {
        return u.encodedValue.asBits.tag;
    }

    ALWAYS_INLINE int32_t& Register::payload()
    {
        return u.encodedValue.asBits.payload;
    }

    ALWAYS_INLINE int32_t& Register::tag()
    {
        return u.encodedValue.asBits.tag;
    }

} // namespace JSC

namespace WTF {

    template<> struct VectorTraits<JSC::Register> : VectorTraitsBase<true, JSC::Register> { };

} // namespace WTF
