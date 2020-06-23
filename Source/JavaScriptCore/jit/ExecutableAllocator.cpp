/*
 * Copyright (C) 2008-2020 Apple Inc. All rights reserved.
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
#include "ExecutableAllocator.h"

#if ENABLE(JIT)

#include "ExecutableAllocationFuzz.h"
#include "IterationStatus.h"
#include "LinkBuffer.h"
#include <wtf/FastBitVector.h>
#include <wtf/FileSystem.h>
#include <wtf/MetaAllocator.h>
#include <wtf/PageReservation.h>
#include <wtf/ProcessID.h>
#include <wtf/SystemTracing.h>
#include <wtf/WorkQueue.h>

#if HAVE(IOS_JIT_RESTRICTIONS)
#include <wtf/cocoa/Entitlements.h>
#endif

#if OS(DARWIN)
#include <fcntl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>

extern "C" {
    /* Routine mach_vm_remap */
#ifdef mig_external
    mig_external
#else
    extern
#endif /* mig_external */
    kern_return_t mach_vm_remap
    (
     vm_map_t target_task,
     mach_vm_address_t *target_address,
     mach_vm_size_t size,
     mach_vm_offset_t mask,
     int flags,
     vm_map_t src_task,
     mach_vm_address_t src_address,
     boolean_t copy,
     vm_prot_t *cur_protection,
     vm_prot_t *max_protection,
     vm_inherit_t inheritance
     );
}
#endif

