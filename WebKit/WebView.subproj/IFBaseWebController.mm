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
    [self setView: view andDataSource: dataSource];
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


- (void)setView: (IFWebView *)view andDataSource: (IFWebDataSource *)dataSource
{
    // FIXME:  this needs to be implemented in terms of IFWebFrame.
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);

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
        [data->viewMap setObject: view forKey: [IFObjectHolder holderWithObject:dataSource]];

        [data->dataSourceMap setObject: dataSource forKey: [IFObjectHolder holderWithObject:view]];
    }
    
    if (dataSource != nil)
        [view dataSourceChanged];
}

- (IFWebFrame *)createFrameNamed: (NSString *)fname for: (IFWebDataSource *)childDataSource inParent: (IFWebDataSource *)parentDataSource
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
    IFWebView *childView;
    IFWebFrame *newFrame;
    //IFDynamicScrollBarsView *scrollView;

    childView = [[IFWebView alloc] initWithFrame: NSMakeRect (0,0,0,0)];

    newFrame = [[[IFWebFrame alloc] initWithName: fname view: childView dataSource: childDataSource] autorelease];

    [parentDataSource addFrame: newFrame];

    [data->viewMap setObject: childView forKey: [IFObjectHolder holderWithObject:childDataSource]];
    [childView _setController: self];
    [data->dataSourceMap setObject: childDataSource forKey: [IFObjectHolder holderWithObject:childView]];
    [childDataSource _setController: self];

    
    //scrollView  = [[[IFDynamicScrollBarsView alloc] initWithFrame: NSMakeRect(0,0,0,0)] autorelease];
    //[childView _setFrameScrollView: scrollView];
        
    [childView dataSourceChanged];
        
    return newFrame;
}


- (IFWebView *)viewForDataSource: (IFWebDataSource *)dataSource
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
    return [data->viewMap objectForKey: [IFObjectHolder holderWithObject:dataSource]];
}


- (IFWebDataSource *)dataSourceForView: (IFWebView *)view
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
    return [data->dataSourceMap objectForKey: [IFObjectHolder holderWithObject:view]];
}

- (void)setMainView: (IFWebView *)m;
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
    [self setView: m andDataSource: data->mainDataSource];
}


- (IFWebView *)mainView
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
    return data->mainView;
}


- (void)setMainDataSource: (IFWebDataSource *)dataSource;
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
    [self setView: data->mainView andDataSource: dataSource];
}

- (IFWebDataSource *)mainDataSource
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
    return data->mainDataSource;
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
- (BOOL)locationWillChangeTo: (NSURL *)url
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFBaseWebController::locationWillChangeTo: is not implemented"];
    return NO;
}


- (void)locationChangeStartedForDataSource: (IFWebDataSource *)dataSource
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFBaseWebController::locationWillChangeTo: is not implemented"];
}


- (void)locationChangeInProgressForDataSource: (IFWebDataSource *)dataSource
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFBaseWebController::locationChangeInProgressForDataSource:forDataSource: is not implemented"];
}


- (void)locationChangeDone: (IFError *)error forDataSource: (IFWebDataSource *)dataSource
{
    IFWebView *view;
    
    view = [self viewForDataSource: dataSource];
    [view setNeedsLayout: YES];
    [view setNeedsDisplay: YES];
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
