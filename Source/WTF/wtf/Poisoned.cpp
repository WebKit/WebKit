/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include <wtf/Poisoned.h>

#include <wtf/CryptographicallyRandomNumber.h>

namespace WTF {

uintptr_t makePoison()
{
    uintptr_t poison = 0;
#if ENABLE(POISON)
    do {
        poison = cryptographicallyRandomNumber();
        poison = (poison << 16) ^ (static_cast<uintptr_t>(cryptographicallyRandomNumber()) << 3);
    } while (!(poison >> 32)); // Ensure that bits 32-47 are not completely 0.

    // Ensure that the poisoned bits (pointer ^ poison) looks like a valid pointer.
    RELEASE_ASSERT(poison && !(poison >> 48));

    // Ensure that the poisoned bits (pointer ^ poison) cannot be 0. This is so that the poisoned
    // bits can also be used for a normal zero check without needing to be decoded first.
    poison |= (1 << 2);

    // Ensure that the bottom 2 alignment bits are still 0 so that the poisoned bits will
    // still preserve the properties of a pointer where these bits are expected to be 0.
    // This allows the poisoned bits to be used in place of the pointer by clients that
    // rely on this property of pointers and sets flags in the low bits.
    // Note: a regular pointer has 3 alignment bits, but the poisoned bits need to use one
    // (see above). Hence, clients can only use 2 bits for flags.
    RELEASE_ASSERT(!(poison & 0x3));
#endif
    return poison;
}

} // namespace WTF