namespace JSC {

using namespace WTF;

#if defined(FIXED_EXECUTABLE_MEMORY_POOL_SIZE_IN_MB) && FIXED_EXECUTABLE_MEMORY_POOL_SIZE_IN_MB > 0
static constexpr size_t fixedExecutableMemoryPoolSize = FIXED_EXECUTABLE_MEMORY_POOL_SIZE_IN_MB * MB;
#elif CPU(ARM)
static constexpr size_t fixedExecutableMemoryPoolSize = 16 * MB;
#elif CPU(ARM64)
#if USE(JUMP_ISLANDS)
static constexpr size_t fixedExecutableMemoryPoolSize = 1 * GB;
// These sizes guarantee that any jump within an island can jump forwards or backwards
// to the adjacent island in a single instruction.
static constexpr size_t regionSize = 112 * MB;
static constexpr size_t islandRegionSize = 16 * MB;
static constexpr size_t numberOfRegions = fixedExecutableMemoryPoolSize / regionSize;
static constexpr size_t islandSizeInBytes = 4;
static constexpr size_t maxIslandsPerRegion = islandRegionSize / islandSizeInBytes;
#else
static constexpr size_t fixedExecutableMemoryPoolSize = 128 * MB;
#endif
#elif CPU(X86_64)
static constexpr size_t fixedExecutableMemoryPoolSize = 1 * GB;
#else
static constexpr size_t fixedExecutableMemoryPoolSize = 32 * MB;
#endif

#if CPU(ARM)
static constexpr double executablePoolReservationFraction = 0.15;
#else
static constexpr double executablePoolReservationFraction = 0.25;
#endif

static bool isJITEnabled()
{
    bool jitEnabled = !g_jscConfig.jitDisabled;
#if HAVE(IOS_JIT_RESTRICTIONS)
    return processHasEntitlement("dynamic-codesigning") && jitEnabled;
#else
    return jitEnabled;
#endif
}

void ExecutableAllocator::setJITEnabled(bool enabled)
{
    bool jitEnabled = !g_jscConfig.jitDisabled;
    ASSERT(!g_jscConfig.fixedVMPoolExecutableAllocator);
    if (jitEnabled == enabled)
        return;

    g_jscConfig.jitDisabled = !enabled;

#if HAVE(IOS_JIT_RESTRICTIONS)
    if (!enabled) {
        // Because of an OS quirk, even after the JIT region has been unmapped,
        // the OS thinks that region is reserved, and as such, can cause Gigacage
        // allocation to fail. We work around this by initializing the Gigacage
        // first.
        // Note: when called, setJITEnabled() is always called extra early in the
        // process bootstrap. Under normal operation (when setJITEnabled() isn't
        // called at all), we will naturally initialize the Gigacage before we
        // allocate the JIT region. Hence, this workaround is merely ensuring the
        // same behavior of allocation ordering.
        Gigacage::ensureGigacage();

        constexpr size_t size = 1;
        constexpr int protection = PROT_READ | PROT_WRITE | PROT_EXEC;
        constexpr int flags = MAP_PRIVATE | MAP_ANON | MAP_JIT;
        constexpr int fd = OSAllocator::JSJITCodePages;
        void* allocation = mmap(nullptr, size, protection, flags, fd, 0);
        const void* executableMemoryAllocationFailure = reinterpret_cast<void*>(-1);
        RELEASE_ASSERT_WITH_MESSAGE(allocation && allocation != executableMemoryAllocationFailure, "We should not have allocated executable memory before disabling the JIT.");
        RELEASE_ASSERT_WITH_MESSAGE(!munmap(allocation, size), "Unmapping executable memory should succeed so we do not have any executable memory in the address space");
        RELEASE_ASSERT_WITH_MESSAGE(mmap(nullptr, size, protection, flags, fd, 0) == executableMemoryAllocationFailure, "Allocating executable memory should fail after setJITEnabled(false) is called.");
    }
#endif
}

#if OS(DARWIN) && HAVE(REMAP_JIT)

#if USE(EXECUTE_ONLY_JIT_WRITE_FUNCTION)
static ALWAYS_INLINE MacroAssemblerCodeRef<JITThunkPtrTag> jitWriteThunkGenerator(void* writableAddr, void* stubBase, size_t stubSize)
{
    using namespace ARM64Registers;
    using TrustedImm32 = MacroAssembler::TrustedImm32;

    MacroAssembler jit;

    jit.tagReturnAddress();
    jit.move(MacroAssembler::TrustedImmPtr(writableAddr), x7);
    jit.addPtr(x7, x0);

    jit.move(x0, x3);
    MacroAssembler::Jump smallCopy = jit.branch64(MacroAssembler::Below, x2, MacroAssembler::TrustedImm64(64));

    jit.add64(TrustedImm32(32), x3);
    jit.and64(TrustedImm32(-32), x3);
    jit.loadPair64(x1, x12, x13);
    jit.loadPair64(x1, TrustedImm32(16), x14, x15);
    jit.sub64(x3, x0, x5);
    jit.addPtr(x5, x1);

    jit.loadPair64(x1, x8, x9);
    jit.loadPair64(x1, TrustedImm32(16), x10, x11);
    jit.add64(TrustedImm32(32), x1);
    jit.sub64(x5, x2);
    jit.storePair64(x12, x13, x0);
    jit.storePair64(x14, x15, x0, TrustedImm32(16));
    MacroAssembler::Jump cleanup = jit.branchSub64(MacroAssembler::BelowOrEqual, TrustedImm32(64), x2);

    MacroAssembler::Label copyLoop = jit.label();
    jit.storePair64WithNonTemporalAccess(x8, x9, x3);
    jit.storePair64WithNonTemporalAccess(x10, x11, x3, TrustedImm32(16));
    jit.add64(TrustedImm32(32), x3);
    jit.loadPair64WithNonTemporalAccess(x1, x8, x9);
    jit.loadPair64WithNonTemporalAccess(x1, TrustedImm32(16), x10, x11);
    jit.add64(TrustedImm32(32), x1);
    jit.branchSub64(MacroAssembler::Above, TrustedImm32(32), x2).linkTo(copyLoop, &jit);

    cleanup.link(&jit);
    jit.add64(x2, x1);
    jit.loadPair64(x1, x12, x13);
    jit.loadPair64(x1, TrustedImm32(16), x14, x15);
    jit.storePair64(x8, x9, x3);
    jit.storePair64(x10, x11, x3, TrustedImm32(16));
    jit.addPtr(x2, x3);
    jit.storePair64(x12, x13, x3, TrustedImm32(32));
    jit.storePair64(x14, x15, x3, TrustedImm32(48));
    jit.ret();

    MacroAssembler::Label local0 = jit.label();
    jit.load64(x1, PostIndex(8), x6);
    jit.store64(x6, x3, PostIndex(8));
    smallCopy.link(&jit);
    jit.branchSub64(MacroAssembler::AboveOrEqual, TrustedImm32(8), x2).linkTo(local0, &jit);
    MacroAssembler::Jump local2 = jit.branchAdd64(MacroAssembler::Equal, TrustedImm32(8), x2);
    MacroAssembler::Label local1 = jit.label();
    jit.load8(x1, PostIndex(1), x6);
    jit.store8(x6, x3, PostIndex(1));
    jit.branchSub64(MacroAssembler::NotEqual, TrustedImm32(1), x2).linkTo(local1, &jit);
    local2.link(&jit);
    jit.ret();

    auto stubBaseCodePtr = MacroAssemblerCodePtr<LinkBufferPtrTag>(tagCodePtr<LinkBufferPtrTag>(stubBase));
    LinkBuffer linkBuffer(jit, stubBaseCodePtr, stubSize);
    // We don't use FINALIZE_CODE() for two reasons.
    // The first is that we don't want the writeable address, as disassembled instructions,
    // to appear in the console or anywhere in memory, via the PrintStream buffer.
    // The second is we can't guarantee that the code is readable when using the
    // asyncDisassembly option as our caller will set our pages execute only.
    return linkBuffer.finalizeCodeWithoutDisassembly<JITThunkPtrTag>();
}
#else // not USE(EXECUTE_ONLY_JIT_WRITE_FUNCTION)
static void genericWriteToJITRegion(off_t offset, const void* data, size_t dataSize)
{
    memcpy((void*)(g_jscConfig.startOfFixedWritableMemoryPool + offset), data, dataSize);
}

static MacroAssemblerCodeRef<JITThunkPtrTag> ALWAYS_INLINE jitWriteThunkGenerator(void* address, void*, size_t)
{
    g_jscConfig.startOfFixedWritableMemoryPool = reinterpret_cast<uintptr_t>(address);
    void* function = reinterpret_cast<void*>(&genericWriteToJITRegion);
#if CPU(ARM_THUMB2)
    // Handle thumb offset
    uintptr_t functionAsInt = reinterpret_cast<uintptr_t>(function);
    functionAsInt -= 1;
    function = reinterpret_cast<void*>(functionAsInt);
#endif
    auto codePtr = MacroAssemblerCodePtr<JITThunkPtrTag>(tagCFunctionPtr<JITThunkPtrTag>(function));
    return MacroAssemblerCodeRef<JITThunkPtrTag>::createSelfManagedCodeRef(codePtr);
}
#endif // USE(EXECUTE_ONLY_JIT_WRITE_FUNCTION)

static ALWAYS_INLINE void initializeSeparatedWXHeaps(void* stubBase, size_t stubSize, void* jitBase, size_t jitSize)
{
    mach_vm_address_t writableAddr = 0;

    // Create a second mapping of the JIT region at a random address.
    vm_prot_t cur, max;
    int remapFlags = VM_FLAGS_ANYWHERE;
#if defined(VM_FLAGS_RANDOM_ADDR)
    remapFlags |= VM_FLAGS_RANDOM_ADDR;
#endif
    kern_return_t ret = mach_vm_remap(mach_task_self(), &writableAddr, jitSize, 0,
        remapFlags,
        mach_task_self(), (mach_vm_address_t)jitBase, FALSE,
        &cur, &max, VM_INHERIT_DEFAULT);

    bool remapSucceeded = (ret == KERN_SUCCESS);
    if (!remapSucceeded)
        return;

    // Assemble a thunk that will serve as the means for writing into the JIT region.
    MacroAssemblerCodeRef<JITThunkPtrTag> writeThunk = jitWriteThunkGenerator(reinterpret_cast<void*>(writableAddr), stubBase, stubSize);

    int result = 0;

#if USE(EXECUTE_ONLY_JIT_WRITE_FUNCTION)
    // Prevent reading the write thunk code.
    result = vm_protect(mach_task_self(), reinterpret_cast<vm_address_t>(stubBase), stubSize, true, VM_PROT_EXECUTE);
    RELEASE_ASSERT(!result);
#endif

    // Prevent writing into the executable JIT mapping.
    result = vm_protect(mach_task_self(), reinterpret_cast<vm_address_t>(jitBase), jitSize, true, VM_PROT_READ | VM_PROT_EXECUTE);
    RELEASE_ASSERT(!result);

    // Prevent execution in the writable JIT mapping.
    result = vm_protect(mach_task_self(), static_cast<vm_address_t>(writableAddr), jitSize, true, VM_PROT_READ | VM_PROT_WRITE);
    RELEASE_ASSERT(!result);

    // Zero out writableAddr to avoid leaking the address of the writable mapping.
    memset_s(&writableAddr, sizeof(writableAddr), 0, sizeof(writableAddr));

#if ENABLE(SEPARATED_WX_HEAP)
    g_jscConfig.jitWriteSeparateHeaps = reinterpret_cast<JITWriteSeparateHeapsFunction>(writeThunk.code().executableAddress());
#endif
}

#else // OS(DARWIN) && HAVE(REMAP_JIT)
static ALWAYS_INLINE void initializeSeparatedWXHeaps(void*, size_t, void*, size_t)
{
}
#endif

struct JITReservation {
    PageReservation pageReservation;
    void* base { nullptr };
    size_t size { 0 };
};

static ALWAYS_INLINE JITReservation initializeJITPageReservation()
{
    JITReservation reservation;
    if (!isJITEnabled())
        return reservation;

    reservation.size = fixedExecutableMemoryPoolSize;
#if !USE(JUMP_ISLANDS)
    // FIXME: Consider making jump islands work with Options::jitMemoryReservationSize
    // https://bugs.webkit.org/show_bug.cgi?id=209037
    if (Options::jitMemoryReservationSize())
        reservation.size = Options::jitMemoryReservationSize();
#endif
    reservation.size = std::max(roundUpToMultipleOf(pageSize(), reservation.size), pageSize() * 2);

    auto tryCreatePageReservation = [] (size_t reservationSize) {
#if OS(LINUX)
        // If we use uncommitted reservation, mmap operation is recorded with small page size in perf command's output.
        // This makes the following JIT code logging broken and some of JIT code is not recorded correctly.
        // To avoid this problem, we use committed reservation if we need perf JITDump logging.
        if (Options::logJITCodeForPerf())
            return PageReservation::reserveAndCommitWithGuardPages(reservationSize, OSAllocator::JSJITCodePages, EXECUTABLE_POOL_WRITABLE, true);
#endif
        return PageReservation::reserveWithGuardPages(reservationSize, OSAllocator::JSJITCodePages, EXECUTABLE_POOL_WRITABLE, true);
    };

    reservation.pageReservation = tryCreatePageReservation(reservation.size);
    if (reservation.pageReservation) {
        ASSERT(reservation.pageReservation.size() == reservation.size);
        reservation.base = reservation.pageReservation.base();

        bool fastJITPermissionsIsSupported = useFastJITPermissions();
        if (fastJITPermissionsIsSupported)
            threadSelfRestrictRWXToRX();

#if ENABLE(SEPARATED_WX_HEAP)
        if (!fastJITPermissionsIsSupported) {
            // First page of our JIT allocation is reserved.
            ASSERT(reservation.size >= pageSize() * 2);
            reservation.base = (void*)((uintptr_t)(reservation.base) + pageSize());
            reservation.size -= pageSize();
            initializeSeparatedWXHeaps(reservation.pageReservation.base(), pageSize(), reservation.base, reservation.size);
        }
#endif

        void* reservationEnd = reinterpret_cast<uint8_t*>(reservation.base) + reservation.size;
        g_jscConfig.startExecutableMemory = tagCodePtr<ExecutableMemoryPtrTag>(reservation.base);
        g_jscConfig.endExecutableMemory = tagCodePtr<ExecutableMemoryPtrTag>(reservationEnd);
    }

    return reservation;
}

class FixedVMPoolExecutableAllocator final {
    WTF_MAKE_FAST_ALLOCATED;

#if USE(JUMP_ISLANDS)
    class Islands;
    class RegionAllocator;
#endif

public:
    FixedVMPoolExecutableAllocator()
#if USE(JUMP_ISLANDS)
        : m_allocators(constructFixedSizeArrayWithArguments<RegionAllocator, numberOfRegions>(*this))
#else
        : m_allocator(*this)
#endif
    {
        JITReservation reservation = initializeJITPageReservation();
        m_reservation = WTFMove(reservation.pageReservation);
        if (m_reservation) {
#if USE(JUMP_ISLANDS)
            uintptr_t start = bitwise_cast<uintptr_t>(memoryStart());
            uintptr_t reservationEnd = bitwise_cast<uintptr_t>(memoryEnd());
            for (size_t i = 0; i < numberOfRegions; ++i) {
                RELEASE_ASSERT(start < reservationEnd);
                m_allocators[i].m_start = tagCodePtr<ExecutableMemoryPtrTag>(bitwise_cast<void*>(start));
                m_allocators[i].m_end = tagCodePtr<ExecutableMemoryPtrTag>(bitwise_cast<void*>(start + regionSize));
                if (m_allocators[i].end() > reservationEnd) {
                    // We may have taken a page for the executable only copy thunk.
                    RELEASE_ASSERT(i == numberOfRegions - 1);
                    m_allocators[i].m_end = tagCodePtr<ExecutableMemoryPtrTag>(bitwise_cast<void*>(reservationEnd));
                }

                m_allocators[i].addFreshFreeSpace(bitwise_cast<void*>(m_allocators[i].start()), m_allocators[i].allocatorSize());

                RELEASE_ASSERT(m_allocators[i].allocatorSize() < regionSize);
                RELEASE_ASSERT(m_allocators[i].islandBegin() > m_allocators[i].start());
                RELEASE_ASSERT(m_allocators[i].islandBegin() < m_allocators[i].end());

                start += regionSize;
            }
#else
            m_allocator.addFreshFreeSpace(reservation.base, reservation.size);
            ASSERT(bytesReserved() == reservation.size); // Since our executable memory is fixed-sized, bytesReserved is never changed after initialization.
#endif
        }
    }

