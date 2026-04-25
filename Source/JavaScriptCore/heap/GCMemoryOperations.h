/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/CPU.h>
#include <JavaScriptCore/JSCJSValue.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

// We use these memory operations when modifying memory that might be scanned by the concurrent collector.
// We don't call the default operations because they're not guaranteed to store to memory in eight byte aligned
// chunks. If we happened to fall into the system's normal byte copy loop, we may see a torn JSValue in the
// concurrent collector.

constexpr size_t smallCutoff = 8 * 8;

// This is a forwards loop so gcSafeMemmove can rely on the direction.
template <typename T>
ALWAYS_INLINE void gcSafeMemcpy(T* dst, const T* src, size_t bytes)
{
    static_assert(sizeof(T) == sizeof(JSValue));
    RELEASE_ASSERT(bytes % 8 == 0);

#if USE(JSVALUE64)
    auto slowPathForwardMemcpy = [&] {
        size_t count = bytes / 8;
        for (unsigned i = 0; i < count; ++i)
            std::bit_cast<volatile uint64_t*>(dst)[i] = std::bit_cast<volatile uint64_t*>(src)[i];
    };

    if (bytes <= smallCutoff) {
        slowPathForwardMemcpy();
        return;
    }

#if CPU(X86_64)
    size_t alignedBytes = (bytes / 64) * 64;
    size_t offset = 0;
    __asm__ volatile(
        ".balign 32\t\n"
        "1:\t\n"
        "cmpq %q[offset], %q[alignedBytes]\t\n"
        "je 2f\t\n"
        "movups (%q[src], %q[offset], 1), %%xmm0\t\n"
        "movups 16(%q[src], %q[offset], 1), %%xmm1\t\n"
        "movups 32(%q[src], %q[offset], 1), %%xmm2\t\n"
        "movups 48(%q[src], %q[offset], 1), %%xmm3\t\n"
        "movups %%xmm0, (%q[dst], %q[offset], 1)\t\n"
        "movups %%xmm1, 16(%q[dst], %q[offset], 1)\t\n"
        "movups %%xmm2, 32(%q[dst], %q[offset], 1)\t\n"
        "movups %%xmm3, 48(%q[dst], %q[offset], 1)\t\n"
        "addq $64, %q[offset]\t\n"
        "jmp 1b\t\n"

        "2:\t\n"
        "cmpq %q[offset], %q[bytes]\t\n"
        "je 3f\t\n"
        "movq (%q[src], %q[offset], 1), %%xmm0\t\n"
        "movq %%xmm0, (%q[dst], %q[offset], 1)\t\n"
        "addq $8, %q[offset]\t\n"
        "jmp 2b\t\n"

        "3:\t\n"

        : [offset] "+r" (offset)
        : [alignedBytes] "r" (alignedBytes), [bytes] "r" (bytes), [dst] "r" (dst), [src] "r" (src)
        : "xmm0", "xmm1", "xmm2", "xmm3", "memory", "cc"
    );
#elif CPU(ARM64)
    uint64_t alignedBytes = (static_cast<uint64_t>(bytes) / 64) * 64;

    uint64_t dstPtr = static_cast<uint64_t>(std::bit_cast<uintptr_t>(dst));
    uint64_t srcPtr = static_cast<uint64_t>(std::bit_cast<uintptr_t>(src));
    uint64_t end = dstPtr + bytes;
    uint64_t alignedEnd = dstPtr + alignedBytes;

    __asm__ volatile(
        "1:\t\n"
        "cmp %x[dstPtr], %x[alignedEnd]\t\n"
        "b.eq 2f\t\n"

        "ldp q0, q1, [%x[srcPtr]], #0x20\t\n"
        "ldp q2, q3, [%x[srcPtr]], #0x20\t\n"
        "stp q0, q1, [%x[dstPtr]], #0x20\t\n"
        "stp q2, q3, [%x[dstPtr]], #0x20\t\n"
        "b 1b\t\n"

        "2:\t\n"
        "cmp %x[dstPtr], %x[end]\t\n"
        "b.eq 3f\t\n"
        "ldr d0, [%x[srcPtr]], #0x8\t\n"
        "str d0, [%x[dstPtr]], #0x8\t\n"
        "b 2b\t\n"

        "3:\t\n"
        : [dstPtr] "+r" (dstPtr), [srcPtr] "+r" (srcPtr)
        : [end] "r" (end), [alignedEnd] "r" (alignedEnd)
        : "v0", "v1", "v2", "v3", "memory", "cc"
    );
#else
    slowPathForwardMemcpy();
#endif
#else
    memcpy(dst, src, bytes);
#endif // USE(JSVALUE64)
}

