/*	
    WebStandardPanels.h
    
    Copyright 2002 Apple, Inc. All rights reserved.
*/

#import <WebKit/WebStandardPanels.h>

#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebPanelAuthenticationHandler.h>
#import <WebKit/WebStandardPanelsPrivate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebAuthenticationManager.h>

#import <Carbon/Carbon.h>

@interface WebStandardPanelsPrivate : NSObject
{
@public
    WebPanelAuthenticationHandler *panelAuthenticationHandler;
    NSMutableDictionary *URLContainers;
}
@end

@implementation WebStandardPanelsPrivate

-(id)init
{
    self = [super init];
    if (self != nil) {
	URLContainers = [[NSMutableDictionary alloc] init];
    }
    return self;
}
    
-(void)dealloc
{
    [URLContainers release];
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

-(void)setUsesStandardAuthenticationPanel:(BOOL)use
{
    if (use) {
        if (![self usesStandardAuthenticationPanel]) {
            _privatePanels->panelAuthenticationHandler = [[WebPanelAuthenticationHandler alloc] init];
            [[WebAuthenticationManager sharedAuthenticationManager] addAuthenticationHandler:_privatePanels->panelAuthenticationHandler];
        }
    } else {
        if ([self usesStandardAuthenticationPanel]) {
            [[WebAuthenticationManager sharedAuthenticationManager] removeAuthenticationHandler:_privatePanels->panelAuthenticationHandler];
            [_privatePanels->panelAuthenticationHandler release];
            _privatePanels->panelAuthenticationHandler = nil;
        }        
    }
}

-(BOOL)usesStandardAuthenticationPanel
{
    return _privatePanels->panelAuthenticationHandler != nil;
}

-(void)didStartLoadingURL:(NSURL *)URL inWindow:(NSWindow *)window
{
    ASSERT_ARG(URL, URL);
    ASSERT_ARG(window, window);
    
    NSString *URLString = [URL absoluteString];
    
    NSCountedSet *set = [_privatePanels->URLContainers objectForKey:URLString];

    if (set == nil) {
	set = [NSCountedSet set];
	[_privatePanels->URLContainers setObject:set forKey:URLString];
    }

    [set addObject:window];
}

-(void)didStopLoadingURL:(NSURL *)URL inWindow:(NSWindow *)window
{
    ASSERT_ARG(URL, URL);
    ASSERT_ARG(window, window);
    
    NSString *URLString = [URL absoluteString];
    
    NSCountedSet *set = [_privatePanels->URLContainers objectForKey:URLString];

    ASSERT([set containsObject:window]);

    if (set == nil) {
	return;
    }

    [set removeObject:window];
    
    if ([set count] == 0) {
	[_privatePanels->URLContainers removeObjectForKey:URLString];
    }
}

-(void)_didStartLoadingURL:(NSURL *)URL inController:(WebView *)webView
{
    ASSERT_ARG(URL, URL);
    ASSERT_ARG(webView, webView);
    
    NSString *URLString = [URL absoluteString];
    
    NSCountedSet *set = [_privatePanels->URLContainers objectForKey:URLString];

    if (set == nil) {
	set = [NSCountedSet set];
	[_privatePanels->URLContainers setObject:set forKey:URLString];
    }

    [set addObject:webView];
}

-(void)_didStopLoadingURL:(NSURL *)URL inController:(WebView *)webView
{
    ASSERT_ARG(URL, URL);
    ASSERT_ARG(webView, webView);
    
    NSString *URLString = [URL absoluteString];
    
    NSCountedSet *set = [_privatePanels->URLContainers objectForKey:URLString];

    ASSERT([set containsObject:webView]);

    if (set == nil) {
	return;
    }

    [set removeObject:webView];
    
    if ([set count] == 0) {
	[_privatePanels->URLContainers removeObjectForKey:URLString];
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

-(NSWindow *)frontmostWindowLoadingURL:(NSURL *)URL
{
    NSString *URLString = [URL absoluteString];
    
    NSCountedSet *set = [_privatePanels->URLContainers objectForKey:URLString];

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
	    window = [[[object mainFrame] frameView] window];
	}

	if (window != nil && [window isVisible] && (frontmostWindow == nil || WindowInFront(window, frontmostWindow))) {
	    frontmostWindow = window;
	}
    }

    return frontmostWindow;
}

@end
