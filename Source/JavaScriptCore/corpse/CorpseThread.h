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

#if OS(MACOS) || USE(APPLE_INTERNAL_SDK)

#include <JavaScriptCore/CorpseAddress.h>
#include <JavaScriptCore/CorpseRegion.h>
#include <mach/mach.h>
#include <stdint.h>
#include <string>
#include <wtf/Vector.h>

namespace JSC {
namespace Corpse {

class Snapshot;

// A snapshot of thread values read out of a corpse.
class Thread {
public:
    // The kernel's system-wide unique 64-bit thread id, as reported by lldb and
    // spindump. This is an identifier, not an address.
    uint64_t id() const { return m_id; }

    // The pthread name, empty if the thread was never named.
    const std::string& name() const { return m_name; }

    int runState() const { return m_runState; }
    int suspendCount() const { return m_suspendCount; }
    uint64_t userTimeUsec() const { return m_userTimeUsec; }
    uint64_t systemTimeUsec() const { return m_systemTimeUsec; }

    Address stackPointer() const { return m_stackPointer; }

    const Region& stackRegion() const { return m_stackRegion; }
    bool hasStack() const { return m_stackRegion.size(); }

    const char* runStateDescription() const;

private:
    static Vector<Thread> collect(const Snapshot&);

    uint64_t m_id { 0 };
    std::string m_name;
    int m_runState { 0 };
    int m_suspendCount { 0 };
    uint64_t m_userTimeUsec { 0 };
    uint64_t m_systemTimeUsec { 0 };
    Address m_stackPointer;
    Region m_stackRegion;

    friend class Snapshot;
};

} // namespace Corpse
} // namespace JSC

#endif // OS(MACOS) || USE(APPLE_INTERNAL_SDK)
