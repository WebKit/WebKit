/*	
    WKDefaultWebController.mm
	Copyright 2001, 2002 Apple, Inc. All rights reserved.
*/
#import <WebKit/WKDefaultWebController.h>
#import <WebKit/WKDefaultWebControllerPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebDataSourcePrivate.h>
#import <WebKit/WKWebFrame.h>
#import <WebKit/WKException.h>


// WKObjectHolder holds objects as keys in dictionaries without
// copying.
@interface WKObjectHolder : NSObject
{
    id object;
}

+ holderWithObject: o;
- initWithObject: o;
- (void)dealloc;
- object;

@end

@implementation WKObjectHolder

+ holderWithObject: o
{
    return [[[WKObjectHolder alloc] initWithObject: o] autorelease];
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


@implementation WKDefaultWebController

- init
{
    [super init];
    _controllerPrivate = [[WKDefaultWebControllerPrivate alloc] init];
    return self;
}


- initWithView: (WKWebView *)view dataSource: (WKWebDataSource *)dataSource
{
    [super init];
    _controllerPrivate = [[WKDefaultWebControllerPrivate alloc] init];
    [self setView: view andDataSource: dataSource];
    return self;   
}


- (void)dealloc
{
    [_controllerPrivate release];
}

- (void)setDirectsAllLinksToSystemBrowser: (BOOL)flag
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKDefaultWebController::setDirectsAllLinksToSystemBrowser: is not implemented"];
}


- (BOOL)directsAllLinksToSystemBrowser
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKDefaultWebController::directsAllLinksToSystemBrowser is not implemented"];
    return NO;
}


- (void)setView: (WKWebView *)view andDataSource: (WKWebDataSource *)dataSource
{
    // FIXME:  this needs to be implemented in terms of WKWebFrame.
    WKDefaultWebControllerPrivate *data = ((WKDefaultWebControllerPrivate *)_controllerPrivate);

    [data->viewMap autorelease];
    data->viewMap = [[NSMutableDictionary alloc] init];
    
    [data->dataSourceMap autorelease];
    data->dataSourceMap = [[NSMutableDictionary alloc] init];
    
    [data->mainView autorelease];
    data->mainView = [view retain];
    [view _setController: self];
    
    [data->mainDataSource autorelease];
    data->mainDataSource = [dataSource retain];
    [dataSource _setController: self];

    if (dataSource != nil && view != nil){
        [data->viewMap setObject: view forKey: [WKObjectHolder holderWithObject:dataSource]];

        [data->dataSourceMap setObject: dataSource forKey: [WKObjectHolder holderWithObject:view]];
    }
    
    if (dataSource != nil)
        [view dataSourceChanged];
}

- (WKWebFrame *)createFrameNamed: (NSString *)fname for: (WKWebDataSource *)childDataSource inParent: (WKWebDataSource *)parentDataSource
{
    WKDefaultWebControllerPrivate *data = ((WKDefaultWebControllerPrivate *)_controllerPrivate);
    WKWebView *childView;
    WKWebFrame *newFrame;
    //WKDynamicScrollBarsView *scrollView;

    childView = [[WKWebView alloc] initWithFrame: NSMakeRect (0,0,0,0)];

    newFrame = [[[WKWebFrame alloc] initWithName: fname view: childView dataSource: childDataSource] autorelease];

    [parentDataSource addFrame: newFrame];

    [data->viewMap setObject: childView forKey: [WKObjectHolder holderWithObject:childDataSource]];
    [childView _setController: self];
    [data->dataSourceMap setObject: childDataSource forKey: [WKObjectHolder holderWithObject:childView]];
    [childDataSource _setController: self];

    
    //scrollView  = [[[WKDynamicScrollBarsView alloc] initWithFrame: NSMakeRect(0,0,0,0)] autorelease];
    //[childView _setFrameScrollView: scrollView];
        
    [childView dataSourceChanged];
        
    return newFrame;
}


