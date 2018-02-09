/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include <wtf/StdLibExtras.h>

namespace WTF {

template<typename T>
void fastZeroFill(T* dst, size_t length)
{
#if CPU(X86_64) && COMPILER(GCC_OR_CLANG)
    uint64_t zero = 0;
    size_t count = length * sizeof(T);
    if (!(sizeof(T) % sizeof(uint64_t))) {
        asm volatile (
            "cmpq $200, %%rcx\n\t"
            "jb 1f\n\t"
            "shrq $3, %%rcx\n\t"
            "rep stosq\n\t"
            "jmp 2f\n\t"
            "3:\n\t"
            "movq %%rax, (%%rdi, %%rcx)\n\t"
            "1:\n\t"
            "subq $8, %%rcx\n\t"
            "jae 3b\n\t"
            "2:\n\t"
            : "+D"(dst), "+c"(count)
            : "a"(zero)
            : "memory"
            );
        return;
    }
    if (!(sizeof(T) % sizeof(uint32_t))) {
        asm volatile (
            "cmpq $200, %%rcx\n\t"
            "jb 1f\n\t"
            "shrq $2, %%rcx\n\t"
            "rep stosl\n\t"
            "jmp 2f\n\t"
            "3:\n\t"
            "movq %%rax, (%%rdi, %%rcx)\n\t"
            "1:\n\t"
            "subq $8, %%rcx\n\t"
            "jae 3b\n\t"
            "cmpq $-8, %%rcx\n\t"
            "je 2f\n\t"
            "addq $4, %%rcx\n\t" // FIXME: This isn't really a loop. https://bugs.webkit.org/show_bug.cgi?id=182617
            "4:\n\t"
            "movl %%eax, (%%rdi, %%rcx)\n\t"
            "subq $4, %%rcx\n\t"
            "jae 4b\n\t"
            "2:\n\t"
            : "+D"(dst), "+c"(count)
            : "a"(zero)
            : "memory"
            );
        return;
    }
    if (!(sizeof(T) % sizeof(uint16_t))) {
        asm volatile (
            "cmpq $200, %%rcx\n\t"
            "jb 1f\n\t"
            "shrq $1, %%rcx\n\t"
            "rep stosw\n\t"
            "jmp 2f\n\t"
            "3:\n\t"
            "movq %%rax, (%%rdi, %%rcx)\n\t"
            "1:\n\t"
            "subq $8, %%rcx\n\t"
            "jae 3b\n\t"
            "cmpq $-8, %%rcx\n\t"
            "je 2f\n\t"
            "addq $6, %%rcx\n\t"
            "4:\n\t"
            "movw %%ax, (%%rdi, %%rcx)\n\t"
            "subq $2, %%rcx\n\t"
            "jae 4b\n\t"
            "2:\n\t"
            : "+D"(dst), "+c"(count)
            : "a"(zero)
            : "memory"
            );
        return;
    }
    asm volatile (
        "cmpq $200, %%rcx\n\t"
        "jb 1f\n\t"
        "rep stosb\n\t"
        "jmp 2f\n\t"
        "3:\n\t"
        "movq %%rax, (%%rdi, %%rcx)\n\t"
        "1:\n\t"
        "subq $8, %%rcx\n\t"
        "jae 3b\n\t"
        "cmpq $-8, %%rcx\n\t"
        "je 2f\n\t"
        "addq $7, %%rcx\n\t"
        "4:\n\t"
        "movb %%al, (%%rdi, %%rcx)\n\t"
        "sub $1, %%rcx\n\t"
        "jae 4b\n\t"
        "2:\n\t"
        : "+D"(dst), "+c"(count)
        : "a"(zero)
        : "memory"
        );
#else
    memset(dst, 0, length * sizeof(T));
#endif
}

inline void fastZeroFillBytes(void* dst, size_t bytes)
{
    fastZeroFill(static_cast<char*>(dst), bytes);
}

} // namespace WTF

using WTF::fastZeroFill;
using WTF::fastZeroFillBytes;
