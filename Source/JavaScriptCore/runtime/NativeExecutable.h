/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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
#include "JSCPoison.h"

namespace JSC {
namespace DOMJIT {
class Signature;
}

class NativeExecutable final : public ExecutableBase {
    friend class JIT;
    friend class LLIntOffsetsExtractor;
public:
    typedef ExecutableBase Base;
    static const unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    static NativeExecutable* create(VM&, Ref<JITCode>&& callThunk, TaggedNativeFunction, Ref<JITCode>&& constructThunk, TaggedNativeFunction constructor, Intrinsic, const DOMJIT::Signature*, const String& name);

    static void destroy(JSCell*);
    
    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.nativeExecutableSpace;
    }

    CodeBlockHash hashFor(CodeSpecializationKind) const;

    TaggedNativeFunction function() { return m_function.unpoisoned(); }
    TaggedNativeFunction constructor() { return m_constructor.unpoisoned(); }
        
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

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue proto);
        
    DECLARE_INFO;

    const String& name() const { return m_name; }
    const DOMJIT::Signature* signature() const { return m_signature; }

    const DOMJIT::Signature* signatureFor(CodeSpecializationKind kind) const
    {
        if (isCall(kind))
            return signature();
        return nullptr;
    }

protected:
    void finishCreation(VM&, Ref<JITCode>&& callThunk, Ref<JITCode>&& constructThunk, const String& name);

private:
    friend class ExecutableBase;
    using PoisonedTaggedNativeFunction = Poisoned<NativeCodePoison, TaggedNativeFunction>;

    NativeExecutable(VM&, TaggedNativeFunction, TaggedNativeFunction constructor, Intrinsic, const DOMJIT::Signature*);

    PoisonedTaggedNativeFunction m_function;
    PoisonedTaggedNativeFunction m_constructor;
    const DOMJIT::Signature* m_signature;

    String m_name;
};

} // namespace JSC
