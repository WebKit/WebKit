/*	
    WebAssertions.m
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
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
