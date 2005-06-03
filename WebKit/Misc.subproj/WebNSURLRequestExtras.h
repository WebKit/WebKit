/*
    WebNSURLRequestExtras.h
    Private (SPI) header
    Copyright 2003, Apple, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>

@interface NSURLRequest (WebNSURLRequestExtras)

- (NSString *)_web_HTTPReferrer;
- (NSString *)_web_HTTPContentType;

@end

@interface NSMutableURLRequest (WebNSURLRequestExtras)

- (void)_web_setHTTPContentType:(NSString *)contentType;
- (void)_web_setHTTPReferrer:(NSString *)theReferrer;
- (void)_web_setHTTPUserAgent:(NSString *)theUserAgent;

@end
