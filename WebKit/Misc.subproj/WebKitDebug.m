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


void WebKitDebugAtLevel(unsigned int level, const char *format, ...) {    
    if (WebKitGetLogLevel() & level){
        va_list args;
        va_start(args, format); 
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

#endif
