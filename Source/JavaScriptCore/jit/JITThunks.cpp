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

#include "config.h"
#include "JITThunks.h"

#if ENABLE(JIT)

#include "CommonSlowPaths.h"
#include "JIT.h"
#include "JITCode.h"
#include "JSCJSValueInlines.h"
#include "SlowPathCall.h"
#include "ThunkGenerators.h"
#include "VM.h"

namespace JSC {

JITThunks::JITThunks()
{
}

JITThunks::~JITThunks()
{
}

static inline NativeExecutable& getMayBeDyingNativeExecutable(const Weak<NativeExecutable>& weak)
{
    // This never gets Deleted / Empty slots.
    WeakImpl* impl = weak.unsafeImpl();
    ASSERT(impl);
    // We have a callback removing entry when finalizing. This means that we never hold Deallocated entry in HashSet.
    ASSERT(impl->state() != WeakImpl::State::Deallocated);
    // Never use jsCast here. This is possible that this value is "Dead" but not "Finalized" yet. In this case,
    // we can still access to non-JS data, as we are doing in a finalize callback.
    auto* executable = static_cast<NativeExecutable*>(impl->jsValue().asCell());
    ASSERT(executable);
    return *executable;
}

inline unsigned JITThunks::WeakNativeExecutableHash::hash(NativeExecutable* executable)
{
    return hash(executable->function(), executable->constructor(), executable->name());
}

inline unsigned JITThunks::WeakNativeExecutableHash::hash(const Weak<NativeExecutable>& key)
{
    return hash(&getMayBeDyingNativeExecutable(key));
}

inline bool JITThunks::WeakNativeExecutableHash::equal(NativeExecutable& a, NativeExecutable& b)
{
    if (&a == &b)
        return true;
    return a.function() == b.function() && a.constructor() == b.constructor() && a.name() == b.name();
}

inline bool JITThunks::WeakNativeExecutableHash::equal(const Weak<NativeExecutable>& a, const Weak<NativeExecutable>& b)
{
    return equal(getMayBeDyingNativeExecutable(a), getMayBeDyingNativeExecutable(b));
}

inline bool JITThunks::WeakNativeExecutableHash::equal(const Weak<NativeExecutable>& a, NativeExecutable* bExecutable)
{
    return equal(getMayBeDyingNativeExecutable(a), *bExecutable);
}

inline bool JITThunks::WeakNativeExecutableHash::equal(const Weak<NativeExecutable>& a, const HostFunctionKey& b)
{
    auto& aExecutable = getMayBeDyingNativeExecutable(a);
    return aExecutable.function() == std::get<0>(b) && aExecutable.constructor() == std::get<1>(b) && aExecutable.name() == std::get<2>(b);
}

MacroAssemblerCodePtr<JITThunkPtrTag> JITThunks::ctiNativeCall(VM& vm)
{
    ASSERT(Options::useJIT());
    return ctiStub(vm, nativeCallGenerator).code();
}

MacroAssemblerCodePtr<JITThunkPtrTag> JITThunks::ctiNativeConstruct(VM& vm)
{
    ASSERT(Options::useJIT());
    return ctiStub(vm, nativeConstructGenerator).code();
}

MacroAssemblerCodePtr<JITThunkPtrTag> JITThunks::ctiNativeTailCall(VM& vm)
{
    ASSERT(Options::useJIT());
    return ctiStub(vm, nativeTailCallGenerator).code();
}

MacroAssemblerCodePtr<JITThunkPtrTag> JITThunks::ctiNativeTailCallWithoutSavedTags(VM& vm)
{
    ASSERT(Options::useJIT());
    return ctiStub(vm, nativeTailCallWithoutSavedTagsGenerator).code();
}

MacroAssemblerCodePtr<JITThunkPtrTag> JITThunks::ctiInternalFunctionCall(VM& vm, Optional<NoLockingNecessaryTag> noLockingNecessary)
{
    ASSERT(Options::useJIT());
    if (noLockingNecessary)
        return existingCTIStub(internalFunctionCallGenerator, NoLockingNecessary).code();
    return ctiStub(vm, internalFunctionCallGenerator).code();
}

MacroAssemblerCodePtr<JITThunkPtrTag> JITThunks::ctiInternalFunctionConstruct(VM& vm, Optional<NoLockingNecessaryTag> noLockingNecessary)
{
    ASSERT(Options::useJIT());
    if (noLockingNecessary)
        return existingCTIStub(internalFunctionConstructGenerator, NoLockingNecessary).code();
    return ctiStub(vm, internalFunctionConstructGenerator).code();
}

MacroAssemblerCodeRef<JITThunkPtrTag> JITThunks::ctiStub(VM& vm, ThunkGenerator generator)
{
    Locker locker { m_lock };
    CTIStubMap::AddResult entry = m_ctiStubMap.add(generator, MacroAssemblerCodeRef<JITThunkPtrTag>());
    if (entry.isNewEntry) {
        // Compilation thread can only retrieve existing entries.
        ASSERT(!isCompilationThread());
        entry.iterator->value = generator(vm);
    }
    return entry.iterator->value;
}

MacroAssemblerCodeRef<JITThunkPtrTag> JITThunks::existingCTIStub(ThunkGenerator generator)
{
    Locker locker { m_lock };
    return existingCTIStub(generator, NoLockingNecessary);
}

MacroAssemblerCodeRef<JITThunkPtrTag> JITThunks::existingCTIStub(ThunkGenerator generator, NoLockingNecessaryTag)
{
    CTIStubMap::iterator entry = m_ctiStubMap.find(generator);
    if (entry == m_ctiStubMap.end())
        return MacroAssemblerCodeRef<JITThunkPtrTag>();
    return entry->value;
}

#if ENABLE(EXTRA_CTI_THUNKS)

MacroAssemblerCodeRef<JITThunkPtrTag> JITThunks::ctiSlowPathFunctionStub(VM& vm, SlowPathFunction slowPathFunction)
{
    Locker locker { m_lock };
    auto key = bitwise_cast<ThunkGenerator>(slowPathFunction);
    CTIStubMap::AddResult entry = m_ctiStubMap.add(key, MacroAssemblerCodeRef<JITThunkPtrTag>());
    if (entry.isNewEntry) {
        // Compilation thread can only retrieve existing entries.
        ASSERT(!isCompilationThread());
        entry.iterator->value = JITSlowPathCall::generateThunk(vm, slowPathFunction);
    }
    return entry.iterator->value;
}

#endif // ENABLE(EXTRA_CTI_THUNKS)

struct JITThunks::HostKeySearcher {
    static unsigned hash(const HostFunctionKey& key) { return WeakNativeExecutableHash::hash(key); }
    static bool equal(const Weak<NativeExecutable>& a, const HostFunctionKey& b) { return WeakNativeExecutableHash::equal(a, b); }
};

struct JITThunks::NativeExecutableTranslator {
    static unsigned hash(NativeExecutable* key) { return WeakNativeExecutableHash::hash(key); }
    static bool equal(const Weak<NativeExecutable>& a, NativeExecutable* b) { return WeakNativeExecutableHash::equal(a, b); }
    static void translate(Weak<NativeExecutable>& location, NativeExecutable* executable, unsigned)
    {
        location = Weak<NativeExecutable>(executable, executable->vm().jitStubs.get());
    }
};

void JITThunks::finalize(Handle<Unknown> handle, void*)
{
    auto* nativeExecutable = static_cast<NativeExecutable*>(handle.get().asCell());
    auto hostFunctionKey = std::make_tuple(nativeExecutable->function(), nativeExecutable->constructor(), nativeExecutable->name());
    {
        DisallowGC disallowGC;
        auto iterator = m_nativeExecutableSet.find<HostKeySearcher>(hostFunctionKey);
        // Because this finalizer is called, this means that we still have dead Weak<> in m_nativeExecutableSet.
        ASSERT(iterator != m_nativeExecutableSet.end());
        ASSERT(iterator->unsafeImpl()->state() == WeakImpl::State::Finalized);
        m_nativeExecutableSet.remove(iterator);
    }
}

NativeExecutable* JITThunks::hostFunctionStub(VM& vm, TaggedNativeFunction function, TaggedNativeFunction constructor, const String& name)
{
    return hostFunctionStub(vm, function, constructor, nullptr, NoIntrinsic, nullptr, name);
}

NativeExecutable* JITThunks::hostFunctionStub(VM& vm, TaggedNativeFunction function, TaggedNativeFunction constructor, ThunkGenerator generator, Intrinsic intrinsic, const DOMJIT::Signature* signature, const String& name)
{
    ASSERT(!isCompilationThread());    
    ASSERT(Options::useJIT());

    auto hostFunctionKey = std::make_tuple(function, constructor, name);
    {
        DisallowGC disallowGC;
        auto iterator = m_nativeExecutableSet.find<HostKeySearcher>(hostFunctionKey);
        if (iterator != m_nativeExecutableSet.end()) {
            // It is possible that this returns Weak<> which is Dead, but not finalized.
            // We should not use this reference to store value created in the subsequent sequence, since allocating NativeExecutable can cause GC, which changes this Set.
            if (auto* executable = iterator->get())
                return executable;
        }
    }

    RefPtr<JITCode> forCall;
    if (generator) {
        MacroAssemblerCodeRef<JSEntryPtrTag> entry = generator(vm).retagged<JSEntryPtrTag>();
        forCall = adoptRef(new DirectJITCode(entry, entry.code(), JITType::HostCallThunk, intrinsic));
    } else if (signature)
        forCall = adoptRef(new NativeDOMJITCode(MacroAssemblerCodeRef<JSEntryPtrTag>::createSelfManagedCodeRef(ctiNativeCall(vm).retagged<JSEntryPtrTag>()), JITType::HostCallThunk, intrinsic, signature));
    else
        forCall = adoptRef(new NativeJITCode(MacroAssemblerCodeRef<JSEntryPtrTag>::createSelfManagedCodeRef(ctiNativeCall(vm).retagged<JSEntryPtrTag>()), JITType::HostCallThunk, intrinsic));
    
    Ref<JITCode> forConstruct = adoptRef(*new NativeJITCode(MacroAssemblerCodeRef<JSEntryPtrTag>::createSelfManagedCodeRef(ctiNativeConstruct(vm).retagged<JSEntryPtrTag>()), JITType::HostCallThunk, NoIntrinsic));
    
    NativeExecutable* nativeExecutable = NativeExecutable::create(vm, forCall.releaseNonNull(), function, WTFMove(forConstruct), constructor, name);
    {
        DisallowGC disallowGC;
        auto addResult = m_nativeExecutableSet.add<NativeExecutableTranslator>(nativeExecutable);
        if (!addResult.isNewEntry) {
            // Override the existing Weak<NativeExecutable> with the new one since it is dead.
            ASSERT(!*addResult.iterator);
            *addResult.iterator = Weak<NativeExecutable>(nativeExecutable, this);
            ASSERT(*addResult.iterator);
#if ASSERT_ENABLED
            auto iterator = m_nativeExecutableSet.find<HostKeySearcher>(hostFunctionKey);
            ASSERT(iterator != m_nativeExecutableSet.end());
            ASSERT(iterator->get() == nativeExecutable);
            ASSERT(iterator->unsafeImpl()->state() == WeakImpl::State::Live);
#endif
        }
    }
    return nativeExecutable;
}

NativeExecutable* JITThunks::hostFunctionStub(VM& vm, TaggedNativeFunction function, ThunkGenerator generator, Intrinsic intrinsic, const String& name)
{
    return hostFunctionStub(vm, function, callHostFunctionAsConstructor, generator, intrinsic, nullptr, name);
}

void JITThunks::preinitializeCTIThunks(VM& vm)
{
    if (!Options::useJIT())
        return;

#if ENABLE(EXTRA_CTI_THUNKS)
    // These 4 should always be initialized first in the following order because
    // the other thunk generators rely on these already being initialized.
    ctiStub(vm, handleExceptionGenerator);
    ctiStub(vm, handleExceptionWithCallFrameRollbackGenerator);
    ctiStub(vm, popThunkStackPreservesAndHandleExceptionGenerator);
    ctiStub(vm, checkExceptionGenerator);

#define INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(name) ctiSlowPathFunctionStub(vm, slow_path_##name)

    // From the BaselineJIT DEFINE_SLOW_OP list:
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(in_by_val);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(has_private_name);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(has_private_brand);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(less);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(lesseq);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(greater);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(greatereq);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(is_callable);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(is_constructor);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(typeof);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(typeof_is_object);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(typeof_is_function);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(strcat);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(push_with_scope);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(create_lexical_environment);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(get_by_val_with_this);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(put_by_id_with_this);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(put_by_val_with_this);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(resolve_scope_for_hoisting_func_decl_in_eval);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(define_data_property);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(define_accessor_property);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(unreachable);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(throw_static_error);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(new_array_with_spread);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(new_array_buffer);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(spread);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(get_enumerable_length);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(has_enumerable_property);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(get_property_enumerator);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(to_index_string);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(create_direct_arguments);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(create_scoped_arguments);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(create_cloned_arguments);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(create_arguments_butterfly);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(create_rest);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(create_promise);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(new_promise);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(create_generator);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(create_async_generator);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(new_generator);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(pow);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(mod);

    ctiSlowPathFunctionStub(vm, iterator_open_try_fast_narrow);
    ctiSlowPathFunctionStub(vm, iterator_open_try_fast_wide16);
    ctiSlowPathFunctionStub(vm, iterator_open_try_fast_wide32);
    ctiSlowPathFunctionStub(vm, iterator_next_try_fast_narrow);
    ctiSlowPathFunctionStub(vm, iterator_next_try_fast_wide16);
    ctiSlowPathFunctionStub(vm, iterator_next_try_fast_wide32);

    // From the BaselineJIT DEFINE_SLOWCASE_SLOW_OP list:
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(unsigned);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(inc);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(dec);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(bitnot);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(bitand);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(bitor);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(bitxor);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(lshift);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(rshift);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(urshift);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(div);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(create_this);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(create_promise);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(create_generator);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(create_async_generator);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(to_this);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(to_primitive);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(to_number);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(to_numeric);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(to_string);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(to_object);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(not);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(stricteq);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(nstricteq);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(get_direct_pname);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(get_prototype_of);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(has_enumerable_structure_property);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(has_own_structure_property);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(in_structure_property);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(resolve_scope);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(check_tdz);
    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(to_property_key);

    INIT_BASELINE_SLOW_PATH_CALL_ROUTINE(throw_strict_mode_readonly_property_write_error);
#undef INIT_BASELINE_ROUTINE

    // From the BaselineJIT DEFINE_SLOWCASE_OP list:
    ctiStub(vm, JIT::slow_op_del_by_id_prepareCallGenerator);
    ctiStub(vm, JIT::slow_op_del_by_val_prepareCallGenerator);
    ctiStub(vm, JIT::slow_op_get_by_id_prepareCallGenerator);
    ctiStub(vm, JIT::slow_op_get_by_id_with_this_prepareCallGenerator);
    ctiStub(vm, JIT::slow_op_get_by_val_prepareCallGenerator);
    ctiStub(vm, JIT::slow_op_get_from_scopeGenerator);
    ctiStub(vm, JIT::slow_op_get_private_name_prepareCallGenerator);
    ctiStub(vm, JIT::slow_op_put_by_id_prepareCallGenerator);
    ctiStub(vm, JIT::slow_op_put_by_val_prepareCallGenerator);
    ctiStub(vm, JIT::slow_op_put_private_name_prepareCallGenerator);
    ctiStub(vm, JIT::slow_op_put_to_scopeGenerator);

    ctiStub(vm, JIT::op_check_traps_handlerGenerator);
    ctiStub(vm, JIT::op_enter_handlerGenerator);
    ctiStub(vm, JIT::op_ret_handlerGenerator);
    ctiStub(vm, JIT::op_throw_handlerGenerator);
#endif // ENABLE(EXTRA_CTI_THUNKS)

    ctiStub(vm, linkCallThunkGenerator);
    ctiStub(vm, arityFixupGenerator);
}


} // namespace JSC

#endif // ENABLE(JIT)
