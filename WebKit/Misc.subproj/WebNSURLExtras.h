/*
    WebNSURLExtras.h
    Private (SPI) header
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <Foundation/NSURL_NSURLExtras.h>

@interface NSURL (WebNSURLExtras)

+ (NSURL *)_web_URLWithData:(NSData *)data;
+ (NSURL *)_web_URLWithData:(NSData *)data relativeToURL:(NSURL *)baseURL;

- (NSData *)_web_URLAsData;

- (NSString *)_web_absoluteString;
- (int)_web_URLStringLength;

@end
