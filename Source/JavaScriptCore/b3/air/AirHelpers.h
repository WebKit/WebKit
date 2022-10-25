/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#include "AirOpcode.h"

namespace JSC::B3::Air {

inline Air::Opcode moveForType(Type type)
{
    switch (type.kind()) {
    case Int32:
        return Move32;
    case Int64:
        RELEASE_ASSERT(is64Bit());
        return Move;
    case Float:
        return MoveFloat;
    case Double:
        return MoveDouble;
    case V128:
        ASSERT(Options::useWebAssemblySIMD());
        return MoveVector;
    case Void:
    case Tuple:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return Air::Oops;
}

inline Air::Opcode relaxedMoveForType(Type type)
{
    switch (type.kind()) {
    case Int32:
    case Int64:
        // For Int32, we could return Move or Move32. It's a trade-off.
        //
        // Move32: Using Move32 guarantees that we use the narrower move, but in cases where the
        //     register allocator can't prove that the variables involved are 32-bit, this will
        //     disable coalescing.
        //
        // Move: Using Move guarantees that the register allocator can coalesce normally, but in
        //     cases where it can't prove that the variables are 32-bit and it doesn't coalesce,
        //     this will force us to use a full 64-bit Move instead of the slightly cheaper
        //     32-bit Move32.
        //
        // Coalescing is a lot more profitable than turning Move into Move32. So, it's better to
        // use Move here because in cases where the register allocator cannot prove that
        // everything is 32-bit, we still get coalescing.
        return Move;
    case Float:
        // MoveFloat is always coalescable and we never convert MoveDouble to MoveFloat, so we
        // should use MoveFloat when we know that the temporaries involved are 32-bit.
        return MoveFloat;
    case Double:
        return MoveDouble;
    case V128:
        return MoveVector;
    case Void:
    case Tuple:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return Air::Oops;
}

} // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
