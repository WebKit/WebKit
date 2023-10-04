/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

namespace WTF {
class PrintStream;
class String;
}

namespace WGSL {

#define ENUM_AddressSpace(value) \
    value(Function, function) \
    value(Handle, handle) \
    value(Private, private) \
    value(Storage, storage) \
    value(Uniform, uniform) \
    value(Workgroup, workgroup) \

#define ENUM_AccessMode(value) \
    value(Read, read) \
    value(ReadWrite, read_write) \
    value(Write, write) \

#define ENUM_TexelFormat(value) \
    value(BGRA8unorm, bgra8unorm) \
    value(R32float, r32float) \
    value(R32sint, r32sint) \
    value(R32uint, r32uint) \
    value(RG32float, rg32float) \
    value(RG32sint, rg32sint) \
    value(RG32uint, rg32uint) \
    value(RGBA16float, rgba16float) \
    value(RGBA16sint, rgba16sint) \
    value(RGBA16uint, rgba16uint) \
    value(RGBA32float, rgba32float) \
    value(RGBA32sint, rgba32sint) \
    value(RGBA32uint, rgba32uint) \
    value(RGBA8sint, rgba8sint) \
    value(RGBA8snorm, rgba8snorm) \
    value(RGBA8uint, rgba8uint) \
    value(RGBA8unorm, rgba8unorm) \

#define ENUM_DECLARE_VALUE(__value, _) \
    __value,

#define ENUM_DECLARE_PRINT_INTERNAL(__name) \
    void printInternal(WTF::PrintStream& out, __name)

#define ENUM_DECLARE_PARSE(__name) \
    const __name* parse##__name(const WTF::String&)

#define ENUM_DECLARE(__name) \
    enum class __name : uint8_t { \
    ENUM_##__name(ENUM_DECLARE_VALUE) \
    }; \
    ENUM_DECLARE_PRINT_INTERNAL(__name); \
    ENUM_DECLARE_PARSE(__name);

ENUM_DECLARE(AddressSpace);
ENUM_DECLARE(AccessMode);
ENUM_DECLARE(TexelFormat);

#undef ENUM_DECLARE

AccessMode defaultAccessModeForAddressSpace(AddressSpace);

} // namespace WGSL
