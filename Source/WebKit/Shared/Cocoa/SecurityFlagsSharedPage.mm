/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "SecurityFlagsSharedPage.h"

#if PLATFORM(COCOA)

#import "Logging.h"
#import "SecurityFlags.h"
#import <mach/mach.h>
#import <mach/mach_error.h>
#import <mach/mach_init.h>
#import <mach/vm_map.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/spi/cocoa/MachVMSPI.h>

#if HAVE(MACH_MEMORY_ENTRY)
#import <mach/memory_entry.h>
#endif

namespace WebKit {

namespace SecurityFlagsSharedPage {

static constexpr size_t pageSizeToShare = SecurityFlags::pageStorageSize;

static mach_vm_address_t pageAddress()
{
    // Not bit_cast straight to mach_vm_address_t: that is ill-formed where pointers are 32 bits, as on watchOS.
    return std::bit_cast<uintptr_t>(&SecurityFlags::page());
}

static MachSendRight& readOnlyEntryForChildProcess()
{
    // Written once from initializeInUIProcess(), inside InitializeWebKit2()'s std::call_once, which is what
    // orders it against the launcher threads that read it.
    static NeverDestroyed<MachSendRight> sendRight;
    return sendRight.get();
}

#if defined(ENGINEERING_BUILD) && ENGINEERING_BUILD
static bool& withholdEntryFromChildProcessesForTesting()
{
    static bool withhold = false;
    return withhold;
}

void setWithholdEntryFromChildProcessesForTesting(bool withhold)
{
    RELEASE_ASSERT(!isInAuxiliaryProcess());
    withholdEntryFromChildProcessesForTesting() = withhold;
}
#endif

void initializeInUIProcess()
{
    RELEASE_ASSERT(!isInAuxiliaryProcess());
    RELEASE_ASSERT(!readOnlyEntryForChildProcess());

    // A named entry, not one over the framework's own mapping: MAP_MEM_VM_SHARE cannot be mapped with
    // VM_FLAGS_PERMANENT.
    memory_object_size_t entrySize = pageSizeToShare;
    mach_port_t writableEntry = MACH_PORT_NULL;
    auto kr = mach_make_memory_entry_64(mach_task_self(), &entrySize, 0,
        MAP_MEM_NAMED_CREATE | VM_PROT_READ | VM_PROT_WRITE, &writableEntry, MACH_PORT_NULL);
    if (kr != KERN_SUCCESS || entrySize < pageSizeToShare) {
        RELEASE_LOG_FAULT(Process, "SecurityFlagsSharedPage: no memory entry for the security flags, so the WebContent processes will enforce every flag: %" PUBLIC_LOG_STRING " (%x)", mach_error_string(kr), kr);
        deallocateSendRightSafely(writableEntry);
        return;
    }

    // Not VM_FLAGS_PERMANENT: pinning this side would foreclose ever handing out a fresh page per process generation.
    mach_vm_address_t address = pageAddress();
    kr = mach_vm_map(mach_task_self(), &address, pageSizeToShare, 0, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
        writableEntry, 0, false, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG_FAULT(Process, "SecurityFlagsSharedPage: could not put the security flags in shareable memory, so the WebContent processes will enforce every flag: %" PUBLIC_LOG_STRING " (%x)", mach_error_string(kr), kr);
        deallocateSendRightSafely(writableEntry);
        return;
    }

    // VM_PROT_IS_MASK makes VM_PROT_READ a ceiling on the entry, not the protection to map it with, so nothing
    // derived from the right we hand out can be writable.
    memory_object_size_t childSize = pageSizeToShare;
    mach_port_t readOnlyEntry = MACH_PORT_NULL;
    kr = mach_make_memory_entry_64(mach_task_self(), &childSize, 0, VM_PROT_READ | VM_PROT_IS_MASK, &readOnlyEntry, writableEntry);
    deallocateSendRightSafely(writableEntry);
    if (kr != KERN_SUCCESS || childSize < pageSizeToShare) {
        RELEASE_LOG_FAULT(Process, "SecurityFlagsSharedPage: no read-only entry for the security flags, so the WebContent processes will enforce every flag: %" PUBLIC_LOG_STRING " (%x)", mach_error_string(kr), kr);
        deallocateSendRightSafely(readOnlyEntry);
        return;
    }

    readOnlyEntryForChildProcess() = MachSendRight::adopt(readOnlyEntry);

    auto& page = SecurityFlags::page();
    page.magic = SecurityFlags::Page::expectedMagic;
    page.layoutHash = SecurityFlags::Page::expectedLayoutHash;
}

MachSendRight createSendRightForChildProcess()
{
#if defined(ENGINEERING_BUILD) && ENGINEERING_BUILD
    if (withholdEntryFromChildProcessesForTesting())
        return { };
#endif
    return MachSendRight { readOnlyEntryForChildProcess() };
}

// The page stays zeroed, which is every flag enforced. Anonymous memory can be VM_FLAGS_PERMANENT even though a
// shared entry cannot.
static void freezeWithEveryFlagEnforced()
{
    mach_vm_address_t address = pageAddress();
    auto kr = mach_vm_map(mach_task_self(), &address, pageSizeToShare, 0,
        VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE | VM_FLAGS_PERMANENT,
        MEMORY_OBJECT_NULL, 0, false, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
    if (kr == KERN_SUCCESS)
        return;

    // Only a permanent mapping somebody else put at this address can refuse anonymous memory here, and reading the
    // flags out of memory they own is worse than not running.
    RELEASE_LOG_FAULT(Process, "SecurityFlagsSharedPage: the security flag page is not ours: %" PUBLIC_LOG_STRING " (%x)", mach_error_string(kr), kr);
    CRASH_WITH_INFO(kr);
}

// Validated through a throwaway mapping first, because the real one is permanent: a page from a build that
// assigned the bits differently has to be rejected before there is no way left to replace it.
static bool entryHasAValidPage(const MachSendRight& memoryEntry)
{
    mach_vm_address_t address = 0;
    auto kr = mach_vm_map(mach_task_self(), &address, pageSizeToShare, 0, VM_FLAGS_ANYWHERE,
        memoryEntry.sendRight(), 0, false, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG_FAULT(Process, "SecurityFlagsSharedPage: could not map the security flags: %" PUBLIC_LOG_STRING " (%x)", mach_error_string(kr), kr);
        return false;
    }

    bool isValid = std::bit_cast<const SecurityFlags::Page*>(static_cast<uintptr_t>(address))->isValid();
    mach_vm_deallocate(mach_task_self(), address, pageSizeToShare);

    if (!isValid)
        RELEASE_LOG_FAULT(Process, "SecurityFlagsSharedPage: the security flag page is not the layout this build expects");
    return isValid;
}

static bool adoptSharedPageOrFreezeOurOwn(const MachSendRight& memoryEntry)
{
    if (!memoryEntry || !entryHasAValidPage(memoryEntry)) {
        freezeWithEveryFlagEnforced();
        return false;
    }

    mach_vm_address_t address = pageAddress();

    // VM_FLAGS_PERMANENT is what stops the address from being reused; without it the protection is one
    // mach_vm_deallocate away from meaningless.
    auto kr = mach_vm_map(mach_task_self(), &address, pageSizeToShare, 0,
        VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE | VM_FLAGS_PERMANENT,
        memoryEntry.sendRight(), 0, false, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG_FAULT(Process, "SecurityFlagsSharedPage: could not map the security flags read-only: %" PUBLIC_LOG_STRING " (%x)", mach_error_string(kr), kr);
        freezeWithEveryFlagEnforced();
        return false;
    }

    RELEASE_ASSERT(address == pageAddress());
    return true;
}

bool mapReadOnlyPermanently(const MachSendRight& memoryEntry)
{
    // A second call must not re-derive this: it would meet the first call's permanent mapping, which
    // freezeWithEveryFlagEnforced() treats as fatal.
    static std::optional<bool> outcome;
    if (!outcome)
        outcome = adoptSharedPageOrFreezeOurOwn(memoryEntry);
    return *outcome;
}

AttackResults runAttacks()
{
    RELEASE_ASSERT(isInWebProcess());

    auto address = pageAddress();
    auto raiseProtection = [&](boolean_t setMaximum, vm_prot_t protection) {
        return mach_vm_protect(mach_task_self(), address, pageSizeToShare, setMaximum, protection);
    };

    AttackResults results { };
    results.raiseCurrentProtection = raiseProtection(false, VM_PROT_READ | VM_PROT_WRITE);
    results.raiseMaximumProtection = raiseProtection(true, VM_PROT_READ | VM_PROT_WRITE);
    results.raiseProtectionByCopying = raiseProtection(false, VM_PROT_COPY | VM_PROT_WRITE);

    auto overwriteAddress = address;
    results.overwriteWithWritableMemory = mach_vm_map(mach_task_self(), &overwriteAddress, pageSizeToShare, 0,
        VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, MEMORY_OBJECT_NULL, 0, false,
        VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);

    memory_object_size_t entrySize = pageSizeToShare;
    mach_port_t attackerEntry = MACH_PORT_NULL;
    results.substituteReadOnlyPage = mach_make_memory_entry_64(mach_task_self(), &entrySize, 0,
        MAP_MEM_NAMED_CREATE | VM_PROT_READ | VM_PROT_WRITE, &attackerEntry, MACH_PORT_NULL);
    if (results.substituteReadOnlyPage == KERN_SUCCESS) {
        auto substituteAddress = address;
        results.substituteReadOnlyPage = mach_vm_map(mach_task_self(), &substituteAddress, pageSizeToShare, 0,
            VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, attackerEntry, 0, false, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
        deallocateSendRightSafely(attackerEntry);
    }

    return results;
}

} // namespace SecurityFlagsSharedPage

} // namespace WebKit

#endif // PLATFORM(COCOA)
