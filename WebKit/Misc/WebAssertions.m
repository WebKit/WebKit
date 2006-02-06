/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WebAssertions.h>

static void vprintf_stderr_objc(const char *format, va_list args)
{
    if (!strstr(format, "%@")) {
        int (* vfprintf_no_warning)(FILE *, const char *, va_list) = vfprintf;
        vfprintf_no_warning(stderr, format, args);
    } else {
        fputs([[[[NSString alloc] initWithFormat:[NSString stringWithCString:format] arguments:args] autorelease] UTF8String], stderr);
    }
}

void WebReportAssertionFailure(const char *file, int line, const char *function, const char *assertion)
{
    if (assertion) {
        fprintf(stderr, "=================\nASSERTION FAILED: %s (%s:%d %s)\n=================\n", assertion, file, line, function);
    } else {
        fprintf(stderr, "=================\nSHOULD NEVER BE REACHED (%s:%d %s)\n=================\n", file, line, function);
    }
}

void WebReportAssertionFailureWithMessage(const char *file, int line, const char *function, const char *assertion, const char *format, ...)
{
    fprintf(stderr, "=================\nASSERTION FAILED: ");
    va_list args;
    va_start(args, format);
    vprintf_stderr_objc(format, args);
    va_end(args);
    fprintf(stderr, "\n%s (%s:%d %s)\n=================\n", assertion, file, line, function);
}

void WebReportArgumentAssertionFailure(const char *file, int line, const char *function, const char *argName, const char *assertion)
{
    fprintf(stderr, "=================\nARGUMENT BAD: %s, %s (%s:%d %s)\n=================\n", argName, assertion, file, line, function);
}

void WebReportFatalError(const char *file, int line, const char *function, const char *format, ...)
{
    fprintf(stderr, "=================\nFATAL ERROR: ");
    va_list args;
    va_start(args, format);
    vprintf_stderr_objc(format, args);
    va_end(args);
    fprintf(stderr, "\n(%s:%d %s)\n=================\n", file, line, function);
}

void WebReportError(const char *file, int line, const char *function, const char *format, ...)
{
    fprintf(stderr, "=================\nERROR: ");
    va_list args;
    va_start(args, format);
    vprintf_stderr_objc(format, args);
    va_end(args);
    fprintf(stderr, "\n(%s:%d %s)\n=================\n", file, line, function);
}

void WebLog(const char *file, int line, const char *function, WebLogChannel *channel, const char *format, ...)
{
    if (channel->state == WebLogChannelUninitialized) {
        channel->state = WebLogChannelOff;
        NSString *logLevelString = [[NSUserDefaults standardUserDefaults] objectForKey:[NSString stringWithCString:channel->defaultName]];
        if (logLevelString) {
            unsigned logLevel;
            if (![[NSScanner scannerWithString:logLevelString] scanHexInt:&logLevel]) {
                NSLog(@"unable to parse hex value for %s (%@), logging is off", channel->defaultName, logLevelString);
            }
            if ((logLevel & channel->mask) == channel->mask) {
                channel->state = WebLogChannelOn;
            }
        }
    }
    
    if (channel->state != WebLogChannelOn) {
        return;
    }
    
    fprintf(stderr, "- %s:%d %s - ", file, line, function);
    va_list args;
    va_start(args, format);
    vprintf_stderr_objc(format, args);
    va_end(args);
    if (format[strlen(format) - 1] != '\n')
        putc('\n', stderr);
}
