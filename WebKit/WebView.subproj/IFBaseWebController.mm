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

#import <WebKit/WebKitDebug.h>


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
    [super dealloc];
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
    NSScrollView *scrollView;

    childView = [[[IFWebView alloc] initWithFrame: NSMakeRect (0,0,0,0)] autorelease];

    newFrame = [[[IFWebFrame alloc] initWithName: fname view: childView provisionalDataSource: childDataSource controller: self] autorelease];

    [parentDataSource addFrame: newFrame];

    [childView _setController: self];
    [childDataSource _setController: self];

    scrollView  = [[[IFDynamicScrollBarsView alloc] initWithFrame: NSMakeRect(0,0,0,0)] autorelease];
    [scrollView setHasVerticalScroller: NO];
    [scrollView setHasHorizontalScroller: NO];
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
    // Do nothing.  Subclasses typically override this method.
}

- (void)receivedError: (IFError *)error forResource: (NSString *)resourceDescription partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource
{
    // Do nothing.  Subclasses typically override this method.
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
    // Do nothing.  Subclasses typically override this method.
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


- (IFWebFrame *)mainFrame
{
    IFBaseWebControllerPrivate *data = (IFBaseWebControllerPrivate *)_controllerPrivate;
    
    return data->mainFrame;
}


@end
