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

#pragma once

#if PLATFORM(COCOA)

#include <wtf/MachSendRight.h>

namespace WebKit {

// Maps SecurityFlags::page() into each child process read-only. Every process reaches it through the same static
// symbol; a pointer to it would be writable memory an attacker could aim elsewhere.
namespace SecurityFlagsSharedPage {

// UIProcess only, before anything writes the flags: this replaces the symbol's contents with zeroed memory.
void initializeInUIProcess();

// UIProcess only, from any thread once initializeInUIProcess() has run.
MachSendRight createSendRightForChildProcess();

// Child processes only, before any attacker-controlled data is parsed. False when the shared page could not be
// adopted, in which case this process's own page is frozen with every flag enforced instead.
bool mapReadOnlyPermanently(const MachSendRight&);

#if defined(ENGINEERING_BUILD) && ENGINEERING_BUILD
// UIProcess only. A process launched after this freezes a page of its own rather than sharing the UIProcess's.
void setWithholdEntryFromChildProcessesForTesting(bool);
#endif

struct AttackResults {
    int raiseCurrentProtection;
    int raiseMaximumProtection;
    int raiseProtectionByCopying;
    int overwriteWithWritableMemory;
    int substituteReadOnlyPage;
};
AttackResults runAttacks();

} // namespace SecurityFlagsSharedPage

} // namespace WebKit

#endif // PLATFORM(COCOA)
