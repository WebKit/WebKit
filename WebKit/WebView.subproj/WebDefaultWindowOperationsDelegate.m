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


- (WebController *)createWindowWithRequest:(WebRequest *)request;
{
    return nil;
}

- (void)showWindow;
{
}

- (void)showWindowBehindFrontmost;
{
}

- (void)closeWindow;
{
}

- (void)focusWindow;
{
}

- (void)unfocusWindow;
{
}

- (NSResponder *)firstResponderInWindow;
{
    return nil;
}

- (void)makeFirstResponderInWindow:(NSResponder *)responder;
{
}

- (void)setStatusText:(NSString *)text;
{
}

- (NSString *)statusText;
{
    return nil;
}

- (void)mouseDidMoveOverElement:(NSDictionary *)elementInformation modifierFlags:(unsigned int)modifierFlags;
{
}

- (BOOL)areToolbarsVisible;
{
    return NO;
}

- (void)setToolbarsVisible:(BOOL)visible;
{
}

- (BOOL)isStatusBarVisible;
{
    return NO;
}

- (void)setStatusBarVisible:(BOOL)visible;
{
}

- (BOOL)isResizable;
{
    return NO;
}

- (void)setResizable:(BOOL)resizable;
{
}

- (void)setFrame:(NSRect)frame;
{
}

- (NSRect)frame;
{
    return NSMakeRect (0,0,0,0);
}

- (void)runJavaScriptAlertPanelWithMessage:(NSString *)message;
{
}

- (BOOL)runJavaScriptConfirmPanelWithMessage:(NSString *)message
{
    return NO;
}

- (NSString *)runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText
{
    return nil;
}

- (void)runOpenPanelForFileButtonWithResultListener:(id<WebOpenPanelResultListener>)resultListener
{
}

@end
