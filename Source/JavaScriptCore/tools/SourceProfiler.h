/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/SourceCode.h>

namespace JSC {
namespace SourceProfiler {

// Do not change the order of the enums in Type. If we have new types to add,
// just append at the end.
enum class Type {
    Program,
    Eval,
    Function,
    Module,
};

// Do not change the order of the enums in CharacterSize. In the unlikely case
// where we have new CharacterSizes, just append them to the end.
enum class CharacterSize {
    Char8Bit,
    Char16Bit
};

// If we need to change the layout of Payload in a backward incompatible manner,
// bump the majorVersion. If we're just appending new fields to the current shape
// of Payload, then just bump the minorVersion.
constexpr unsigned majorVersion = 1;
constexpr unsigned minorVersion = 0;

struct Payload {
    unsigned majorVersion;
    unsigned minorVersion;
    const void* sourceStr;
    size_t sourceLength;
    CharacterSize sourceCharSize;
    const void* urlStr;
    size_t urlLength;
    CharacterSize urlCharSize;
};

using ProfilerHook = void (*)(Type, const Payload* payload);

extern JS_EXPORT_PRIVATE ProfilerHook g_profilerHook;

void profile(Type, const SourceCode&);

} // namespace SourceProfiler
} // namespace JSC
