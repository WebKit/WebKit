//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// debug.h: Debugging utilities.

#ifndef COMMON_DEBUG_H_
#define COMMON_DEBUG_H_

#include <assert.h>
#include <stdio.h>
#include <string>

#include "common/angleutils.h"

#if !defined(TRACE_OUTPUT_FILE)
#define TRACE_OUTPUT_FILE "angle_debug.txt"
#endif

namespace gl
{

enum MessageType
{
    MESSAGE_TRACE,
    MESSAGE_FIXME,
    MESSAGE_ERR,
    MESSAGE_EVENT,
};

// Outputs text to the debugging log, or the debugging window
void trace(bool traceInDebugOnly, MessageType messageType, const char *format, ...);

// Pairs a D3D begin event with an end event.
class ScopedPerfEventHelper : angle::NonCopyable
{
  public:
    ScopedPerfEventHelper(const char* format, ...);
    ~ScopedPerfEventHelper();
};

// Wraps the D3D9/D3D11 debug annotation functions.
class DebugAnnotator : angle::NonCopyable
{
  public:
    DebugAnnotator() { };
    virtual ~DebugAnnotator() { };
    virtual void beginEvent(const wchar_t *eventName) = 0;
    virtual void endEvent() = 0;
    virtual void setMarker(const wchar_t *markerName) = 0;
    virtual bool getStatus() = 0;
};

void InitializeDebugAnnotations(DebugAnnotator *debugAnnotator);
void UninitializeDebugAnnotations();
bool DebugAnnotationsActive();

}  // namespace gl

#if defined(ANGLE_ENABLE_DEBUG_TRACE) || defined(ANGLE_ENABLE_DEBUG_ANNOTATIONS)
#define ANGLE_TRACE_ENABLED
#endif

#define ANGLE_EMPTY_STATEMENT for (;;) break
#if !defined(NDEBUG) || defined(ANGLE_ENABLE_RELEASE_ASSERTS)
#define ANGLE_ENABLE_ASSERTS
#endif

// A macro to output a trace of a function call and its arguments to the debugging log
#if defined(ANGLE_TRACE_ENABLED)
#define TRACE(message, ...) gl::trace(true, gl::MESSAGE_TRACE, "trace: %s(%d): " message "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define TRACE(message, ...) (void(0))
#endif

// A macro to output a function call and its arguments to the debugging log, to denote an item in need of fixing.
#if defined(ANGLE_TRACE_ENABLED)
#define FIXME(message, ...) gl::trace(false, gl::MESSAGE_FIXME, "fixme: %s(%d): " message "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define FIXME(message, ...) (void(0))
#endif

// A macro to output a function call and its arguments to the debugging log, in case of error.
#if defined(ANGLE_TRACE_ENABLED) || defined(ANGLE_ENABLE_ASSERTS)
#define ERR(message, ...) gl::trace(false, gl::MESSAGE_ERR, "err: %s(%d): " message "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define ERR(message, ...) (void(0))
#endif

// A macro to log a performance event around a scope.
#if defined(ANGLE_TRACE_ENABLED)
#if defined(_MSC_VER)
#define EVENT(message, ...) gl::ScopedPerfEventHelper scopedPerfEventHelper ## __LINE__("%s" message "\n", __FUNCTION__, __VA_ARGS__);
#else
#define EVENT(message, ...) gl::ScopedPerfEventHelper scopedPerfEventHelper("%s" message "\n", __FUNCTION__, ##__VA_ARGS__);
#endif // _MSC_VER
#else
#define EVENT(message, ...) (void(0))
#endif

#if defined(ANGLE_TRACE_ENABLED)
#undef ANGLE_TRACE_ENABLED
#endif

#if defined(COMPILER_GCC) || defined(__clang__)
#define ANGLE_CRASH() __builtin_trap()
#else
#define ANGLE_CRASH() ((void)(*(volatile char *)0 = 0))
#endif

#if !defined(NDEBUG)
#define ANGLE_ASSERT_IMPL(expression) assert(expression)
#else
// TODO(jmadill): Detect if debugger is attached and break.
#define ANGLE_ASSERT_IMPL(expression) ANGLE_CRASH()
#endif  // !defined(NDEBUG)

// A macro asserting a condition and outputting failures to the debug log
#if defined(ANGLE_ENABLE_ASSERTS)
#define ASSERT(expression)                                                                        \
    (expression ? static_cast<void>(0)                                                            \
                : (ERR("\t! Assert failed in %s(%d): %s\n", __FUNCTION__, __LINE__, #expression), \
                   ANGLE_ASSERT_IMPL(expression)))
#define UNUSED_ASSERTION_VARIABLE(variable)
#else
#define ASSERT(expression) (void(0))
#define UNUSED_ASSERTION_VARIABLE(variable) ((void)variable)
#endif

#define UNUSED_VARIABLE(variable) ((void)variable)

// A macro to indicate unimplemented functionality

#if defined (ANGLE_TEST_CONFIG)
#define NOASSERT_UNIMPLEMENTED 1
#endif

// This will allow us to test with some automated test suites (eg dEQP) without crashing
#ifndef NOASSERT_UNIMPLEMENTED
#define NOASSERT_UNIMPLEMENTED 0
#endif

#define UNIMPLEMENTED()                                               \
    {                                                                 \
        FIXME("\t! Unimplemented: %s(%d)\n", __FUNCTION__, __LINE__); \
        ASSERT(NOASSERT_UNIMPLEMENTED);                               \
    }                                                                 \
    ANGLE_EMPTY_STATEMENT

// A macro for code which is not expected to be reached under valid assumptions
#define UNREACHABLE() \
    (ERR("\t! Unreachable reached: %s(%d)\n", __FUNCTION__, __LINE__), ASSERT(false))

#endif   // COMMON_DEBUG_H_
