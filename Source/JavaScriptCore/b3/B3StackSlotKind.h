/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#ifndef B3StackSlotKind_h
#define B3StackSlotKind_h

#if ENABLE(B3_JIT)

namespace JSC { namespace B3 {

enum class StackSlotKind : uint8_t {
    // A locked stack slot needs to be kept disjoint from all others because we intend to escape
    // it even if that's not obvious. This happens when the runtime takes the address of a stack
    // slot.
    Locked,

    // These stack slots behave like variables. Undefined behavior happens if you store less than
    // the width of the slot.
    Anonymous

    // FIXME: We should add a third mode, which means that the stack slot will be read asynchronously
    // as with Locked, but never written to asynchronously. Then, Air could optimize spilling and
    // filling by tracking whether the value had been stored to a read-only locked slot. If it had,
    // then we can refill from that slot.
    // https://bugs.webkit.org/show_bug.cgi?id=150587
};

} } // namespace JSC::B3

namespace WTF {

class PrintStream;

void printInternal(PrintStream&, JSC::B3::StackSlotKind);

} // namespace WTF

#endif // ENABLE(B3_JIT)

#endif // B3StackSlotKind_h

