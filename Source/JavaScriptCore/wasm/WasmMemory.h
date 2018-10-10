/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include "WasmMemoryMode.h"
#include "WasmPageCount.h"

#include <wtf/Expected.h>
#include <wtf/Function.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WTF {
class PrintStream;
}

namespace JSC {

namespace Wasm {

class Instance;

class Memory : public RefCounted<Memory> {
    WTF_MAKE_NONCOPYABLE(Memory);
    WTF_MAKE_FAST_ALLOCATED;
public:
    void dump(WTF::PrintStream&) const;

    explicit operator bool() const { return !!m_memory; }
    
    enum NotifyPressure { NotifyPressureTag };
    enum SyncTryToReclaim { SyncTryToReclaimTag };
    enum GrowSuccess { GrowSuccessTag };

    static Ref<Memory> create();
    static RefPtr<Memory> tryCreate(PageCount initial, PageCount maximum, WTF::Function<void(NotifyPressure)>&& notifyMemoryPressure, WTF::Function<void(SyncTryToReclaim)>&& syncTryToReclaimMemory, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback);

    ~Memory();

    static size_t fastMappedRedzoneBytes();
    static size_t fastMappedBytes(); // Includes redzone.
    static bool addressIsInActiveFastMemory(void*);

    void* memory() const { return m_memory; }
    size_t size() const { return m_size; }
    PageCount sizeInPages() const { return PageCount::fromBytes(m_size); }

    PageCount initial() const { return m_initial; }
    PageCount maximum() const { return m_maximum; }

    MemoryMode mode() const { return m_mode; }

    enum class GrowFailReason {
        InvalidDelta,
        InvalidGrowSize,
        WouldExceedMaximum,
        OutOfMemory,
    };
    Expected<PageCount, GrowFailReason> grow(PageCount);
    void registerInstance(Instance*);

    void check() {  ASSERT(!deletionHasBegun()); }

    static ptrdiff_t offsetOfMemory() { return OBJECT_OFFSETOF(Memory, m_memory); }
    static ptrdiff_t offsetOfSize() { return OBJECT_OFFSETOF(Memory, m_size); }

private:
    Memory();
    Memory(void* memory, PageCount initial, PageCount maximum, size_t mappedCapacity, MemoryMode, WTF::Function<void(NotifyPressure)>&& notifyMemoryPressure, WTF::Function<void(SyncTryToReclaim)>&& syncTryToReclaimMemory, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback);
    Memory(PageCount initial, PageCount maximum, WTF::Function<void(NotifyPressure)>&& notifyMemoryPressure, WTF::Function<void(SyncTryToReclaim)>&& syncTryToReclaimMemory, WTF::Function<void(GrowSuccess, PageCount, PageCount)>&& growSuccessCallback);

    void* m_memory { nullptr };
    size_t m_size { 0 };
    PageCount m_initial;
    PageCount m_maximum;
    size_t m_mappedCapacity { 0 };
    MemoryMode m_mode { MemoryMode::BoundsChecking };
    WTF::Function<void(NotifyPressure)> m_notifyMemoryPressure;
    WTF::Function<void(SyncTryToReclaim)> m_syncTryToReclaimMemory;
    WTF::Function<void(GrowSuccess, PageCount, PageCount)> m_growSuccessCallback;
    Vector<WeakPtr<Instance>> m_instances;
};

} } // namespace JSC::Wasm

#else

namespace JSC { namespace Wasm {

class Memory {
public:
    static size_t maxFastMemoryCount() { return 0; }
    static bool addressIsInActiveFastMemory(void*) { return false; }
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
