/*	
    IFBaseWebController.mm
	Copyright 2001, 2002 Apple, Inc. All rights reserved.
*/
#import <WebKit/IFBaseWebController.h>
#import <WebKit/IFBaseWebControllerPrivate.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebFrame.h>
#import <WebKit/IFException.h>


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


- (void)setMainView: (IFWebView *)view andMainDataSource: (IFWebDataSource *)dataSource
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
        
    [data->mainFrame autorelease];
    
    data->mainFrame = [[IFWebFrame alloc] init];
    [data->mainFrame setView: view];
    [view _setController: self];
    
    [data->mainFrame setDataSource: dataSource];
    [dataSource _setController: self];
    
    if (dataSource != nil)
        [view dataSourceChanged: dataSource];
}

- (IFWebFrame *)createFrameNamed: (NSString *)fname for: (IFWebDataSource *)childDataSource inParent: (IFWebDataSource *)parentDataSource
{
    IFWebView *childView;
    IFWebFrame *newFrame;
    IFDynamicScrollBarsView *scrollView;

    childView = [[IFWebView alloc] initWithFrame: NSMakeRect (0,0,0,0)];

    newFrame = [[[IFWebFrame alloc] initWithName: fname view: childView dataSource: childDataSource] autorelease];

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


- (void)setMainDataSource: (IFWebDataSource *)dataSource;
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
    [self setMainView: [data->mainFrame view] andMainDataSource: dataSource];
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
    [NSException raise:IFMethodNotYetImplemented format:@"IFBaseWebController::receivedProgress:forResource:fromDataSource: is not implemented"];
}


- (void)receivedError: (IFError *)error forResource: (NSString *)resourceDescription partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFBaseWebController::receivedError:forResource:partialProgress:fromDataSource: is not implemented"];
}


// ---------------------------------------------------------------------
// IFLocationChangeHandler
// ---------------------------------------------------------------------
- (BOOL)locationWillChangeTo: (NSURL *)url forFrame: (IFWebFrame *)frame;
{
    return YES;
}

- (void)changeLocationTo: (NSURL *)url forFrame: (IFWebFrame *)frame
{
    IFWebDataSource *dataSource = [[[IFWebDataSource alloc] initWithURL: url] autorelease];
    IFWebDataSource *oldDataSource;
    
    oldDataSource = [frame dataSource];
    [dataSource _setParent: [oldDataSource parent]];
    [dataSource _setController: self];
    [frame setDataSource: dataSource];
    
    [[frame view] dataSourceChanged: dataSource];
    
    //[dataSource startLoading: YES];
}


- (void)locationChangeStartedForFrame: (IFWebFrame *)frame
{
    // Do nothing.
}


- (void)locationChangeInProgressForFrame: (IFWebFrame *)frame
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


@end
