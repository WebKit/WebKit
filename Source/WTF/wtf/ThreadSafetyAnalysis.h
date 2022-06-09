/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

// See <https://clang.llvm.org/docs/ThreadSafetyAnalysis.html> for details.

#if COMPILER(CLANG)
#define WTF_THREAD_ANNOTATION_ATTRIBUTE(x)  __attribute__((x))
#else
#define WTF_THREAD_ANNOTATION_ATTRIBUTE(x)
#endif

#define WTF_ACQUIRES_LOCK_IF(...) WTF_THREAD_ANNOTATION_ATTRIBUTE(try_acquire_capability(__VA_ARGS__))
#define WTF_ACQUIRES_LOCK(...) WTF_THREAD_ANNOTATION_ATTRIBUTE(acquire_capability(__VA_ARGS__))
#define WTF_ACQUIRES_SHARED_LOCK_IF(...) WTF_THREAD_ANNOTATION_ATTRIBUTE(try_acquire_shared_capability(__VA_ARGS__))
#define WTF_ACQUIRES_SHARED_LOCK(...) WTF_THREAD_ANNOTATION_ATTRIBUTE(acquire_shared_capability(__VA_ARGS__))
#define WTF_ASSERTS_ACQUIRED_LOCK(x) WTF_THREAD_ANNOTATION_ATTRIBUTE(assert_capability(x))
#define WTF_ASSERTS_ACQUIRED_SHARED_LOCK(x) WTF_THREAD_ANNOTATION_ATTRIBUTE(assert_shared_capability(x))
#define WTF_CAPABILITY_LOCK WTF_THREAD_ANNOTATION_ATTRIBUTE(capability("mutex"))
#define WTF_CAPABILITY_SCOPED_LOCK WTF_THREAD_ANNOTATION_ATTRIBUTE(scoped_lockable)
#define WTF_EXCLUDES_LOCK(...) WTF_THREAD_ANNOTATION_ATTRIBUTE(locks_excluded(__VA_ARGS__))
#define WTF_GUARDED_BY_LOCK(x) WTF_THREAD_ANNOTATION_ATTRIBUTE(guarded_by(x))
#define WTF_IGNORES_THREAD_SAFETY_ANALYSIS WTF_THREAD_ANNOTATION_ATTRIBUTE(no_thread_safety_analysis)
#define WTF_POINTEE_GUARDED_BY_LOCK(x) WTF_THREAD_ANNOTATION_ATTRIBUTE(pt_guarded_by(x))
#define WTF_RELEASES_LOCK_GENERIC(...) WTF_THREAD_ANNOTATION_ATTRIBUTE(release_generic_capability(__VA_ARGS__))
#define WTF_RELEASES_LOCK(...) WTF_THREAD_ANNOTATION_ATTRIBUTE(release_capability(__VA_ARGS__))
#define WTF_RELEASES_SHARED_LOCK(...) WTF_THREAD_ANNOTATION_ATTRIBUTE(release_shared_capability(__VA_ARGS__))
#define WTF_REQUIRES_LOCK(...) WTF_THREAD_ANNOTATION_ATTRIBUTE(requires_capability(__VA_ARGS__))
#define WTF_REQUIRES_SHARED_LOCK(...) WTF_THREAD_ANNOTATION_ATTRIBUTE(requires_shared_capability(__VA_ARGS__))
#define WTF_RETURNS_LOCK(x) WTF_THREAD_ANNOTATION_ATTRIBUTE(lock_returned(x))
