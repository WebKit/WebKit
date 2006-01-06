// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef KXMLCORE_ASSERTIONS_H
#define KXMLCORE_ASSERTIONS_H

// no namespaces because this file has to be includable from C and Objective-C

// Note, this file uses many GCC extensions, but it should be compatible with
// C, Objective C, C++, and Objective C++.

// For non-debug builds, everything is disabled by default.
// Defining any of the symbols explicitly prevents this from having any effect.

#ifdef WIN32
#define ASSERT_DISABLED 1 // FIXME: We have to undo all the assert macros, since they are currently in a .mm file and use obj-c.
#else
#ifdef NDEBUG
#define ASSERTIONS_DISABLED_DEFAULT 1
#else
#define ASSERTIONS_DISABLED_DEFAULT 0
#endif
#endif

#ifndef ASSERT_DISABLED
#define ASSERT_DISABLED ASSERTIONS_DISABLED_DEFAULT
#endif

#ifndef ASSERT_ARG_DISABLED
#define ASSERT_ARG_DISABLED ASSERTIONS_DISABLED_DEFAULT
#endif

#ifndef FATAL_DISABLED
#define FATAL_DISABLED ASSERTIONS_DISABLED_DEFAULT
#endif

#ifndef ERROR_DISABLED
#define ERROR_DISABLED ASSERTIONS_DISABLED_DEFAULT
#endif

#ifndef LOG_DISABLED
#define LOG_DISABLED ASSERTIONS_DISABLED_DEFAULT
#endif

#ifdef __GNUC__
#define KXMLCORE_PRETTY_FUNCTION __PRETTY_FUNCTION__
#else
#define KXMLCORE_PRETTY_FUNCTION __FUNCTION__
#endif

// These helper functions are always declared, but not necessarily always defined if the corresponding function is disabled.

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { KXCLogChannelUninitialized, KXCLogChannelOff, KXCLogChannelOn } KXCLogChannelState;

typedef struct {
    unsigned mask;
    const char *defaultName;
    KXCLogChannelState state;
} KXCLogChannel;

void KXCReportAssertionFailure(const char *file, int line, const char *function, const char *assertion);
void KXCReportAssertionFailureWithMessage(const char *file, int line, const char *function, const char *assertion, const char *format, ...);
void KXCReportArgumentAssertionFailure(const char *file, int line, const char *function, const char *argName, const char *assertion);
void KXCReportFatalError(const char *file, int line, const char *function, const char *format, ...) ;
void KXCReportError(const char *file, int line, const char *function, const char *format, ...);
void KXCLog(const char *file, int line, const char *function, KXCLogChannel *channel, const char *format, ...);

#ifdef __cplusplus
}
#endif

// CRASH -- gets us into the debugger or the crash reporter -- signals are ignored by the crash reporter so we must do better

#define CRASH() *(int *)0xbbadbeef = 0

// ASSERT, ASSERT_WITH_MESSAGE, ASSERT_NOT_REACHED

#if ASSERT_DISABLED

#define ASSERT(assertion) ((void)0)
#define ASSERT_WITH_MESSAGE(assertion, formatAndArgs, ...) ((void)0)
#define ASSERT_NOT_REACHED() ((void)0)

#else

#define ASSERT(assertion) do \
    if (!(assertion)) { \
        KXCReportAssertionFailure(__FILE__, __LINE__, KXMLCORE_PRETTY_FUNCTION, #assertion); \
        CRASH(); \
    } \
while (0)
#define ASSERT_WITH_MESSAGE(assertion, formatAndArgs, ...) do \
    if (!(assertion)) { \
        KXCReportAssertionFailureWithMessage(__FILE__, __LINE__, KXMLCORE_PRETTY_FUNCTION, #assertion, formatAndArgs); \
        CRASH(); \
    } \
while (0)
#define ASSERT_NOT_REACHED() do { \
    KXCReportAssertionFailure(__FILE__, __LINE__, KXMLCORE_PRETTY_FUNCTION, 0); \
    CRASH(); \
} while (0)

#endif

// ASSERT_ARG

#if ASSERT_ARG_DISABLED

#define ASSERT_ARG(argName, assertion) ((void)0)

#else

#define ASSERT_ARG(argName, assertion) do \
    if (!(assertion)) { \
        KXCReportArgumentAssertionFailure(__FILE__, __LINE__, KXMLCORE_PRETTY_FUNCTION, #argName, #assertion); \
        CRASH(); \
    } \
while (0)

#endif

// FATAL

#if FATAL_DISABLED
#define FATAL(formatAndArgs, ...) ((void)0)
#else
#define FATAL(formatAndArgs, ...) do { \
    KXCReportFatalError(__FILE__, __LINE__, KXMLCORE_PRETTY_FUNCTION, formatAndArgs); \
    CRASH(); \
} while (0)
#endif

// ERROR

#if ERROR_DISABLED
#define ERROR(formatAndArgs, ...) ((void)0)
#else
#define ERROR(formatAndArgs, ...) KXCReportError(__FILE__, __LINE__, KXMLCORE_PRETTY_FUNCTION, formatAndArgs)
#endif

// LOG

#if LOG_DISABLED
#define LOG(channel, formatAndArgs, ...) ((void)0)
#else
#define LOG(channel, formatAndArgs, ...) KXCLog(__FILE__, __LINE__, KXMLCORE_PRETTY_FUNCTION, &JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, channel), formatAndArgs)
#define JOIN_LOG_CHANNEL_WITH_PREFIX(prefix, channel) JOIN_LOG_CHANNEL_WITH_PREFIX_LEVEL_2(prefix, channel)
#define JOIN_LOG_CHANNEL_WITH_PREFIX_LEVEL_2(prefix, channel) prefix ## channel
#endif

#endif // KXMLCORE_ASSERTIONS_H
