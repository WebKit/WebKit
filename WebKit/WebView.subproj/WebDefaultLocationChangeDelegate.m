/*	
        WebLocationChangeDelegate.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/
#import <WebFoundation/WebError.h>

#import <WebKit/WebDefaultLocationChangeDelegate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>

@implementation WebDefaultLocationChangeDelegate

static WebDefaultLocationChangeDelegate *sharedDelegate = nil;

// Return a object with vanilla implementations of the protocol's methods
// Note this feature relies on our default delegate being stateless
+ (WebDefaultLocationChangeDelegate *)sharedLocationChangeDelegate
{
    if (!sharedDelegate) {
        sharedDelegate = [[WebDefaultLocationChangeDelegate alloc] init];
    }
    return sharedDelegate;
}

- (void)webView: (WebView *)wv locationChangeStartedForDataSource:(WebDataSource *)dataSource { }

- (void)webView: (WebView *)wv serverRedirectedForDataSource:(WebDataSource *)dataSource { }

- (void)webView: (WebView *)wv locationChangeCommittedForDataSource:(WebDataSource *)dataSource { }

- (void)webView: (WebView *)wv receivedPageTitle:(NSString *)title forDataSource:(WebDataSource *)dataSource { }
- (void)webView: (WebView *)wv receivedPageIcon:(NSImage *)image forDataSource:(WebDataSource *)dataSource { }

- (void)webView: (WebView *)wv locationChangeDone:(WebError *)error forDataSource:(WebDataSource *)dataSource { }

- (void)webView: (WebView *)wv willCloseLocationForDataSource:(WebDataSource *)dataSource { }

- (void)webView: (WebView *)wv locationChangedWithinPageForDataSource:(WebDataSource *)dataSource { }

- (void)webView: (WebView *)wv clientWillRedirectTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date forFrame:(WebFrame *)frame { }
- (void)webView: (WebView *)wv clientRedirectCancelledForFrame:(WebFrame *)frame { }

@end
