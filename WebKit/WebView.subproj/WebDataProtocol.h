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
+(BOOL)_webIsDataProtocolURL:(NSURL *)URL;
@end

@interface NSURLRequest (WebDataRequest)
+ (NSURL *)_webDataRequestURLForData: (NSData *)data;
- (NSData *)_webDataRequestData;
- (void)_webDataRequestSetData:(NSData *)data;
- (NSString *)_webDataRequestEncoding;
- (void)_webDataRequestSetEncoding:(NSString *)encoding;
- (NSString *)_webDataRequestMIMEType;
- (void)_webDataRequestSetMIMEType:(NSString *)MIMEType;
- (NSURL *)_webDataRequestBaseURL;
- (void)_webDataRequestSetBaseURL:(NSURL *)baseURL;
- (NSMutableURLRequest *)_webDataRequestExternalRequest;
@end