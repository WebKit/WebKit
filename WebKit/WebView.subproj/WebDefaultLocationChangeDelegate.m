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

- (void)locationChangeStartedForDataSource:(WebDataSource *)dataSource { }

- (void)serverRedirectedForDataSource:(WebDataSource *)dataSource { }

- (void)locationChangeCommittedForDataSource:(WebDataSource *)dataSource { }

- (void)receivedPageTitle:(NSString *)title forDataSource:(WebDataSource *)dataSource { }
- (void)receivedPageIcon:(NSImage *)image forDataSource:(WebDataSource *)dataSource { }

- (void)locationChangeDone:(WebError *)error forDataSource:(WebDataSource *)dataSource { }

- (void)willCloseLocationForDataSource:(WebDataSource *)dataSource { }

- (void)locationChangedWithinPageForDataSource:(WebDataSource *)dataSource { }

- (void)clientWillRedirectTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date forFrame:(WebFrame *)frame { }
- (void)clientRedirectCancelledForFrame:(WebFrame *)frame { }

@end
