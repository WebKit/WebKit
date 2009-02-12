/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "RandomNumber.h"

#include "RandomNumberSeed.h"

#include <limits>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

namespace WTF {

double randomNumber()
{
#if !ENABLE(JSC_MULTIPLE_THREADS)
    static bool s_initialized = false;
    if (!s_initialized) {
        initializeRandomNumberGenerator();
        s_initialized = true;
    }
#endif
    
    uint64_t fullRandom;
#if COMPILER(MSVC) && defined(_CRT_RAND_S)
    uint32_t part1;
    rand_s(&part1);
    fullRandom = part1;
#elif PLATFORM(DARWIN)
    fullRandom = arc4random();
#elif PLATFORM(UNIX)
    uint32_t part1 = random() & (RAND_MAX - 1);
    uint32_t part2 = random() & (RAND_MAX - 1);
    // random only provides 31 bits
    fullRandom = part1;
    fullRandom <<= 31;
    fullRandom |= part2;
#else
    uint32_t part1 = rand() & (RAND_MAX - 1);
    uint32_t part2 = rand() & (RAND_MAX - 1);
    // rand only provides 31 bits, and the low order bits of that aren't very random
    // so we take the high 26 bits of part 1, and the high 27 bits of part2.
    part1 >>= 5; // drop the low 5 bits
    part2 >>= 4; // drop the low 4 bits
    fullRandom = part1;
    fullRandom <<= 27;
    fullRandom |= part2;
#endif
    // Mask off the low 53bits
    fullRandom &= (1LL << 53) - 1;
    return static_cast<double>(fullRandom)/static_cast<double>(1LL << 53);
}

}
