/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

namespace JSC {

namespace WASM {

// These should be in the order that we expect them to be in the binary.
#define FOR_EACH_WASM_SECTION_TYPE(macro) \
    macro(FunctionTypes, "type") \
    macro(Signatures, "function") \
    macro(Definitions, "code") \
    macro(End, "end")

struct Sections {
    enum Section {
#define CREATE_SECTION_ENUM(name, str) name,
        FOR_EACH_WASM_SECTION_TYPE(CREATE_SECTION_ENUM)
#undef CREATE_SECTION_ENUM
        Unknown
    };
    static Section lookup(const uint8_t*, unsigned);
    static bool validateOrder(Section previous, Section next)
    {
        // This allows unknown sections after End, which I doubt will ever be supported but
        // there is no reason to potentially break backwards compatability.
        if (previous == Unknown)
            return true;
        return previous < next;
    }
};

} // namespace WASM

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
