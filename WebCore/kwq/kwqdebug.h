/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#ifndef KWQDEBUG_H_
#define KWQDEBUG_H_

#import <Foundation/Foundation.h>

#ifdef NDEBUG

#define KWQDEBUGLEVEL(level,format...) ((void)0)

#define KWQ_ASSERT(expr) ((void)0)
#define KWQ_ASSERT_VALID_ARG(arg,expr) ((void)0)
#define KWQ_ASSERT_NOT_NIL(arg) ((void)0)

#else /* NDEBUG */

/*
*/
#define _KWQ_TIMING 1
long _GetMillisecondsSinceEpoch();

/*-----------------------------------------------------------------------------
 * Logging macros
 */

#define KWQ_LOG_NEVER_IMPLEMENTED	0x00000001
#define KWQ_LOG_PARTIALLY_IMPLEMENTED	0x00000002
#define KWQ_LOG_NOT_YET_IMPLEMENTED	0x00000004
#define KWQ_LOG_DEBUG			0x00000008
#define KWQ_LOG_ERROR			0x00000010
#define KWQ_LOG_TIMING			0x00000020
#define KWQ_LOG_LOADING			0x00000040
#define KWQ_LOG_MEMUSAGE		0x00000080
#define KWQ_LOG_FONTCACHE		0x00000100
#define KWQ_LOG_FONTCACHECHARMISS	0x00000200
#define KWQ_LOG_FRAMES			0x00000400
#define KWQ_LOG_DOWNLOAD		0x00000800
#define KWQ_LOG_PLUGINS			0x00002000

#define KWQ_LOG_NONE			0

// bits in high byte are reserved for clients
#define KWQ_LOG_ALL_EXCEPT_CLIENTS	0x0000FFFF
#define KWQ_LOG_ALL			0xFFFFFFFF

void KWQSetLogLevel(int mask);
unsigned int KWQGetLogLevel();
void KWQLog(unsigned int level, const char *file, int line, const char *function, const char *format, ...)
// FIXME: When Radar 2920557 is fixed, we can add this back and turn the -Wmissing-format-attribute
// switch back on. PFE precompiled headers currently prevent this from working.
//    __attribute__((__format__ (__printf__, 5, 6)))
;

#define DEBUG_OBJECT(object) [[object description] lossyCString]

#define KWQDEBUGLEVEL(level,format...) \
   KWQLog(level, __FILE__, __LINE__, __PRETTY_FUNCTION__, format);

/*-----------------------------------------------------------------------------
 * Assertion macros
 */

#import <signal.h>
#import <sys/types.h>
#import <sys/time.h>
#import <sys/resource.h>

#define KWQ_ASSERTION_FAILURE \
    do { \
        struct rlimit _rlimit = {RLIM_INFINITY, RLIM_INFINITY}; \
        setrlimit(RLIMIT_CORE, &_rlimit); \
        fprintf(stderr, "assertion failure at %s:%d %s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__); \
        raise(SIGQUIT); \
    } while (0)

#define KWQ_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            KWQ_ASSERTION_FAILURE; \
        } \
    } while (0)

#define KWQ_ASSERT_VALID_ARG(arg,expr) \
    do { \
        if (!(expr)) { \
            KWQ_ASSERTION_FAILURE; \
        } \
    } while (0)

#define KWQ_ASSERT_NOT_NIL(arg) \
    do { \
        if ((arg) == nil) { \
            KWQ_ASSERTION_FAILURE; \
        } \
    } while (0)

#endif

#define KWQDEBUG(format...) KWQDEBUGLEVEL(KWQ_LOG_DEBUG,format)

#define _logNeverImplemented() \
   KWQDEBUGLEVEL(KWQ_LOG_NEVER_IMPLEMENTED, "ERROR (NOT IMPLEMENTED)")

#define _logPartiallyImplemented() \
   KWQDEBUGLEVEL(KWQ_LOG_PARTIALLY_IMPLEMENTED, "WARNING (PARTIALLY IMPLEMENTED)")

#define _logNotYetImplemented() \
   KWQDEBUGLEVEL (KWQ_LOG_NOT_YET_IMPLEMENTED, "WARNING (NOT YET IMPLEMENTED)")

#endif /* KWQDEBUG_H_ */
