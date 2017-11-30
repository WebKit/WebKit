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

#include <ios>
#include <iomanip>
#include <sstream>
#include <string>

#include "common/angleutils.h"

#if !defined(TRACE_OUTPUT_FILE)
#define TRACE_OUTPUT_FILE "angle_debug.txt"
#endif

namespace gl
{

// Pairs a D3D begin event with an end event.
class ScopedPerfEventHelper : angle::NonCopyable
{
  public:
    ScopedPerfEventHelper(const char* format, ...);
    ~ScopedPerfEventHelper();
};

using LogSeverity = int;
// Note: the log severities are used to index into the array of names,
// see g_logSeverityNames.
constexpr LogSeverity LOG_EVENT          = 0;
constexpr LogSeverity LOG_WARN           = 1;
constexpr LogSeverity LOG_ERR            = 2;
constexpr LogSeverity LOG_NUM_SEVERITIES = 3;

void Trace(LogSeverity severity, const char *message);

// This class more or less represents a particular log message.  You
// create an instance of LogMessage and then stream stuff to it.
// When you finish streaming to it, ~LogMessage is called and the
// full message gets streamed to the appropriate destination.
//
// You shouldn't actually use LogMessage's constructor to log things,
// though.  You should use the ERR() and WARN() macros.
class LogMessage : angle::NonCopyable
{
  public:
    // Used for ANGLE_LOG(severity).
    LogMessage(const char *function, int line, LogSeverity severity);
    ~LogMessage();
    std::ostream &stream() { return mStream; }

    LogSeverity getSeverity() const;
    std::string getMessage() const;

  private:
    const char *mFunction;
    const int mLine;
    const LogSeverity mSeverity;

    std::ostringstream mStream;
};

// Wraps the D3D9/D3D11 debug annotation functions.
// Also handles redirecting logging destination.
class DebugAnnotator : angle::NonCopyable
{
  public:
    DebugAnnotator(){};
    virtual ~DebugAnnotator() { };
    virtual void beginEvent(const wchar_t *eventName) = 0;
    virtual void endEvent() = 0;
    virtual void setMarker(const wchar_t *markerName) = 0;
    virtual bool getStatus() = 0;
    // Log Message Handler that gets passed every log message,
    // when debug annotations are initialized,
    // replacing default handling by LogMessage.
    virtual void logMessage(const LogMessage &msg) const = 0;
};

void InitializeDebugAnnotations(DebugAnnotator *debugAnnotator);
void UninitializeDebugAnnotations();
bool DebugAnnotationsActive();
bool DebugAnnotationsInitialized();

namespace priv
{
// This class is used to explicitly ignore values in the conditional logging macros. This avoids
// compiler warnings like "value computed is not used" and "statement has no effect".
class LogMessageVoidify
{
  public:
    LogMessageVoidify() {}
    // This has to be an operator with a precedence lower than << but higher than ?:
    void operator&(std::ostream &) {}
};

// Used by ANGLE_LOG_IS_ON to lazy-evaluate stream arguments.
bool ShouldCreatePlatformLogMessage(LogSeverity severity);

template <int N, typename T>
std::ostream &FmtHex(std::ostream &os, T value)
{
    os << "0x";

    std::ios_base::fmtflags oldFlags = os.flags();
    std::streamsize oldWidth         = os.width();
    std::ostream::char_type oldFill  = os.fill();

    os << std::hex << std::uppercase << std::setw(N) << std::setfill('0') << value;

    os.flags(oldFlags);
    os.width(oldWidth);
    os.fill(oldFill);

    return os;
}
}  // namespace priv

#if defined(ANGLE_PLATFORM_WINDOWS)
class FmtHR
{
  public:
    explicit FmtHR(HRESULT hresult) : mHR(hresult) {}
  private:
    HRESULT mHR;
    friend std::ostream &operator<<(std::ostream &os, const FmtHR &fmt);
};

class FmtErr
{
  public:
    explicit FmtErr(DWORD err) : mErr(err) {}

