/*
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "ExecutableBase.h"

namespace JSC {

class NativeExecutable final : public ExecutableBase {
    friend class JIT;
    friend class LLIntOffsetsExtractor;
public:
    typedef ExecutableBase Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    static NativeExecutable* create(VM&, Ref<JITCode>&& callThunk, TaggedNativeFunction, Ref<JITCode>&& constructThunk, TaggedNativeFunction constructor, const String& name);

    static void destroy(JSCell*);
    
    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.nativeExecutableSpace;
    }

    CodeBlockHash hashFor(CodeSpecializationKind) const;

    TaggedNativeFunction function() const { return m_function; }
    TaggedNativeFunction constructor() const { return m_constructor; }
        
    TaggedNativeFunction nativeFunctionFor(CodeSpecializationKind kind)
    {
        if (kind == CodeForCall)
            return function();
        ASSERT(kind == CodeForConstruct);
        return constructor();
    }
        
    static ptrdiff_t offsetOfNativeFunctionFor(CodeSpecializationKind kind)
    {
        if (kind == CodeForCall)
            return OBJECT_OFFSETOF(NativeExecutable, m_function);
        ASSERT(kind == CodeForConstruct);
        return OBJECT_OFFSETOF(NativeExecutable, m_constructor);
    }

    DECLARE_VISIT_CHILDREN;
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue proto);
        
    DECLARE_INFO;

    const String& name() const { return m_name; }

    const DOMJIT::Signature* signatureFor(CodeSpecializationKind) const;
    Intrinsic intrinsic() const;

    JSString* toString(JSGlobalObject* globalObject)
    {
        if (!m_asString)
            return toStringSlow(globalObject);
        return m_asString.get();
    }

    JSString* asStringConcurrently() const { return m_asString.get(); }
    static inline ptrdiff_t offsetOfAsString() { return OBJECT_OFFSETOF(NativeExecutable, m_asString); }

private:
    NativeExecutable(VM&, TaggedNativeFunction, TaggedNativeFunction constructor);
    void finishCreation(VM&, Ref<JITCode>&& callThunk, Ref<JITCode>&& constructThunk, const String& name);

    JSString* toStringSlow(JSGlobalObject*);

    TaggedNativeFunction m_function;
    TaggedNativeFunction m_constructor;

    String m_name;
    WriteBarrier<JSString> m_asString;
};

} // namespace JSC
