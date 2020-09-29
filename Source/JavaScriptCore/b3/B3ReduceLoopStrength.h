/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

namespace JSC { namespace B3 {

class Procedure;

// Apply transformations that match and simplify entire loops.

bool reduceLoopStrength(Procedure&);



#if CPU(X86_64)
/*
 * The semantics of B3 require that loads and stores are not reduced in granularity.
 * If we would use system memcpy, then it is possible that we would get a byte copy loop,
 * and this could cause GC tearing.
 */
ALWAYS_INLINE void fastForwardCopy32(uint32_t* dst, const uint32_t* src, size_t size)
{
    uint64_t i;
    int64_t counter;
    uint64_t tmp, tmp2;

    asm volatile(
        "test %q[size], %q[size]\t\n"
        "jz 900f\t\n" // exit
        "xorq %q[i], %q[i]\t\n"
        // if size < 8
        "cmp $8, %q[size]\t\n"
        "jl 100f\t\n" // backporch
        // if (dst + size*4 <= src || src + size*4 <= dst)
        "leaq (%q[src], %q[size], 4), %q[tmp]\t\n"
        "cmp %q[tmp], %q[dst]\t\n"
        "jae 500f\t\n" // simd no overlap
        "leaq (%q[dst], %q[size], 4), %q[tmp]\t\n"
        "cmp %q[tmp], %q[src]\t\n"
        "jae 500f\t\n" // simd no overlap
        "jmp 100f\t\n" // Backporch

        // Backporch (assumes we can write at least one int)
        "100:\t\n"
        // counter = (size - i)%4
        "mov %q[size], %q[counter]\t\n"
        "subq %q[i], %q[counter]\t\n"

        // If we are a multiple of 4, unroll the loop
        // We already know we are nonzero
        "and $3, %q[counter]\t\n"
        "jz 200f\t\n" // unrolled loop

        "negq %q[counter]\t\n"
        ".balign 32\t\n"
        "101:\t\n"
        "mov (%q[src], %q[i], 4), %k[tmp]\t\n"
        "mov %k[tmp], (%q[dst], %q[i], 4)\t\n"
        "incq %q[i]\t\n"
        "incq %q[counter]\t\n"
        "jnz 101b\t\n"
        // Do we still have anything left?
        "cmpq %q[size], %q[i]\t\n"
        "je 900f\t\n" // exit
        // Then go to the unrolled loop
        "jmp 200f\t\n"

        // Unrolled loop (assumes multiple of 4, can do one iteration)
        "200:\t\n"
        "shr $2, %q[counter]\t\n"
        "negq %q[counter]\t\n"
        ".balign 32\t\n"
        "201:\t\n"
        "mov (%q[src], %q[i], 4), %k[tmp]\t\n"
        "mov %k[tmp], (%q[dst], %q[i], 4)\t\n"
        "mov 4(%q[src], %q[i], 4), %k[tmp2]\t\n"
        "mov %k[tmp2], 4(%q[dst], %q[i], 4)\t\n"
        "mov 8(%q[src], %q[i], 4), %k[tmp2]\t\n"
        "mov %k[tmp2], 8(%q[dst], %q[i], 4)\t\n"
        "mov 12(%q[src], %q[i], 4), %k[tmp]\t\n"
        "mov %k[tmp], 12(%q[dst], %q[i], 4)\t\n"
        "addq $4, %q[i]\t\n"
        "cmpq %q[size], %q[i]\t\n"
        "jb 201b\t\n"
        "jmp 900f\t\n" // exit

        // simd no overlap
        // counter = -(size/8);
        "500:\t\n"
        "mov %q[size], %q[counter]\t\n"
        "shrq $3, %q[counter]\t\n"
        "negq %q[counter]\t\n"
        ".balign 32\t\n"
        "501:\t\n"
        "movups (%q[src], %q[i], 4), %%xmm0\t\n"
        "movups 16(%q[src], %q[i], 4), %%xmm1\t\n"
        "movups %%xmm0, (%q[dst], %q[i], 4)\t\n"
        "movups %%xmm1, 16(%q[dst], %q[i], 4)\t\n"
        "addq $8, %q[i]\t\n"
        "incq %q[counter]\t\n"
        "jnz 501b\t\n"
        // if (i == size)
        "cmpq %q[size], %q[i]\t\n"
        "je 900f\t\n" // end
        "jmp 100b\t\n" // backporch
        "900:\t\n"
    : [i] "=&c" (i), [counter] "=&r" (counter), [tmp] "=&a" (tmp),  [tmp2] "=&r" (tmp2)
    : [dst] "D" (dst), [src] "S" (src), [size] "r" (size)
    : "xmm0", "xmm1");
}

JSC_DECLARE_JIT_OPERATION(operationFastForwardCopy32, void, (uint32_t*, const uint32_t*, size_t));
#endif

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

