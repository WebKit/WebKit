//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// debug.h: Debugging utilities.

#ifndef COMMON_DEBUG_H_
#define COMMON_DEBUG_H_

#include <stdio.h>
#include <assert.h>

namespace gl
{
    // Outputs text to the debugging log
    void trace(const char *format, ...);
}

// A macro to output a trace of a function call and its arguments to the debugging log
#if !defined(NDEBUG) && !defined(ANGLE_DISABLE_TRACE) 
    #define TRACE(message, ...) gl::trace("trace: %s"message"\n", __FUNCTION__, __VA_ARGS__)
#else
    #define TRACE(...) ((void)0)
#endif

// A macro to output a function call and its arguments to the debugging log, to denote an item in need of fixing. Will occur even in release mode.
#define FIXME(message, ...) gl::trace("fixme: %s"message"\n", __FUNCTION__, __VA_ARGS__)

// A macro to output a function call and its arguments to the debugging log, in case of error. Will occur even in release mode.
#define ERR(message, ...) gl::trace("err: %s"message"\n", __FUNCTION__, __VA_ARGS__)

// A macro asserting a condition and outputting failures to the debug log
#define ASSERT(expression) do { \
    if(!(expression)) \
        ERR("\t! Assert failed in %s(%d): "#expression"\n", __FUNCTION__, __LINE__); \
    assert(expression); \
    } while(0)


// A macro to indicate unimplemented functionality
#ifndef NDEBUG
    #define UNIMPLEMENTED() do { \
        FIXME("\t! Unimplemented: %s(%d)\n", __FUNCTION__, __LINE__); \
        assert(false); \
        } while(0)
#else
    #define UNIMPLEMENTED() FIXME("\t! Unimplemented: %s(%d)\n", __FUNCTION__, __LINE__)
#endif

// A macro for code which is not expected to be reached under valid assumptions
#ifndef NDEBUG
    #define UNREACHABLE() do { \
        ERR("\t! Unreachable reached: %s(%d)\n", __FUNCTION__, __LINE__); \
        assert(false); \
        } while(0)
#else
    #define UNREACHABLE() ERR("\t! Unreachable reached: %s(%d)\n", __FUNCTION__, __LINE__)
#endif

// A macro functioning as a compile-time assert to validate constant conditions
#define META_ASSERT(condition) typedef int COMPILE_TIME_ASSERT_##__LINE__[static_cast<bool>(condition)?1:-1]

#endif   // COMMON_DEBUG_H_
