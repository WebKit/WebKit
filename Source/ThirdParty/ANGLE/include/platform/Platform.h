//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform.h: The public interface ANGLE exposes to the API layer, for
//   doing platform-specific tasks like gathering data, or for tracing.

#ifndef ANGLE_PLATFORM_H
#define ANGLE_PLATFORM_H

#include <stdint.h>

#if defined(_WIN32)
#   if !defined(LIBANGLE_IMPLEMENTATION)
#       define ANGLE_PLATFORM_EXPORT __declspec(dllimport)
#   endif
#elif defined(__GNUC__)
#   if defined(LIBANGLE_IMPLEMENTATION)
#       define ANGLE_PLATFORM_EXPORT __attribute__((visibility ("default")))
#   endif
#endif
#if !defined(ANGLE_PLATFORM_EXPORT)
#   define ANGLE_PLATFORM_EXPORT
#endif

#if defined(_WIN32)
#   define ANGLE_APIENTRY __stdcall
#else
#   define ANGLE_APIENTRY
#endif

namespace angle
{
struct WorkaroundsD3D;
using TraceEventHandle = uint64_t;
using EGLDisplayType   = void *;
struct PlatformMethods;

// Use a C-like API to not trigger undefined calling behaviour.

// System --------------------------------------------------------------

// Wall clock time in seconds since the epoch.
// TODO(jmadill): investigate using an ANGLE internal time library
inline double ANGLE_currentTime(PlatformMethods *platform)
{
    return 0.0;
}

// Monotonically increasing time in seconds from an arbitrary fixed point in the past.
// This function is expected to return at least millisecond-precision values. For this reason,
// it is recommended that the fixed point be no further in the past than the epoch.
inline double ANGLE_monotonicallyIncreasingTime(PlatformMethods *platform)
{
    return 0.0;
}

// Logging ------------------------------------------------------------

// Log an error message within the platform implementation.
inline void ANGLE_logError(PlatformMethods *platform, const char *errorMessage)
{
}

// Log a warning message within the platform implementation.
inline void ANGLE_logWarning(PlatformMethods *platform, const char *warningMessage)
{
}

// Log an info message within the platform implementation.
inline void ANGLE_logInfo(PlatformMethods *platform, const char *infoMessage)
{
}

// Tracing --------

// Get a pointer to the enabled state of the given trace category. The
// embedder can dynamically change the enabled state as trace event
// recording is started and stopped by the application. Only long-lived
// literal strings should be given as the category name. The implementation
// expects the returned pointer to be held permanently in a local static. If
// the unsigned char is non-zero, tracing is enabled. If tracing is enabled,
// addTraceEvent is expected to be called by the trace event macros.
inline const unsigned char *ANGLE_getTraceCategoryEnabledFlag(PlatformMethods *platform,
                                                              const char *categoryName)
{
    return nullptr;
}

//
// Add a trace event to the platform tracing system. Depending on the actual
// enabled state, this event may be recorded or dropped.
// - phase specifies the type of event:
//   - BEGIN ('B'): Marks the beginning of a scoped event.
//   - END ('E'): Marks the end of a scoped event.
//   - COMPLETE ('X'): Marks the beginning of a scoped event, but doesn't
//     need a matching END event. Instead, at the end of the scope,
//     updateTraceEventDuration() must be called with the TraceEventHandle
//     returned from addTraceEvent().
//   - INSTANT ('I'): Standalone, instantaneous event.
//   - START ('S'): Marks the beginning of an asynchronous event (the end
//     event can occur in a different scope or thread). The id parameter is
//     used to match START/FINISH pairs.
//   - FINISH ('F'): Marks the end of an asynchronous event.
//   - COUNTER ('C'): Used to trace integer quantities that change over
//     time. The argument values are expected to be of type int.
//   - METADATA ('M'): Reserved for internal use.
// - categoryEnabled is the pointer returned by getTraceCategoryEnabledFlag.
// - name is the name of the event. Also used to match BEGIN/END and
//   START/FINISH pairs.
// - id optionally allows events of the same name to be distinguished from
//   each other. For example, to trace the consutruction and destruction of
//   objects, specify the pointer as the id parameter.
// - timestamp should be a time value returned from monotonicallyIncreasingTime.
// - numArgs specifies the number of elements in argNames, argTypes, and
//   argValues.
// - argNames is the array of argument names. Use long-lived literal strings
//   or specify the COPY flag.
// - argTypes is the array of argument types:
//   - BOOL (1): bool
//   - UINT (2): unsigned long long
//   - INT (3): long long
//   - DOUBLE (4): double
//   - POINTER (5): void*
//   - STRING (6): char* (long-lived null-terminated char* string)
//   - COPY_STRING (7): char* (temporary null-terminated char* string)
//   - CONVERTABLE (8): WebConvertableToTraceFormat
// - argValues is the array of argument values. Each value is the unsigned
//   long long member of a union of all supported types.
// - flags can be 0 or one or more of the following, ORed together:
//   - COPY (0x1): treat all strings (name, argNames and argValues of type
//     string) as temporary so that they will be copied by addTraceEvent.
//   - HAS_ID (0x2): use the id argument to uniquely identify the event for
//     matching with other events of the same name.
//   - MANGLE_ID (0x4): specify this flag if the id parameter is the value
//     of a pointer.
inline angle::TraceEventHandle ANGLE_addTraceEvent(PlatformMethods *platform,
                                                   char phase,
                                                   const unsigned char *categoryEnabledFlag,
                                                   const char *name,
                                                   unsigned long long id,
                                                   double timestamp,
                                                   int numArgs,
                                                   const char **argNames,
                                                   const unsigned char *argTypes,
                                                   const unsigned long long *argValues,
                                                   unsigned char flags)
{
    return 0;
}

// Set the duration field of a COMPLETE trace event.
inline void ANGLE_updateTraceEventDuration(PlatformMethods *platform,
                                           const unsigned char *categoryEnabledFlag,
                                           const char *name,
                                           angle::TraceEventHandle eventHandle)
{
}

// Callbacks for reporting histogram data.
// CustomCounts histogram has exponential bucket sizes, so that min=1, max=1000000, bucketCount=50
// would do.
inline void ANGLE_histogramCustomCounts(PlatformMethods *platform,
                                        const char *name,
                                        int sample,
                                        int min,
                                        int max,
                                        int bucketCount)
{
}
// Enumeration histogram buckets are linear, boundaryValue should be larger than any possible sample
// value.
inline void ANGLE_histogramEnumeration(PlatformMethods *platform,
                                       const char *name,
                                       int sample,
                                       int boundaryValue)
{
}
// Unlike enumeration histograms, sparse histograms only allocate memory for non-empty buckets.
inline void ANGLE_histogramSparse(PlatformMethods *platform, const char *name, int sample)
{
}
// Boolean histograms track two-state variables.
inline void ANGLE_histogramBoolean(PlatformMethods *platform, const char *name, bool sample)
{
}

// Allows us to programatically override ANGLE's default workarounds for testing purposes.
inline void ANGLE_overrideWorkaroundsD3D(PlatformMethods *platform,
                                         angle::WorkaroundsD3D *workaroundsD3D)
{
}

// Platform methods are enumerated here once.
#define ANGLE_PLATFORM_OP(OP)       \
    OP(currentTime)                 \
    OP(monotonicallyIncreasingTime) \
    OP(logError)                    \
    OP(logWarning)                  \
    OP(logInfo)                     \
    OP(getTraceCategoryEnabledFlag) \
    OP(addTraceEvent)               \
    OP(updateTraceEventDuration)    \
    OP(histogramCustomCounts)       \
    OP(histogramEnumeration)        \
    OP(histogramSparse)             \
    OP(histogramBoolean)            \
    OP(overrideWorkaroundsD3D)

#define ANGLE_PLATFORM_METHOD_DEF(Name) decltype(&ANGLE_##Name) Name = ANGLE_##Name;

struct PlatformMethods
{
    ANGLE_PLATFORM_OP(ANGLE_PLATFORM_METHOD_DEF);

