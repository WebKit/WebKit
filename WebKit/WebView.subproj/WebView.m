/*	
    WebController.mm
	Copyright 2001, 2002 Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDocument.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebException.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPolicyHandler.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitDebug.h>

#import <WebFoundation/WebFoundation.h>

@implementation WebController

- init
{
    return [self initWithView: nil provisionalDataSource: nil];
}

- initWithView: (WebView *)view provisionalDataSource: (WebDataSource *)dataSource
{
    [super init];
    
    _private = [[WebControllerPrivate alloc] init];
    _private->mainFrame = [[WebFrame alloc] initWithName: @"_top" webView: view provisionalDataSource: dataSource controller: self];

    return self;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (void)setDirectsAllLinksToSystemBrowser: (BOOL)flag
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebController::setDirectsAllLinksToSystemBrowser: is not implemented"];
}

- (BOOL)directsAllLinksToSystemBrowser
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebController::directsAllLinksToSystemBrowser is not implemented"];
    return NO;
}

- (WebFrame *)createFrameNamed: (NSString *)fname for: (WebDataSource *)childDataSource inParent: (WebDataSource *)parentDataSource allowsScrolling: (BOOL)allowsScrolling
{
    WebView *childView;
    WebFrame *newFrame;

    childView = [[[WebView alloc] initWithFrame: NSMakeRect (0,0,0,0)] autorelease];

    newFrame = [[[WebFrame alloc] initWithName: fname webView: childView provisionalDataSource: childDataSource controller: self] autorelease];

    [parentDataSource addFrame: newFrame];

    [childView _setController: self];
    [childDataSource _setController: self];

    [childView setAllowsScrolling: allowsScrolling];
        
    return newFrame;
}


- (void)setWindowContext: (id<WebWindowContext>)context
{
    [_private->windowContext autorelease];
    _private->windowContext = [context retain];
}

- (id<WebWindowContext>)windowContext
{
    return _private->windowContext;
}

- (void)setResourceProgressHandler: (id<WebResourceProgressHandler>)handler
{
    [_private->resourceProgressHandler autorelease];
    _private->resourceProgressHandler = [handler retain];
}


- (id<WebResourceProgressHandler>)resourceProgressHandler
{
    return _private->resourceProgressHandler;
}


- (void)setDownloadProgressHandler: (id<WebResourceProgressHandler>)handler
{
    [_private->downloadProgressHandler autorelease];
    _private->downloadProgressHandler = [handler retain];
}


- (id<WebResourceProgressHandler>)downloadProgressHandler
{
    return _private->downloadProgressHandler;
}


- (void)setPolicyHandler: (id<WebControllerPolicyHandler>)handler
{
    [_private->policyHandler autorelease];
    _private->policyHandler = [handler retain];
}

- (id<WebControllerPolicyHandler>)policyHandler
{
    return _private->policyHandler;
}


- (WebFrame *)_frameForDataSource: (WebDataSource *)dataSource fromFrame: (WebFrame *)frame
{
    NSArray *frames;
    int i, count;
    WebFrame *result, *aFrame;
    
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


- (WebFrame *)frameForDataSource: (WebDataSource *)dataSource
{
    WebFrame *frame = [self mainFrame];
    
    return [self _frameForDataSource: dataSource fromFrame: frame];
}


- (WebFrame *)_frameForView: (WebView *)aView fromFrame: (WebFrame *)frame
{
    NSArray *frames;
    int i, count;
    WebFrame *result, *aFrame;
    
    if ([frame webView] == aView)
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


- (WebFrame *)frameForView: (WebView *)aView
{
    WebFrame *frame = [self mainFrame];
    
    return [self _frameForView: aView fromFrame: frame];
}


- (WebFrame *)frameNamed: (NSString *)name
{
    return [[self mainFrame] frameNamed: name];
}

- (WebFrame *)mainFrame
{
    return _private->mainFrame;
}


+ (WebURLPolicy)defaultURLPolicyForURL: (NSURL *)url
{
    if([WebResourceHandle canInitWithURL:url]){
        return WebURLPolicyUseContentPolicy;
    }else{
        return WebURLPolicyOpenExternally;
    }
}


- (void)haveContentPolicy: (WebContentPolicy)policy andPath: (NSString *)path forDataSource: (WebDataSource *)dataSource
{
    if (policy == WebContentPolicyNone) {
        [NSException raise:NSInvalidArgumentException format:@"Can't set policy of WebContentPolicyNone. Use WebContentPolicyIgnore instead"];
    }
        
    if ([dataSource contentPolicy] != WebContentPolicyNone) {
        [NSException raise:NSGenericException format:@"Content policy can only be set once on for a dataSource."];
    }
    
    [dataSource _setContentPolicy:policy];
    [dataSource _setDownloadPath:path];
        
    if (policy == WebContentPolicyShow){
	if ([[self class] canShowMIMEType:[dataSource contentType]]){
	    WebView *webView = [[dataSource webFrame] webView];
	    [dataSource makeRepresentation];
	    [webView makeDocumentViewForMIMEType:[dataSource contentType]];
	    // FIXME: this ought to be part of makeDocumentView but I need to figure out
	    // the provisional / committed situation
	    [[webView documentView] provisionalDataSourceChanged:dataSource];
	} else {
	    WebError *error = [[WebError alloc] initWithErrorCode:WebErrorCannotShowMIMEType 
			           inDomain:WebErrorDomainWebKit failingURL: [dataSource inputURL]];
	    [[self policyHandler] unableToImplementContentPolicy:error forDataSource:dataSource];
	}
    }
}

- (void)stopAnimatedImages
{
}

- (void)startAnimatedImages
{
}

- (void)stopAnimatedImageLooping
{
}

- (void)startAnimatedImageLooping
{
}

+ (BOOL)canShowMIMEType:(NSString *)MIMEType
{
    if([WebView _canShowMIMEType:MIMEType] && [WebDataSource _canShowMIMEType:MIMEType]){
        return YES;
    }else{
        // Have the plug-ins register views and representations
        [WebPluginDatabase installedPlugins];
        if([WebView _canShowMIMEType:MIMEType] && [WebDataSource _canShowMIMEType:MIMEType])
            return YES;
    }
    return NO;
}

+ (BOOL)canShowFile:(NSString *)path
{    
    NSString *MIMEType;
    
    MIMEType = [[self class] _MIMETypeForFile:path];   
    return [[self class] canShowMIMEType:MIMEType];
}

@end
