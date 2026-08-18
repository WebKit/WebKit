/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/MacroAssemblerCodeRef.h>
#include <JavaScriptCore/MemoryMode.h>
#include <JavaScriptCore/WasmCallee.h>
#include <JavaScriptCore/WasmJS.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/FixedBitVector.h>
#include <wtf/FixedVector.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/SharedTask.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class VM;

namespace Wasm {

class EntryPlan;
struct ModuleInformation;
struct UnlinkedWasmToWasmCall;

class CalleeGroup final : public ThreadSafeRefCounted<CalleeGroup> {
public:
    typedef void CallbackType(Ref<CalleeGroup>&&, bool);
    using AsyncCompilationCallback = RefPtr<WTF::SharedTask<CallbackType>>;

    struct OptimizedCallees {
#if ENABLE(WEBASSEMBLY_BBQJIT)
        class BBQCalleeReference {
        public:
            BBQCalleeReference() = default;

            ~BBQCalleeReference()
            {
                if (m_isWeak)
                    std::destroy_at(&m_pointer.weak);
                else
                    std::destroy_at(&m_pointer.strong);
            }

            Lock& lock() const LIFETIME_BOUND WTF_RETURNS_LOCK(m_bbqCalleeLock) { return m_bbqCalleeLock; }

            bool isWeak() const WTF_REQUIRES_LOCK(m_bbqCalleeLock) { return m_isWeak; }
            bool isStrong() const WTF_REQUIRES_LOCK(m_bbqCalleeLock) { return !isWeak(); }

            RefPtr<BBQCallee> get() const WTF_REQUIRES_LOCK(m_bbqCalleeLock) { return isWeak() ? m_pointer.weak.get() : m_pointer.strong; }

            RefPtr<BBQCallee> convertToWeak() WTF_REQUIRES_LOCK(m_bbqCalleeLock)
            {
                ASSERT(isStrong());
                RefPtr<BBQCallee> strong = WTF::move(m_pointer.strong);
                std::destroy_at(&m_pointer.strong);
                std::construct_at(&m_pointer.weak, strong);
                m_isWeak = true;
                return strong;
            }

            void set(Ref<BBQCallee>&& strongReference) WTF_REQUIRES_LOCK(m_bbqCalleeLock)
            {
                if (m_isWeak) {
                    std::destroy_at(&m_pointer.weak);
                    std::construct_at(&m_pointer.strong);
                    m_isWeak = false;
                }
                m_pointer.strong = WTF::move(strongReference);
            }

        private:
            union Pointer {
                // Whichever member is active is destroyed by ~BBQCalleeReference().
                ~Pointer() { }
                RefPtr<BBQCallee> strong { };
                ThreadSafeWeakPtr<BBQCallee> weak;
            };

            mutable Lock m_bbqCalleeLock;
            bool m_isWeak WTF_GUARDED_BY_LOCK(m_bbqCalleeLock) { false };
            Pointer m_pointer WTF_GUARDED_BY_LOCK(m_bbqCalleeLock);
        };

        BBQCalleeReference m_bbqCallee;
#endif
#if ENABLE(WEBASSEMBLY_OMGJIT)
        RefPtr<OMGCallee> m_omgCallee;
#endif
    };


    static Ref<CalleeGroup> createFromIPInt(VM&, MemoryMode, ModuleInformation&, Ref<IPIntCallees>&&);
    static Ref<CalleeGroup> createFromExisting(MemoryMode, const CalleeGroup&);

    void waitUntilFinished();
    void compileAsync(VM&, AsyncCompilationCallback&&);

    bool compilationFinished()
    {
        return m_compilationFinished.load();
    }
    bool runnable() { return compilationFinished() && !m_errorMessage; }

    // Note, we do this copy to ensure it's thread safe to have this
    // called from multiple threads simultaneously.
    String errorMessage()
    {
        ASSERT(!runnable());
        return crossThreadCopy(m_errorMessage);
    }

    unsigned functionImportCount() const { return m_wasmToWasmExitStubs.size(); }
    FunctionSpaceIndex toSpaceIndex(FunctionCodeIndex codeIndex) const
    {
        ASSERT(codeIndex < m_calleeCount);
        return FunctionSpaceIndex(codeIndex + functionImportCount());
    }
    FunctionCodeIndex toCodeIndex(FunctionSpaceIndex spaceIndex) const
    {
        ASSERT(functionImportCount() <= spaceIndex);
        ASSERT(spaceIndex < m_calleeCount + functionImportCount());
        return FunctionCodeIndex(spaceIndex - functionImportCount());
    }

    JSToWasmCallee& ensureJSToWasmCallee(const ModuleInformation&, FunctionSpaceIndex functionIndexSpace);