    ~FixedVMPoolExecutableAllocator()
    {
        m_reservation.deallocate();
    }

    void* memoryStart() { return untagCodePtr<ExecutableMemoryPtrTag>(g_jscConfig.startExecutableMemory); }
    void* memoryEnd() { return untagCodePtr<ExecutableMemoryPtrTag>(g_jscConfig.endExecutableMemory); }
    bool isJITPC(void* pc) { return memoryStart() <= pc && pc < memoryEnd(); }
    bool isValid() { return !!m_reservation; }

    RefPtr<ExecutableMemoryHandle> allocate(size_t sizeInBytes)
    {
#if USE(JUMP_ISLANDS)
        auto locker = holdLock(getLock());

        unsigned start = 0;
        if (Options::useRandomizingExecutableIslandAllocation())
            start = cryptographicallyRandomNumber() % m_allocators.size();

        unsigned i = start;
        while (true) {
            RegionAllocator& allocator = m_allocators[i];
            if (RefPtr<ExecutableMemoryHandle> result = allocator.allocate(locker, sizeInBytes))
                return result;
            i = (i + 1) % m_allocators.size();
            if (i == start)
                break;
        }
        return nullptr;
#else
        return m_allocator.allocate(sizeInBytes);
#endif // USE(JUMP_ISLANDS)
    }

