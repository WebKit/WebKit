/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <wtf/Compiler.h>

// FIXME: Remove this header when <rdar://problem/56654773> is fixed.

#if TSAN_ENABLED

WTF_EXTERN_C_BEGIN
void AnnotateHappensBefore(const char* file, int line, const void* addr);
void AnnotateHappensAfter(const char* file, int line, const void* addr);
WTF_EXTERN_C_END

#define TSAN_ANNOTATE_HAPPENS_BEFORE(addr) \
    do { \
        AnnotateHappensBefore(__FILE__, __LINE__, addr); \
    } while (0)
#define TSAN_ANNOTATE_HAPPENS_AFTER(addr)  \
    do { \
        AnnotateHappensAfter(__FILE__, __LINE__, addr); \
    } while (0)

#else

#define TSAN_ANNOTATE_HAPPENS_BEFORE(addr) ((void)0)
#define TSAN_ANNOTATE_HAPPENS_AFTER(addr)  ((void)0)

#endif // TSAN_ENABLED
