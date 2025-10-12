/*
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
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

#if ENABLE(ASSEMBLER)

#include <JavaScriptCore/LinkBuffer.h>
#include <stdio.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/CString.h>

extern "C" {
struct JITCodeEntry;

struct GdbJITAddressRegionLess {
    inline bool operator()(const std::span<const uint8_t>& a, const std::span<const uint8_t>& b) const
    {
        if (a.data() == b.data()) return a.size() < b.size();
        return a.data() < b.data();
    }
};

using GdbJITCodeMap = std::map<std::span<const uint8_t>, JITCodeEntry*, GdbJITAddressRegionLess>;
}

namespace JSC {

// This is a little helper for writing DWARF information
// so that GDB can recognize our jit functions.
// This file originally came from v8/src/diagnostics/gdb-jit.h
class GdbJIT {
    WTF_MAKE_TZONE_ALLOCATED(GdbJIT);
    WTF_MAKE_NONCOPYABLE(GdbJIT);
    friend class LazyNeverDestroyed<GdbJIT>;
public:
    static void log(const CString& name, MacroAssemblerCodeRef<LinkBufferPtrTag>);

private:
    GdbJIT() = default;
    static GdbJIT& singleton();

    Lock m_lock;
    GdbJITCodeMap m_map;
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)
