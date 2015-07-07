/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GCAssertions_h
#define GCAssertions_h

#include <type_traits>
#include <wtf/Assertions.h>

#if ENABLE(GC_VALIDATION)
#define ASSERT_GC_OBJECT_LOOKS_VALID(cell) do { \
    RELEASE_ASSERT(cell);\
    RELEASE_ASSERT(cell->structure()->structure() == cell->structure()->structure()->structure()); \
} while (0)

#define ASSERT_GC_OBJECT_INHERITS(object, classInfo) do {\
    ASSERT_GC_OBJECT_LOOKS_VALID(object); \
    RELEASE_ASSERT(object->inherits(classInfo)); \
} while (0)

#else
#define ASSERT_GC_OBJECT_LOOKS_VALID(cell) do { (void)cell; } while (0)
#define ASSERT_GC_OBJECT_INHERITS(object, classInfo) do { (void)object; (void)classInfo; } while (0)
#endif

#if COMPILER(CLANG)
#define STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(klass) static_assert(std::is_trivially_destructible<klass>::value, #klass " must have a trivial destructor")
#elif COMPILER(MSVC)
// An earlier verison of the C++11 spec used to call this type trait std::has_trivial_destructor, and that's what MSVC uses.
#define STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(klass) static_assert(std::has_trivial_destructor<klass>::value, #klass " must have a trivial destructor")
#else
// This is not enabled on GCC due to http://gcc.gnu.org/bugzilla/show_bug.cgi?id=52702
#define STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(klass)
#endif

#endif // GCAssertions_h
