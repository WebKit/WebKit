/*	WebDownload.m
        Copyright 2003, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDownload.h>

#import <WebFoundation/NSURLDownload.h>
#import <WebFoundation/NSURLAuthenticationChallenge.h>
#import <WebFoundation/NSURLDownloadPrivate.h>
#import <WebKit/WebPanelAuthenticationHandler.h>

@interface WebDownloadInternal : NSObject
{
@public
    id realDelegate;
    WebDownload *webDownload;
}

- (void)setRealDelegate:(id)rd;

@end

@implementation WebDownloadInternal

- (id)initWithDownload:(WebDownload *)dl
{
    self = [super init];
    if (self != nil) {
	webDownload = [dl retain];
    }
    return self;
}

- (void)dealloc
{
    [webDownload release];
    [realDelegate release];
}

- (void)setRealDelegate:(id)rd
{
    [rd retain];
    [realDelegate release];
    realDelegate = rd;
}

- (BOOL)respondsToSelector:(SEL)selector
{
    if (selector == @selector(downloadDidBegin:) ||
	selector == @selector(download:willSendRequest:redirectResponse:) ||
	selector == @selector(download:didReceiveResponse:) ||
	selector == @selector(download:didReceiveDataOfLength:) ||
	selector == @selector(download:shouldDecodeSourceDataOfMIMEType:) ||
	selector == @selector(download:decideDestinationWithSuggestedFilename:) ||
	selector == @selector(download:didCreateDestination:) ||
	selector == @selector(downloadDidFinish:) ||
	selector == @selector(download:didFailWithError:) ||
	selector == @selector(download:shouldBeginChildDownloadOfSource:delegate:) ||
	selector == @selector(download:didBeginChildDownload:)) {
	return [realDelegate respondsToSelector:selector];
    }

    return [super respondsToSelector:selector];
}

- (void)downloadDidBegin:(NSURLDownload *)download
{
    [realDelegate downloadDidBegin:download];
}

- (NSURLRequest *)download:(NSURLDownload *)download willSendRequest:(NSURLRequest *)request redirectResponse:(NSURLResponse *)redirectResponse
{
    return [realDelegate download:download willSendRequest:request redirectResponse:redirectResponse];
}

- (void)download:(NSURLDownload *)download didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if ([realDelegate respondsToSelector:@selector(download:didReceiveAuthenticationChallenge:)]) {
	[realDelegate download:download didReceiveAuthenticationChallenge:challenge];
    } else {
	NSWindow *window = nil;
	if ([realDelegate respondsToSelector:@selector(downloadWindowForAuthenticationSheet:)]) {
	    window = [realDelegate downloadWindowForAuthenticationSheet:webDownload];
	}

	[[WebPanelAuthenticationHandler sharedHandler] startAuthentication:challenge window:window];
    }
}

- (void)download:(NSURLDownload *)download didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if ([realDelegate respondsToSelector:@selector(download:didCancelAuthenticationChallenge:)]) {
	[realDelegate download:download didCancelAuthenticationChallenge:challenge];
    } else {
	[[WebPanelAuthenticationHandler sharedHandler] cancelAuthentication:challenge];
    }
}

- (void)download:(NSURLDownload *)download didReceiveResponse:(NSURLResponse *)response
{
    [realDelegate download:download didReceiveResponse:response];
}

- (void)download:(NSURLDownload *)download didReceiveDataOfLength:(unsigned)length
{
    [realDelegate download:download didReceiveDataOfLength:length];
}

- (BOOL)download:(NSURLDownload *)download shouldDecodeSourceDataOfMIMEType:(NSString *)encodingType
{
    return [realDelegate download:download shouldDecodeSourceDataOfMIMEType:encodingType];
}

- (void)download:(NSURLDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename
{
    [realDelegate download:download decideDestinationWithSuggestedFilename:filename];
}

- (void)download:(NSURLDownload *)download didCreateDestination:(NSString *)path
{
    [realDelegate download:download didCreateDestination:path];
}

- (void)downloadDidFinish:(NSURLDownload *)download
{
    [realDelegate downloadDidFinish:download];
}

- (void)download:(NSURLDownload *)download didFailWithError:(NSError *)error
{
    [realDelegate download:download didFailWithError:error];
}

- (NSURLRequest *)download:(NSURLDownload *)download shouldBeginChildDownloadOfSource:(NSURLRequest *)child delegate:(id *)childDelegate
{
    return [realDelegate download:download shouldBeginChildDownloadOfSource:child delegate:childDelegate];
}

- (void)download:(NSURLDownload *)parent didBeginChildDownload:(NSURLDownload *)child
{
    [realDelegate download:parent didBeginChildDownload:child];
}

@end



@implementation WebDownload

- (id)init
{
    self = [super init];
    if (self != nil) {
	_webInternal = [[WebDownloadInternal alloc] init];
    }
    return self;
}

- (id)initWithSource:(NSURLRequest *)request delegate:(id)delegate
{
    self = [self init];
    [_webInternal setRealDelegate:delegate];
    [super initWithSource:request delegate:_webInternal];
    return self;
}

- (id)_initWithLoadingResource:(NSURLConnection *)connection
                       request:(NSURLRequest *)request
                      response:(NSURLResponse *)response
                      delegate:(id)delegate
                         proxy:(NSURLConnectionDelegateProxy *)proxy;
{
    self = [self init];
    [_webInternal setRealDelegate:delegate];
    [super _initWithLoadingResource:connection request:request response:response delegate:_webInternal proxy:proxy];
    return self;
}

- (id)_initWithSource:(NSURLRequest *)request
             delegate:(id)delegate
            directory:(NSString *)directory
{
    self = [self init];
    [_webInternal setRealDelegate:delegate];
    [super _initWithSource:request delegate:_webInternal directory:directory];
    return self;
}

@end