- (WKWebView *)viewForDataSource: (WKWebDataSource *)dataSource
{
    WKDefaultWebControllerPrivate *data = ((WKDefaultWebControllerPrivate *)_controllerPrivate);
    return [data->viewMap objectForKey: [WKObjectHolder holderWithObject:dataSource]];
}


- (WKWebDataSource *)dataSourceForView: (WKWebView *)view
{
    WKDefaultWebControllerPrivate *data = ((WKDefaultWebControllerPrivate *)_controllerPrivate);
    return [data->dataSourceMap objectForKey: [WKObjectHolder holderWithObject:view]];
}

- (void)setMainView: (WKWebView *)m;
{
    WKDefaultWebControllerPrivate *data = ((WKDefaultWebControllerPrivate *)_controllerPrivate);
    [self setView: m andDataSource: data->mainDataSource];
}


- (WKWebView *)mainView
{
    WKDefaultWebControllerPrivate *data = ((WKDefaultWebControllerPrivate *)_controllerPrivate);
    return data->mainView;
}


- (void)setMainDataSource: (WKWebDataSource *)dataSource;
{
    WKDefaultWebControllerPrivate *data = ((WKDefaultWebControllerPrivate *)_controllerPrivate);
    [self setView: data->mainView andDataSource: dataSource];
}

- (WKWebDataSource *)mainDataSource
{
    WKDefaultWebControllerPrivate *data = ((WKDefaultWebControllerPrivate *)_controllerPrivate);
    return data->mainDataSource;
}





// ---------------------------------------------------------------------
// WKScriptContextHandler
// ---------------------------------------------------------------------
- (void)setStatusText: (NSString *)text forDataSource: (WKWebDataSource *)dataSource
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKDefaultWebController::setStatusText:forDataSource: is not implemented"];
}


- (NSString *)statusTextForDataSource: (WKWebDataSource *)dataSource
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKDefaultWebController::statusTextForDataSource: is not implemented"];
    return nil;
}


// ---------------------------------------------------------------------
// WKAuthenticationHandler
// ---------------------------------------------------------------------
- (WKSimpleAuthenticationResult *) authenticate: (WKSimpleAuthenticationRequest *)request
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKDefaultWebController::authenticate: is not implemented"];
    return nil;
}


// ---------------------------------------------------------------------
// WKLoadHandler
// ---------------------------------------------------------------------
- (void)receivedProgress: (WKLoadProgress *)progress forResource: (NSString *)resourceDescription fromDataSource: (WKWebDataSource *)dataSource
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKDefaultWebController::receivedProgress:forResource:fromDataSource: is not implemented"];
}


- (void)receivedError: (WKError *)error forResource: (NSString *)resourceDescription partialProgress: (WKLoadProgress *)progress fromDataSource: (WKWebDataSource *)dataSource
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKDefaultWebController::receivedError:forResource:partialProgress:fromDataSource: is not implemented"];
}


// ---------------------------------------------------------------------
// WKLocationChangeHandler
// ---------------------------------------------------------------------
- (BOOL)locationWillChangeTo: (NSURL *)url
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKDefaultWebController::locationWillChangeTo: is not implemented"];
    return NO;
}


- (void)locationChangeStartedForDataSource: (WKWebDataSource *)dataSource
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKDefaultWebController::locationWillChangeTo: is not implemented"];
}


- (void)locationChangeInProgressForDataSource: (WKWebDataSource *)dataSource
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKDefaultWebController::locationChangeInProgressForDataSource:forDataSource: is not implemented"];
}


- (void)locationChangeDone: (WKError *)error forDataSource: (WKWebDataSource *)dataSource
{
    WKWebView *view;
    
    view = [self viewForDataSource: dataSource];
    [view setNeedsLayout: YES];
    [view setNeedsDisplay: YES];
}

- (void)receivedPageTitle: (NSString *)title forDataSource: (WKWebDataSource *)dataSource
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKDefaultWebController::receivedPageTitle:forDataSource: is not implemented"];
}


- (void)serverRedirectTo: (NSURL *)url forDataSource: (WKWebDataSource *)dataSource
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKDefaultWebController::serverRedirectTo:forDataSource: is not implemented"];
}


@end