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


- (WebController *)controller:(WebController *)c createWindowWithRequest:(WebRequest *)request;
{
    return nil;
}

- (void)controllerShowWindow:(WebController *)c;
{
}

- (void)controllerShowWindowBehindFrontmost:(WebController *)c;
{
}

- (void)controllerCloseWindow:(WebController *)c;
{
}

- (void)controllerFocusWindow:(WebController *)c;
{
}

- (void)controllerUnfocusWindow:(WebController *)c;
{
}

- (NSResponder *)controllerFirstResponderInWindow:(WebController *)c;
{
    return nil;
}

- (void)controller:(WebController *)c makeFirstResponderInWindow:(NSResponder *)responder;
{
}

- (void)controller:(WebController *)c setStatusText:(NSString *)text;
{
}

- (NSString *)controllerStatusText:(WebController *)c;
{
    return nil;
}

- (void)controller:(WebController *)c mouseDidMoveOverElement:(NSDictionary *)elementInformation modifierFlags:(unsigned int)modifierFlags;
{
}

- (BOOL)controllerAreToolbarsVisible:(WebController *)c;
{
    return NO;
}

- (void)controller:(WebController *)c setToolbarsVisible:(BOOL)visible;
{
}

- (BOOL)controllerIsStatusBarVisible:(WebController *)c;
{
    return NO;
}

- (void)controller:(WebController *)c setStatusBarVisible:(BOOL)visible;
{
}

- (BOOL)controllerIsResizable:(WebController *)c;
{
    return NO;
}

- (void)controller:(WebController *)c setResizable:(BOOL)resizable;
{
}

- (void)controller:(WebController *)c setFrame:(NSRect)frame;
{
}

- (NSRect)controllerFrame:(WebController *)c;
{
    return NSMakeRect (0,0,0,0);
}

- (void)controller:(WebController *)c runJavaScriptAlertPanelWithMessage:(NSString *)message;
{
}

- (BOOL)controller:(WebController *)c runJavaScriptConfirmPanelWithMessage:(NSString *)message
{
    return NO;
}

- (NSString *)controller:(WebController *)c runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText
{
    return nil;
}

- (void)controller:(WebController *)c runOpenPanelForFileButtonWithResultListener:(id<WebOpenPanelResultListener>)resultListener
{
}

@end
