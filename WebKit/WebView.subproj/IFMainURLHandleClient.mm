/*	
    IFMainURLHandleClient.mm
	    
    Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/IFError.h>
#import <WebKit/IFBaseWebControllerPrivate.h>
#import <WebKit/IFMainURLHandleClient.h>

#import <WebKit/WebKitDebug.h>

#include <khtmlview.h>

@implementation IFMainURLHandleClient

- initWithDataSource: (IFWebDataSource *)ds part:(KHTMLPart *)p
{
    if ((self = [super init])) {
        dataSource = ds;	// Non-retained.
        part = p;
        return self;
    }

    return nil;
}

- (void)IFURLHandleResourceDidBeginLoading:(IFURLHandle *)sender
{
    WEBKITDEBUGLEVEL1 (WEBKIT_LOG_LOADING, "url = %s\n", [[[sender url] absoluteString] cString]);
}

- (void)IFURLHandleResourceDidCancelLoading:(IFURLHandle *)sender
{
    WEBKITDEBUGLEVEL1 (WEBKIT_LOG_LOADING, "url = %s\n", [[[sender url] absoluteString] cString]);

    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = -1;
    loadProgress->bytesSoFar = -1;
    [[dataSource controller] _mainReceivedProgress: (IFLoadProgress *)loadProgress forResource: [[sender url] absoluteString] fromDataSource: dataSource];
    [loadProgress release];
}

- (void)IFURLHandleResourceDidFinishLoading:(IFURLHandle *)sender data: (NSData *)data
{
    WEBKITDEBUGLEVEL1 (WEBKIT_LOG_LOADING, "url = %s\n", [[[sender url] absoluteString] cString]);

    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [data length];
    loadProgress->bytesSoFar = [data length];
    [[dataSource controller] _mainReceivedProgress: (IFLoadProgress *)loadProgress forResource: [[sender url] absoluteString] fromDataSource: dataSource];
    [loadProgress release];
}

- (void)IFURLHandle:(IFURLHandle *)sender resourceDataDidBecomeAvailable:(NSData *)data
{
    WEBKITDEBUGLEVEL3 (WEBKIT_LOG_LOADING, "url = %s, data = 0x%08x, length %d\n", [[[sender url] absoluteString] cString], data, [data length]);
    
    part->slotData(sender, (const char *)[data bytes], [data length]);
    
    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [sender contentLength];
    loadProgress->bytesSoFar = [sender contentLengthReceived];
    [[dataSource controller] _mainReceivedProgress: (IFLoadProgress *)loadProgress forResource: [[sender url] absoluteString] fromDataSource: dataSource];
    [loadProgress release];
}

- (void)IFURLHandle:(IFURLHandle *)sender resourceDidFailLoadingWithResult:(int)result
{
    WEBKITDEBUGLEVEL2 (WEBKIT_LOG_LOADING, "url = %s, result = %d\n", [[[sender url] absoluteString] cString], result);

    IFLoadProgress *loadProgress = [[IFLoadProgress alloc] init];
    loadProgress->totalToLoad = [sender contentLength];
    loadProgress->bytesSoFar = [[sender availableResourceData] length];

    IFError *error = [[IFError alloc] initWithErrorCode: result failingURL: [sender url]];
    [[dataSource controller] _mainReceivedError: error forResource: [[sender url] absoluteString] partialProgress: loadProgress fromDataSource: dataSource];
    [error release];
}

- (void)IFURLHandle:(IFURLHandle *)sender didRedirectToURL:(NSURL *)url
{
    WEBKITDEBUGLEVEL1 (WEBKIT_LOG_REDIRECT, "url = %s\n", [[url absoluteString] cString]);
    part->setBaseURL([[url absoluteString] cString]);
    
    [[dataSource controller] serverRedirectTo: url forDataSource: dataSource];
}

@end
