/*	
        WebDefaultFrameLoadDelegate.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/
#import <WebKit/WebDefaultFrameLoadDelegate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>

@implementation WebDefaultFrameLoadDelegate

static WebDefaultFrameLoadDelegate *sharedDelegate = nil;

// Return a object with vanilla implementations of the protocol's methods
// Note this feature relies on our default delegate being stateless
+ (WebDefaultFrameLoadDelegate *)sharedFrameLoadDelegate
{
    if (!sharedDelegate) {
        sharedDelegate = [[WebDefaultFrameLoadDelegate alloc] init];
    }
    return sharedDelegate;
}

- (void)webView:(WebView *)sender didStartProvisionalLoadForFrame:(WebFrame *)frame { }

- (void)webView:(WebView *)sender didReceiveServerRedirectForProvisionalLoadForFrame:(WebFrame *)frame { }

- (void)webView:(WebView *)sender didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame { }

- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)frame { }

- (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title forFrame:(WebFrame *)frame { }

- (void)webView:(WebView *)sender didReceiveIcon:(NSImage *)image forFrame:(WebFrame *)frame { }

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame { }

- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame { }

- (void)webView:(WebView *)sender didChangeLocationWithinPageForFrame:(WebFrame *)frame { }

- (void)webView:(WebView *)sender willPerformClientRedirectToURL:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date forFrame:(WebFrame *)frame { }

- (void)webView:(WebView *)sender didCancelClientRedirectForFrame:(WebFrame *)frame { }

- (void)webView:(WebView *)sender willCloseFrame:(WebFrame *)frame { }

@end
