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
#import <WebKit/IFException.h>

#import <WebKit/WebKitDebug.h>

#include <WCLoadProgress.h>


// IFObjectHolder holds objects as keys in dictionaries without
// copying.
@interface IFObjectHolder : NSObject
{
    id object;
}

+ holderWithObject: o;
- initWithObject: o;
- (void)dealloc;
- object;

@end

@implementation IFObjectHolder

+ holderWithObject: o
{
    return [[[IFObjectHolder alloc] initWithObject: o] autorelease];
}

- initWithObject: o
{
    [super init];
    object = [o retain];
    return self;
}

- (void)dealloc
{
    [object release];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (unsigned)hash
{
    return [object hash];
}

- object
{
    return object;
}

- (BOOL)isEqual:(id)anObject
{
    return object == [anObject object];
}

@end

@implementation IFLoadProgress

static id IFLoadProgressMake() 
{
    return [[[IFLoadProgress alloc] init] autorelease];
}

+(void) load
{
    WCSetIFLoadProgressMakeFunc(IFLoadProgressMake);
}

- init
{
    return [super init];
}

@end


@implementation IFBaseWebController

- init
{
    return [self initWithView: nil provisionalDataSource: nil];
}


- initWithView: (IFWebView *)view provisionalDataSource: (IFWebDataSource *)dataSource
{
    IFBaseWebControllerPrivate *data;
    [super init];
    
    data = [[IFBaseWebControllerPrivate alloc] init];
    _controllerPrivate = data;
    data->mainFrame = [[IFWebFrame alloc] initWithName: @"top" view: view provisionalDataSource: dataSource controller: self];

    return self;   
}


- (void)dealloc
{
    [_controllerPrivate release];
    [super dealloc];
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


- (IFWebFrame *)createFrameNamed: (NSString *)fname for: (IFWebDataSource *)childDataSource inParent: (IFWebDataSource *)parentDataSource
{
    IFWebView *childView;
    IFWebFrame *newFrame;
    IFDynamicScrollBarsView *scrollView;

    childView = [[[IFWebView alloc] initWithFrame: NSMakeRect (0,0,0,0)] autorelease];

    newFrame = [[[IFWebFrame alloc] initWithName: fname view: childView provisionalDataSource: childDataSource controller: self] autorelease];

    [parentDataSource addFrame: newFrame];

    [childView _setController: self];
    [childDataSource _setController: self];

    scrollView  = [[[IFDynamicScrollBarsView alloc] initWithFrame: NSMakeRect(0,0,0,0)] autorelease];
    [childView _setFrameScrollView: scrollView];
        
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
    IFWebFrame *frame = [dataSource frame];
    
    WEBKIT_ASSERT (dataSource != nil);

    WEBKIT_ASSERT (frame != nil);

    // Check to see if this is the first load for a data source, if so
    // we need to transition the data source from provisional to committed.
    if (progress->bytesSoFar == progress->totalToLoad && [frame provisionalDataSource] == dataSource){
        WEBKITDEBUGLEVEL1 (0x2000, "resource = %s\n", [resourceDescription cString]);
        [frame _transitionProvisionalToCommitted];
    }
    
    // Check if the load is complete for this data source.
    if (progress->bytesSoFar == progress->totalToLoad)
        [self _checkLoadCompleteForDataSource: dataSource];
}


- (void)receivedError: (IFError *)error forResource: (NSString *)resourceDescription partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource
{
    WEBKIT_ASSERT (dataSource != nil);

    // FIXME What should we do if the error is for a provisional data source?
    [self _checkLoadCompleteForDataSource: dataSource];
}


// ---------------------------------------------------------------------
// IFLocationChangeHandler
// ---------------------------------------------------------------------
- (BOOL)locationWillChangeTo: (NSURL *)url forFrame: (IFWebFrame *)frame;
{
    return YES;
}


- (void)locationChangeStartedForFrame: (IFWebFrame *)frame;
{
    // Do nothing.  Subclasses typically override this method.
}


- (void)locationChangeCommittedForFrame: (IFWebFrame *)frame
{
    // Do nothing.  Subclasses typically override this method.
}


- (void)locationChangeDone: (IFError *)error forFrame: (IFWebFrame *)frame
{    
    [[frame view] setNeedsLayout: YES];
    [[frame view] setNeedsDisplay: YES];
}

- (void)receivedPageTitle: (NSString *)title forDataSource: (IFWebDataSource *)dataSource
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFBaseWebController::receivedPageTitle:forDataSource: is not implemented"];
}


- (void)serverRedirectTo: (NSURL *)url forDataSource: (IFWebDataSource *)dataSource
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFBaseWebController::serverRedirectTo:forDataSource: is not implemented"];
}

- (IFWebFrame *)_frameForDataSource: (IFWebDataSource *)dataSource fromFrame: (IFWebFrame *)frame
{
    NSArray *frames;
    int i, count;
    IFWebFrame *result;
    
    if ([frame dataSource] == dataSource)
        return frame;
        
    if ([frame provisionalDataSource] == dataSource)
        return frame;
        
    frames = [[frame dataSource] children];
    count = [frames count];
    for (i = 0; i < count; i++){
        frame = [frames objectAtIndex: i];
        result = [self _frameForDataSource: dataSource fromFrame: frame];
        if (result)
            return result;
    }

    frames = [[frame provisionalDataSource] children];
    count = [frames count];
    for (i = 0; i < count; i++){
        frame = [frames objectAtIndex: i];
        result = [self _frameForDataSource: dataSource fromFrame: frame];
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


- (IFWebFrame *)mainFrame
{
    IFBaseWebControllerPrivate *data = (IFBaseWebControllerPrivate *)_controllerPrivate;
    
    return data->mainFrame;
}


@end