    Lock& getLock() { return m_lock; }

    // Non atomic
    size_t bytesAllocated() 
    {
        size_t result = 0;
        forEachAllocator([&] (Allocator& allocator) {
            result += allocator.bytesAllocated();
        });
        return result;
    }
    size_t bytesReserved() 
    {
        size_t result = 0;
        forEachAllocator([&] (Allocator& allocator) {
            result += allocator.bytesReserved();
        });
        return result;
    }
    size_t bytesCommitted()
    {
        size_t result = 0;
        forEachAllocator([&] (Allocator& allocator) {
            result += allocator.bytesCommitted();
        });
        return result;
    }

    bool isInAllocatedMemory(const AbstractLocker& locker, void* address)
    {
#if USE(JUMP_ISLANDS)
        if (RegionAllocator* allocator = findRegion(bitwise_cast<uintptr_t>(address)))
            return allocator->isInAllocatedMemory(locker, address);
        return false;
#else
        return m_allocator.isInAllocatedMemory(locker, address);
#endif
    }

#if ENABLE(META_ALLOCATOR_PROFILE)
    void dumpProfile()
    {
        forEachAllocator([&] (Allocator& allocator) {
            allocator.dumpProfile();
        });
    }
#endif

    MetaAllocator::Statistics currentStatistics()
    {
        auto locker = holdLock(getLock());
        MetaAllocator::Statistics result { 0, 0, 0 };
        forEachAllocator([&] (Allocator& allocator) {
            auto allocatorStats = allocator.currentStatistics(locker);
            result.bytesAllocated += allocatorStats.bytesAllocated;
            result.bytesReserved += allocatorStats.bytesReserved;
            result.bytesCommitted += allocatorStats.bytesCommitted;
        });
        return result;
    }

#if USE(JUMP_ISLANDS)
    void handleWillBeReleased(const LockHolder& locker, MetaAllocatorHandle& handle)
    {
        if (m_islandsForJumpSourceLocation.isEmpty())
            return;

        Vector<Islands*, 16> toRemove;
        void* start = handle.start().untaggedPtr();
        void* end = handle.end().untaggedPtr();
        m_islandsForJumpSourceLocation.iterate([&] (Islands& islands, bool& visitLeft, bool& visitRight) {
            if (start <= islands.key() && islands.key() < end)
                toRemove.append(&islands);
            if (islands.key() > start)
                visitLeft = true;
            if (islands.key() < end)
                visitRight = true;
        });

        for (Islands* islands : toRemove)
            freeIslands(locker, islands);

        if (ASSERT_ENABLED) {
            m_islandsForJumpSourceLocation.iterate([&] (Islands& islands, bool& visitLeft, bool& visitRight) {
                if (start <= islands.key() && islands.key() < end) {
                    dataLogLn("did not remove everything!");
                    RELEASE_ASSERT_NOT_REACHED();
                }
                visitLeft = true;
                visitRight = true;
            });
        }
    }

