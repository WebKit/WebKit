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
    [super init];
    _controllerPrivate = [[IFBaseWebControllerPrivate alloc] init];
    return self;
}


- initWithView: (IFWebView *)view dataSource: (IFWebDataSource *)dataSource
{
    [super init];
    _controllerPrivate = [[IFBaseWebControllerPrivate alloc] init];
    [self setMainView: view andMainDataSource: dataSource];
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



// This is the designated method to change the main view and/or main data source.
// The main view and data source are held in the main frame.
- (BOOL)setMainView: (IFWebView *)view andMainDataSource: (IFWebDataSource *)dataSource
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
        
    if (dataSource != nil && dataSource != [data->mainFrame dataSource]){
        if (![self locationWillChangeTo: [dataSource inputURL] forFrame: data->mainFrame])
            return NO;
    }
    
    // Do we need to delete and recreate the main frame?  Or can we reuse it?
    [data->mainFrame reset];
    [data->mainFrame autorelease];
    
    data->mainFrame = [[IFWebFrame alloc] initWithName: @"top" view: view dataSource: dataSource controller: self];
    
    return YES;
}



- (IFWebFrame *)createFrameNamed: (NSString *)fname for: (IFWebDataSource *)childDataSource inParent: (IFWebDataSource *)parentDataSource
{
    IFWebView *childView;
    IFWebFrame *newFrame;
    IFDynamicScrollBarsView *scrollView;

    childView = [[[IFWebView alloc] initWithFrame: NSMakeRect (0,0,0,0)] autorelease];

    newFrame = [[[IFWebFrame alloc] initWithName: fname view: childView dataSource: childDataSource controller: self] autorelease];

    [parentDataSource addFrame: newFrame];

    [childView _setController: self];
    [childDataSource _setController: self];

    scrollView  = [[[IFDynamicScrollBarsView alloc] initWithFrame: NSMakeRect(0,0,0,0)] autorelease];
    [childView _setFrameScrollView: scrollView];
        
    return newFrame;
}


- (void)setMainView: (IFWebView *)m;
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
    [self setMainView: m andMainDataSource: [data->mainFrame dataSource]];
}

- (IFWebFrame *)mainFrame
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
    return data->mainFrame;
}


- (IFWebView *)mainView
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
    return [data->mainFrame view];
}


- (BOOL)setMainDataSource: (IFWebDataSource *)dataSource;
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
    return [self setMainView: [data->mainFrame view] andMainDataSource: dataSource];
}

- (IFWebDataSource *)mainDataSource
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
    return [data->mainFrame dataSource];
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
// IFAuthenticationHandler
// ---------------------------------------------------------------------
- (IFSimpleAuthenticationResult *) authenticate: (IFSimpleAuthenticationRequest *)request
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFBaseWebController::authenticate: is not implemented"];
    return nil;
}


// ---------------------------------------------------------------------
// IFLoadHandler
// ---------------------------------------------------------------------
- (void)receivedProgress: (IFLoadProgress *)progress forResource: (NSString *)resourceDescription fromDataSource: (IFWebDataSource *)dataSource
{
    // Check if the load is complete for this data source.
    if (progress->bytesSoFar == progress->totalToLoad)
        [self _checkLoadCompleteForDataSource: dataSource];
}



- (void)receivedError: (IFError *)error forResource: (NSString *)resourceDescription partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource
{
    [self _checkLoadCompleteForDataSource: dataSource];
}


// ---------------------------------------------------------------------
// IFLocationChangeHandler
// ---------------------------------------------------------------------
- (BOOL)locationWillChangeTo: (NSURL *)url forFrame: (IFWebFrame *)frame;
{
    return YES;
}


- (void)locationChangeStartedForFrame: (IFWebFrame *)frame initiatedByUserEvent: (BOOL)flag;
{
    // Do nothing.
}


- (void)locationChangeCommittedForFrame: (IFWebFrame *)frame
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFBaseWebController::locationChangeInProgressForDataSource:forDataSource: is not implemented"];
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
        
    frames = [[frame dataSource] children];
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



@end
