/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "CPU.h"
#include "Options.h"

namespace JSC { namespace B3 { namespace Air {

class Code;

// This implements the Poletto and Sarkar register allocator called "linear scan":
// http://dl.acm.org/citation.cfm?id=330250
//
// This is not Air's primary register allocator. We use it only when running at optLevel<2.
// That's not the default level. This register allocator is optimized primarily for running
// quickly. It's expected that improvements to this register allocator should focus on improving
// its execution time without much regard for the quality of generated code. If you want good
// code, use graph coloring.
//
// For Air's primary register allocator, see AirAllocateRegistersByGraphColoring.h|cpp.
//
// This also does stack allocation as an afterthought. It does not do any spill coalescing.
void allocateRegistersAndStackByLinearScan(Code&);

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
