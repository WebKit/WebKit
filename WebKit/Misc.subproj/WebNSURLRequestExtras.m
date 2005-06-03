/*
    WebNSURLRequestExtras.m
    Private (SPI) header
    Copyright 2005, Apple, Inc. All rights reserved.
 */

#import <WebKit/WebNSURLRequestExtras.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebNSURLExtras.h>

#define WebContentType	(@"Content-Type")
#define WebUserAgent	(@"User-Agent")
#define WebReferrer	(@"Referer")

@implementation NSURLRequest (WebNSURLRequestExtras)

- (NSString *)_web_HTTPReferrer
{
    return [self valueForHTTPHeaderField:WebReferrer];
}

- (NSString *)_web_HTTPContentType
{
    return [self valueForHTTPHeaderField:WebContentType];
}

@end


@implementation NSMutableURLRequest (WebNSURLRequestExtras)

- (void)_web_setHTTPContentType:(NSString *)contentType
{
    [self setValue:contentType forHTTPHeaderField:WebContentType];
}

- (void)_web_setHTTPReferrer:(NSString *)theReferrer
{
    // Do not set the referrer to a string that refers to a file URL.
    // That is a potential security hole.
    if ([theReferrer _webkit_isFileURL]) {
        return;
    }

    // Don't allow empty Referer: headers; some servers refuse them
    if( [theReferrer length] == 0 )
        theReferrer = nil;

    [self setValue:theReferrer forHTTPHeaderField:WebReferrer];
}

- (void)_web_setHTTPUserAgent:(NSString *)theUserAgent
{
    [self setValue:theUserAgent forHTTPHeaderField:WebUserAgent];
}

@end