template <typename T>
ALWAYS_INLINE void gcSafeMemmove(T* dst, const T* src, size_t bytes)
{
    static_assert(sizeof(T) == sizeof(JSValue));
    RELEASE_ASSERT(bytes % 8 == 0);
#if USE(JSVALUE64)
    if (std::bit_cast<uintptr_t>(src) >= std::bit_cast<uintptr_t>(dst)) {
        // This is written to do a forwards loop, so calling it is ok.
        gcSafeMemcpy(dst, src, bytes);
        return;
    }

    if ((static_cast<uint64_t>(std::bit_cast<uintptr_t>(src)) + static_cast<uint64_t>(bytes)) <= static_cast<uint64_t>(std::bit_cast<uintptr_t>(dst))) {
        gcSafeMemcpy(dst, src, bytes);
        return;
    }

    auto slowPathBackwardsMemmove = [&] {
        size_t count = bytes / 8;
        for (size_t i = count; i--; )
            std::bit_cast<volatile uint64_t*>(dst)[i] = std::bit_cast<volatile uint64_t*>(src)[i];
    };

    if (bytes <= smallCutoff) {
        slowPathBackwardsMemmove();
        return;
    }

#if CPU(X86_64)
    size_t alignedBytes = (bytes / 64) * 64;
    size_t tail = alignedBytes;
    __asm__ volatile(
        "2:\t\n"
        "cmpq %q[tail], %q[bytes]\t\n"
        "je 1f\t\n"
        "addq $-8, %q[bytes]\t\n"
        "movq (%q[src], %q[bytes], 1), %%xmm0\t\n"
        "movq %%xmm0, (%q[dst], %q[bytes], 1)\t\n"
        "jmp 2b\t\n"

        "1:\t\n"
        "test %q[alignedBytes], %q[alignedBytes]\t\n"
        "jz 3f\t\n"

        ".balign 32\t\n"
        "100:\t\n"

        "movups -64(%q[src], %q[alignedBytes], 1), %%xmm0\t\n"
        "movups -48(%q[src], %q[alignedBytes], 1), %%xmm1\t\n"
        "movups -32(%q[src], %q[alignedBytes], 1), %%xmm2\t\n"
        "movups -16(%q[src], %q[alignedBytes], 1), %%xmm3\t\n"
        "movups %%xmm0, -64(%q[dst], %q[alignedBytes], 1)\t\n"
        "movups %%xmm1, -48(%q[dst], %q[alignedBytes], 1)\t\n"
        "movups %%xmm2, -32(%q[dst], %q[alignedBytes], 1)\t\n"
        "movups %%xmm3, -16(%q[dst], %q[alignedBytes], 1)\t\n"
        "addq $-64, %q[alignedBytes]\t\n"
        "jnz 100b\t\n"

        "3:\t\n"

        : [alignedBytes] "+r" (alignedBytes), [bytes] "+r" (bytes)
        : [tail] "r" (tail), [dst] "r" (dst), [src] "r" (src)
        : "xmm0", "xmm1", "xmm2", "xmm3", "memory", "cc"
    );
#elif CPU(ARM64)
    uint64_t alignedBytes = (static_cast<uint64_t>(bytes) / 64) * 64;

    uint64_t dstPtr = static_cast<uint64_t>(std::bit_cast<uintptr_t>(dst) + static_cast<uint64_t>(bytes));
    uint64_t srcPtr = static_cast<uint64_t>(std::bit_cast<uintptr_t>(src) + static_cast<uint64_t>(bytes));

    uint64_t alignedStart = std::bit_cast<uintptr_t>(dst) + (static_cast<uint64_t>(bytes) - alignedBytes);
    uint64_t start = std::bit_cast<uintptr_t>(dst);

    __asm__ volatile(
        "1:\t\n"
        "cmp %x[dstPtr], %x[alignedStart]\t\n"
        "b.eq 2f\t\n"

        "ldp q2, q3, [%x[srcPtr], #-0x20]!\t\n"
        "ldp q0, q1, [%x[srcPtr], #-0x20]!\t\n"
        "stp q2, q3, [%x[dstPtr], #-0x20]!\t\n"
        "stp q0, q1, [%x[dstPtr], #-0x20]!\t\n"
        "b 1b\t\n"

        "2:\t\n"
        "cmp %x[dstPtr], %x[start]\t\n"
        "b.eq 3f\t\n"
        "ldr d0, [%x[srcPtr], #-0x8]!\t\n"
        "str d0, [%x[dstPtr], #-0x8]!\t\n"
        "b 2b\t\n"

        "3:\t\n"

        : [dstPtr] "+r" (dstPtr), [srcPtr] "+r" (srcPtr)
        : [alignedStart] "r" (alignedStart), [start] "r" (start)
        : "v0", "v1", "v2", "v3", "memory", "cc"
    );
#else
    slowPathBackwardsMemmove();
#endif // CPU(X86_64) || CPU(ARM64)
#else
    memmove(dst, src, bytes);
#endif // USE(JSVALUE64)
}

