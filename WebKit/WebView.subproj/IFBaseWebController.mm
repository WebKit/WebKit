/*	
    IFBaseWebController.mm
	Copyright 2001, 2002 Apple, Inc. All rights reserved.
*/
#import <WebKit/IFBaseWebController.h>
#import <WebKit/IFBaseWebControllerPrivate.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFDynamicScrollBarsView.h>
#import <WebKit/IFException.h>
#import <WebKit/IFWebController.h>

#import <WebKit/WebKitDebug.h>

@interface _IFControllerHolder : NSObject
{
    IFBaseWebController *controller;
}
- initWithController: (IFBaseWebController *)c;
- (void)_checkReadyToDealloc: userInfo;
@end
@implementation _IFControllerHolder
- initWithController: (IFBaseWebController *)c
{
    controller = c;	// Non-retained
    return [super init];
}

- (void)_checkReadyToDealloc: userInfo
{
    if (![[[controller mainFrame] dataSource] isLoading] && ![[[controller mainFrame] provisionalDataSource] isLoading])
        [controller dealloc];
    else {
        [NSTimer scheduledTimerWithTimeInterval:1.0 target:self selector: @selector(_checkReadyToDealloc:) userInfo: nil repeats:FALSE];
    }
}
@end

@implementation IFBaseWebController

- init
{
    return [self initWithView: nil provisionalDataSource: nil];
}

- initWithView: (IFWebView *)view provisionalDataSource: (IFWebDataSource *)dataSource
{
    [super init];
    
    _private = [[IFBaseWebControllerPrivate alloc] init];
    _private->mainFrame = [[IFWebFrame alloc] initWithName: @"_top" view: view provisionalDataSource: dataSource controller: self];

    return self;   
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}


- (oneway void)release {
    if ([self retainCount] == 1){
        _IFControllerHolder *ch = [[[_IFControllerHolder alloc] initWithController: self] autorelease];
        [[self mainFrame] stopLoading];
        [NSTimer scheduledTimerWithTimeInterval:1.0 target:ch selector: @selector(_checkReadyToDealloc:) userInfo: nil repeats:FALSE];
        return;
    }
    [super release];
}

- (void)setDirectsAllLinksToSystemBrowser: (BOOL)flag
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFBaseWebController::setDirectsAllLinksToSystemBrowser: is not implemented"];
}


- (BOOL)directsAllLinksToSystemBrowser
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFBaseWebController::directsAllLinksToSystemBrowser is not implemented"];
    return NO;
}


- (IFWebFrame *)createFrameNamed: (NSString *)fname for: (IFWebDataSource *)childDataSource inParent: (IFWebDataSource *)parentDataSource inScrollView: (BOOL)inScrollView
{
    IFWebView *childView;
    IFWebFrame *newFrame;
    NSScrollView *scrollView;

    childView = [[[IFWebView alloc] initWithFrame: NSMakeRect (0,0,0,0)] autorelease];

    newFrame = [[[IFWebFrame alloc] initWithName: fname view: childView provisionalDataSource: childDataSource controller: self] autorelease];

    [parentDataSource addFrame: newFrame];

    [childView _setController: self];
    [childDataSource _setController: self];

    if (inScrollView == YES){
        scrollView  = [[[IFDynamicScrollBarsView alloc] initWithFrame: NSMakeRect(0,0,0,0)] autorelease];
        [scrollView setHasVerticalScroller: NO];
        [scrollView setHasHorizontalScroller: NO];
        [childView _setFrameScrollView: scrollView];
    }
        
    return newFrame;
}



// ---------------------------------------------------------------------
// IFScriptContextHandler
// ---------------------------------------------------------------------
- (void)setStatusText: (NSString *)text forDataSource: (IFWebDataSource *)dataSource
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFBaseWebController::setStatusText:forDataSource: is not implemented"];
}


- (NSString *)statusTextForDataSource: (IFWebDataSource *)dataSource
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFBaseWebController::statusTextForDataSource: is not implemented"];
    return nil;
}



// ---------------------------------------------------------------------
// IFLoadHandler
// ---------------------------------------------------------------------
- (void)receivedProgress: (IFLoadProgress *)progress forResource: (NSString *)resourceDescription fromDataSource: (IFWebDataSource *)dataSource
{
    // Do nothing.  Subclasses typically override this method.
}

