/*	
    IFWebControllerPrivate.mm
	Copyright (c) 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFLoadProgress.h>
#import <WebKit/IFPreferencesPrivate.h>
#import <WebKit/IFStandardPanelsPrivate.h>
#import <WebKit/IFWebControllerPrivate.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFWebViewPrivate.h>

#import <WebKit/WebKitDebug.h>

#import <WebFoundation/IFError.h>
#import <WebFoundation/IFFileTypeMappings.h>
#import <WebFoundation/IFURLCacheLoaderConstants.h>
#import <WebFoundation/IFURLHandle.h>

#import <rendering/render_frames.h>


@implementation IFWebControllerPrivate

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
    [[aFrame webView] _setController: nil];
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
    [windowContext autorelease];
    [resourceProgressHandler autorelease];
    [policyHandler autorelease];

    [super dealloc];
}

@end


@implementation IFWebController (IFPrivate)

- (void)_receivedProgress: (IFLoadProgress *)progress forResourceHandle: (IFURLHandle *)resourceHandle fromDataSource: (IFWebDataSource *)dataSource complete: (BOOL)isComplete
{
    IFWebFrame *frame = [dataSource webFrame];
    
    WEBKIT_ASSERT (dataSource != nil);
    
    if (progress->bytesSoFar == -1 && progress->totalToLoad == -1){
	WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "cancelled resource = %s\n", [[[dataSource inputURL] absoluteString] cString]);
        if (frame != nil) {
            NSString *resourceString;
            IFError *error = [[IFError alloc] initWithErrorCode: IFURLHandleResultCancelled inDomain:IFErrorCodeDomainWebFoundation failingURL: [dataSource inputURL]];
            [[self resourceProgressHandler] receivedError: error forResourceHandle: resourceHandle partialProgress: progress fromDataSource: dataSource];
            if (resourceHandle != nil) {
                resourceString = [[resourceHandle url] absoluteString];
            } else {
                WEBKIT_ASSERT ([error failingURL] != nil);
                resourceString = [[error failingURL] absoluteString];
            }
            [dataSource _addError: error forResource: [[resourceHandle url] absoluteString]];
            [error release];
            [frame _checkLoadComplete];
        }
        return;
    }

    [[self resourceProgressHandler] receivedProgress: progress forResourceHandle: resourceHandle 
        fromDataSource: dataSource complete:isComplete];

    // This resouce has completed, so check if the load is complete for all frames.
    if (isComplete){
        if (frame != nil){
            [frame _transitionProvisionalToLayoutAcceptable];
            [frame _checkLoadComplete];
        }
    }
}

- (void)_mainReceivedProgress: (IFLoadProgress *)progress forResourceHandle: (IFURLHandle *)resourceHandle fromDataSource: (IFWebDataSource *)dataSource complete: (BOOL)isComplete
{
    IFWebFrame *frame = [dataSource webFrame];
    
    WEBKIT_ASSERT (dataSource != nil);

    if (progress->bytesSoFar == -1 && progress->totalToLoad == -1){
	WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "cancelled resource = %s\n", [[[dataSource inputURL] absoluteString] cString]);
        [dataSource _setPrimaryLoadComplete: YES];
        IFError *error = [[IFError alloc] initWithErrorCode: IFURLHandleResultCancelled inDomain:IFErrorCodeDomainWebFoundation failingURL: [dataSource inputURL]];
        [[self resourceProgressHandler] receivedError: error forResourceHandle: resourceHandle partialProgress: progress fromDataSource: dataSource];
        [dataSource _setMainDocumentError: error];
        [error release];
        [frame _checkLoadComplete];
        return;
    }

    [[self resourceProgressHandler] receivedProgress: progress forResourceHandle: resourceHandle 
        fromDataSource: dataSource complete:isComplete];
    
    // The frame may be nil if a previously cancelled load is still making progress callbacks.
    if (frame == nil)
        return;
        
    // Check to see if this is these are the first bits of a provisional data source,
    // if so we need to transition the data source from provisional to committed.
    // This transition is only done for the IFContentPolicyShow policy.
    if([frame provisionalDataSource] == dataSource && [dataSource contentPolicy] == IFContentPolicyShow){
        WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "committing resource = %s\n", [[[dataSource inputURL] absoluteString] cString]);
        [frame _transitionProvisionalToCommitted];
    }

    // This resouce has completed, so check if the load is complete for all frames.
    if (isComplete){
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



- (void)_receivedError: (IFError *)error forResourceHandle: (IFURLHandle *)resourceHandle partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource
{
    IFWebFrame *frame = [dataSource webFrame];

    [[self resourceProgressHandler] receivedError: error forResourceHandle: resourceHandle partialProgress: progress fromDataSource: dataSource];

    if ([dataSource _isStopping])
        return;
    
    WEBKIT_ASSERT (frame != nil);

    [dataSource _addError: error forResource:
        (resourceHandle != nil ? [[resourceHandle url] absoluteString] : [[error failingURL] absoluteString])];
    
    [frame _checkLoadComplete];
}


- (void)_mainReceivedError: (IFError *)error forResourceHandle: (IFURLHandle *)resourceHandle partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource
{
    IFWebFrame *frame = [dataSource webFrame];

    [[self resourceProgressHandler] receivedError: error forResourceHandle: resourceHandle partialProgress: progress fromDataSource: dataSource];
    
    if ([dataSource _isStopping])
        return;
    
    WEBKIT_ASSERT (frame != nil);

    [dataSource _setPrimaryLoadComplete: YES];

    [dataSource _setMainDocumentError: error];
    [frame _checkLoadComplete];
}

- (void)_didStartLoading: (NSURL *)url
{
    [[IFStandardPanels sharedStandardPanels] _didStartLoadingURL:url inController:self];
}

- (void)_didStopLoading: (NSURL *)url
{
    [[IFStandardPanels sharedStandardPanels] _didStopLoadingURL:url inController:self];
}

+ (NSString *)_MIMETypeForFile: (NSString *)path
{
    NSString *extension = [path pathExtension];
    
    if([extension isEqualToString:@""])
        return @"text/html";
        
    return [[IFFileTypeMappings sharedMappings] MIMETypeForExtension:extension];
}

- (BOOL)_openedByScript
{
    return _private->openedByScript;
}

- (void)_setOpenedByScript:(BOOL)openedByScript
{
    _private->openedByScript = openedByScript;
}

@end
