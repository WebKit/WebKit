/*	IFWebDataSourcePrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _dataSourcePrivate in
        NSWebPageDataSource.
*/
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFMainURLHandleClient.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFException.h>
#import <WebKit/WebKitDebug.h>



@implementation IFWebDataSourcePrivate 

- init
{
    // Unnecessary, but I like to know that these ivars should be nil.
    parent = nil;
    frames = nil;
    controller = nil;
    inputURL = nil;

    part = new KHTMLPart();
    
    primaryLoadComplete = NO;
    
    return self;
}

- (void)dealloc
{
    // controller is not retained!  IFWebControllers maintain
    // a reference to the main frame, which in turn refers to it's
    // view and data source.
    [parent release];
    [frames release];
    [inputURL release];
    [urlHandles release];
    [mainURLHandleClient release];
    
    delete part;

    [super dealloc];
}

@end

@implementation IFWebDataSource (IFPrivate)
- (void)_setController: (id <IFWebController>)controller
{
    IFWebDataSourcePrivate *data = (IFWebDataSourcePrivate *)_dataSourcePrivate;

    WEBKIT_ASSERT (data->part != nil);

    data->controller = controller;
    data->part->setDataSource (self);
}


- (KHTMLPart *)_part
{
    return ((IFWebDataSourcePrivate *)_dataSourcePrivate)->part;
}

- (void)_setParent: (IFWebDataSource *)p
{
    ((IFWebDataSourcePrivate *)_dataSourcePrivate)->parent = [p retain];
}

- (void)_setPrimaryLoadComplete: (BOOL)flag
{
    IFWebDataSourcePrivate *data = (IFWebDataSourcePrivate *)_dataSourcePrivate;
    
    data->primaryLoadComplete = flag;
}

- (void)_startLoading: (BOOL)forceRefresh
{
    IFWebDataSourcePrivate *data = (IFWebDataSourcePrivate *)_dataSourcePrivate;
    NSString *urlString = [[self inputURL] absoluteString];
    NSURL *theURL;
    KURL url = [[[self inputURL] absoluteString] cString];
    IFURLHandle *handle;

    [self _setPrimaryLoadComplete: NO];
    
    WEBKIT_ASSERT ([self frame] != nil);
    
    [[self frame] _clearErrors];
    
    // FIXME [mjs]: temporary hack to make file: URLs work right
    if ([urlString hasPrefix:@"file:/"] && [urlString characterAtIndex:6] != '/') {
        urlString = [@"file:///" stringByAppendingString:[urlString substringFromIndex:6]];
    }
    if ([urlString hasSuffix:@"/"]) {
        urlString = [urlString substringToIndex:([urlString length] - 1)];
    }
    theURL = [NSURL URLWithString:urlString];

    data->mainURLHandleClient = [[IFMainURLHandleClient alloc] initWithDataSource: self part: [self _part]];
    
    // The handle will be released by the client upon receipt of a 
    // terminal callback.
    handle = [[IFURLHandle alloc] initWithURL:theURL];
    [handle addClient: data->mainURLHandleClient];
    
    // Mark the start loading time.
    data->loadingStartedTime = CFAbsoluteTimeGetCurrent();
    
    // Fire this guy up.
    [handle loadInBackground];

    // FIXME:  Do any work need in the kde engine.  This should be removed.
    // We should move any code needed out of KWQ.
    [self _part]->openURL (url);
    
    [[self controller] locationChangeStartedForFrame: [self frame]];
}


- (void)_addURLHandle: (IFURLHandle *)handle
{
    IFWebDataSourcePrivate *data = (IFWebDataSourcePrivate *)_dataSourcePrivate;
    
    if (data->urlHandles == nil)
        data->urlHandles = [[NSMutableArray alloc] init];
    [data->urlHandles addObject: handle];
}

- (void)_removeURLHandle: (IFURLHandle *)handle
{
    IFWebDataSourcePrivate *data = (IFWebDataSourcePrivate *)_dataSourcePrivate;

    [data->urlHandles removeObject: handle];
}

- (void)_stopLoading
{
    IFWebDataSourcePrivate *data = (IFWebDataSourcePrivate *)_dataSourcePrivate;
    int i, count;
    IFURLHandle *handle;
        
    // Tell all handles to stop loading.
    count = [data->urlHandles count];
    for (i = 0; i < count; i++) {
        handle = [data->urlHandles objectAtIndex: i];
        WEBKITDEBUGLEVEL1 (WEBKIT_LOG_LOADING, "cancelling %s\n", [[[handle url] absoluteString] cString] );
        [[data->urlHandles objectAtIndex: i] cancelLoadInBackground];
    }

    [self _part]->closeURL ();
}


- (void)_recursiveStopLoading
{
    NSArray *frames;
    IFWebFrame *nextFrame;
    int i, count;
    IFWebDataSource *childDataSource, *childProvisionalDataSource;
    
    [self _stopLoading];
    
    frames = [self children];
    count = [frames count];
    for (i = 0; i < count; i++){
        nextFrame = [frames objectAtIndex: i];
        childDataSource = [nextFrame dataSource];
        [childDataSource _recursiveStopLoading];
        childProvisionalDataSource = [nextFrame provisionalDataSource];
        [childProvisionalDataSource _recursiveStopLoading];
    }
}

- (double)_loadingStartedTime
{
    IFWebDataSourcePrivate *data = (IFWebDataSourcePrivate *)_dataSourcePrivate;
    return data->loadingStartedTime;
}


@end