  private:
    DWORD mErr;
    friend std::ostream &operator<<(std::ostream &os, const FmtErr &fmt);
};
#endif  // defined(ANGLE_PLATFORM_WINDOWS)

template <typename T>
std::ostream &FmtHexShort(std::ostream &os, T value)
{
    return priv::FmtHex<4>(os, value);
}

template <typename T>
std::ostream &FmtHexInt(std::ostream &os, T value)
{
    return priv::FmtHex<8>(os, value);
}

// A few definitions of macros that don't generate much code. These are used
// by ANGLE_LOG(). Since these are used all over our code, it's
// better to have compact code for these operations.
#define COMPACT_ANGLE_LOG_EX_EVENT(ClassName, ...) \
    ::gl::ClassName(__FUNCTION__, __LINE__, ::gl::LOG_EVENT, ##__VA_ARGS__)
#define COMPACT_ANGLE_LOG_EX_WARN(ClassName, ...) \
    ::gl::ClassName(__FUNCTION__, __LINE__, ::gl::LOG_WARN, ##__VA_ARGS__)
#define COMPACT_ANGLE_LOG_EX_ERR(ClassName, ...) \
    ::gl::ClassName(__FUNCTION__, __LINE__, ::gl::LOG_ERR, ##__VA_ARGS__)

#define COMPACT_ANGLE_LOG_EVENT COMPACT_ANGLE_LOG_EX_EVENT(LogMessage)
#define COMPACT_ANGLE_LOG_WARN COMPACT_ANGLE_LOG_EX_WARN(LogMessage)
#define COMPACT_ANGLE_LOG_ERR COMPACT_ANGLE_LOG_EX_ERR(LogMessage)

#define ANGLE_LOG_IS_ON(severity) (::gl::priv::ShouldCreatePlatformLogMessage(::gl::LOG_##severity))

// Helper macro which avoids evaluating the arguments to a stream if the condition doesn't hold.
// Condition is evaluated once and only once.
#define ANGLE_LAZY_STREAM(stream, condition) \
    !(condition) ? static_cast<void>(0) : ::gl::priv::LogMessageVoidify() & (stream)

// We use the preprocessor's merging operator, "##", so that, e.g.,
// ANGLE_LOG(EVENT) becomes the token COMPACT_ANGLE_LOG_EVENT.  There's some funny
// subtle difference between ostream member streaming functions (e.g.,
// ostream::operator<<(int) and ostream non-member streaming functions
// (e.g., ::operator<<(ostream&, string&): it turns out that it's
// impossible to stream something like a string directly to an unnamed
// ostream. We employ a neat hack by calling the stream() member
// function of LogMessage which seems to avoid the problem.
#define ANGLE_LOG_STREAM(severity) COMPACT_ANGLE_LOG_##severity.stream()

#define ANGLE_LOG(severity) ANGLE_LAZY_STREAM(ANGLE_LOG_STREAM(severity), ANGLE_LOG_IS_ON(severity))

}  // namespace gl

#if defined(ANGLE_ENABLE_DEBUG_TRACE) || defined(ANGLE_ENABLE_DEBUG_ANNOTATIONS)
#define ANGLE_TRACE_ENABLED
#endif

#define ANGLE_EMPTY_STATEMENT for (;;) break
#if !defined(NDEBUG) || defined(ANGLE_ENABLE_RELEASE_ASSERTS)
#define ANGLE_ENABLE_ASSERTS
#endif

#define WARN() ANGLE_LOG(WARN)
#define ERR() ANGLE_LOG(ERR)

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
#define ASSERT(expression)                                                                         \
    (expression ? static_cast<void>(0) : ((ERR() << "\t! Assert failed in " << __FUNCTION__ << "(" \
                                                 << __LINE__ << "): " << #expression),             \
                                          ANGLE_ASSERT_IMPL(expression)))
#else
// These are just dummy values.
#define COMPACT_ANGLE_LOG_EX_ASSERT(ClassName, ...) \
    COMPACT_ANGLE_LOG_EX_EVENT(ClassName, ##__VA_ARGS__)
#define COMPACT_ANGLE_LOG_ASSERT COMPACT_ANGLE_LOG_EVENT
namespace gl
{
constexpr LogSeverity LOG_ASSERT = LOG_EVENT;
}  // namespace gl

#define ASSERT(condition)                                                     \
    ANGLE_LAZY_STREAM(ANGLE_LOG_STREAM(ASSERT), false ? !(condition) : false) \
        << "Check failed: " #condition ". "
#endif  // defined(ANGLE_ENABLE_ASSERTS)

#define UNUSED_VARIABLE(variable) ((void)variable)

// A macro to indicate unimplemented functionality
#ifndef NOASSERT_UNIMPLEMENTED
#define NOASSERT_UNIMPLEMENTED 1
#endif

#if defined(ANGLE_TRACE_ENABLED) || defined(ANGLE_ENABLE_ASSERTS)
#define UNIMPLEMENTED()                                                                      \
    {                                                                                        \
        ERR() << "\t! Unimplemented: " << __FUNCTION__ << "(" << __FILE__ << ":" << __LINE__ \
              << ")";                                                                        \
        ASSERT(NOASSERT_UNIMPLEMENTED);                                                      \
    }                                                                                        \
    ANGLE_EMPTY_STATEMENT

// A macro for code which is not expected to be reached under valid assumptions
#define UNREACHABLE()                                                                            \
    ((ERR() << "\t! Unreachable reached: " << __FUNCTION__ << "(" << __FILE__ << ":" << __LINE__ \
            << ")"),                                                                             \
     ASSERT(false))
#else
#define UNIMPLEMENTED()                 \
    {                                   \
        ASSERT(NOASSERT_UNIMPLEMENTED); \
    }                                   \
    ANGLE_EMPTY_STATEMENT

// A macro for code which is not expected to be reached under valid assumptions
#define UNREACHABLE() ASSERT(false)
#endif  // defined(ANGLE_TRACE_ENABLED) || defined(ANGLE_ENABLE_ASSERTS)

#endif   // COMMON_DEBUG_H_
