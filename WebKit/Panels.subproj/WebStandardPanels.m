/*	
    WebStandardPanels.h
    
    Copyright 2002 Apple, Inc. All rights reserved.
*/

#import <WebKit/WebStandardPanels.h>
#import <WebKit/WebStandardPanelsPrivate.h>
#import <WebKit/WebPanelAuthenticationHandler.h>
#import <WebKit/WebPanelCookieAcceptHandler.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebView.h>
#import <WebFoundation/WebAuthenticationManager.h>
#import <WebFoundation/WebCookieManager.h>

#import <Carbon/Carbon.h>

@interface WebStandardPanelsPrivate : NSObject
{
@public
    WebPanelAuthenticationHandler *panelAuthenticationHandler;
    WebPanelCookieAcceptHandler *panelCookieAcceptHandler;
    NSMutableDictionary *urlContainers;
}
@end

@implementation WebStandardPanelsPrivate

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

@end

@implementation WebStandardPanels

// Private init method to implement the singleton pattern
-(id)_init
{
    self = [super init];
    if (self != nil) {
        _privatePanels = [[WebStandardPanelsPrivate alloc] init];
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

static WebStandardPanels *sharedStandardPanels = NULL;

static void initSharedStandardPanels(void)
{
    sharedStandardPanels = [[WebStandardPanels alloc] _init];
}

+(WebStandardPanels *)sharedStandardPanels
{
    static pthread_once_t sharedStandardPanelsOnce = PTHREAD_ONCE_INIT;
    pthread_once(&sharedStandardPanelsOnce, initSharedStandardPanels);
    return sharedStandardPanels;
}

-(void)setUseStandardAuthenticationPanel:(BOOL)use
{
    if (use) {
        if (![self useStandardAuthenticationPanel]) {
            _privatePanels->panelAuthenticationHandler = [[WebPanelAuthenticationHandler alloc] init];
            [[WebAuthenticationManager sharedAuthenticationManager] addAuthenticationHandler:_privatePanels->panelAuthenticationHandler];
        }
    } else {
        if ([self useStandardAuthenticationPanel]) {
            [[WebAuthenticationManager sharedAuthenticationManager] removeAuthenticationHandler:_privatePanels->panelAuthenticationHandler];
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
    if (use) {
        if (![self useStandardCookieAcceptPanel]) {
            _privatePanels->panelCookieAcceptHandler = [[WebPanelCookieAcceptHandler alloc] init];
            [[WebCookieManager sharedCookieManager] addAcceptHandler:_privatePanels->panelCookieAcceptHandler];
        }
    } else {
        if ([self useStandardCookieAcceptPanel]) {
            [[WebCookieManager sharedCookieManager] removeAcceptHandler:_privatePanels->panelCookieAcceptHandler];
            [_privatePanels->panelCookieAcceptHandler release];
            _privatePanels->panelCookieAcceptHandler = nil;
        }
    }
}

-(BOOL)useStandardCookieAcceptPanel
{
    return _privatePanels->panelCookieAcceptHandler != nil;
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
	[_privatePanels->urlContainers removeObjectForKey:url];
    }
}

-(void)_didStartLoadingURL:(NSURL *)url inController:(WebController *)controller
{
    NSCountedSet *set = [_privatePanels->urlContainers objectForKey:url];

    if (set == nil) {
	set = [NSCountedSet set];
	[_privatePanels->urlContainers setObject:set forKey:url];
    }

    [set addObject:controller];
}

-(void)_didStopLoadingURL:(NSURL *)url inController:(WebController *)controller
{
    NSCountedSet *set = [_privatePanels->urlContainers objectForKey:url];

    if (set == nil) {
	return;
    }

    [set removeObject:controller];
    
    if ([set count] == 0) {
	[_privatePanels->urlContainers removeObjectForKey:url];
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

	if (window != nil && [window isVisible] && (frontmostWindow == nil || WindowInFront(window, frontmostWindow))) {
	    frontmostWindow = window;
	}
    }

    return frontmostWindow;
}

@end