    void* makeIsland(uintptr_t jumpLocation, uintptr_t newTarget, bool concurrently)
    {
        auto locker = holdLock(getLock());
        return islandForJumpLocation(locker, jumpLocation, newTarget, concurrently);
    }

private:
    RegionAllocator* findRegion(uintptr_t ptr)
    {
        RegionAllocator* result = nullptr;
        forEachAllocator([&] (RegionAllocator& allocator) {
            if (allocator.start() <= ptr && ptr < allocator.end()) {
                result = &allocator;
                return IterationStatus::Done;
            }
            return IterationStatus::Continue;
        });
        return result;
    }

    void freeJumpIslands(const LockHolder&, Islands* islands)
    {
        for (CodeLocationLabel<ExecutableMemoryPtrTag> jumpIsland : islands->jumpIslands) {
            uintptr_t untaggedJumpIsland = bitwise_cast<uintptr_t>(jumpIsland.dataLocation());
            RegionAllocator* allocator = findRegion(untaggedJumpIsland);
            RELEASE_ASSERT(allocator);
            allocator->freeIsland(untaggedJumpIsland);
        }
        islands->jumpIslands.clear();
    }

    void freeIslands(const LockHolder& locker, Islands* islands)
    {
        freeJumpIslands(locker, islands);
        m_islandsForJumpSourceLocation.remove(islands);
        delete islands;
    }

    void* islandForJumpLocation(const LockHolder& locker, uintptr_t jumpLocation, uintptr_t target, bool concurrently)
    {
        Islands* islands = m_islandsForJumpSourceLocation.findExact(bitwise_cast<void*>(jumpLocation));
        if (islands) {
            // FIXME: We could create some method of reusing already allocated islands here, but it's
            // unlikely to matter in practice.
            if (!concurrently)
                freeJumpIslands(locker, islands);
        } else {
            islands = new Islands;
            islands->jumpSourceLocation = CodeLocationLabel<ExecutableMemoryPtrTag>(tagCodePtr<ExecutableMemoryPtrTag>(bitwise_cast<void*>(jumpLocation)));
            m_islandsForJumpSourceLocation.insert(islands);
        }

        RegionAllocator* allocator = findRegion(jumpLocation > target ? jumpLocation - regionSize : jumpLocation);
        RELEASE_ASSERT(allocator);
        void* result = allocator->allocateIsland();
        void* currentIsland = result;
        jumpLocation = bitwise_cast<uintptr_t>(currentIsland);
        while (true) {
            islands->jumpIslands.append(CodeLocationLabel<ExecutableMemoryPtrTag>(tagCodePtr<ExecutableMemoryPtrTag>(currentIsland)));

            auto emitJumpTo = [&] (void* target) {
                RELEASE_ASSERT(ARM64Assembler::canEmitJump(bitwise_cast<void*>(jumpLocation), target));

                MacroAssembler jit;
                auto jump = jit.jump();
                LinkBuffer linkBuffer(jit, MacroAssemblerCodePtr<NoPtrTag>(currentIsland), islandSizeInBytes, JITCompilationMustSucceed, false);
                RELEASE_ASSERT(linkBuffer.isValid());

                // We use this to appease the assertion that we're not finalizing on a compiler thread. In this situation, it's
                // ok to do this on a compiler thread, since the compiler thread is linking a jump to this code (and no other live
                // code can jump to these islands). It's ok because the CPU protocol for exposing code to other CPUs is:
                // - Self modifying code fence (what FINALIZE_CODE does below). This does various memory flushes + instruction sync barrier (isb).
                // - Any CPU that will run the code must run a crossModifyingCodeFence (isb) before running it. Since the code that
                // has a jump linked to this island hasn't finalized yet, they're guaranteed to finalize there code and run an isb.
                linkBuffer.setIsJumpIsland();

                linkBuffer.link(jump, CodeLocationLabel<NoPtrTag>(target));
                FINALIZE_CODE(linkBuffer, NoPtrTag, "Jump Island: %lu", jumpLocation);
            };

            if (ARM64Assembler::canEmitJump(bitwise_cast<void*>(jumpLocation), bitwise_cast<void*>(target))) {
                emitJumpTo(bitwise_cast<void*>(target));
                break;
            }

            uintptr_t nextIslandRegion;
            if (jumpLocation > target)
                nextIslandRegion = jumpLocation - regionSize;
            else
                nextIslandRegion = jumpLocation + regionSize;

            RegionAllocator* allocator = findRegion(nextIslandRegion);
            RELEASE_ASSERT(allocator);
            void* nextIsland = allocator->allocateIsland();
            emitJumpTo(nextIsland);
            jumpLocation = bitwise_cast<uintptr_t>(nextIsland);
            currentIsland = nextIsland;
        }

        return result;
    }
#endif // USE(JUMP_ISLANDS)

private:
    class Allocator : public MetaAllocator {
        using Base = MetaAllocator;
    public:
        Allocator(FixedVMPoolExecutableAllocator& allocator)
            : Base(allocator.getLock(), jitAllocationGranule, pageSize()) // round up all allocations to 32 bytes
            , m_fixedAllocator(allocator)
        {
        }

