/*	WebKitDebug.m
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import "WebKitDebug.h"

#ifndef xNDEBUG

unsigned int WEBKIT_LOG_LEVEL = 0;


void WebKitSetLogLevel(int mask) {
    WEBKIT_LOG_LEVEL = mask;    
}

bool __checkedDefault = 0;

unsigned int WebKitGetLogLevel(){
    if (!__checkedDefault){
        NSString *logLevelString = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitLogLevel"];
        if (logLevelString != nil){
            if (![[NSScanner scannerWithString: logLevelString] scanHexInt: &WEBKIT_LOG_LEVEL]){
                NSLog (@"Unable to scan hex value for WebKitLogLevel, default to value of %d", WEBKIT_LOG_LEVEL);
            }
        }
        __checkedDefault = 1; 
    }
    return WEBKIT_LOG_LEVEL;
}

static const char *
timestamp(void)
{
    NSString *date = [[NSDate date] descriptionWithCalendarFormat:@"%b %d %H:%M:%S.%F"
                                                         timeZone:[NSTimeZone systemTimeZone]
                                                           locale:nil];
    return [date cString];
}

void WebKitLog(unsigned int level, const char *file, int line, const char *function, const char *format, ...)
{    
    if (WebKitGetLogLevel() & level) {
        va_list args;
        fprintf(stderr, "%s - [WEBKIT] - ", timestamp());
        va_start(args, format); 
        vfprintf(stderr, format, args);
        va_end(args);
        if (format[strlen(format) - 1] != '\n')
            putc('\n', stderr);
    }
}

#endif
