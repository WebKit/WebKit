/*
    WebNSURLExtras.h
    Private (SPI) header
    Copyright 2003, Apple, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>

@interface NSData (WebNSURLExtras)

-(BOOL)_web_isCaseInsensitiveEqualToCString:(const char *)string;

@end
