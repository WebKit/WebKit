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

#define Fixed MacFixed
#define Rect MacRect
#define Boolean MacBoolean
#include <Foundation/Foundation.h>
#undef Fixed
#undef Rect
#undef Boolean

/*
*/
#define _KWQ_TIMING 1
long _GetMillisecondsSinceEpoch();

/*-----------------------------------------------------------------------------
 * Logging macros
 */

#define KWQ_LOG_NEVER_IMPLEMENTED	0x1
#define KWQ_LOG_PARTIALLY_IMPLEMENTED	0x2
#define KWQ_LOG_NOT_YET_IMPLEMENTED	0x4
#define KWQ_LOG_DEBUG			0x8
#define KWQ_LOG_ERROR			0x10

#define KWQ_LOG_NONE			0
#define KWQ_LOG_ALL			0xffffffff

void KWQSetLogLevel(int mask);
unsigned int KWQGetLogLevel();
void KWQDebug(const char *format, ...);
void KWQDebugAtLevel(unsigned int level, const char *format, ...);
void KWQLog(NSString *format, ...);
void KWQLogAtLevel(unsigned int level, NSString *format, ...);


#define _logNeverImplemented() \
   KWQDEBUGLEVEL(KWQ_LOG_NEVER_IMPLEMENTED, "ERROR (NOT IMPLEMENTED)\n")

#define _logPartiallyImplemented() \
   KWQDEBUGLEVEL(KWQ_LOG_PARTIALLY_IMPLEMENTED, "WARNING (PARTIALLY IMPLEMENTED)\n")

#define _logNotYetImplemented() \
   KWQDEBUGLEVEL (KWQ_LOG_NOT_YET_IMPLEMENTED, "WARNING (NOT YET IMPLEMENTED)\n")


#define KWQDEBUG(format) KWQDEBUGLEVEL(KWQ_LOG_DEBUG,format)
#define KWQDEBUG1(format,arg1) KWQDEBUGLEVEL1(KWQ_LOG_DEBUG,format,arg1)
#define KWQDEBUG2(format,arg1,arg2) KWQDEBUGLEVEL2(KWQ_LOG_DEBUG,format,arg1,arg2)
#define KWQDEBUG3(format,arg1,arg2,arg3) KWQDEBUGLEVEL3(KWQ_LOG_DEBUG,format,arg1,arg2,arg3)
#define KWQDEBUG4(format,arg1,arg2,arg3,arg4) KWQDEBUGLEVEL4(KWQ_LOG_DEBUG,format,arg1,arg2,arg3,arg4)
#define KWQDEBUG5(format,arg1,arg2,arg3,arg4,arg5) KWQDEBUGLEVEL5(KWQ_LOG_DEBUG,format,arg1,arg2,arg3,arg4,arg5)
#define KWQDEBUG6(format,arg1,arg2,arg3,arg4,arg5,arg6) KWQDEBUGLEVEL6(KWQ_LOG_DEBUG,format,arg1,arg2,arg3,arg4,arg5,arg6)

#define KWQDEBUGLEVEL(level,format) \
   KWQDebugAtLevel (level,"[%s:%d  %s] ",  __FILE__, __LINE__, __FUNCTION__, format);\
   KWQDebugAtLevel (level,format);
            
#define KWQDEBUGLEVEL1(level,format,arg1) \
   KWQDebugAtLevel (level,"[%s:%d  %s] ",  __FILE__, __LINE__, __FUNCTION__);\
   KWQDebugAtLevel (level,format,arg1);
            
#define KWQDEBUGLEVEL2(level,format,arg1,arg2) \
   KWQDebugAtLevel (level,"[%s:%d  %s] ",  __FILE__, __LINE__, __FUNCTION__);\
   KWQDebugAtLevel (level,format,arg1,arg2);
            
#define KWQDEBUGLEVEL3(level,format,arg1,arg2,arg3) \
   KWQDebugAtLevel (level,"[%s:%d  %s] ",  __FILE__, __LINE__, __FUNCTION__);\
   KWQDebugAtLevel (level,format,arg1,arg2,arg3);
            
#define KWQDEBUGLEVEL4(level,format,arg1,arg2,arg3,arg4) \
   KWQDebugAtLevel (level,"[%s:%d  %s] ",  __FILE__, __LINE__, __FUNCTION__);\
   KWQDebugAtLevel (level,format,arg1,arg2,arg3,arg4);
            
#define KWQDEBUGLEVEL5(level,format,arg1,arg2,arg3,arg4,arg5) \
   KWQDebugAtLevel (level,"[%s:%d  %s] ",  __FILE__, __LINE__, __FUNCTION__);\
   KWQDebugAtLevel (level,format,arg1,arg2,arg3,arg4,arg5);
            
#define KWQDEBUGLEVEL6(level,format,arg1,arg2,arg3,arg4,arg5,arg6) \
   KWQDebugAtLevel (level,"[%s:%d  %s] ",  __FILE__, __LINE__, __FUNCTION__);\
   KWQDebugAtLevel (level,format,arg1,arg2,arg3,arg4,arg5,arg6);
            
#define DEBUG_OBJECT(object) [[object description] cString]

/*-----------------------------------------------------------------------------
 * Assertion macros
 */

#define KWQ_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            NSString *reason = [NSString stringWithFormat:@"assertion failed: '%s'", #expr]; \
            [[NSException exceptionWithName:NSGenericException reason:reason userInfo: nil] raise]; \
        } \
    } while (0)

#define KWQ_ASSERT_VALID_ARG(arg,expr) \
    do { \
        if (!(expr)) { \
            NSString *reason = [NSString stringWithFormat:@"'%s' fails check: '%s'", #arg, #expr]; \
            [[NSException exceptionWithName:NSInvalidArgumentException reason:reason userInfo: nil] raise]; \
        } \
    } while (0)

#define WEBKIT_ASSERT_NOT_NIL(arg) \
    do { \
        if ((arg) == nil) { \
            NSString *reason = [NSString stringWithFormat:@"'%s' is nil", #arg]; \
            [[NSException exceptionWithName:NSInvalidArgumentException reason:reason userInfo: nil] raise]; \
        } \
    } while (0)


#endif KWQDEBUG_H_

