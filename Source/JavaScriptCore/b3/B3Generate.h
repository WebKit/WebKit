/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef B3Generate_h
#define B3Generate_h

#if ENABLE(B3_JIT)

namespace JSC {

class CCallHelpers;

namespace B3 {

class Procedure;
namespace Air { class Code; }

// This takes a B3::Procedure, optimizes it in-place, and generates it to machine code by first
// internally converting to an Air::Code and then generating that.
JS_EXPORT_PRIVATE void generate(Procedure&, CCallHelpers&);

// This takes a B3::Procedure, optimizes it in-place, and lowers it to Air. You can then generate
// the Air to machine code using Air::generate(). Note that an Air::Code will have pointers into the
// B3::Procedure, so you need to ensure that the B3::Procedure outlives the Air::Code.
void generateToAir(Procedure&, Air::Code&);

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

#endif // B3Generate_h

