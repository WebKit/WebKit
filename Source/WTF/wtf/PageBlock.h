/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include <wtf/StdLibExtras.h>

namespace WTF {

// We attempt to guess a value that is *AT LEAST* as large as the system's actual page size.
// This is impossible to do correctly at build time, but JSC really needs it at build time, so
// we have a RELEASE_ASSERT() inside WTF::pageSize to make sure it is set properly at runtime.
// All of these values are going to be incorrect on systems configured to use larger than normal
// page size, so on such systems it is expected that WebKit will crash until this value is changed
// and recompiled. Sorry.
//
// macOS x86_64 uses 4 KiB, but Apple's aarch64 systems use 16 KiB. Use 16 KiB on all Apple systems
// for consistency.
//
// Most Linux and Windows systems use a page size of 4 KiB.
//
// On Linux, Power systems normally use 64 KiB pages.
//
// Use 64 KiB for any unknown CPUs to be conservative.
#if OS(DARWIN) || PLATFORM(PLAYSTATION) || CPU(MIPS) || CPU(MIPS64)
constexpr size_t CeilingOnPageSize = 16 * KB;
#elif CPU(PPC) || CPU(PPC64) || CPU(PPC64LE) || CPU(UNKNOWN)
constexpr size_t CeilingOnPageSize = 64 * KB;
#elif OS(WINDOWS) || CPU(X86) || CPU(X86_64) || CPU(ARM) || CPU(ARM64) || CPU(RISCV64)
constexpr size_t CeilingOnPageSize = 4 * KB;
#else
#error Must set CeilingOnPageSize in PageBlock.h when adding a new CPU architecture!
#endif

WTF_EXPORT_PRIVATE size_t pageSize();
WTF_EXPORT_PRIVATE size_t pageMask();
inline bool isPageAligned(void* address) { return !(reinterpret_cast<intptr_t>(address) & (pageSize() - 1)); }
inline bool isPageAligned(size_t size) { return !(size & (pageSize() - 1)); }
inline bool isPowerOfTwo(size_t size) { return !(size & (size - 1)); }

class PageBlock {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PageBlock() = default;
    PageBlock(void*, size_t, bool hasGuardPages);
    
    void* base() const { return m_base; }
    size_t size() const { return m_size; }

    operator bool() const { return !!m_realBase; }

    bool contains(void* containedBase, size_t containedSize)
    {
        return containedBase >= m_base
            && (static_cast<char*>(containedBase) + containedSize) <= (static_cast<char*>(m_base) + m_size);
    }

private:
    void* m_realBase { nullptr };
    void* m_base { nullptr };
    size_t m_size { 0 };
};

inline PageBlock::PageBlock(void* base, size_t size, bool hasGuardPages)
    : m_realBase(base)
    , m_base(static_cast<char*>(base) + ((base && hasGuardPages) ? pageSize() : 0))
    , m_size(size)
{
}

} // namespace WTF

using WTF::CeilingOnPageSize;
using WTF::pageSize;
using WTF::isPageAligned;
using WTF::isPowerOfTwo;
