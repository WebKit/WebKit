/*
    WebDataProtocol.h
    Copyright 2003, Apple, Inc. All rights reserved.

    Private header file.
*/

#import <Foundation/Foundation.h>

#import <WebFoundation/NSURLRequest.h>
#import <WebFoundation/WebProtocol.h>


@interface WebDataProtocol : WebProtocol
{
}
@end

@interface NSURLRequest (WebDataRequest)
+ (NSURL *)_webDataRequestURLForData: (NSData *)data;
- (NSData *)_webDataRequestData;
- (void)_webDataRequestSetData:(NSData *)data;
- (NSString *)_webDataRequestEncoding;
- (void)_webDataRequestSetEncoding:(NSString *)encoding;
- (NSURL *)_webDataRequestBaseURL;
- (void)_webDataRequestSetBaseURL:(NSURL *)baseURL;
- (NSMutableURLRequest *)_webDataRequestExternalRequest;
@end