    // User data pointer for any implementation specific members.
    void *context = 0;
};

#undef ANGLE_PLATFORM_METHOD_DEF

// Subtract one to account for the context pointer.
constexpr unsigned int g_NumPlatformMethods = (sizeof(PlatformMethods) / sizeof(uintptr_t)) - 1;

#define ANGLE_PLATFORM_METHOD_STRING(Name) #Name
#define ANGLE_PLATFORM_METHOD_STRING2(Name) ANGLE_PLATFORM_METHOD_STRING(Name),

constexpr const char *const g_PlatformMethodNames[g_NumPlatformMethods] = {
    ANGLE_PLATFORM_OP(ANGLE_PLATFORM_METHOD_STRING2)};

#undef ANGLE_PLATFORM_METHOD_STRING2
#undef ANGLE_PLATFORM_METHOD_STRING

}  // namespace angle

extern "C" {

// Gets the platform methods on the passed-in EGL display. If the method name signature does not
// match the compiled signature for this ANGLE, false is returned. On success true is returned.
// The application should set any platform methods it cares about on the returned pointer.
// If display is not valid, behaviour is undefined.
//
// Use a void * here to silence a sanitizer limitation with decltype.
// TODO(jmadill): Use angle::PlatformMethods ** if UBSAN is fixed to handle decltype.

ANGLE_PLATFORM_EXPORT bool ANGLE_APIENTRY ANGLEGetDisplayPlatform(angle::EGLDisplayType display,
                                                                  const char *const methodNames[],
                                                                  unsigned int methodNameCount,
                                                                  void *context,
                                                                  void *platformMethodsOut);

// Sets the platform methods back to their defaults.
// If display is not valid, behaviour is undefined.
ANGLE_PLATFORM_EXPORT void ANGLE_APIENTRY ANGLEResetDisplayPlatform(angle::EGLDisplayType display);

}  // extern "C"

namespace angle
{
// Use typedefs here instead of decltype to work around sanitizer limitations.
// TODO(jmadill): Use decltype here if UBSAN is fixed.
typedef bool(ANGLE_APIENTRY *GetDisplayPlatformFunc)(angle::EGLDisplayType,
                                                     const char *const *,
                                                     unsigned int,
                                                     void *,
                                                     void *);
typedef void(ANGLE_APIENTRY *ResetDisplayPlatformFunc)(angle::EGLDisplayType);
}  // namespace angle

// This function is not exported
angle::PlatformMethods *ANGLEPlatformCurrent();

#endif // ANGLE_PLATFORM_H
