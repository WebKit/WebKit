/*
    WebNSUserDefaultsExtras.h
    Private (SPI) header
    Copyright 2003, Apple, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>

@interface NSUserDefaults (WebNSUserDefaultsExtras)
+ (NSString *)_webkit_preferredLanguageCode;
@end
