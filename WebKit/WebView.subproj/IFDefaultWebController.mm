/*	
    WKDefaultWebController.mm
	Copyright 2001, 2002 Apple, Inc. All rights reserved.
*/
#import <WebKit/WKDefaultWebController.h>
#import <WebKit/WKDefaultWebControllerPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebDataSourcePrivate.h>
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
    
    [view dataSourceChanged];
}


- (void)addFrame: (WKWebFrame *)childFrame toParent: (WKWebDataSource *)parent;
{
    WKDefaultWebControllerPrivate *data = ((WKDefaultWebControllerPrivate *)_controllerPrivate);
    id view = [childFrame view];
    WKWebDataSource *child = [childFrame dataSource];

    [data->viewMap setObject: view forKey: [WKObjectHolder holderWithObject:child]];
    [view _setController: self];
    [data->dataSourceMap setObject: child forKey: [WKObjectHolder holderWithObject:view]];
    [child _setController: self];

    [view dataSourceChanged];
    
    [child startLoading: YES];
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
    [NSException raise:WKMethodNotYetImplemented format:@"WKDefaultWebController::locationChangeDone:forDataSource: is not implemented"];
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