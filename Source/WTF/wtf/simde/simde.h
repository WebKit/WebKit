/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Compiler.h>
#include <wtf/Platform.h>

#if !CPU(ARM64)
#ifndef SIMDE_ARM_NEON_A32V8_NO_NATIVE
#define SIMDE_ARM_NEON_A32V8_NO_NATIVE 1
#endif
#endif

// Workaround: neon.h contains MSVC-specific workarounds guarded by _MSC_VER < 1938
// that use intrinsics not available in clang-cl. Temporarily set _MSC_VER >= 1938
// to skip those workarounds when building with clang-cl.
// https://developercommunity.visualstudio.com/t/In-arm64_neonh-vsqaddb_u8-vsqaddh_u16/10271747
#if defined(_MSC_VER) && defined(__clang__)
#pragma push_macro("_MSC_VER")
#undef _MSC_VER
#define _MSC_VER 1938
#endif

IGNORE_WARNINGS_BEGIN("constant-conversion")
IGNORE_WARNINGS_BEGIN("uninitialized")
#include <wtf/simde/arm/neon.h>
#include <wtf/simde/arm/sve.h>
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END

#if defined(_MSC_VER) && defined(__clang__)
#pragma pop_macro("_MSC_VER")
#endif
