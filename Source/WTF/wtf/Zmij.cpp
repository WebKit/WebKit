/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"

// zmij's NEON and SSE4.1 paths use intrinsics that exist only on 64-bit targets.
#if !CPU(ARM64) && !CPU(X86_64)
#define ZMIJ_USE_SIMD 0
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
IGNORE_WARNINGS_BEGIN("unused-function")
IGNORE_WARNINGS_BEGIN("unused-parameter")
IGNORE_WARNINGS_BEGIN("unused-variable")
IGNORE_WARNINGS_BEGIN("cast-align")
IGNORE_CLANG_WARNINGS_BEGIN("comma")
IGNORE_CLANG_WARNINGS_BEGIN("constant-logical-operand")

#include "zmij/zmij.cc.h"

IGNORE_CLANG_WARNINGS_END
IGNORE_CLANG_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
