/*	WebKitDebug.m
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import "WebKitDebug.h"

unsigned int WEBKIT_LOG_LEVEL = WEBKIT_LOG_ALL;


void WebKitSetLogLevel(int mask) {
    WEBKIT_LOG_LEVEL = mask;    
}

bool checkedDefault = 0;

unsigned int WebKitGetLogLevel(){
    if (!checkedDefault){
        NSString *logLevelString = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitLogLevel"];
        if (logLevelString != nil){
            if (![[NSScanner scannerWithString: logLevelString] scanHexInt: &WEBKIT_LOG_LEVEL]){
                NSLog (@"Unable to scan hex value for WebKitLogLevel, default to value of %d", WEBKIT_LOG_LEVEL);
            }
        }
        checkedDefault = 1; 
    }
    return WEBKIT_LOG_LEVEL;
}


void WebKitLog(NSString *format, ...) {    
    if (WebKitGetLogLevel() & WEBKIT_LOG_ERROR){
        va_list args;
        va_start(args, format); 
        NSLogv(format, args);
        va_end(args);
    }
}


void WebKitLogAtLevel(unsigned int level, NSString *format, ...) {    
    if (WebKitGetLogLevel() & level){
        va_list args;
        va_start(args, format); 
        NSLogv(format, args);
        va_end(args);
    }
}


void WebKitDebug(const char *format, ...) {    
    if (WebKitGetLogLevel() & WEBKIT_LOG_DEBUG){
        va_list args;
        va_start(args, format); 
        vfprintf(stderr, format, args);
        va_end(args);
    }
}


void WebKitDebugAtLevel(unsigned int level, const char *format, ...) {    
    if (WebKitGetLogLevel() & level){
        va_list args;
        va_start(args, format); 
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