    RefPtr<JITCallee> replacement(const AbstractLocker& locker, FunctionSpaceIndex functionIndexSpace) WTF_REQUIRES_LOCK(m_lock)
    {
        ASSERT(runnable());
        ASSERT(functionIndexSpace >= functionImportCount());
        if (auto* tuple = optimizedCalleesTuple(locker, toCodeIndex(functionIndexSpace))) {
            UNUSED_VARIABLE(tuple);
#if ENABLE(WEBASSEMBLY_OMGJIT)
            if (RefPtr callee = tuple->m_omgCallee)
                return callee;
#endif
#if ENABLE(WEBASSEMBLY_BBQJIT)
            {
                Locker bbqLocker { tuple->m_bbqCallee.lock() };
                if (RefPtr callee = tuple->m_bbqCallee.get())
                    return callee;
            }
#endif
        }
        return nullptr;
    }

#if ENABLE(WEBASSEMBLY_BBQJIT)
    // Tier-up peeks that run on execution threads and deliberately skip m_lock, whose critical
    // section they would otherwise contend with every compiler thread for. They are sound because a
    // callee is only reachable through m_optimizedCallees once its code is linked, flushed, and
    // published; m_bbqCalleeLock covers the pointer itself. Thread safety analysis cannot express
    // this, hence WTF_IGNORES_THREAD_SAFETY_ANALYSIS.
    RefPtr<BBQCallee> tryGetBBQCalleeForLoopOSRConcurrently(VM&, FunctionCodeIndex) WTF_IGNORES_THREAD_SAFETY_ANALYSIS;
#endif
#if ENABLE(WEBASSEMBLY_OMGJIT)
    RefPtr<OMGCallee> tryGetOMGCalleeConcurrently(FunctionCodeIndex) WTF_IGNORES_THREAD_SAFETY_ANALYSIS;
#endif

    Ref<Callee> wasmEntrypointCalleeFromFunctionIndexSpace(const AbstractLocker& locker, FunctionSpaceIndex functionIndexSpace) WTF_REQUIRES_LOCK(m_lock)
    {

        if (RefPtr replacement = this->replacement(locker, functionIndexSpace))
            return replacement.releaseNonNull();
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        return m_ipintCallees->at(calleeIndex).get();
    }

    Ref<IPIntCallee> ipintCalleeFromFunctionIndexSpace(FunctionSpaceIndex functionIndexSpace) const
    {
        ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        return m_ipintCallees->at(calleeIndex).get();
    }

#if ENABLE(WEBASSEMBLY_BBQJIT) || ENABLE(WEBASSEMBLY_OMGJIT)
    struct CallerCallsiteFlushes {
        void flush();

        struct Callsite {
            CodeLocationNearCall<WasmEntryPtrTag> callLocation;
            CodeLocationLabel<WasmEntryPtrTag> target;
        };

        // FIXME: These inline capacities were picked semi-randomly. We should figure out if there's a better number.
        Vector<Callsite, 16> callsites;
        // The keep-alive references ensure the caller code backing each callsite stays live until flush() runs.
        // Note: We don't need OMGCallees since their lifetime is equivalent to the CalleeGroup's but it simplifies
        // the code to do so anyway.
        Vector<Ref<JITCallee>, 8> keepAliveCallees;
    };

    bool installOptimizedCallee(Locker<Lock>&, const ModuleInformation&, FunctionCodeIndex, Ref<OptimizingJITCallee>&&, const FixedBitVector& outgoingJITDirectCallees, CallerCallsiteFlushes&) WTF_REQUIRES_LOCK(m_lock);
#endif

#if ENABLE(WEBASSEMBLY_BBQJIT)
    RefPtr<BBQCallee> bbqCallee(const AbstractLocker& locker, FunctionCodeIndex functionIndex) WTF_REQUIRES_LOCK(m_lock)
    {
        if (auto* tuple = optimizedCalleesTuple(locker, functionIndex)) {
            Locker bbqLocker { tuple->m_bbqCallee.lock() };
            return tuple->m_bbqCallee.get();
        }
        return nullptr;
    }


    void releaseBBQCallee(const AbstractLocker&, FunctionCodeIndex) WTF_REQUIRES_LOCK(m_lock);
#endif

    CodePtr<WasmEntryPtrTag>* entrypointLoadLocationFromFunctionIndexSpace(FunctionSpaceIndex functionIndexSpace)
    {
        RELEASE_ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        return &m_wasmIndirectCallEntrypoints[calleeIndex];
    }

    RefPtr<Wasm::IPIntCallee> wasmCalleeFromFunctionIndexSpace(FunctionSpaceIndex functionIndexSpace)
    {
        RELEASE_ASSERT(functionIndexSpace >= functionImportCount());
        unsigned calleeIndex = functionIndexSpace - functionImportCount();
        return m_wasmIndirectCallWasmCallees[calleeIndex];
    }

    CodePtr<WasmEntryPtrTag> wasmToWasmExitStub(FunctionSpaceIndex functionIndex)
    {
        return m_wasmToWasmExitStubs[functionIndex].code();
    }

    bool NODELETE isSafeToRun(MemoryMode);

    MemoryMode mode() const { return m_mode; }

    // TriState::Indeterminate means weakly referenced.
    TriState calleeIsReferenced(const AbstractLocker&, Wasm::Callee*) const WTF_REQUIRES_LOCK(m_lock);

