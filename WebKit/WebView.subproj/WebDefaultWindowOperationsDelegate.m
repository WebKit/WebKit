/*	
    WebDefaultPolicyDelegate.m
    Copyright 2003, Apple Computer, Inc.
*/
#import <Cocoa/Cocoa.h>

#import <WebFoundation/WebRequest.h>

#import <WebKit/WebController.h>
#import <WebKit/WebDefaultWindowOperationsDelegate.h>
#import <WebKit/WebWindowOperationsDelegate.h>


@implementation WebDefaultWindowOperationsDelegate

static WebDefaultWindowOperationsDelegate *sharedDelegate = nil;

// Return a object with vanilla implementations of the protocol's methods
// Note this feature relies on our default delegate being stateless.  This
// is probably an invalid assumption for the WebWindowOperationsDelegate.
// If we add any real functionality to this default delegate we probably
// won't be able to use a singleton.
+ (WebDefaultWindowOperationsDelegate *)sharedWindowOperationsDelegate
{
    if (!sharedDelegate) {
        sharedDelegate = [[WebDefaultWindowOperationsDelegate alloc] init];
    }
    return sharedDelegate;
}


- (WebController *)controller: (WebController *)wv createWindowWithRequest:(WebRequest *)request;
{
    return nil;
}

- (void)controllerShowWindow: (WebController *)wv;
{
}

- (void)controllerShowWindowBehindFrontmost: (WebController *)wv;
{
}

- (void)controllerCloseWindow: (WebController *)wv;
{
}

- (void)controllerFocusWindow: (WebController *)wv;
{
}

- (void)controllerUnfocusWindow: (WebController *)wv;
{
}

- (NSResponder *)controllerFirstResponderInWindow: (WebController *)wv;
{
    return nil;
}

- (void)controller: (WebController *)wv makeFirstResponderInWindow:(NSResponder *)responder;
{
}

- (void)controller: (WebController *)wv setStatusText:(NSString *)text;
{
}

- (NSString *)controllerStatusText: (WebController *)wv;
{
    return nil;
}

- (void)controller: (WebController *)wv mouseDidMoveOverElement:(NSDictionary *)elementInformation modifierFlags:(unsigned int)modifierFlags;
{
}

- (BOOL)controllerAreToolbarsVisible: (WebController *)wv;
{
    return NO;
}

- (void)controller: (WebController *)wv setToolbarsVisible:(BOOL)visible;
{
}

- (BOOL)controllerIsStatusBarVisible: (WebController *)wv;
{
    return NO;
}

- (void)controller: (WebController *)wv setStatusBarVisible:(BOOL)visible;
{
}

- (BOOL)controllerIsResizable: (WebController *)wv;
{
    return NO;
}

- (void)controller: (WebController *)wv setResizable:(BOOL)resizable;
{
}

- (void)controller: (WebController *)wv setFrame:(NSRect)frame;
{
}

- (NSRect)controllerFrame: (WebController *)wv;
{
    return NSMakeRect (0,0,0,0);
}

- (void)controller: (WebController *)wv runJavaScriptAlertPanelWithMessage:(NSString *)message;
{
}

- (BOOL)controller: (WebController *)wv runJavaScriptConfirmPanelWithMessage:(NSString *)message
{
    return NO;
}

- (NSString *)controller: (WebController *)wv runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText
{
    return nil;
}

- (void)controller: (WebController *)wv runOpenPanelForFileButtonWithResultListener:(id<WebOpenPanelResultListener>)resultListener
{
}

@end
