// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#include "Assertions.h"
#include "stdio.h"
#include "stdarg.h"

#if __APPLE__
#include <CoreFoundation/CFString.h>
#endif

extern "C" {

// This is to work around the "you should use a printf format attribute" warning on GCC
// We can't use _attribute__ ((format (printf, 2, 3))) since we allow %@
static int (* vfprintf_no_warning)(FILE *, const char *, va_list) = vfprintf;

static void vprintf_stderr_common(const char *format, va_list args)
{
#if __APPLE__
    if (!strstr(format, "%@")) {
        CFStringRef cfFormat = CFStringCreateWithCString(NULL, format, kCFStringEncodingUTF8);
        CFStringRef str = CFStringCreateWithFormatAndArguments(NULL, NULL, cfFormat, args);
        
        int length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
        char *buffer = (char *)malloc(length + 1);

        CFStringGetCString(str, buffer, length, kCFStringEncodingUTF8);

        fputs(buffer, stderr);

        free(buffer);
        CFRelease(str);
        CFRelease(cfFormat);
    } else
#endif
        vfprintf_no_warning(stderr, format, args);
}

void KXCReportAssertionFailure(const char *file, int line, const char *function, const char *assertion)
{
    if (assertion)
        fprintf(stderr, "=================\nASSERTION FAILED: %s (%s:%d %s)\n=================\n", assertion, file, line, function);
    else
        fprintf(stderr, "=================\nSHOULD NEVER BE REACHED (%s:%d %s)\n=================\n", file, line, function);
}

void KXCReportAssertionFailureWithMessage(const char *file, int line, const char *function, const char *assertion, const char *format, ...)
{
    fprintf(stderr, "=================\nASSERTION FAILED: ");
    va_list args;
    va_start(args, format);
    vprintf_stderr_common(format, args);
    va_end(args);
    fprintf(stderr, "\n%s (%s:%d %s)\n=================\n", assertion, file, line, function);
}

void KXCReportArgumentAssertionFailure(const char *file, int line, const char *function, const char *argName, const char *assertion)
{
    fprintf(stderr, "=================\nARGUMENT BAD: %s, %s (%s:%d %s)\n=================\n", argName, assertion, file, line, function);
}

void KXCReportFatalError(const char *file, int line, const char *function, const char *format, ...)
{
    fprintf(stderr, "=================\nFATAL ERROR: ");
    va_list args;
    va_start(args, format);
    vprintf_stderr_common(format, args);
    va_end(args);
    fprintf(stderr, "\n(%s:%d %s)\n=================\n", file, line, function);
}

void KXCReportError(const char *file, int line, const char *function, const char *format, ...)
{
    fprintf(stderr, "=================\nERROR: ");
    va_list args;
    va_start(args, format);
    vprintf_stderr_common(format, args);
    va_end(args);
    fprintf(stderr, "\n(%s:%d %s)\n=================\n", file, line, function);
}

void KXCLog(const char*, int, const char*, KXCLogChannel *channel, const char *format, ...)
{    
    if (channel->state != KXCLogChannelOn)
        return;
    
    va_list args;
    va_start(args, format);
    vprintf_stderr_common(format, args);
    va_end(args);
    if (format[strlen(format) - 1] != '\n')
        putc('\n', stderr);
}

} // extern "C"