    ~CalleeGroup();
private:
    friend class Plan;
#if ENABLE(WEBASSEMBLY_BBQJIT)
    friend class BBQPlan;
#endif
#if ENABLE(WEBASSEMBLY_OMGJIT)
    friend class OMGPlan;
#endif

    CalleeGroup(VM&, MemoryMode, ModuleInformation&, Ref<IPIntCallees>&&);
    CalleeGroup(MemoryMode, const CalleeGroup&);
    void setCompilationFinished();

    OptimizedCallees* optimizedCalleesTuple(const AbstractLocker&, FunctionCodeIndex index) WTF_REQUIRES_LOCK(m_lock)
    {
        if (m_optimizedCallees.isEmpty())
            return nullptr;
        return &m_optimizedCallees[index];
    }

    const OptimizedCallees* optimizedCalleesTuple(const AbstractLocker&, FunctionCodeIndex index) const WTF_REQUIRES_LOCK(m_lock)
    {
        if (m_optimizedCallees.isEmpty())
            return nullptr;
        return &m_optimizedCallees[index];
    }

    void ensureOptimizedCalleesSlow(const AbstractLocker&) WTF_REQUIRES_LOCK(m_lock);

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
    bool tryReserveCalleeForInstall(const AbstractLocker&, FunctionCodeIndex, OptimizingJITCallee&) WTF_REQUIRES_LOCK(m_lock);

    // Callees are added here for as long as their code is linked but not yet flushed, i.e. across the
    // window where installOptimizedCallee drops m_lock. Both bookends must run under one lock hold, so
    // only installOptimizedCallee may call them.
    void addPendingPublishCallee(const AbstractLocker&, OptimizingJITCallee& callee) WTF_REQUIRES_LOCK(m_lock)
    {
        m_pendingPublishCallees.append(callee);
    }

    void removePendingPublishCallee(const AbstractLocker&, OptimizingJITCallee& callee) WTF_REQUIRES_LOCK(m_lock)
    {
        m_pendingPublishCallees.removeFirst(&callee);
    }

    void updateCallsitesToCallUs(const AbstractLocker&, CodeLocationLabel<WasmEntryPtrTag> entrypoint, FunctionCodeIndex functionIndex, CallerCallsiteFlushes&) WTF_REQUIRES_LOCK(m_lock);
    void reportCallees(const AbstractLocker&, JITCallee* caller, const FixedBitVector& callees) WTF_REQUIRES_LOCK(m_lock);
#endif

    unsigned m_calleeCount;
    MemoryMode m_mode;

    FixedVector<OptimizedCallees> m_optimizedCallees WTF_GUARDED_BY_LOCK(m_lock);

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
    // Callees whose code is fully linked but not yet published, i.e. their owner has dropped m_lock to
    // flush the instruction cache. They are deliberately absent from m_optimizedCallees so execution
    // threads cannot reach code that has not been flushed yet. They must still be discoverable as
    // direct-call targets: an installer that links a call to one of these must be able to find the
    // callee here so it can be relinked (and kept alive) if that callee is retired before we publish.
    Vector<Ref<OptimizingJITCallee>, 4> m_pendingPublishCallees WTF_GUARDED_BY_LOCK(m_lock);
#endif
    const Ref<IPIntCallees> m_ipintCallees;
    UncheckedKeyHashMap<uint32_t, RefPtr<JSToWasmCallee>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_jsToWasmCallees WTF_GUARDED_BY_LOCK(m_jsToWasmCalleesLock);
    mutable Lock m_jsToWasmCalleesLock;
#if ENABLE(WEBASSEMBLY_BBQJIT) || ENABLE(WEBASSEMBLY_OMGJIT)
    // FIXME: We should probably find some way to prune dead entries periodically.
    UncheckedKeyHashMap<uint32_t, ThreadSafeWeakPtr<OMGOSREntryCallee>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_osrEntryCallees WTF_GUARDED_BY_LOCK(m_lock);
#endif

    // functionCodeIndex -> functionCodeIndex of internal functions that have direct JIT callsites to the lhs.
    // Note, this can grow over time since OMG inlining can add to the set of callers and we'll tranisition from
    // a sparse adjacency matrix to a bit vector based one if that's more space efficient.
    // FIXME: This should be a WTF class and we should use it in the JIT Plans.
    using SparseCallers = UncheckedKeyHashSet<uint32_t, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    using DenseCallers = BitVector;
    FixedVector<Variant<SparseCallers, DenseCallers>> m_callers WTF_GUARDED_BY_LOCK(m_lock);
    FixedVector<CodePtr<WasmEntryPtrTag>> m_wasmIndirectCallEntrypoints;
    FixedVector<RefPtr<Wasm::IPIntCallee>> m_wasmIndirectCallWasmCallees;
    FixedVector<MacroAssemblerCodeRef<WasmEntryPtrTag>> m_wasmToWasmExitStubs;
    RefPtr<EntryPlan> m_plan;
    std::atomic<bool> m_compilationFinished { false };
    String m_errorMessage;
public:
    Lock m_lock;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
