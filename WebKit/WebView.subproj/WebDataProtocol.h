/*
    WebDataProtocol.h
    Copyright 2003, Apple, Inc. All rights reserved.

    Private header file.
*/

#import <Foundation/Foundation.h>

#import <WebFoundation/NSURLRequest.h>
#import <WebFoundation/NSURLProtocol.h>


@interface WebDataProtocol : NSURLProtocol
{
}
+ (BOOL)_webIsDataProtocolURL:(NSURL *)URL;
@end

@interface NSURLRequest (WebDataRequest)
+ (NSURL *)_webDataRequestURLForData:(NSData *)data;
- (NSURL *)_webDataRequestBaseURL;
- (NSData *)_webDataRequestData;
- (NSString *)_webDataRequestEncoding;
- (NSString *)_webDataRequestMIMEType;
- (NSMutableURLRequest *)_webDataRequestExternalRequest;
@end

@interface NSMutableURLRequest (WebDataRequest)
- (void)_webDataRequestSetData:(NSData *)data;
- (void)_webDataRequestSetEncoding:(NSString *)encoding;
- (void)_webDataRequestSetMIMEType:(NSString *)MIMEType;
- (void)_webDataRequestSetBaseURL:(NSURL *)baseURL;
@end