        FreeSpacePtr allocateNewSpace(size_t&) override
        {
            // We're operating in a fixed pool, so new allocation is always prohibited.
            return nullptr;
        }

        void notifyNeedPage(void* page, size_t count) override
        {
            m_fixedAllocator.m_reservation.commit(page, pageSize() * count);
        }

        void notifyPageIsFree(void* page, size_t count) override
        {
            m_fixedAllocator.m_reservation.decommit(page, pageSize() * count);
        }

        FixedVMPoolExecutableAllocator& m_fixedAllocator;
    };

#if USE(JUMP_ISLANDS)
    class RegionAllocator final : public Allocator {
        using Base = Allocator;
    public:
        RegionAllocator(FixedVMPoolExecutableAllocator& allocator)
            : Base(allocator)
        {
        }

        //  ------------------------------------
        //  | jit allocations -->   <-- islands |
        //  -------------------------------------

        uintptr_t start() { return bitwise_cast<uintptr_t>(untagCodePtr<ExecutableMemoryPtrTag>(m_start)); }
        uintptr_t end() { return bitwise_cast<uintptr_t>(untagCodePtr<ExecutableMemoryPtrTag>(m_end)); }

        uintptr_t islandBegin()
        {
            // [start, allocatorEnd)
            return end() - islandRegionSize;
        }

        uintptr_t allocatorSize()
        {
            return islandBegin() - start();
        }

        size_t islandsPerPage()
        {
            size_t islandsPerPage = pageSize() / islandSizeInBytes;
            ASSERT(islandsPerPage * islandSizeInBytes == pageSize());
            ASSERT(isPowerOfTwo(islandsPerPage));
            return islandsPerPage;
        }

        void release(const LockHolder& locker, MetaAllocatorHandle& handle) final
        {
            m_fixedAllocator.handleWillBeReleased(locker, handle);
            Base::release(locker, handle);
        }

        void* allocateIsland()
        {
            uintptr_t end = this->end();
            auto findResult = [&] () -> void* {
                size_t resultBit = islandBits.findClearBit(0);
                if (resultBit == islandBits.size())
                    return nullptr;
                islandBits[resultBit] = true;
                uintptr_t result = end - ((resultBit + 1) * islandSizeInBytes); 
                return bitwise_cast<void*>(result);
            };

            if (void* result = findResult())
                return result;

            islandBits.resize(islandBits.size() + islandsPerPage());
            if (UNLIKELY(islandBits.size() > maxIslandsPerRegion))
                crashOnJumpIslandExhaustion();

            uintptr_t pageBegin = end - (islandBits.size() * islandSizeInBytes); // [islandBegin, end)
            m_fixedAllocator.m_reservation.commit(bitwise_cast<void*>(pageBegin), pageSize());

            void* result = findResult();
            RELEASE_ASSERT(result);
            return result;
        }

        NEVER_INLINE NO_RETURN_DUE_TO_CRASH void crashOnJumpIslandExhaustion()
        {
            CRASH();
        }

        Optional<size_t> islandBit(uintptr_t island)
        {
            uintptr_t end = this->end();
            if (islandBegin() <= island && island < end)
                return ((end - island) / islandSizeInBytes) - 1;
            return WTF::nullopt;
        }

        void freeIsland(uintptr_t island)
        {
            RELEASE_ASSERT(islandBegin() <= island && island < end());
            size_t bit = islandBit(island).value();
            RELEASE_ASSERT(!!islandBits[bit]);
            islandBits[bit] = false;
        }

        bool isInAllocatedMemory(const AbstractLocker& locker, void* address)
        {
            if (Base::isInAllocatedMemory(locker, address))
                return true;
            if (Optional<size_t> bit = islandBit(bitwise_cast<uintptr_t>(address))) {
                if (bit.value() < islandBits.size())
                    return !!islandBits[bit.value()];
            }
            return false;
        }

        // Range: [start, end)
        void* m_start;
        void* m_end;
        FastBitVector islandBits;
    };
#endif // USE(JUMP_ISLANDS)

