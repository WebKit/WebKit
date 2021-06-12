/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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

#if ENABLE(JIT)

#include "CallData.h"
#include "Intrinsic.h"
#include "MacroAssemblerCodeRef.h"
#include "SlowPathFunction.h"
#include "ThunkGenerator.h"
#include "Weak.h"
#include "WeakHandleOwner.h"
#include <tuple>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/PackedRefPtr.h>
#include <wtf/RecursiveLockAdapter.h>
#include <wtf/text/StringHash.h>

namespace JSC {
namespace DOMJIT {
class Signature;
}

class VM;
class NativeExecutable;

class JITThunks final : private WeakHandleOwner {
    WTF_MAKE_FAST_ALLOCATED;
public:
    JITThunks();
    ~JITThunks() final;

    MacroAssemblerCodePtr<JITThunkPtrTag> ctiNativeCall(VM&);
    MacroAssemblerCodePtr<JITThunkPtrTag> ctiNativeConstruct(VM&);
    MacroAssemblerCodePtr<JITThunkPtrTag> ctiNativeTailCall(VM&);
    MacroAssemblerCodePtr<JITThunkPtrTag> ctiNativeTailCallWithoutSavedTags(VM&);
    MacroAssemblerCodePtr<JITThunkPtrTag> ctiInternalFunctionCall(VM&);
    MacroAssemblerCodePtr<JITThunkPtrTag> ctiInternalFunctionConstruct(VM&);

    MacroAssemblerCodeRef<JITThunkPtrTag> ctiStub(VM&, ThunkGenerator);
#if ENABLE(EXTRA_CTI_THUNKS)
    MacroAssemblerCodeRef<JITThunkPtrTag> ctiSlowPathFunctionStub(VM&, SlowPathFunction);
#endif

    NativeExecutable* hostFunctionStub(VM&, TaggedNativeFunction, TaggedNativeFunction constructor, const String& name);
    NativeExecutable* hostFunctionStub(VM&, TaggedNativeFunction, TaggedNativeFunction constructor, ThunkGenerator, Intrinsic, const DOMJIT::Signature*, const String& name);
    NativeExecutable* hostFunctionStub(VM&, TaggedNativeFunction, ThunkGenerator, Intrinsic, const String& name);

private:
    template <typename GenerateThunk>
    MacroAssemblerCodeRef<JITThunkPtrTag> ctiStubImpl(ThunkGenerator key, GenerateThunk);

    void finalize(Handle<Unknown>, void* context) final;
    
    struct Entry {
        PackedRefPtr<ExecutableMemoryHandle> handle;
        bool needsCrossModifyingCodeFence;
    };
    using CTIStubMap = HashMap<ThunkGenerator, Entry>;
    CTIStubMap m_ctiStubMap;

    using HostFunctionKey = std::tuple<TaggedNativeFunction, TaggedNativeFunction, String>;

    struct WeakNativeExecutableHash {
        static inline unsigned hash(const Weak<NativeExecutable>&);
        static inline unsigned hash(NativeExecutable*);
        static unsigned hash(const HostFunctionKey& key)
        {
            return hash(std::get<0>(key), std::get<1>(key), std::get<2>(key));
        }

        static inline bool equal(const Weak<NativeExecutable>&, const Weak<NativeExecutable>&);
        static inline bool equal(const Weak<NativeExecutable>&, const HostFunctionKey&);
        static inline bool equal(const Weak<NativeExecutable>&, NativeExecutable*);
        static inline bool equal(NativeExecutable&, NativeExecutable&);
        static constexpr bool safeToCompareToEmptyOrDeleted = false;

    private:
        static inline unsigned hashPointer(TaggedNativeFunction p)
        {
            return DefaultHash<TaggedNativeFunction>::hash(p);
        }

        static unsigned hash(TaggedNativeFunction function, TaggedNativeFunction constructor, const String& name)
        {
            // FIXME: Use WTF::computeHash.
            // https://bugs.webkit.org/show_bug.cgi?id=207835
            unsigned hash = WTF::pairIntHash(hashPointer(function), hashPointer(constructor));
            if (!name.isNull())
                hash = WTF::pairIntHash(hash, DefaultHash<String>::hash(name));
            return hash;
        }
    };
    struct HostKeySearcher;
    struct NativeExecutableTranslator;

    using WeakNativeExecutableSet = HashSet<Weak<NativeExecutable>, WeakNativeExecutableHash>;
    WeakNativeExecutableSet m_nativeExecutableSet;

    WTF::RecursiveLock m_lock;
};

} // namespace JSC

#endif // ENABLE(JIT)
