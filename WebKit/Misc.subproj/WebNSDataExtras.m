/*
    WebNSDataExtras.m
    Private (SPI) header
    Copyright 2003, Apple, Inc. All rights reserved.
 */

#import <WebKit/WebNSDataExtras.h>

#import <WebKit/WebAssertions.h>

@implementation NSData (WebNSDataExtras)

-(BOOL)_web_isCaseInsensitiveEqualToCString:(const char *)string
{
    ASSERT(string);
    
    const char *bytes = [self bytes];
    return strncasecmp(bytes, string, [self length]) == 0;
}

@end