    template <typename Function>
    void forEachAllocator(Function function)
    {
#if USE(JUMP_ISLANDS)
        for (RegionAllocator& allocator : m_allocators) {
            using FunctionResultType = decltype(function(allocator));
            if constexpr (std::is_same<IterationStatus, FunctionResultType>::value) {
                if (function(allocator) == IterationStatus::Done)
                    break;
            } else {
                static_assert(std::is_same<void, FunctionResultType>::value);
                function(allocator);
            }
        }
#else
        function(m_allocator);
#endif // USE(JUMP_ISLANDS)
    }

#if USE(JUMP_ISLANDS)
    class Islands : public RedBlackTree<Islands, void*>::Node {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        void* key() { return jumpSourceLocation.dataLocation(); }
        CodeLocationLabel<ExecutableMemoryPtrTag> jumpSourceLocation;
        Vector<CodeLocationLabel<ExecutableMemoryPtrTag>> jumpIslands;
    };
#endif // USE(JUMP_ISLANDS)

    Lock m_lock;
    PageReservation m_reservation;
#if USE(JUMP_ISLANDS)
    std::array<RegionAllocator, numberOfRegions> m_allocators;
    RedBlackTree<Islands, void*> m_islandsForJumpSourceLocation;
#else
    Allocator m_allocator;
#endif // USE(JUMP_ISLANDS)
};

// Keep this pointer in a mutable global variable to help Leaks find it.
// But we do not use this pointer.
static FixedVMPoolExecutableAllocator* globalFixedVMPoolExecutableAllocatorToWorkAroundLeaks = nullptr;
void ExecutableAllocator::initializeUnderlyingAllocator()
{
    RELEASE_ASSERT(!g_jscConfig.fixedVMPoolExecutableAllocator);
    g_jscConfig.fixedVMPoolExecutableAllocator = new FixedVMPoolExecutableAllocator();
    globalFixedVMPoolExecutableAllocatorToWorkAroundLeaks = g_jscConfig.fixedVMPoolExecutableAllocator;
}

bool ExecutableAllocator::isValid() const
{
    FixedVMPoolExecutableAllocator* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        return Base::isValid();
    return allocator->isValid();
}

bool ExecutableAllocator::underMemoryPressure()
{
    FixedVMPoolExecutableAllocator* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        return Base::underMemoryPressure();
    return allocator->bytesAllocated() > allocator->bytesReserved() / 2;
}

double ExecutableAllocator::memoryPressureMultiplier(size_t addedMemoryUsage)
{
    FixedVMPoolExecutableAllocator* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        return Base::memoryPressureMultiplier(addedMemoryUsage);
    ASSERT(allocator->bytesAllocated() <= allocator->bytesReserved());
    size_t bytesAllocated = allocator->bytesAllocated() + addedMemoryUsage;
    size_t bytesAvailable = static_cast<size_t>(
        allocator->bytesReserved() * (1 - executablePoolReservationFraction));
    if (bytesAllocated >= bytesAvailable)
        bytesAllocated = bytesAvailable;
    double result = 1.0;
    size_t divisor = bytesAvailable - bytesAllocated;
    if (divisor)
        result = static_cast<double>(bytesAvailable) / divisor;
    if (result < 1.0)
        result = 1.0;
    return result;
}

RefPtr<ExecutableMemoryHandle> ExecutableAllocator::allocate(size_t sizeInBytes, JITCompilationEffort effort)
{
    FixedVMPoolExecutableAllocator* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        return Base::allocate(sizeInBytes, effort);
    if (Options::logExecutableAllocation()) {
        MetaAllocator::Statistics stats = allocator->currentStatistics();
        dataLog("Allocating ", sizeInBytes, " bytes of executable memory with ", stats.bytesAllocated, " bytes allocated, ", stats.bytesReserved, " bytes reserved, and ", stats.bytesCommitted, " committed.\n");
    }

    if (effort != JITCompilationCanFail && Options::reportMustSucceedExecutableAllocations()) {
        dataLog("Allocating ", sizeInBytes, " bytes of executable memory with JITCompilationMustSucceed.\n");
        WTFReportBacktrace();
    }

    if (effort == JITCompilationCanFail
        && doExecutableAllocationFuzzingIfEnabled() == PretendToFailExecutableAllocation)
        return nullptr;

    if (effort == JITCompilationCanFail) {
        // Don't allow allocations if we are down to reserve.
        size_t bytesAllocated = allocator->bytesAllocated() + sizeInBytes;
        size_t bytesAvailable = static_cast<size_t>(
            allocator->bytesReserved() * (1 - executablePoolReservationFraction));
        if (bytesAllocated > bytesAvailable) {
            if (Options::logExecutableAllocation())
                dataLog("Allocation failed because bytes allocated ", bytesAllocated,  " > ", bytesAvailable, " bytes available.\n");
            return nullptr;
        }
    }

    RefPtr<ExecutableMemoryHandle> result = allocator->allocate(sizeInBytes);
    if (!result) {
        if (effort != JITCompilationCanFail) {
            dataLog("Ran out of executable memory while allocating ", sizeInBytes, " bytes.\n");
            CRASH();
        }
        return nullptr;
    }

    void* start = allocator->memoryStart();
    void* end = allocator->memoryEnd();
    void* resultStart = result->start().untaggedPtr();
    void* resultEnd = result->end().untaggedPtr();
    RELEASE_ASSERT(start <= resultStart && resultStart < end);
    RELEASE_ASSERT(start < resultEnd && resultEnd <= end);
    return result;
}

bool ExecutableAllocator::isValidExecutableMemory(const AbstractLocker& locker, void* address)
{
    FixedVMPoolExecutableAllocator* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        return Base::isValidExecutableMemory(locker, address);
    return allocator->isInAllocatedMemory(locker, address);
}

Lock& ExecutableAllocator::getLock() const
{
    FixedVMPoolExecutableAllocator* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        return Base::getLock();
    return allocator->getLock();
}

size_t ExecutableAllocator::committedByteCount()
{
    FixedVMPoolExecutableAllocator* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        return Base::committedByteCount();
    return allocator->bytesCommitted();
}

#if ENABLE(META_ALLOCATOR_PROFILE)
void ExecutableAllocator::dumpProfile()
{
    FixedVMPoolExecutableAllocator* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        return;
    allocator->dumpProfile();
}
#endif

#if USE(JUMP_ISLANDS)
void* ExecutableAllocator::getJumpIslandTo(void* from, void* newDestination)
{
    FixedVMPoolExecutableAllocator* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        RELEASE_ASSERT_NOT_REACHED();

    return allocator->makeIsland(bitwise_cast<uintptr_t>(from), bitwise_cast<uintptr_t>(newDestination), false);
}

void* ExecutableAllocator::getJumpIslandToConcurrently(void* from, void* newDestination)
{
    FixedVMPoolExecutableAllocator* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        RELEASE_ASSERT_NOT_REACHED();

    return allocator->makeIsland(bitwise_cast<uintptr_t>(from), bitwise_cast<uintptr_t>(newDestination), true);
}
#endif

void* startOfFixedExecutableMemoryPoolImpl()
{
    FixedVMPoolExecutableAllocator* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        return nullptr;
    return allocator->memoryStart();
}

void* endOfFixedExecutableMemoryPoolImpl()
{
    FixedVMPoolExecutableAllocator* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        return nullptr;
    return allocator->memoryEnd();
}

bool isJITPC(void* pc)
{
    FixedVMPoolExecutableAllocator* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    return allocator && allocator->isJITPC(pc);
}

void dumpJITMemory(const void* dst, const void* src, size_t size)
{
    RELEASE_ASSERT(Options::dumpJITMemoryPath());

#if OS(DARWIN)
    static int fd = -1;
    static uint8_t* buffer;
    static constexpr size_t bufferSize = fixedExecutableMemoryPoolSize;
    static size_t offset = 0;
    static Lock dumpJITMemoryLock;
    static bool needsToFlush = false;
    static auto flush = [](const AbstractLocker&) {
        if (fd == -1) {
            String path = Options::dumpJITMemoryPath();
            path = path.replace("%pid", String::number(getCurrentProcessID()));
            fd = open(FileSystem::fileSystemRepresentation(path).data(), O_CREAT | O_TRUNC | O_APPEND | O_WRONLY | O_EXLOCK | O_NONBLOCK, 0666);
            RELEASE_ASSERT(fd != -1);
        }
        write(fd, buffer, offset);
        offset = 0;
        needsToFlush = false;
    };

    static std::once_flag once;
    static LazyNeverDestroyed<Ref<WorkQueue>> flushQueue;
    std::call_once(once, [] {
        buffer = bitwise_cast<uint8_t*>(malloc(bufferSize));
        flushQueue.construct(WorkQueue::create("jsc.dumpJITMemory.queue", WorkQueue::Type::Serial, WorkQueue::QOS::Background));
        std::atexit([] {
            LockHolder locker(dumpJITMemoryLock);
            flush(locker);
            close(fd);
            fd = -1;
        });
    });

    static auto enqueueFlush = [](const AbstractLocker&) {
        if (needsToFlush)
            return;

        needsToFlush = true;
        flushQueue.get()->dispatchAfter(Seconds(Options::dumpJITMemoryFlushInterval()), [] {
            LockHolder locker(dumpJITMemoryLock);
            if (!needsToFlush)
                return;
            flush(locker);
        });
    };

    static auto write = [](const AbstractLocker& locker, const void* src, size_t size) {
        if (UNLIKELY(offset + size > bufferSize))
            flush(locker);
        memcpy(buffer + offset, src, size);
        offset += size;
        enqueueFlush(locker);
    };

    LockHolder locker(dumpJITMemoryLock);
    uint64_t time = mach_absolute_time();
    uint64_t dst64 = bitwise_cast<uintptr_t>(dst);
    uint64_t size64 = size;
    TraceScope(DumpJITMemoryStart, DumpJITMemoryStop, time, dst64, size64);
    write(locker, &time, sizeof(time));
    write(locker, &dst64, sizeof(dst64));
    write(locker, &size64, sizeof(size64));
    write(locker, src, size);
#else
    UNUSED_PARAM(dst);
    UNUSED_PARAM(src);
    UNUSED_PARAM(size);
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

} // namespace JSC

#endif // ENABLE(JIT)

namespace JSC {

// Keep this pointer in a mutable global variable to help Leaks find it.
// But we do not use this pointer.
static ExecutableAllocator* globalExecutableAllocatorToWorkAroundLeaks = nullptr;
void ExecutableAllocator::initialize()
{
    g_jscConfig.executableAllocator = new ExecutableAllocator;
    globalExecutableAllocatorToWorkAroundLeaks = g_jscConfig.executableAllocator;
}

ExecutableAllocator& ExecutableAllocator::singleton()
{
    ASSERT(g_jscConfig.executableAllocator);
    return *g_jscConfig.executableAllocator;
}

} // namespace JSC