template <typename T>
ALWAYS_INLINE void gcSafeZeroMemory(T* dst, size_t bytes)
{
    static_assert(sizeof(T) == sizeof(JSValue));
    RELEASE_ASSERT(bytes % 8 == 0);
#if USE(JSVALUE64)
#if CPU(X86_64)
    size_t alignedBytes = (bytes / 64) * 64;
    size_t offset = 0;
    __asm__ volatile(
        "xorps %%xmm0, %%xmm0\t\n"

        ".balign 32\t\n"
        "1:\t\n"
        "cmpq %q[offset], %q[alignedBytes]\t\n"
        "je 2f\t\n"
        "movups %%xmm0, (%q[dst], %q[offset], 1)\t\n"
        "movups %%xmm0, 16(%q[dst], %q[offset], 1)\t\n"
        "movups %%xmm0, 32(%q[dst], %q[offset], 1)\t\n"
        "movups %%xmm0, 48(%q[dst], %q[offset], 1)\t\n"
        "addq $64, %q[offset]\t\n"
        "jmp 1b\t\n"

        "2:\t\n"
        "cmpq %q[offset], %q[bytes]\t\n"
        "je 3f\t\n"
        "movq %%xmm0, (%q[dst], %q[offset], 1)\t\n"
        "addq $8, %q[offset]\t\n"
        "jmp 2b\t\n"

        "3:\t\n"

        : [offset] "+r" (offset)
        : [alignedBytes] "r" (alignedBytes), [bytes] "r" (bytes), [dst] "r" (dst)
        : "xmm0", "memory", "cc"
    );
#elif CPU(ARM64)
    uint64_t alignedBytes = (static_cast<uint64_t>(bytes) / 64) * 64;
    uint64_t dstPtr = static_cast<uint64_t>(std::bit_cast<uintptr_t>(dst));
    uint64_t end = dstPtr + bytes;
    uint64_t alignedEnd = dstPtr + alignedBytes;

    __asm__ volatile(
        "movi v0.16b, #0\t\n"

#if !OS(WINDOWS)
        // On Windows ARM64, LLVM has a bug (llvm/llvm-project#47432) that causes a
        // fatal error "Failed to evaluate function length in SEH unwind info" when
        // inline assembly contains alignment directives like .p2align.
        ".p2align 4\t\n"
#endif
        "1:\t\n"
        "cmp %x[dstPtr], %x[alignedEnd]\t\n"
        "b.eq 2f\t\n"

        "stnp q0, q0, [%x[dstPtr]]\t\n"
        "stnp q0, q0, [%x[dstPtr], #0x20]\t\n"
        "add %x[dstPtr], %x[dstPtr], #0x40\t\n"
        "b 1b\t\n"

        "2:\t\n"
        "cmp %x[dstPtr], %x[end]\t\n"
        "b.eq 3f\t\n"

        "str d0, [%x[dstPtr]], #0x8\t\n"
        "b 2b\t\n"

        "3:\t\n"

        : [dstPtr] "+r" (dstPtr)
        : [end] "r" (end), [alignedEnd] "r" (alignedEnd)
        : "v0", "memory", "cc"
    );
#else
    size_t count = bytes / 8;
    for (size_t i = 0; i < count; ++i)
        std::bit_cast<volatile uint64_t*>(dst)[i] = 0;
#endif
#else
    memset(reinterpret_cast<char*>(dst), 0, bytes);
#endif // USE(JSVALUE64)
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