- (void)receivedError: (IFError *)error forResource: (NSString *)resourceDescription partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource
{
    // Do nothing.  Subclasses typically override this method.
}

// ---------------------------------------------------------------------
// IFLocationChangeHandler
// ---------------------------------------------------------------------
- (id <IFLocationChangeHandler>)provideLocationChangeHandlerForFrame: (IFWebFrame *)frame
{
    return nil;
}


- (void)receivedPageTitle: (NSString *)title forDataSource: (IFWebDataSource *)dataSource
{
    // Do nothing.  Subclasses typically override this method.
}


- (void)serverRedirectTo: (NSURL *)url forDataSource: (IFWebDataSource *)dataSource
{
    // Do nothing.  Subclasses typically override this method.
}

- (IFWebFrame *)_frameForDataSource: (IFWebDataSource *)dataSource fromFrame: (IFWebFrame *)frame
{
    NSArray *frames;
    int i, count;
    IFWebFrame *result, *aFrame;
    
    if ([frame dataSource] == dataSource)
        return frame;
        
    if ([frame provisionalDataSource] == dataSource)
        return frame;
        
    frames = [[frame dataSource] children];
    count = [frames count];
    for (i = 0; i < count; i++){
        aFrame = [frames objectAtIndex: i];
        result = [self _frameForDataSource: dataSource fromFrame: aFrame];
        if (result)
            return result;
    }

    frames = [[frame provisionalDataSource] children];
    count = [frames count];
    for (i = 0; i < count; i++){
        aFrame = [frames objectAtIndex: i];
        result = [self _frameForDataSource: dataSource fromFrame: aFrame];
        if (result)
            return result;
    }
    
    return nil;       
}


- (IFWebFrame *)frameForDataSource: (IFWebDataSource *)dataSource
{
    IFWebFrame *frame = [self mainFrame];
    
    return [self _frameForDataSource: dataSource fromFrame: frame];
}


- (IFWebFrame *)_frameForView: (NSView *)aView fromFrame: (IFWebFrame *)frame
{
    NSArray *frames;
    int i, count;
    IFWebFrame *result, *aFrame;
    
    if ([frame view] == aView)
        return frame;
        
    frames = [[frame dataSource] children];
    count = [frames count];
    for (i = 0; i < count; i++){
        aFrame = [frames objectAtIndex: i];
        result = [self _frameForView: aView fromFrame: aFrame];
        if (result)
            return result;
    }

    frames = [[frame provisionalDataSource] children];
    count = [frames count];
    for (i = 0; i < count; i++){
        aFrame = [frames objectAtIndex: i];
        result = [self _frameForView: aView fromFrame: aFrame];
        if (result)
            return result;
    }
    
    return nil;       
}


- (IFWebFrame *)frameForView: (NSView *)aView
{
    IFWebFrame *frame = [self mainFrame];
    
    return [self _frameForView: aView fromFrame: frame];
}


- (IFWebFrame *)_frameNamed: (NSString *)name fromFrame: (IFWebFrame *)frame
{
    if ([[frame name] isEqual: name])
        return frame;

    int i, count;
    IFWebFrame *aFrame;
    NSArray *children = [[frame dataSource] children];
    count = [children count];
    for (i = 0; i < count; i++){
        aFrame = [children objectAtIndex: i];
        if ([self _frameNamed: name fromFrame: aFrame])
            return aFrame;
    }
    return nil;
}

- (IFWebFrame *)frameNamed: (NSString *)name
{
    return [self _frameNamed: name fromFrame: [self mainFrame]];
}

- (IFWebFrame *)mainFrame
{
    return _private->mainFrame;
}

- (void)pluginNotFoundForMIMEType:(NSString *)mime pluginPageURL:(NSURL *)url
{
    // Do nothing.  Subclasses typically override this method.
}

- (void) startedDownloadWithHandler:(IFDownloadHandler *)downloadHandler
{
    // Do nothing.  Subclasses typically override this method.
}

- (void) receivedProgress:(IFLoadProgress *)progress forDownloadHandler:(IFDownloadHandler *)downloadHandler
{
    // Do nothing.  Subclasses typically override this method.
}

- (void) receivedError:(IFError *)error forDownloadHandler:(IFDownloadHandler *)downloadHandler partialProgress: (IFLoadProgress *)progress
{
    // Do nothing.  Subclasses typically override this method.
}

@end
