/*	
    IFWebController.mm
	Copyright 2001, 2002 Apple, Inc. All rights reserved.
*/

#import <WebKit/IFDocument.h>
#import <WebKit/IFDynamicScrollBarsView.h>
#import <WebKit/IFException.h>
#import <WebKit/IFPluginDatabase.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebControllerPrivate.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFWebController.h>
#import <WebKit/WebKitDebug.h>

#import <WebFoundation/WebFoundation.h>
#import <WebFoundation/IFFileTypeMappings.h>

@implementation IFWebController

- init
{
    return [self initWithView: nil provisionalDataSource: nil];
}

- initWithView: (IFWebView *)view provisionalDataSource: (IFWebDataSource *)dataSource
{
    [super init];
    
    _private = [[IFWebControllerPrivate alloc] init];
    _private->mainFrame = [[IFWebFrame alloc] initWithName: @"_top" webView: view provisionalDataSource: dataSource controller: self];

    return self;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (void)setDirectsAllLinksToSystemBrowser: (BOOL)flag
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebController::setDirectsAllLinksToSystemBrowser: is not implemented"];
}

- (BOOL)directsAllLinksToSystemBrowser
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebController::directsAllLinksToSystemBrowser is not implemented"];
    return NO;
}

- (IFWebFrame *)createFrameNamed: (NSString *)fname for: (IFWebDataSource *)childDataSource inParent: (IFWebDataSource *)parentDataSource inScrollView: (BOOL)inScrollView
{
    IFWebView *childView;
    IFWebFrame *newFrame;

    childView = [[[IFWebView alloc] initWithFrame: NSMakeRect (0,0,0,0)] autorelease];

    newFrame = [[[IFWebFrame alloc] initWithName: fname webView: childView provisionalDataSource: childDataSource controller: self] autorelease];

    [parentDataSource addFrame: newFrame];

    [childView _setController: self];
    [childDataSource _setController: self];

    [childView setAllowsScrolling: inScrollView];
        
    return newFrame;
}



// ---------------------------------------------------------------------
// IFScriptContextHandler
// ---------------------------------------------------------------------
- (void)setStatusText: (NSString *)text forDataSource: (IFWebDataSource *)dataSource
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebController::setStatusText:forDataSource: is not implemented"];
}


- (NSString *)statusTextForDataSource: (IFWebDataSource *)dataSource
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebController::statusTextForDataSource: is not implemented"];
    return nil;
}


- (IFWebController *)openNewWindowWithURL:(NSURL *)url
{
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


- (IFWebFrame *)_frameForView: (IFWebView *)aView fromFrame: (IFWebFrame *)frame
{
    NSArray *frames;
    int i, count;
    IFWebFrame *result, *aFrame;
    
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


- (IFWebFrame *)frameForView: (IFWebView *)aView
{
    IFWebFrame *frame = [self mainFrame];
    
    return [self _frameForView: aView fromFrame: frame];
}


- (IFWebFrame *)frameNamed: (NSString *)name
{
    return [[self mainFrame] frameNamed: name];
}

- (IFWebFrame *)mainFrame
{
    return _private->mainFrame;
}

- (void)pluginNotFoundForMIMEType:(NSString *)mime pluginPageURL:(NSURL *)url
{
    // Do nothing.  Subclasses typically override this method.
}

- (id <IFLocationChangeHandler>)provideLocationChangeHandlerForFrame: (IFWebFrame *)frame andURL: (NSURL *)url
{
    return nil;
}

- (IFURLPolicy)URLPolicyForURL: (NSURL *)url
{
    if([IFURLHandle canInitWithURL:url]){
        return IFURLPolicyUseContentPolicy;
    }else{
        return IFURLPolicyOpenExternally;
    }
}

- (void)unableToImplementURLPolicyForURL: (NSURL *)url error: (IFError *)error
{
}


- (void)haveContentPolicy: (IFContentPolicy)policy andPath: (NSString *)path forLocationChangeHandler: (id <IFLocationChangeHandler>)handler
{
    IFWebDataSource *mainDataSource, *mainProvisionalDataSource, *dataSource;
    NSString *MIMEType;
    
    if(policy == IFContentPolicyNone)
        [NSException raise:NSInvalidArgumentException format:@"Can't set policy of IFContentPolicyNone. Use IFContentPolicyIgnore instead"];
        
    mainProvisionalDataSource = [_private->mainFrame provisionalDataSource];
    mainDataSource = [_private->mainFrame dataSource];
    
    dataSource = [mainDataSource _recursiveDataSourceForLocationChangeHandler:handler];
    if(!dataSource)
        dataSource = [mainProvisionalDataSource _recursiveDataSourceForLocationChangeHandler:handler];
        
    if(dataSource){
        if([dataSource contentPolicy] != IFContentPolicyNone){
            [NSException raise:NSGenericException format:@"Content policy can only be set once on a location change handler."];
        }else{
            [dataSource _setContentPolicy:policy];
            [dataSource _setDownloadPath:path];
            
            if(policy == IFContentPolicyShow){
                MIMEType = [dataSource contentType];
                if([[self class] canShowMIMEType:MIMEType]){
                    id documentView;
                    IFWebView *webView;
                    id <IFDocumentRepresentation> dataRepresentation;
                    
                    dataRepresentation = [IFWebDataSource createRepresentationForMIMEType:MIMEType];
                    [dataSource _setRepresentation:dataRepresentation];
                    webView = [[dataSource webFrame] webView];
                    documentView = [IFWebView createViewForMIMEType:MIMEType];
                    [webView _setDocumentView: documentView];
                    [documentView provisionalDataSourceChanged: dataSource];
                }else{
                    // return error with unableToImplementContentPolicy
                }
            }
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
    if([IFWebView _canShowMIMEType:MIMEType] && [IFWebDataSource _canShowMIMEType:MIMEType]){
        return YES;
    }else{
        // Have the plug-ins register views and representations
        [IFPluginDatabase installedPlugins];
        if([IFWebView _canShowMIMEType:MIMEType] && [IFWebDataSource _canShowMIMEType:MIMEType])
            return YES;
    }
    return NO;
}

+ (BOOL)canShowFile:(NSString *)path
{
    NSString *MIMEType, *extension = [path pathExtension];
    
    MIMEType = [[IFFileTypeMappings sharedMappings] MIMETypeForExtension:extension];
    
    return [[self class] canShowMIMEType:MIMEType];
}

@end
