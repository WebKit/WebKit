/*
    WebDataProtocol.m
    Copyright 2003, Apple, Inc. All rights reserved.
*/
#import <WebKit/WebDataProtocol.h>

#import <WebFoundation/NSURLResponse.h>
#import <WebFoundation/NSURLResponsePrivate.h>
#import <WebFoundation/WebNSErrorExtras.h>
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebAssertions.h>

NSString *WebDataProtocolScheme = @"applewebdata";
static NSString *WebDataRequestPropertyKey = @"WebDataRequest";

@interface WebDataRequestParameters : NSObject <NSCopying>
{
@public
    NSData *data;
    NSString *MIMEType;
    NSString *encoding;
    NSURL *baseURL;
}
@end

@implementation WebDataRequestParameters

-(id)copyWithZone:(NSZone *)zone
{
    WebDataRequestParameters *newInstance = [[WebDataRequestParameters allocWithZone:zone] init];
    newInstance->data = [data copyWithZone:zone];
    newInstance->encoding = [encoding copyWithZone:zone];
    newInstance->baseURL = [baseURL copyWithZone:zone];
    newInstance->MIMEType = [MIMEType copyWithZone:zone];
    return newInstance;
}

- (void)dealloc
{
    [data release];
    [MIMEType release];
    [encoding release];
    [baseURL release];
    [super dealloc];
}

@end

@implementation NSURLRequest (WebDataRequest)

+ (NSURL *)_webDataRequestURLForData: (NSData *)data
{
    static BOOL registered;
    
    if (!registered) {
        [NSURLProtocol registerClass:[WebDataProtocol class]];
        registered = YES;
    }

    static unsigned int counter = 1;
    
    // The URL we generate is meaningless.  The only interesting properties of the URL
    // are it's scheme and that they be unique for the lifespan of the application.
    NSURL *fakeURL = [NSURL URLWithString: [NSString stringWithFormat: @"%@://%p", WebDataProtocolScheme, counter++, 0]];
    return fakeURL;
}

- (WebDataRequestParameters *)_webDataRequestParametersForReading
{
    return [NSURLProtocol propertyForKey:WebDataRequestPropertyKey inRequest:self];
}

- (WebDataRequestParameters *)_webDataRequestParametersForWriting
{
    WebDataRequestParameters *result = [NSURLProtocol propertyForKey:WebDataRequestPropertyKey inRequest:self];
    if (!result) {
        result = [[WebDataRequestParameters alloc] init];
        [NSURLProtocol setProperty:result forKey:WebDataRequestPropertyKey inRequest:self];
    }
    return result;
}

- (NSData *)_webDataRequestData
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForReading];
    return parameters ? parameters->data : nil;
}

- (void)_webDataRequestSetData: (NSData *)data
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForWriting];
    [parameters->data release];
    parameters->data = [data retain];
}

- (NSString *)_webDataRequestEncoding
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForReading];
    return parameters ? parameters->encoding: nil;
}

- (void)_webDataRequestSetEncoding:(NSString *)encoding
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForWriting];
    [parameters->encoding release];
    parameters->encoding = [encoding retain];
}

- (NSString *)_webDataRequestMIMEType
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForReading];
    return parameters ? parameters->MIMEType: nil;
}

- (void)_webDataRequestSetMIMEType:(NSString *)type
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForWriting];
    [parameters->MIMEType release];
    parameters->MIMEType = [type retain];
}

- (NSURL *)_webDataRequestBaseURL
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForReading];
    return parameters ? parameters->baseURL : nil;
}

- (void)_webDataRequestSetBaseURL:(NSURL *)baseURL
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForWriting];
    [parameters->baseURL release];
    parameters->baseURL = [baseURL retain];
}

- (NSMutableURLRequest *)_webDataRequestExternalRequest
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForReading];
    NSMutableURLRequest *newRequest = nil;
    
    if (parameters){
        newRequest = [[self mutableCopyWithZone: [self zone]] autorelease];
        NSURL *baseURL = [self _webDataRequestBaseURL];
        if (baseURL)
            [newRequest setURL: baseURL]; 
        else
            [newRequest setURL: [NSURL URLWithString: @"about:blank"]];
    } 
    return newRequest;
}


@end

// Implement the required methods for the concrete subclass of WebProtocolHandler
// that will handle our custom protocol.
@implementation WebDataProtocol

+(BOOL)_webIsDataProtocolURL:(NSURL *)URL
{
    ASSERT(URL);
    NSString *scheme = [URL scheme];
    return scheme && [scheme _web_isCaseInsensitiveEqualToString:WebDataProtocolScheme];
}

+(BOOL)canInitWithRequest:(NSURLRequest *)request
{
    NSURL *URL = [request URL];
    ASSERT(request);
    ASSERT(URL);
    return [self _webIsDataProtocolURL:URL];
}

+(NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request
{
    return request;
}

- (void)startLoading
{
    id<NSURLProtocolClient> client = [self client];
    NSURLRequest *request = [self request];
    NSData *data = [request _webDataRequestData];

    if (data) {
        NSURLResponse *response = [[NSURLResponse alloc] init];
        [response _setURL:[request URL]];
        [response _setMIMEType:[request _webDataRequestMIMEType]];
        [response _setTextEncodingName:[request _webDataRequestEncoding]];
        [client URLProtocol:self didReceiveResponse:response cacheStoragePolicy:NSURLCacheStorageNotAllowed];
        [client URLProtocol:self didLoadData:data];
        [client URLProtocolDidFinishLoading:self];
        [response release];
    } else {
        int resultCode;

        resultCode = NSURLErrorResourceUnavailable;

        [client URLProtocol:self didFailWithError:[NSError _web_errorWithDomain:NSURLErrorDomain code:resultCode failingURL:[[request URL] absoluteString]]];
    }
}

- (void)stopLoading
{
}

@end

