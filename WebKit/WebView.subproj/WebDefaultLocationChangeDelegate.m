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

- (void)controller: (WebController *)controller locationChangeStartedForDataSource:(WebDataSource *)dataSource { }

- (void)controller: (WebController *)controller serverRedirectedForDataSource:(WebDataSource *)dataSource { }

- (void)controller: (WebController *)controller locationChangeCommittedForDataSource:(WebDataSource *)dataSource { }

- (void)controller: (WebController *)controller receivedPageTitle:(NSString *)title forDataSource:(WebDataSource *)dataSource { }
- (void)controller: (WebController *)controller receivedPageIcon:(NSImage *)image forDataSource:(WebDataSource *)dataSource { }

- (void)controller: (WebController *)controller locationChangeDone:(WebError *)error forDataSource:(WebDataSource *)dataSource { }

- (void)controller: (WebController *)controller willCloseLocationForDataSource:(WebDataSource *)dataSource { }

- (void)controller: (WebController *)controller locationChangedWithinPageForDataSource:(WebDataSource *)dataSource { }

- (void)controller: (WebController *)controller clientWillRedirectTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date forFrame:(WebFrame *)frame { }
- (void)controller: (WebController *)controller clientRedirectCancelledForFrame:(WebFrame *)frame { }

@end
