/*	
        IFError.m
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <IFError.h>

#include <WCError.h>
#include <WebFoundation/IFURLCacheLoaderConstants.h>


@implementation IFError

static NSDictionary *descriptions = nil;

static id IFErrorMake(int code) 
{
    return [[[IFError alloc] initWithErrorCode: (int)code] autorelease];
}

+(void) initialize
{
    NSDictionary *dict;
    
    // +initialize is not thread-safe
    if (descriptions != nil) {
        return;
    }

    // Specifying all strings explicitly here, including table name, allows genstrings to work its magic
    dict = [NSDictionary dictionaryWithObjectsAndKeys:
        NSLocalizedStringFromTable (@"cancelled", @"IFError", @"IFURLHandleResultCancelled description"),
        [NSNumber numberWithInt: IFURLHandleResultCancelled],
        NSLocalizedStringFromTable (@"bad URL", @"IFError", @"IFURLHandleResultBadURLError description"),
        [NSNumber numberWithInt: IFURLHandleResultBadURLError],
        NSLocalizedStringFromTable (@"error requesting create", @"IFError", @"IFURLHandleResultRequestCreateError description"),
        [NSNumber numberWithInt: IFURLHandleResultRequestCreateError],
        NSLocalizedStringFromTable (@"error reading stream", @"IFError", @"IFURLHandleResultReadStreamError description"),
        [NSNumber numberWithInt: IFURLHandleResultReadStreamError],
        NSLocalizedStringFromTable (@"error creating read stream", @"IFError", @"IFURLHandleResultReadStreamCreateError description"),
        [NSNumber numberWithInt: IFURLHandleResultReadStreamCreateError],
        NSLocalizedStringFromTable (@"error setting client for read stream", @"IFError", @"IFURLHandleResultReadStreamSetClientError description"),
        [NSNumber numberWithInt: IFURLHandleResultReadStreamSetClientError],
        NSLocalizedStringFromTable (@"error opening read stream", @"IFError", @"IFURLHandleResultReadStreamOpenError description"),
        [NSNumber numberWithInt: IFURLHandleResultReadStreamOpenError],
        NSLocalizedStringFromTable (@"time out", @"IFError", @"IFURLHandleResultTimedOut description"),
        [NSNumber numberWithInt: IFURLHandleResultTimedOut],
        NSLocalizedStringFromTable (@"unsupported URL handle", @"IFError", @"IFURLHandleResultUnsupportedURLHandleError description"),
        [NSNumber numberWithInt: IFURLHandleResultUnsupportedURLHandleError],
        NSLocalizedStringFromTable (@"can't connect to host", @"IFError", @"IFURLHandleResultCantConnectToHostError description"),
        [NSNumber numberWithInt: IFURLHandleResultCantConnectToHostError],
        NSLocalizedStringFromTable (@"lost network connection", @"IFError", @"IFURLHandleResultNetworkConnectionLostError description"),
        [NSNumber numberWithInt: IFURLHandleResultNetworkConnectionLostError],
        NSLocalizedStringFromTable (@"DNS lookup error", @"IFError", @"IFURLHandleResultDNSLookupError description"),
        [NSNumber numberWithInt: IFURLHandleResultDNSLookupError],
        NSLocalizedStringFromTable (@"HTTP redirection loop error", @"IFError", @"IFURLHandleResultHTTPRedirectionLoopError description"),
        [NSNumber numberWithInt: IFURLHandleResultHTTPRedirectionLoopError],
        NSLocalizedStringFromTable (@"resource unavailable", @"IFError", @"IFURLHandleResultResourceUnavailableError description"),
        [NSNumber numberWithInt: IFURLHandleResultResourceUnavailableError],
        NSLocalizedStringFromTable (@"can't load from network", @"IFError", @"IFURLHandleResultCantLoadFromNetworkError description"),
        [NSNumber numberWithInt: IFURLHandleResultCantLoadFromNetworkError],

        NSLocalizedStringFromTable (@"bad request", @"IFError", @"IFURLHandleResultBadRequest description"),
        [NSNumber numberWithInt: IFURLHandleResultBadRequest],
        NSLocalizedStringFromTable (@"unauthorized", @"IFError", @"IFURLHandleResultUnauthorized description"),
        [NSNumber numberWithInt: IFURLHandleResultUnauthorized],
        NSLocalizedStringFromTable (@"payment required", @"IFError", @"IFURLHandleResultPaymentRequired description"),
        [NSNumber numberWithInt: IFURLHandleResultPaymentRequired],
        NSLocalizedStringFromTable (@"forbidden", @"IFError", @"IFURLHandleResultForbidden description"),
        [NSNumber numberWithInt: IFURLHandleResultForbidden],
        NSLocalizedStringFromTable (@"not found", @"IFError", @"IFURLHandleResultNotFound description"),
        [NSNumber numberWithInt: IFURLHandleResultNotFound],
        NSLocalizedStringFromTable (@"not allowed", @"IFError", @"IFURLHandleResultMethodNotAllowed description"),
        [NSNumber numberWithInt: IFURLHandleResultMethodNotAllowed],
        NSLocalizedStringFromTable (@"not acceptable", @"IFError", @"IFURLHandleResultNotAcceptable description"),
        [NSNumber numberWithInt: IFURLHandleResultNotAcceptable],
        NSLocalizedStringFromTable (@"not found", @"IFError", @"IFURLHandleResultNotFound description"),
        [NSNumber numberWithInt: IFURLHandleResultNotFound],
        NSLocalizedStringFromTable (@"proxy authentication required", @"IFError", @"IFURLHandleResultProxyAuthenticationRequired description"),
        [NSNumber numberWithInt: IFURLHandleResultProxyAuthenticationRequired],
        NSLocalizedStringFromTable (@"request time out", @"IFError", @"IFURLHandleResultRequestTimeOut description"),
        [NSNumber numberWithInt: IFURLHandleResultRequestTimeOut],
        NSLocalizedStringFromTable (@"conflict", @"IFError", @"IFURLHandleResultConflict description"),
        [NSNumber numberWithInt: IFURLHandleResultConflict],
        NSLocalizedStringFromTable (@"gone", @"IFError", @"IFURLHandleResultGone description"),
        [NSNumber numberWithInt: IFURLHandleResultGone],
        NSLocalizedStringFromTable (@"length required", @"IFError", @"IFURLHandleResultLengthRequired description"),
        [NSNumber numberWithInt: IFURLHandleResultLengthRequired],
        NSLocalizedStringFromTable (@"request entity too large", @"IFError", @"IFURLHandleResultRequestEntityTooLarge description"),
        [NSNumber numberWithInt: IFURLHandleResultRequestEntityTooLarge],
        NSLocalizedStringFromTable (@"request URI too large", @"IFError", @"IFURLHandleResultRequestURITooLarge description"),
        [NSNumber numberWithInt: IFURLHandleResultRequestURITooLarge],
        NSLocalizedStringFromTable (@"unsupported media type", @"IFError", @"IFURLHandleResultUnsupportedMediaType description"),
        [NSNumber numberWithInt: IFURLHandleResultUnsupportedMediaType],
        NSLocalizedStringFromTable (@"request range not satisfiable", @"IFError", @"IFURLHandleResultRequestRangeNotSatisfiable description"),
        [NSNumber numberWithInt: IFURLHandleResultRequestRangeNotSatisfiable],
        NSLocalizedStringFromTable (@"expectation failed", @"IFError", @"IFURLHandleResultExpectationFailed description"),
        [NSNumber numberWithInt: IFURLHandleResultExpectationFailed],

        NSLocalizedStringFromTable (@"internal server error", @"IFError", @"IFURLHandleResultInternalServerError description"),
        [NSNumber numberWithInt: IFURLHandleResultInternalServerError],
        NSLocalizedStringFromTable (@"not implemented", @"IFError", @"IFURLHandleResultNotImplemented description"),
        [NSNumber numberWithInt: IFURLHandleResultNotImplemented],
        NSLocalizedStringFromTable (@"bad gateway", @"IFError", @"IFURLHandleResultBadGateway description"),
        [NSNumber numberWithInt: IFURLHandleResultBadGateway],
        NSLocalizedStringFromTable (@"service unavailable", @"IFError", @"IFURLHandleResultServiceUnavailable description"),
        [NSNumber numberWithInt: IFURLHandleResultServiceUnavailable],
        NSLocalizedStringFromTable (@"gateway time out", @"IFError", @"IFURLHandleResultGatewayTimeOut description"),
        [NSNumber numberWithInt: IFURLHandleResultGatewayTimeOut],
        NSLocalizedStringFromTable (@"HTTP version not supported", @"IFError", @"IFURLHandleResultHTTPVersionNotSupported description"),
        [NSNumber numberWithInt: IFURLHandleResultHTTPVersionNotSupported],
        
        NSLocalizedStringFromTable (@"non-HTML content not currently supported", @"IFError", @"IFNonHTMLContentNotSupportedError description"),
        [NSNumber numberWithInt: IFNonHTMLContentNotSupportedError],
        NSLocalizedStringFromTable (@"file download not currently supported", @"IFError", @"IFFileDownloadNotSupportedError description"),
        [NSNumber numberWithInt: IFFileDownloadNotSupportedError],
        nil];

    if (descriptions == nil) {
        descriptions = [dict retain];
    }
}

+(void) load
{
    WCSetIFErrorMakeFunc(IFErrorMake);
}

- initWithErrorCode: (int)c
{
    return [self initWithErrorCode: c failingURL: nil];
}

- initWithErrorCode: (int)c failingURL: (NSURL *)url;
{
    [super init];
    errorCode = c;
    _failingURL = [url retain];
    return self;
}

- (void)dealloc
{
    [_failingURL autorelease];
    [super dealloc];
}

- (int)errorCode
{
    return errorCode;
}

- (NSString *)errorDescription
{
    return [descriptions objectForKey: [NSNumber numberWithInt: errorCode]];
}

- (NSURL *)failingURL
{
    return _failingURL;
}

- (NSString *)description
{
    return [NSString stringWithFormat: @"url %@, code %d: %@", [_failingURL absoluteString], errorCode, [self errorDescription]];
}

@end
