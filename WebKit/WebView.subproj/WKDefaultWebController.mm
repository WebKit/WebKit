/*	
    WKDefaultWebController.mm
	Copyright 2001, 2002 Apple, Inc. All rights reserved.
*/
#import <WebKit/WKDefaultWebController.h>
#import <WebKit/WKDefaultWebControllerPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebDataSourcePrivate.h>
#import <WebKit/WKException.h>


// Used so we can use objects as keys in dictionaries without
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

    [data->viewMap release];
    data->viewMap = [[NSMutableDictionary alloc] init];
    
    [data->dataSourceMap release];
    data->dataSourceMap = [[NSMutableDictionary alloc] init];
    
    [data->mainView release];
    data->mainView = [view retain];
    [view _setController: self];
    
    [data->mainDataSource release];
    data->mainDataSource = [dataSource retain];
    [dataSource _setController: self];

    [data->viewMap setObject: dataSource forKey: [WKObjectHolder holderWithObject:view]];
    [data->dataSourceMap setObject: view forKey: [WKObjectHolder holderWithObject:dataSource]];
    
    [view dataSourceChanged];
}

- (WKWebView *)viewForDataSource: (WKWebDataSource *)dataSource
{
    WKDefaultWebControllerPrivate *data = ((WKDefaultWebControllerPrivate *)_controllerPrivate);
    return [data->viewMap objectForKey: dataSource];
}


- (WKWebDataSource *)dataSourceForView: (WKWebView *)view
{
    WKDefaultWebControllerPrivate *data = ((WKDefaultWebControllerPrivate *)_controllerPrivate);
    return [data->dataSourceMap objectForKey: view];
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


- (void)createViewForDataSource: (WKWebDataSource *)dataSource inFrameNamed: (NSString *)name
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKDefaultWebController::createViewForDataSource:inFrameNamed: is not implemented"];
}


- (void)createViewForDataSource: (WKWebDataSource *)dataSource inIFrame: (id)iFrameIdentifier
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKDefaultWebController::createViewForDataSource:inIFrame: is not implemented"];
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