/*	
    IFWebController.mm
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/IFBaseWebControllerPrivate.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFPreferencesPrivate.h>
#import <WebKit/IFLoadProgress.h>
#import <WebKit/IFWebController.h>

#import <WebFoundation/IFError.h>
#import <WebFoundation/IFURLCacheLoaderConstants.h>
#import <khtml_part.h>
#import <rendering/render_frames.h>

#import <WebKit/WebKitDebug.h>


@implementation IFBaseWebControllerPrivate

- init 
{
    mainFrame = nil;
    return self;
}

- (void)_clearControllerReferences: (IFWebFrame *)aFrame
{
    NSArray *frames;
    IFWebFrame *nextFrame;
    int i, count;
        
    [[aFrame dataSource] _setController: nil];
    [[aFrame view] _setController: nil];
    [aFrame _setController: nil];

    // Walk the frame tree, niling the controller.
    frames = [[aFrame dataSource] children];
    count = [frames count];
    for (i = 0; i < count; i++){
        nextFrame = [frames objectAtIndex: i];
        [self _clearControllerReferences: nextFrame];
    }
}

- (void)dealloc
{
    [self _clearControllerReferences: mainFrame];

    [mainFrame reset];
    
    [mainFrame autorelease];
    [super dealloc];
}

@end


@implementation IFBaseWebController (IFPrivate)

- (void)_receivedProgress: (IFLoadProgress *)progress forResource: (NSString *)resourceDescription fromDataSource: (IFWebDataSource *)dataSource
{
    IFWebFrame *frame = [dataSource webFrame];
    
    WEBKIT_ASSERT (dataSource != nil);
    
    if (progress->bytesSoFar == -1 && progress->totalToLoad == -1){
	WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "cancelled resource = %s\n", [[[dataSource inputURL] absoluteString] cString]);
        if (frame != nil) {
            IFError *error = [[IFError alloc] initWithErrorCode: IFURLHandleResultCancelled failingURL: [dataSource inputURL]];
            [dataSource _addError: error forResource: resourceDescription];
            [error release];
            [frame _checkLoadComplete];
        }
        return;
    }

    [self receivedProgress: progress forResource: resourceDescription fromDataSource: dataSource];

    // This resouce has completed, so check if the load is complete for all frames.
    if (progress->bytesSoFar == progress->totalToLoad){
        if (frame != nil){
            [frame _transitionProvisionalToLayoutAcceptable];
            [frame _checkLoadComplete];
        }
    }
}

- (void)_mainReceivedProgress: (IFLoadProgress *)progress forResource: (NSString *)resourceDescription fromDataSource: (IFWebDataSource *)dataSource
{
    IFWebFrame *frame = [dataSource webFrame];
    
    WEBKIT_ASSERT (dataSource != nil);

    if (progress->bytesSoFar == -1 && progress->totalToLoad == -1){
	WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "cancelled resource = %s\n", [[[dataSource inputURL] absoluteString] cString]);
        [dataSource _setPrimaryLoadComplete: YES];
        if (frame != nil) {
            IFError *error = [[IFError alloc] initWithErrorCode: IFURLHandleResultCancelled failingURL: [dataSource inputURL]];
            [dataSource _setMainDocumentError: error];
            [error release];
            [frame _checkLoadComplete];
        }
        return;
    }

    [self receivedProgress: progress forResource: resourceDescription fromDataSource: dataSource];

    // The frame may be nil if a previously cancelled load is still making progress callbacks.
    if (frame == nil)
        return;
        
    // Check to see if this is these are the first bits of a provisional data source,
    // if so we need to transition the data source from provisional to committed.
    // This transition is only done for the IFContentPolicyShow policy.
    if([frame provisionalDataSource] == dataSource && [dataSource _contentPolicy] == IFContentPolicyShow){
        WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "committing resource = %s\n", [[[dataSource inputURL] absoluteString] cString]);
        [frame _transitionProvisionalToCommitted];
    }

    // This resouce has completed, so check if the load is complete for all frames.
    if (progress->bytesSoFar == progress->totalToLoad){
        // If the load is complete, mark the primary load as done.  The primary load is the load
        // of the main document.  Other resources may still be arriving.
        [dataSource _setPrimaryLoadComplete: YES];
        [frame _checkLoadComplete];
    }
    else {
        // If the frame isn't complete it might be ready for a layout.  Perform that check here.
        // Note that transitioning a frame to this state doesn't guarantee a layout, rather it
        // just indicates that an early layout can be performed.
        int timedLayoutSize = [[IFPreferences standardPreferences] _initialTimedLayoutSize];
        if (progress->bytesSoFar > timedLayoutSize)
            [frame _transitionProvisionalToLayoutAcceptable];
    }
}



- (void)_receivedError: (IFError *)error forResource: (NSString *)resourceDescription partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource
{
    IFWebFrame *frame = [dataSource webFrame];

    [self receivedError: error forResource: resourceDescription partialProgress: progress fromDataSource: dataSource];

    if ([dataSource _isStopping])
        return;
    
    WEBKIT_ASSERT (frame != nil);

    [dataSource _addError: error forResource: resourceDescription];
    
    [frame _checkLoadComplete];
}


- (void)_mainReceivedError: (IFError *)error forResource: (NSString *)resourceDescription partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource
{
    IFWebFrame *frame = [dataSource webFrame];

    [self receivedError: error forResource: resourceDescription partialProgress: progress fromDataSource: dataSource];
    
    if ([dataSource _isStopping])
        return;
    
    WEBKIT_ASSERT (frame != nil);

    [dataSource _setPrimaryLoadComplete: YES];

    [dataSource _setMainDocumentError: error];
    [frame _checkLoadComplete];
}



@end
