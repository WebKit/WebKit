/*	
    WebAssertions.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

// Note, this file uses many GCC extensions, but it should be compatible with
// C, Objective C, C++, and Objective C++.

// For non-debug builds, everything is disabled by default.
// Defining any of the symbols explicitly prevents this from having any effect.

#ifdef NDEBUG
#define WEB_ASSERTIONS_DISABLED_DEFAULT 1
#else
#define WEB_ASSERTIONS_DISABLED_DEFAULT 0
#endif

#ifndef ASSERT_DISABLED
#define ASSERT_DISABLED WEB_ASSERTIONS_DISABLED_DEFAULT
#endif

#ifndef ASSERT_ARG_DISABLED
#define ASSERT_ARG_DISABLED WEB_ASSERTIONS_DISABLED_DEFAULT
#endif

#ifndef FATAL_DISABLED
#define FATAL_DISABLED WEB_ASSERTIONS_DISABLED_DEFAULT
#endif

#ifndef ERROR_DISABLED
#define ERROR_DISABLED WEB_ASSERTIONS_DISABLED_DEFAULT
#endif

#ifndef LOG_DISABLED
#define LOG_DISABLED WEB_ASSERTIONS_DISABLED_DEFAULT
#endif

// These helper functions are always declared, but not necessarily always defined if the corresponding function is disabled.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned mask;
    const char *defaultName;
    enum { WebLogChannelUninitialized, WebLogChannelOff, WebLogChannelOn } state;
} WebLogChannel;

void WebReportAssertionFailure(const char *file, int line, const char *function, const char *assertion);
void WebReportAssertionFailureWithMessage(const char *file, int line, const char *function, const char *assertion, const char *format, ...);
void WebReportArgumentAssertionFailure(const char *file, int line, const char *function, const char *argName, const char *assertion);
void WebReportFatalError(const char *file, int line, const char *function, const char *format, ...) ;
void WebReportError(const char *file, int line, const char *function, const char *format, ...);
void WebLog(const char *file, int line, const char *function, WebLogChannel *channel, const char *format, ...);

#ifdef __cplusplus
}
#endif

// CRASH -- gets us into the debugger or the crash reporter -- signals are ignored by the crash reporter so we must do better

#define CRASH() *(int *)0xbbadbeef = 0

// ASSERT, ASSERT_WITH_MESSAGE, ASSERT_NOT_REACHED

#if ASSERT_DISABLED

#define ASSERT(assertion) ((void)0)
#define ASSERT_WITH_MESSAGE(assertion, formatAndArgs...) ((void)0)
#define ASSERT_NOT_REACHED() ((void)0)

#else

#define ASSERT(assertion) do \
    if (!(assertion)) { \
        WebReportAssertionFailure(__FILE__, __LINE__, __PRETTY_FUNCTION__, #assertion); \
        CRASH(); \
    } \
while (0)
#define ASSERT_WITH_MESSAGE(assertion, formatAndArgs...) do \
    if (!(assertion)) { \
        WebReportAssertionFailureWithMessage(__FILE__, __LINE__, __PRETTY_FUNCTION__, #assertion, formatAndArgs); \
        CRASH(); \
    } \
while (0)
#define ASSERT_NOT_REACHED() do { \
    WebReportAssertionFailure(__FILE__, __LINE__, __PRETTY_FUNCTION__, 0); \
    CRASH(); \
} while (0)

#endif

// ASSERT_ARG

#if ASSERT_ARG_DISABLED

#define ASSERT_ARG(argName, assertion) ((void)0)

#else

#define ASSERT_ARG(argName, assertion) do \
    if (!(assertion)) { \
        WebReportArgumentAssertionFailure(__FILE__, __LINE__, __PRETTY_FUNCTION__, #argName, #assertion); \
        CRASH(); \
    } \
while (0)

#endif

// FATAL

#if FATAL_DISABLED
#define FATAL(formatAndArgs...) ((void)0)
#else
#define FATAL(formatAndArgs...) do { \
    WebReportFatalError(__FILE__, __LINE__, __PRETTY_FUNCTION__, formatAndArgs); \
    CRASH(); \
} while (0)
#endif

// FATAL_ALWAYS

#if FATAL_ALWAYS_DISABLED
#define FATAL_ALWAYS(formatAndArgs...) ((void)0)
#else
#define FATAL_ALWAYS(formatAndArgs...) do { \
    WebReportFatalError(__FILE__, __LINE__, __PRETTY_FUNCTION__, formatAndArgs); \
    CRASH(); \
} while (0)
#endif

// ERROR

#if ERROR_DISABLED
#define ERROR(formatAndArgs...) ((void)0)
#else
#define ERROR(formatAndArgs...) WebReportError(__FILE__, __LINE__, __PRETTY_FUNCTION__, formatAndArgs)
#endif

// LOG

#if LOG_DISABLED
#define LOG(channel, formatAndArgs...) ((void)0)
#else
#define LOG(channel, formatAndArgs...) WebLog(__FILE__, __LINE__, __PRETTY_FUNCTION__, &JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, channel), formatAndArgs)
#define JOIN_LOG_CHANNEL_WITH_PREFIX(prefix, channel) JOIN_LOG_CHANNEL_WITH_PREFIX_LEVEL_2(prefix, channel)
#define JOIN_LOG_CHANNEL_WITH_PREFIX_LEVEL_2(prefix, channel) prefix ## channel
#endif
