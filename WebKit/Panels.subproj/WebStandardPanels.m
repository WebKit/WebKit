/*	
    IFStandardPanels.h
    
    Copyright 2002 Apple, Inc. All rights reserved.
*/

#import <WebKit/IFStandardPanels.h>
#import <WebKit/IFStandardPanelsPrivate.h>
#import <WebKit/IFPanelAuthenticationHandler.h>
#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebView.h>
#import <WebFoundation/IFAuthenticationManager.h>
#import <WebFoundation/IFCookieManager.h>

#import <Carbon/Carbon.h>

@interface IFStandardPanelsPrivate : NSObject
{
@public
    IFPanelAuthenticationHandler *panelAuthenticationHandler;
    NSMutableDictionary *urlContainers;
}
@end

@implementation IFStandardPanelsPrivate
-(id)init
{
    self = [super init];
    if (self != nil) {
	urlContainers = [[NSMutableDictionary alloc] init];
    }
    return self;
}
    
-(void)dealloc
{
    [urlContainers release];
    [panelAuthenticationHandler release];
    [super dealloc];
}

// Private init method to implement the singleton pattern
-(id)_init
{
    self = [super init];
    if (self != nil) {
        _privatePanels = [[IFStandardPanelsPrivate alloc] init];
    }
    return self;
}

-(id)init
{
    // FIXME: should assert instead (or return a reference to the shared instance)
    [self release];
    return nil;
}

-(void)dealloc
{
    [_privatePanels release];
    [super dealloc];
}

// The next line is a workaround for Radar 2905545. Once that's fixed, it can use PTHREAD_ONCE_INIT.
pthread_once_t sharedStandardPanelsOnce = {_PTHREAD_ONCE_SIG_init, {}};
IFStandardPanels *sharedStandardPanels = NULL;

static void initSharedStandardPanels(void)
{
    sharedStandardPanels = [[IFStandardPanels alloc] _init];
}

+(IFStandardPanels *)sharedStandardPanels
{
    pthread_once(&sharedStandardPanelsOnce, initSharedStandardPanels);
    return sharedStandardPanels;
}

-(void)setUseStandardAuthenticationPanel:(BOOL)use
{
    if (use) {
        if (![self useStandardAuthenticationPanel]) {
            _privatePanels->panelAuthenticationHandler = [[IFPanelAuthenticationHandler alloc] init];
            [[IFAuthenticationManager sharedAuthenticationManager] addAuthenticationHandler:_privatePanels->panelAuthenticationHandler];
        }
    } else {
        if ([self useStandardAuthenticationPanel]) {
            [[IFAuthenticationManager sharedAuthenticationManager] removeAuthenticationHandler:_privatePanels->panelAuthenticationHandler];
            [_privatePanels->panelAuthenticationHandler release];
            _privatePanels->panelAuthenticationHandler = nil;
        }        
    }
}

-(BOOL)useStandardAuthenticationPanel
{
    return _privatePanels->panelAuthenticationHandler != nil;
}

-(void)setUseStandardCookieAcceptPanel:(BOOL)use
{
}

-(BOOL)useStandardCookieAcceptPanel
{
    return FALSE;
}

-(void)didStartLoadingURL:(NSURL *)url inWindow:(NSWindow *)window
{
    NSCountedSet *set = [_privatePanels->urlContainers objectForKey:url];

    if (set == nil) {
	set = [NSCountedSet set];
	[_privatePanels->urlContainers setObject:set forKey:url];
    }

    [set addObject:window];
}

-(void)didStopLoadingURL:(NSURL *)url inWindow:(NSWindow *)window
{
    NSCountedSet *set = [_privatePanels->urlContainers objectForKey:url];

    if (set == nil) {
	return;
    }

    [set removeObject:window];
    
    if ([set count] == 0) {
	[_privatePanels->urlContainers objectForKey:url];
    }
}

-(void)_didStartLoadingURL:(NSURL *)url inController:(IFWebController *)controller
{
    NSCountedSet *set = [_privatePanels->urlContainers objectForKey:url];

    if (set == nil) {
	set = [NSCountedSet set];
	[_privatePanels->urlContainers setObject:set forKey:url];
    }

    [set addObject:controller];
}

-(void)_didStopLoadingURL:(NSURL *)url inController:(IFWebController *)controller
{
    NSCountedSet *set = [_privatePanels->urlContainers objectForKey:url];

    if (set == nil) {
	return;
    }

    [set removeObject:controller];
    
    if ([set count] == 0) {
	[_privatePanels->urlContainers objectForKey:url];
    }
}

static BOOL WindowInFront(NSWindow *a, NSWindow *b)
{
    UInt32 aIndex;
    UInt32 bIndex;

    GetWindowIndex([a windowRef], NULL, kWindowGroupContentsReturnWindows, &aIndex);
    GetWindowIndex([b windowRef], NULL, kWindowGroupContentsReturnWindows, &bIndex);

    return aIndex < bIndex;
    // void NSWindowList(int size, int list[])
    // void NSCountWindowsForContext(int context, int *count)
    // void NSWindowListForContext(int context, int size, int list[])
}

-(NSWindow *)frontmostWindowLoadingURL:(NSURL *)url
{
    NSCountedSet *set = [_privatePanels->urlContainers objectForKey:url];

    if (set == nil) {
	return nil;
    }

    NSEnumerator *enumerator = [set objectEnumerator];
    NSWindow *frontmostWindow = nil;
    id object;

    while ((object = [enumerator nextObject]) != nil) {
	NSWindow *window;
	if ([object isKindOfClass:[NSWindow class]]) {
	    window = object;
	} else {
	    window = [[[object mainFrame] webView] window];
	}

	if (window != nil && (frontmostWindow == nil || WindowInFront(window, frontmostWindow))) {
	    frontmostWindow = window;
	}
    }

    return frontmostWindow;
}

@end



