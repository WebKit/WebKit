/*	
    IFWebController.mm
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/IFBaseWebControllerPrivate.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFPreferencesPrivate.h>

#include <KWQKHTMLPart.h>
#include <rendering/render_frames.h>

#import <WebKit/WebKitDebug.h>


@implementation IFBaseWebControllerPrivate

- init 
{
    mainFrame = nil;
    return self;
}

- (void)dealloc
{
    [mainFrame reset];
    [mainFrame autorelease];
    [super dealloc];
}

@end


@implementation IFBaseWebController (IFPrivate)

- (void)_receivedProgress: (IFLoadProgress *)progress forResource: (NSString *)resourceDescription fromDataSource: (IFWebDataSource *)dataSource
{
    IFWebFrame *frame = [dataSource frame];
    
    WEBKIT_ASSERT (dataSource != nil);
    WEBKIT_ASSERT (frame != nil);

    [self receivedProgress: progress forResource: resourceDescription fromDataSource: dataSource];

    // This resouce has completed, so check if the load is complete for all frames.
    if (progress->bytesSoFar == progress->totalToLoad){
        [frame _transitionProvisionalToLayoutAcceptable];
        [frame _checkLoadCompleteResource: resourceDescription error: nil isMainDocument: NO];
    }
}

- (void)_mainReceivedProgress: (IFLoadProgress *)progress forResource: (NSString *)resourceDescription fromDataSource: (IFWebDataSource *)dataSource
{
    IFWebFrame *frame = [dataSource frame];
    
    WEBKIT_ASSERT (dataSource != nil);
    WEBKIT_ASSERT (frame != nil);

    [self receivedProgress: progress forResource: resourceDescription fromDataSource: dataSource];

    if (progress->bytesSoFar == -1 && progress->totalToLoad == -1){
	WEBKITDEBUGLEVEL1 (WEBKIT_LOG_LOADING, "cancelled resource = %s\n", [[[dataSource inputURL] absoluteString] cString]);
    }

    // Check to see if this is these are the first bits of a provisional data source,
    // if so we need to transition the data source from provisional to committed.
    if([frame provisionalDataSource] == dataSource){
        WEBKITDEBUGLEVEL1 (WEBKIT_LOG_LOADING, "committing resource = %s\n", [[[dataSource inputURL] absoluteString] cString]);
        [frame _transitionProvisionalToCommitted];
    }

    // This resouce has completed, so check if the load is complete for all frames.
    if (progress->bytesSoFar == progress->totalToLoad){
        [dataSource _setPrimaryLoadComplete: YES];
        [frame _checkLoadCompleteResource: resourceDescription error: nil  isMainDocument: YES];
    }
    else {
        // If the load is complete, make the primary load as done.  The primary load is the load
        // of the main document.  Other resources may still be arriving.
        int timedLayoutSize = [[IFPreferences standardPreferences] _initialTimedLayoutSize];
        if (progress->bytesSoFar > timedLayoutSize)
            [frame _transitionProvisionalToLayoutAcceptable];
    }
    
}



- (void)_receivedError: (IFError *)error forResource: (NSString *)resourceDescription partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource
{
    IFWebFrame *frame = [dataSource frame];

    WEBKIT_ASSERT (frame != nil);

    [self receivedError: error forResource: resourceDescription partialProgress: progress fromDataSource: dataSource];
    
    [frame _checkLoadCompleteResource: resourceDescription error: error isMainDocument: NO];
}


- (void)_mainReceivedError: (IFError *)error forResource: (NSString *)resourceDescription partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource
{
    IFWebFrame *frame = [dataSource frame];

    WEBKIT_ASSERT (frame != nil);

    [self receivedError: error forResource: resourceDescription partialProgress: progress fromDataSource: dataSource];
    
    [dataSource _setPrimaryLoadComplete: YES];

    [frame _checkLoadCompleteResource: resourceDescription error: error isMainDocument: YES];
}



@end
